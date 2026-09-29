#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "knomi.h"
#include "moonraker.h"
#include "knomi_ble.h"

#define LINK_TIMEOUT_MS 5000   // plugin sends status at least every 2 s

static bool running = false;
static NimBLEServer *server = nullptr;
static NimBLECharacteristic *ch_cmd = nullptr;

static volatile bool authed = false;
static volatile uint16_t authed_conn = 0xFFFF;
static volatile uint32_t last_status_ms = 0;
static volatile uint32_t pending_passkey = 0;
static volatile int pair_result = 0;
static volatile bool pairing = false;   // a passkey was shown for this connection
static volatile bool wifi_request = false;

static SemaphoreHandle_t lock;
static String status_buf;          // latest status JSON (guarded by lock)
static bool status_new = false;
static String files_accum;         // frames being assembled (NimBLE task only)
static String files_cached;        // guarded by lock
static bool files_valid = false;

// status flag bits, same order as the plugin's FLAGS tuple
enum {
    K_HOMING = 0, K_PROBING, K_QGLING, K_HEAT_NOZZLE, K_HEAT_BED,
    K_SHAPING, K_PID, K_CLEANING, K_FILAMENT, K_PAUSED, K_RUNOUT,
};

bool knomi_ble_apply_status(const char *json, size_t len) {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, json, len) != DeserializationError::Ok) return false;
    moonraker_data_t &d = moonraker.data;
    moonraker.data_unlock = false;
    moonraker.unconnected = false;
    moonraker.auth_failed = false;
    moonraker.unready = !(doc["o"] | 0);
    d.printing = doc["p"] | 0;
    d.pause = doc["pa"] | 0;
    d.printing |= d.pause;
    int g = doc["g"] | 0;
    d.progress = (uint8_t)constrain(g, 0, 100);
    d.print_time = doc["t"] | 0;
    d.time_left = doc["l"] | -1;
    d.z_um = doc.containsKey("z") ? (int32_t)(doc["z"] | 0) : INT32_MIN;
    d.layer = doc["ly"][0] | 0;
    d.layer_total = doc["ly"][1] | 0;
    strlcpy(d.file_path, doc["n"] | "", sizeof(d.file_path));
    d.nozzle_actual = doc["nt"][0] | 0;
    d.nozzle_target = doc["nt"][1] | 0;
    d.bed_actual = doc["bt"][0] | 0;
    d.bed_target = doc["bt"][1] | 0;
    uint32_t k = doc["k"] | 0;
    d.homing = k & (1 << K_HOMING);
    d.probing = k & (1 << K_PROBING);
    d.qgling = k & (1 << K_QGLING);
    d.heating_nozzle = k & (1 << K_HEAT_NOZZLE);
    d.heating_bed = k & (1 << K_HEAT_BED);
    d.shaping = k & (1 << K_SHAPING);
    d.pid_tuning = k & (1 << K_PID);
    d.cleaning = k & (1 << K_CLEANING);
    d.filament = k & (1 << K_FILAMENT);
    d.paused_ext = k & (1 << K_PAUSED);
    d.runout = k & (1 << K_RUNOUT);
    d.fan = doc["f"] | 0;
    d.speed = doc["sp"] | 0;
    if (doc.containsKey("mi")) moonraker_set_msg(doc["m"] | "", doc["mi"] | 0L);
    if (doc["w"] | 0) wifi_request = true;
    moonraker.data_unlock = true;
    return true;
}

class ServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *s, ble_gap_conn_desc *desc) override {
        Serial.println("ble: central connected");
        // keep advertising off while someone is connected; restarted on disconnect
    }
    void onDisconnect(NimBLEServer *s, ble_gap_conn_desc *desc) override {
        if (desc->conn_handle == authed_conn) {
            authed = false;
            authed_conn = 0xFFFF;
            last_status_ms = 0;
        }
        Serial.println("ble: central disconnected");
        NimBLEDevice::startAdvertising();
    }
    uint32_t onPassKeyRequest() override {
        uint32_t pk = 100000 + (esp_random() % 900000);
        pending_passkey = pk;       // shown on screen by the LVGL task
        pairing = true;
        Serial.printf("ble: pairing passkey %06u\r\n", (unsigned)pk);
        return pk;
    }
    void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
        bool ok = desc->sec_state.encrypted && desc->sec_state.authenticated;
        if (ok) {
            authed = true;
            authed_conn = desc->conn_handle;
        }
        // report to the screen only for a fresh pairing (not a bonded reconnect), or on failure
        if (pairing || !ok) pair_result = ok ? 1 : -1;
        pairing = false;
        Serial.printf("ble: security %s (bonded=%d)\r\n", ok ? "ok" : "FAILED", desc->sec_state.bonded);
    }
};

class StatusCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        xSemaphoreTake(lock, portMAX_DELAY);
        status_buf = String(v.c_str());
        status_new = true;
        xSemaphoreGive(lock);
        last_status_ms = millis();
    }
};

class FilesCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        if (v.empty()) return;
        uint8_t flags = (uint8_t)v[0];
        if (flags & 1) files_accum = "";
        if (files_accum.length() + v.size() < 2048) files_accum += String(v.c_str() + 1);
        if (flags & 2) {
            xSemaphoreTake(lock, portMAX_DELAY);
            files_cached = files_accum;
            files_valid = true;
            xSemaphoreGive(lock);
            files_accum = "";
        }
    }
};

void knomi_ble_init(void) {
    lock = xSemaphoreCreateMutex();
    if (!knomi_config.bt_enabled) return;

    String name = String("KNOMI-") + knomi_config.hostname;
    NimBLEDevice::init(name.c_str());
    NimBLEDevice::setMTU(517);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    // LE Secure Connections + MITM + bonding; the KNOMI can only display a code
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

    server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCB());
    NimBLEService *svc = server->createService(KNOMI_BLE_SERVICE_UUID);

    const uint32_t secure_write = NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN;
    svc->createCharacteristic(KNOMI_BLE_STATUS_UUID, secure_write, 512)->setCallbacks(new StatusCB());
    svc->createCharacteristic(KNOMI_BLE_FILES_UUID, secure_write, 512)->setCallbacks(new FilesCB());
    ch_cmd = svc->createCharacteristic(KNOMI_BLE_CMD_UUID,
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN, 512);
    NimBLECharacteristic *info = svc->createCharacteristic(KNOMI_BLE_INFO_UUID, NIMBLE_PROPERTY::READ);
    String info_s = String("{\"fw\":\"" FW_VERSION "\",\"host\":\"") + knomi_config.hostname + "\"}";
    info->setValue((const uint8_t *)info_s.c_str(), info_s.length());
    svc->start();

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(KNOMI_BLE_SERVICE_UUID);
    adv->setScanResponse(true);   // name goes in the scan response (128-bit UUID fills the adv packet)
    adv->start();
    running = true;
    Serial.printf("ble: advertising as %s, %s\r\n", name.c_str(), NimBLEDevice::getAddress().toString().c_str());
}

bool knomi_ble_running(void) { return running; }
bool knomi_ble_connected(void) { return running && authed; }

bool knomi_ble_link_active(void) {
    uint32_t last = last_status_ms;
    return running && authed && last && (millis() - last) < LINK_TIMEOUT_MS;
}

bool knomi_ble_process(void) {
    if (!running) return false;
    String s;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (status_new) { s = status_buf; status_new = false; }
    xSemaphoreGive(lock);
    if (s.isEmpty()) return false;
    return knomi_ble_apply_status(s.c_str(), s.length());
}

bool knomi_ble_send_command(const String &path) {
    if (!knomi_ble_link_active() || !ch_cmd) return false;
    ch_cmd->setValue((const uint8_t *)path.c_str(), path.length());
    ch_cmd->notify();
    return true;
}

bool knomi_ble_file_list(String &out) {
    if (!running) return false;
    knomi_ble_send_command("/knomi/files");  // ask for a fresh list for next time
    xSemaphoreTake(lock, portMAX_DELAY);
    bool ok = files_valid;
    if (ok) out = files_cached;
    xSemaphoreGive(lock);
    return ok;
}

String knomi_ble_address(void) {
    return running ? String(NimBLEDevice::getAddress().toString().c_str()) : String("");
}

void knomi_ble_forget_bonds(void) {
    if (running) NimBLEDevice::deleteAllBonds();
}

uint32_t knomi_ble_take_passkey(void) {
    uint32_t pk = pending_passkey;
    if (pk) pending_passkey = 0;
    return pk;
}

int knomi_ble_take_pair_result(void) {
    int r = pair_result;
    pair_result = 0;
    return r;
}

bool knomi_ble_take_wifi_request(void) {
    bool r = wifi_request;
    wifi_request = false;
    return r;
}
