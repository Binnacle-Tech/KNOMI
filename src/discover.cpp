#include <Arduino.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "knomi.h"
#include "discover.h"

static volatile bool scan_requested = false;
static volatile bool scanning = false;
static String results_json = "[]";
static SemaphoreHandle_t lock = NULL;

static void ensure_lock(void) {
    if (!lock) lock = xSemaphoreCreateMutex();
}

void octoprint_discover_start(void) {
    if (!scanning) scan_requested = true;
}

void octoprint_discover_loop(void) {
    if (!scan_requested) return;
    scan_requested = false;
    ensure_lock();
    if (wifi_get_connect_status() != WIFI_STATUS_CONNECTED) {
        xSemaphoreTake(lock, portMAX_DELAY);
        results_json = "[]";
        xSemaphoreGive(lock);
        return;
    }
    scanning = true;
    Serial.println("mDNS: looking for _octoprint._tcp");
    int n = MDNS.queryService("octoprint", "tcp"); // blocks ~3s
    DynamicJsonDocument doc(2048);
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < n && i < 10; i++) {
        JsonObject o = arr.createNestedObject();
        o["name"] = MDNS.hostname(i);
        o["ip"] = MDNS.IP(i).toString();
        o["port"] = MDNS.port(i);
        String path = MDNS.txt(i, "path");
        o["path"] = path.isEmpty() ? "/" : path;
    }
    String out;
    serializeJson(doc, out);
    xSemaphoreTake(lock, portMAX_DELAY);
    results_json = out;
    xSemaphoreGive(lock);
    Serial.printf("mDNS: %d OctoPrint instance(s)\r\n", n);
    scanning = false;
}

String octoprint_discover_json(void) {
    ensure_lock();
    xSemaphoreTake(lock, portMAX_DELAY);
    String r = results_json;
    xSemaphoreGive(lock);
    bool busy = scanning || scan_requested;
    return String("{\"scanning\":") + (busy ? "true" : "false") + ",\"results\":" + r + "}";
}
