#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "config.h"
#include "knomi_update.h"
#include "knomi_ble.h"
#include "moonraker.h"
#include "knomi_flavor.h"
#include "github_roots.h"
#include <esp_heap_caps.h>
#include "mbedtls/platform.h"

// TLS to GitHub needs ~40 KB, and mbedTLS is set to take it from internal RAM only. With Bluetooth on
// there isn't that much left (OP42: free RAM fell to under 1 KB and GitHub "answered -1"), so updates put
// mbedTLS's memory in PSRAM (8 MB, nearly all free). Only the updater uses TLS.
static void * tls_calloc(size_t n, size_t size) {
    void * p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
static void tls_free(void * p) { heap_caps_free(p); }

#include "knomi_flavor.h"
#define BOARD_ASSET KNOMI_ASSET   // this board and build flavour

enum { UP_IDLE, UP_CHECKING, UP_CURRENT, UP_DOWNLOADING, UP_DONE, UP_ERROR };
static const char * STATE_NAMES[] = {"idle", "checking", "current", "downloading", "done", "error"};
static volatile int state = UP_IDLE;
static volatile int pct = 0;
static char msg[120] = "";
static char tag[32] = "";
static bool force_flag = false;

static volatile bool checking = false;   // the Moonraker build asking GitHub what's out
static bool remote = false;            // OctoPrint build: the plugin is doing the install (state mirrors it)
static uint32_t remote_since = 0;
bool knomi_update_busy(void) { return ((state == UP_CHECKING || state == UP_DOWNLOADING) && !remote) || checking; }

static void fail(const char * m) {
    strlcpy(msg, m, sizeof(msg));
    state = UP_ERROR;
    Serial.printf("update: %s\r\n", m);
}

// "v1.0.2-op16" -> comparable numbers; returns false if it doesn't look like a version
static bool parse_ver(const char * s, int v[4]) {
    v[0] = v[1] = v[2] = v[3] = 0;
    while (*s && !isdigit((unsigned char)*s)) s++;
    if (sscanf(s, "%d.%d.%d", &v[0], &v[1], &v[2]) != 3) return false;
    const char * op = strcasestr(s, "-op");
    if (op) v[3] = atoi(op + 3);
    return true;
}

static bool newer(const int a[4], const int b[4]) {
    for (int i = 0; i < 4; i++) if (a[i] != b[i]) return a[i] > b[i];
    return false;
}

// the newest release's tag ("v1.0.2-op59"). Not through api.github.com: that allows 60 requests an hour per home IP
// (shared with OctoPrint's own update checks) and then answers 403. github.com/.../releases/latest just redirects.
static bool fetch_latest(WiFiClientSecure * tls, HTTPClient & http, char * out, size_t n, char * err, size_t en) {
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if (!http.begin(*tls, "https://github.com/" UPDATE_REPO "/releases/latest")) { strlcpy(err, "Couldn't reach GitHub", en); return false; }
    const char * keep[] = {"Location"};
    http.collectHeaders(keep, 1);
    int code = http.GET();
    String loc = http.header("Location");
    http.end();
    if (code < 0) { snprintf(err, en, "Couldn't reach GitHub (%s)", http.errorToString(code).c_str()); return false; }
    int at = loc.indexOf("/releases/tag/");
    if (code / 100 != 3 || at < 0) { snprintf(err, en, "GitHub answered %d", code); return false; }
    strlcpy(out, loc.substring(at + 14).c_str(), n);
    return true;
}

/* ---- what's out: for the pages, /log/info and Coaster ----
 * Moonraker build: the KNOMI asks GitHub itself, 2 minutes after starting and then every 6 hours.
 * OctoPrint build: it isn't on the internet; the plugin tells it (status "fl"). */
static char latest_tag[32] = "";
static portMUX_TYPE latest_mux = portMUX_INITIALIZER_UNLOCKED;
static void set_latest(const char * t) {
    portENTER_CRITICAL(&latest_mux);
    strlcpy(latest_tag, t, sizeof(latest_tag));
    portEXIT_CRITICAL(&latest_mux);
}
String knomi_update_latest(void) {
    char b[32];
    portENTER_CRITICAL(&latest_mux);
    strlcpy(b, latest_tag, sizeof(b));
    portEXIT_CRITICAL(&latest_mux);
    return String(b);
}
static bool parse_ver(const char * s, int v[4]);
static bool newer(const int a[4], const int b[4]);
bool knomi_update_newer_out(void) {
    String t = knomi_update_latest();
    int l[4], c[4];
    return t.length() && parse_ver(t.c_str(), l) && parse_ver(FW_VERSION, c) && newer(l, c);
}
void knomi_update_set_latest(const char * tag) {
    int v[4];
    if (tag && *tag && strlen(tag) < sizeof(latest_tag) && parse_ver(tag, v)) set_latest(tag);
}

static void update_task(void * arg) {
    mbedtls_platform_set_calloc_free(tls_calloc, tls_free);
    WiFiClientSecure * tls = new WiFiClientSecure();
    tls->setCACert(github_roots);
    HTTPClient http;
    http.setUserAgent("KNOMI-" FW_VERSION);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    http.useHTTP10(true);   // no chunked replies: the JSON is parsed straight off the stream
    String url;

    // 1. latest release
    Serial.printf("update: checking GitHub for the latest release (free RAM %u)\r\n", ESP.getFreeHeap());
    {
        char err[96];
        if (!fetch_latest(tls, http, tag, sizeof(tag), err, sizeof(err))) { fail(err); goto out; }
        set_latest(tag);
        int latest[4], current[4];
        if (!parse_ver(tag, latest) || !parse_ver(FW_VERSION, current)) { fail("Unknown version on GitHub"); goto out; }
        if (!newer(latest, current) && !force_flag) {
            snprintf(msg, sizeof(msg), "Already up to date (%s)", FW_VERSION);
            state = UP_CURRENT;
            goto out;
        }
        url = "https://github.com/" UPDATE_REPO "/releases/download/" + String(tag) + "/" BOARD_ASSET;
        Serial.printf("update: latest is %s, this is %s\r\n", tag, FW_VERSION);
    }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);   // the download hops to GitHub's file host

    // 2. download straight into the other OTA partition
    snprintf(msg, sizeof(msg), "Downloading %s", tag);
    state = UP_DOWNLOADING;
    pct = 0;
    Serial.printf("update: downloading %s\r\n", url.c_str());
    if (!http.begin(*tls, url)) { fail("Couldn't start the download"); goto out; }
    {
        int code = http.GET();
        if (code != 200) {
            snprintf(msg, sizeof(msg), code == 404 ? "The release has no " BOARD_ASSET : "Download failed (%d)", code);
            fail(msg); http.end(); goto out;
        }
        int total = http.getSize();
        Serial.printf("update: %d bytes\r\n", total);
        if (total <= 0) { fail("Download has no size"); http.end(); goto out; }
        if (!Update.begin(total, U_FLASH)) { fail("Not enough room for the update"); http.end(); goto out; }
        WiFiClient * stream = http.getStreamPtr();
        // in PSRAM (it used to take 4 KB of internal RAM for good)
        static uint8_t *buf = nullptr;
        const size_t buf_n = 4096;
        if (!buf) buf = (uint8_t *)heap_caps_malloc(buf_n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!buf) { Update.abort(); fail("Out of memory"); http.end(); goto out; }
        int got = 0;
        uint32_t last_data = millis();
        while (got < total) {
            size_t avail = stream->available();
            if (!avail) {
                if (!http.connected() || millis() - last_data > 20000) break;
                delay(2);
                continue;
            }
            int n = stream->readBytes(buf, min(avail, buf_n));
            if (n <= 0) continue;
            if (Update.write(buf, n) != (size_t)n) { Update.abort(); fail("Writing to flash failed"); http.end(); goto out; }
            got += n;
            last_data = millis();
            pct = (int)((int64_t)got * 100 / total);
        }
        http.end();
        if (got < total) { Update.abort(); fail("Download stopped early, try again"); goto out; }
        if (!Update.end(true)) { snprintf(msg, sizeof(msg), "Update check failed (%s)", Update.errorString()); fail(msg); goto out; }
    }
    snprintf(msg, sizeof(msg), "Installed %s. Restarting…", tag);
    state = UP_DONE;
    pct = 100;
    Serial.printf("update: installed %s, restarting\r\n", tag);
    delay(1500);
    ESP.restart();

out:
    delete tls;
    vTaskDelete(NULL);
}

bool knomi_ota_upload_busy(void);   // webserver.cpp: a firmware upload has the flash

bool knomi_update_start(bool force) {
    if (knomi_ota_upload_busy()) return false;
    if (knomi_update_busy() || state == UP_DONE) return false;
#if KNOMI_OCTOPRINT
    // not on the internet: the plugin downloads the release on the Pi and sends it over (the KNOMI network or Bluetooth)
    if (remote && (state == UP_CHECKING || state == UP_DOWNLOADING)) return false;
    if (!knomi_ble_link_active()) { fail("No Bluetooth link to OctoPrint right now"); return false; }
    remote = true; remote_since = millis();
    state = UP_CHECKING; pct = 0; tag[0] = 0;
    strlcpy(msg, "Asking OctoPrint to install the latest release…", sizeof(msg));
    knomi_ble_send_command(String("/knomi/fw_install?asset=") + BOARD_ASSET + (force ? "&force=1" : ""));
    return true;
#endif
    if (WiFi.status() != WL_CONNECTED) { fail("The KNOMI isn't on WiFi"); return false; }
    force_flag = force;
    state = UP_CHECKING;
    pct = 0;
    strlcpy(msg, "Checking GitHub…", sizeof(msg));
    // TLS needs a big stack; runs next to the web server
    if (xTaskCreate(update_task, "update", 12288, NULL, 5, NULL) != pdPASS) { fail("Not enough memory to start"); return false; }
    return true;
}

#if KNOMI_OCTOPRINT
// the plugin's install, as it goes (BLE status "fu": [state, pct, message])
void knomi_update_remote(const char * st, int p, const char * m) {
    if (!st || !*st) return;
    if (!strcmp(st, "downloading") || !strcmp(st, "sending")) state = UP_DOWNLOADING;
    else if (!strcmp(st, "done")) state = UP_DONE;
    else if (!strcmp(st, "error")) state = UP_ERROR;
    else return;
    remote = true; remote_since = millis();
    pct = constrain(p, 0, 100);
    if (m && *m) strlcpy(msg, m, sizeof(msg));
}
#endif

String knomi_update_status_json(void) {
#if KNOMI_OCTOPRINT
    if (remote && state == UP_CHECKING && millis() - remote_since > 30000)
        fail("OctoPrint didn't start the install (needs plugin 0.15 or newer)");
#endif
    StaticJsonDocument<256> d;
    d["state"] = STATE_NAMES[state];
    d["pct"] = pct;
    d["msg"] = msg;
    d["tag"] = tag;
    String out;
    serializeJson(d, out);
    return out;
}

#if !KNOMI_OCTOPRINT
static void check_task(void *) {
    mbedtls_platform_set_calloc_free(tls_calloc, tls_free);
    WiFiClientSecure * tls = new WiFiClientSecure();
    tls->setCACert(github_roots);
    HTTPClient http;
    http.setUserAgent("KNOMI-" FW_VERSION);
    http.setTimeout(15000);
    char t[32], err[96];
    if (fetch_latest(tls, http, t, sizeof(t), err, sizeof(err))) {
        set_latest(t);
        Serial.printf("update: latest release is %s (this is %s)\r\n", t, FW_VERSION);
    } else Serial.printf("update: couldn't check for a new release: %s\r\n", err);
    delete tls;
    checking = false;
    vTaskDelete(NULL);
}
#endif

// WiFi task: the Moonraker build checks GitHub now and then and has Coaster mention a new release (once, not mid-print)
void knomi_update_loop(void) {
#if !KNOMI_OCTOPRINT
    static uint32_t next = 120000;
    static char told[32] = "";
    if (!checking && knomi_update_newer_out() && !moonraker.data.printing) {
        String t = knomi_update_latest();
        if (strcmp(told, t.c_str()) != 0) {
            strlcpy(told, t.c_str(), sizeof(told));
            int v[4];
            parse_ver(t.c_str(), v);
            char m[96];
            snprintf(m, sizeof(m), "KNOMI firmware OP%d is out. Update it on the KNOMI's settings page", v[3]);
            moonraker_set_msg(m, (long)millis() | 1);
        }
    }
    if ((int32_t)(millis() - next) < 0 || checking || knomi_update_busy() || WiFi.status() != WL_CONNECTED) return;
    next = millis() + 6 * 3600 * 1000UL;
    checking = true;
    if (xTaskCreate(check_task, "update check", 12288, NULL, 3, NULL) != pdPASS) { checking = false; next = millis() + 600000; }
#endif
}
