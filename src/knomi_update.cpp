#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "config.h"
#include "knomi_update.h"
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

#ifdef KNOMIV1
#define BOARD_ASSET "knomiv1-octoprint-firmware.bin"
#else
#define BOARD_ASSET "knomiv2-octoprint-firmware.bin"
#endif

enum { UP_IDLE, UP_CHECKING, UP_CURRENT, UP_DOWNLOADING, UP_DONE, UP_ERROR };
static const char * STATE_NAMES[] = {"idle", "checking", "current", "downloading", "done", "error"};
static volatile int state = UP_IDLE;
static volatile int pct = 0;
static char msg[120] = "";
static char tag[32] = "";
static bool force_flag = false;

bool knomi_update_busy(void) { return state == UP_CHECKING || state == UP_DOWNLOADING; }

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

    // 1. latest release. Not through api.github.com: that allows 60 requests an hour per home IP (shared with
    // OctoPrint's own update checks) and then answers 403. github.com/.../releases/latest just redirects to the tag.
    Serial.printf("update: checking GitHub for the latest release (free RAM %u)\r\n", ESP.getFreeHeap());
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if (!http.begin(*tls, "https://github.com/" UPDATE_REPO "/releases/latest")) { fail("Couldn't reach GitHub"); goto out; }
    {
        const char * keep[] = {"Location"};
        http.collectHeaders(keep, 1);
        int code = http.GET();
        String loc = http.header("Location");
        http.end();
        if (code < 0) { snprintf(msg, sizeof(msg), "Couldn't reach GitHub (%s)", http.errorToString(code).c_str()); fail(msg); goto out; }
        int at = loc.indexOf("/releases/tag/");
        if (code / 100 != 3 || at < 0) { snprintf(msg, sizeof(msg), "GitHub answered %d", code); fail(msg); goto out; }
        strlcpy(tag, loc.substring(at + 14).c_str(), sizeof(tag));
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
        static uint8_t buf[4096];
        int got = 0;
        uint32_t last_data = millis();
        while (got < total) {
            size_t avail = stream->available();
            if (!avail) {
                if (!http.connected() || millis() - last_data > 20000) break;
                delay(2);
                continue;
            }
            int n = stream->readBytes(buf, min(avail, sizeof(buf)));
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

bool knomi_update_start(bool force) {
    if (knomi_update_busy() || state == UP_DONE) return false;
    if (WiFi.status() != WL_CONNECTED) { fail("The KNOMI isn't on WiFi"); return false; }
    force_flag = force;
    state = UP_CHECKING;
    pct = 0;
    strlcpy(msg, "Checking GitHub…", sizeof(msg));
    // TLS needs a big stack; runs next to the web server
    if (xTaskCreate(update_task, "update", 12288, NULL, 5, NULL) != pdPASS) { fail("Not enough memory to start"); return false; }
    return true;
}

String knomi_update_status_json(void) {
    StaticJsonDocument<256> d;
    d["state"] = STATE_NAMES[state];
    d["pct"] = pct;
    d["msg"] = msg;
    d["tag"] = tag;
    String out;
    serializeJson(d, out);
    return out;
}
