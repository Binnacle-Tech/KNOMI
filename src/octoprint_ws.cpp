#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "knomi.h"
#include "moonraker.h"
#include "octoprint_ws.h"
#include "knomi_ble.h"

#define WS_QUIET_MS 5000   // no "current" for this long -> fall back to polling

static WebSocketsClient ws;
static bool ws_started = false;
static bool ws_authed = false;
static uint32_t ws_last_current = 0;
static String ws_target;   // "host:port" the socket was opened with

bool octoprint_ws_healthy(void) {
    return ws_started && ws_authed && ws_last_current &&
           (millis() - ws_last_current) < WS_QUIET_MS;
}

// Passive login with the API key gives a session for the socket "auth" message
static void ws_authenticate(void) {
    ws_authed = false;
    String resp = moonraker.send_request("POST", "/api/login", "{\"passive\":true}");
    if (moonraker.last_code != 200 || resp.isEmpty()) {
        Serial.printf("octoprint ws: login failed (%d)\r\n", moonraker.last_code);
        return;
    }
    DynamicJsonDocument doc(resp.length() * 2 + 256);
    if (deserializeJson(doc, resp) != DeserializationError::Ok) return;
    String name = doc["name"] | "";
    String session = doc["session"] | "";
    if (name.isEmpty() || session.isEmpty()) return;
    String msg = "{\"auth\":\"" + name + ":" + session + "\"}";
    ws.sendTXT(msg);
    ws_authed = true;
    Serial.println("octoprint ws: authenticated");
}

static void apply_plugin(JsonVariantConst d) {
    moonraker.data.homing = d["homing"] | false;
    moonraker.data.probing = d["probing"] | false;
    moonraker.data.qgling = d["qgling"] | false;
    moonraker.data.heating_nozzle = d["heating_nozzle"] | false;
    moonraker.data.heating_bed = d["heating_bed"] | false;
    moonraker.data.shaping = d["shaping"] | false;
    moonraker.data.pid_tuning = d["pid_tuning"] | false;
    moonraker.data.cleaning = d["cleaning"] | false;
    moonraker.data.filament = d["filament"] | false;
    moonraker.data.paused_ext = d["paused"] | false;
}

static void apply_current(JsonVariantConst c) {
    JsonVariantConst flags = c["state"]["flags"];
    moonraker_data_t &data = moonraker.data;
    data.pause = (flags["pausing"] | false) || (flags["paused"] | false);
    data.printing = (flags["printing"] | false) || (flags["cancelling"] | false) ||
                    (flags["resuming"] | false) || (flags["finishing"] | false) || data.pause;
    moonraker.unready = !(flags["operational"] | false);
    moonraker.unconnected = false;
    moonraker.auth_failed = false;

    double completion = c["progress"]["completion"] | 0.0;
    if (completion < 0) completion = 0;
    if (completion > 100) completion = 100;
    data.progress = (uint8_t)(completion + 0.5f);
    const char *name = c["job"]["file"]["name"] | "";
    const char *slash = strrchr(name, '/');
    strlcpy(data.file_path, slash ? slash + 1 : name, sizeof(data.file_path));
    data.print_time = c["progress"]["printTime"] | 0;
    JsonVariantConst left = c["progress"]["printTimeLeft"];
    data.time_left = left.isNull() ? -1 : left.as<int32_t>();
    JsonVariantConst z = c["currentZ"];
    data.z_um = z.isNull() ? INT32_MIN : (int32_t)(z.as<double>() * 1000);
    data.layer = data.layer_total = 0;

    // temps only carries new samples; keep the previous values when empty
    JsonArrayConst temps = c["temps"];
    if (temps.size()) {
        JsonVariantConst t = temps[temps.size() - 1];
        JsonVariantConst tool = t[knomi_config.moonraker_tool];
        if (!tool.isNull()) {
            data.nozzle_actual = int16_t((tool["actual"] | 0.0) + 0.5f);
            data.nozzle_target = int16_t((tool["target"] | 0.0) + 0.5f);
        }
        JsonVariantConst bed = t["bed"];
        if (!bed.isNull()) {
            data.bed_actual = int16_t((bed["actual"] | 0.0) + 0.5f);
            data.bed_target = int16_t((bed["target"] | 0.0) + 0.5f);
        }
    }
}

static void on_text(uint8_t *payload, size_t length) {
    // Only look at what we use; "history"/"connected" can be large
    static StaticJsonDocument<512> filter;
    if (filter.isNull()) {
        JsonObject cur = filter.createNestedObject("current");
        cur["state"]["flags"] = true;
        cur["progress"]["completion"] = true;
        cur["progress"]["printTime"] = true;
        cur["progress"]["printTimeLeft"] = true;
        cur["currentZ"] = true;
        cur["job"]["file"]["name"] = true;
        cur["temps"] = true;
        filter["plugin"] = true;
        filter["reauthRequired"] = true;
    }
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter)) != DeserializationError::Ok)
        return;

    if (doc.containsKey("reauthRequired")) {
        ws_authenticate();
        return;
    }
    moonraker.data_unlock = false;
    if (doc.containsKey("current")) {
        apply_current(doc["current"]);
        ws_last_current = millis();
    }
    if (doc.containsKey("plugin") && strcmp(doc["plugin"]["plugin"] | "", "knomi") == 0) {
        apply_plugin(doc["plugin"]["data"]);
    }
    moonraker.data_unlock = true;
}

static void on_event(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.println("octoprint ws: connected");
            ws_authenticate();
            break;
        case WStype_DISCONNECTED:
            if (ws_authed) Serial.println("octoprint ws: disconnected");
            ws_authed = false;
            ws_last_current = 0;
            break;
        case WStype_TEXT:
            on_text(payload, length);
            break;
        default:
            break;
    }
}

void octoprint_ws_loop(void) {
    String target = String(knomi_config.moonraker_ip) + ":" + knomi_config.moonraker_port;
    bool want = knomi_backend_is_octoprint() && knomi_config.moonraker_ip[0] &&
                wifi_get_connect_status() == WIFI_STATUS_CONNECTED && !knomi_ble_link_active();

    // (re)start when settings change, stop when not using OctoPrint
    if (ws_started && (!want || target != ws_target)) {
        ws.disconnect();
        ws_started = false;
        ws_authed = false;
        ws_last_current = 0;
    }
    if (!want) return;
    if (!ws_started) {
        ws_target = target;
        ws.begin(knomi_config.moonraker_ip, atoi(knomi_config.moonraker_port), "/sockjs/websocket");
        ws.onEvent(on_event);
        ws.setReconnectInterval(5000);
        ws_started = true;
    }
    ws.loop();
}
