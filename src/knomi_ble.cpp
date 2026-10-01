#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "knomi.h"
#include "moonraker.h"
#include "knomi_ble.h"
#include <WiFi.h>
#include <freertos/stream_buffer.h>
#include "nimble/porting/nimble/include/os/os_mbuf.h"
#include "nimble/nimble/host/services/gatt/include/services/gatt/ble_svc_gatt.h"
extern bool coaster_plugin_watched;   // knomi_coaster.cpp

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
static SemaphoreHandle_t cmd_lock;   // commands are sent from the post task and the LVGL task
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
    strlcpy(d.material, doc["mt"] | "", sizeof(d.material));
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
    coaster_plugin_watched = doc["cw"] | 0;   // someone has the OctoPrint sidebar open
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
            // tell the Pi our services may have changed: BlueZ caches a bonded device's services, so after a
            // firmware update that adds one (the page tunnel, OP41) it wouldn't see it until it looks again
            ble_svc_gatt_changed(0x0001, 0xFFFF);
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

static void tunnel_frame(const uint8_t *v, size_t len, bool via_cmd);   // HTTP tunnel, below

class FilesCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        if (v.empty()) return;
        uint8_t flags = (uint8_t)v[0];
        // the page tunnel can also ride on this characteristic (flag 0x80, answers come back on CMD): BlueZ
        // remembers a paired KNOMI's characteristics, so a Pi paired before OP41 may not see the tunnel's own
        if (flags & 0x80) {
            std::string t = v;
            t[0] = (char)(flags & 0x7F);
            tunnel_frame((const uint8_t *)t.data(), t.size(), true);
            return;
        }
        static bool files_full = false;   // once a frame didn't fit, keep only the complete names so far
        if (flags & 1) { files_accum = ""; files_full = false; }
        if (!files_full && files_accum.length() + v.size() < 2048) files_accum += String(v.c_str() + 1);
        else if (!files_full) {
            files_full = true;
            int nl = files_accum.lastIndexOf('\n');   // drop the name the missing frame would have finished
            files_accum = nl >= 0 ? files_accum.substring(0, nl) : String();
        }
        if (flags & 2) {
            xSemaphoreTake(lock, portMAX_DELAY);
            files_cached = files_accum;
            files_valid = true;
            xSemaphoreGive(lock);
            files_accum = "";
        }
    }
};

/* ---------------- HTTP tunnel ----------------
 * The plugin's settings page opens the KNOMI's own web pages through OctoPrint; with WiFi off they
 * come over Bluetooth. The plugin writes a request (first frame: "METHOD path\ncontent-type\nlength",
 * then the body in frames); a task here replays it against the KNOMI's web server on 127.0.0.1
 * (the loopback works with WiFi off) and notifies the raw HTTP response back.
 * Frames both ways: [flags][id][seq][data], flags 1 = first, 2 = last, 4 = error / abort. */
static NimBLECharacteristic *ch_tun = nullptr;
static StreamBufferHandle_t tun_body = nullptr;
static QueueHandle_t tun_q = nullptr;
typedef struct { uint8_t id; char method[8]; char path[240]; char ctype[96]; uint32_t len; } tun_req_t;
static volatile bool tun_abort = false, tun_busy = false;
static volatile bool tun_via_cmd = false;   // the current request came over FILES: answer on CMD

static void tun_notify(uint8_t flags, uint8_t id, uint8_t seq, const uint8_t *data, size_t n) {
    uint8_t f[516];
    size_t o = tun_via_cmd ? 1 : 0;   // on CMD, a leading 0x01 tells it apart from the command paths
    f[0] = 1;
    f[o] = flags; f[o + 1] = id; f[o + 2] = seq;
    if (n) memcpy(f + o + 3, data, n);
    // NimBLE's notify() drops a notification silently when its buffers are full ("part of the answer got
    // lost"), so send it here and wait and retry until the stack takes it. Leave buffers spare for the
    // Pi's own writes, which otherwise fail with "Insufficient Resource".
    NimBLECharacteristic *ch = tun_via_cmd ? ch_cmd : ch_tun;
    size_t len = n + 3 + o;
    if (tun_via_cmd) xSemaphoreTake(cmd_lock, portMAX_DELAY);
    for (int i = 0; i < 1500 && authed_conn != 0xFFFF; i++) {   // up to ~3 s
        if (os_msys_num_free() >= 12) {
            os_mbuf *om = ble_hs_mbuf_from_flat(f, len);
            if (om && ble_gattc_notify_custom(authed_conn, ch->getHandle(), om) == 0) break;   // om is freed on error
        }
        delay(2);
    }
    if (tun_via_cmd) xSemaphoreGive(cmd_lock);
}

static void tunnel_task(void *) {
    static uint8_t buf[512];
    tun_req_t r;
    for (;;) {
        if (xQueueReceive(tun_q, &r, portMAX_DELAY) != pdTRUE) continue;
        tun_busy = true;
        WiFiClient c;
        if (!c.connect(IPAddress(127, 0, 0, 1), 80, 3000)) {
            const char *e = "can't reach the KNOMI's web server";
            tun_notify(1 | 2 | 4, r.id, 0, (const uint8_t *)e, strlen(e));
            tun_busy = false;
            continue;
        }
        c.printf("%s %s HTTP/1.1\r\nHost: knomi\r\nConnection: close\r\nContent-Length: %u\r\n", r.method, r.path, (unsigned)r.len);
        if (r.ctype[0]) c.printf("Content-Type: %s\r\n", r.ctype);
        c.print("\r\n");
        uint32_t left = r.len;
        while (left && !tun_abort) {
            size_t n = xStreamBufferReceive(tun_body, buf, left < sizeof(buf) ? left : sizeof(buf), pdMS_TO_TICKS(15000));
            if (!n) break;   // the plugin stopped sending
            c.write(buf, n);
            left -= n;
        }
        if (left || tun_abort) {
            c.stop();
            if (!tun_abort) tun_notify(1 | 2 | 4, r.id, 0, (const uint8_t *)"request body stopped", 20);
            tun_busy = false;
            continue;
        }
        // stream the response back, sized to the connection's MTU
        uint16_t mtu = server ? server->getPeerMTU(authed_conn) : 23;
        size_t chunk = (mtu > 30 ? mtu - 3 : 20) - 4;
        if (chunk > 508) chunk = 508;
        uint8_t seq = 0;
        bool first = true;
        uint32_t idle = millis();
        while (!tun_abort) {
            int n = c.available() ? c.read(buf, chunk) : 0;
            if (n > 0) {
                tun_notify(first ? 1 : 0, r.id, seq++, buf, n);
                first = false;
                idle = millis();
            } else if (!c.connected()) {
                break;
            } else if (millis() - idle > 15000) {
                break;
            } else {
                delay(2);
            }
        }
        c.stop();
        if (!tun_abort) tun_notify((first ? 1 : 0) | 2, r.id, seq, nullptr, 0);
        tun_busy = false;
    }
}

static void tunnel_frame(const uint8_t *v, size_t len, bool via_cmd) {
    {
        if (len < 3) return;
        uint8_t flags = v[0];
        const uint8_t *d = v + 3;
        size_t n = len - 3;
        if (flags & 4) { tun_abort = true; return; }
        if (flags & 1) {
            // a new request: finish off any old one first
            if (tun_busy) {
                tun_abort = true;
                for (int i = 0; i < 100 && tun_busy; i++) delay(10);
            }
            tun_abort = false;
            tun_via_cmd = via_cmd;
            xStreamBufferReset(tun_body);
            tun_req_t r;
            memset(&r, 0, sizeof(r));
            r.id = v[1];
            String head((const char *)d, n);   // "METHOD path\ncontent-type\nlength"
            int a = head.indexOf(' '), b = head.indexOf('\n'), e = head.indexOf('\n', b + 1);
            if (a < 0 || b < 0 || e < 0) return;
            strlcpy(r.method, head.substring(0, a).c_str(), sizeof(r.method));
            strlcpy(r.path, head.substring(a + 1, b).c_str(), sizeof(r.path));
            strlcpy(r.ctype, head.substring(b + 1, e).c_str(), sizeof(r.ctype));
            r.len = (uint32_t)head.substring(e + 1).toInt();
            xQueueSend(tun_q, &r, 0);
            return;
        }
        // body: waits (holding up this write's reply, which paces the plugin) until the task has room
        if (n) xStreamBufferSend(tun_body, d, n, pdMS_TO_TICKS(5000));
    }
}

class TunnelCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        tunnel_frame((const uint8_t *)v.data(), v.size(), false);
    }
};

void knomi_ble_init(void) {
    lock = xSemaphoreCreateMutex();
    cmd_lock = xSemaphoreCreateMutex();
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
    ch_tun = svc->createCharacteristic(KNOMI_BLE_TUNNEL_UUID, secure_write |
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN, 512);
    ch_tun->setCallbacks(new TunnelCB());
    tun_body = xStreamBufferCreate(4096, 1);
    tun_q = xQueueCreate(1, sizeof(tun_req_t));
    xTaskCreate(tunnel_task, "ble tunnel", 4096, NULL, 4, NULL);
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
    // log when the plugin's link comes up or drops, so the log page shows whether it's working
    static bool was_active = false;
    bool active = knomi_ble_link_active();
    if (active != was_active) {
        if (active) Serial.println("ble: OctoPrint plugin link up (status arriving over Bluetooth)");
        else Serial.printf("ble: OctoPrint plugin link lost (%s)\r\n", !authed ? "not paired or disconnected" : "no status for a while");
        was_active = active;
    }
    static uint32_t authed_since = 0;   // paired and connected but nothing sent: say so once
    if (authed && !last_status_ms) {
        if (!authed_since) authed_since = millis();
        else if (millis() - authed_since > 30000) {
            Serial.println("ble: connected and paired, but the plugin isn't sending anything (Bluetooth enabled in the plugin? bleak installed?)");
            authed_since = 0xFFFFFFFF;
        }
    } else if (!authed) authed_since = 0;
    String s;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (status_new) { s = status_buf; status_new = false; }
    xSemaphoreGive(lock);
    if (s.isEmpty()) return false;
    return knomi_ble_apply_status(s.c_str(), s.length());
}

bool knomi_ble_send_command(const String &path) {
    if (!knomi_ble_link_active() || !ch_cmd) return false;
    xSemaphoreTake(cmd_lock, portMAX_DELAY);   // one command's value and notify together
    ch_cmd->setValue((const uint8_t *)path.c_str(), path.length());
    ch_cmd->notify();
    xSemaphoreGive(cmd_lock);
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
