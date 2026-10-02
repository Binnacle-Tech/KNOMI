// The settings page as data (GET /settings.json). The OctoPrint plugin draws its own settings tab from
// this, so settings can be added, moved or reworded here without touching the plugin. It talks to the
// same form handlers as the web page (webserver.cpp), adding "quiet" to get a short JSON answer back.
//
// {"v":1, "fw", "board", "sections":[section...]}
// section: id, title, hint?, status?, post (path, "" = the settings page), json? (post the fields as a
//          JSON object instead of a form), save? (button label; no button without it),
//          fields:[field...], actions:[{l, post, data?:{name:value}, confirm?, danger?}...]
// field:   n (form name), t (type), l (label), h? (hint), v? (value; compare as text), g? (fields sharing g share a row)
//   text      max?, ph? (placeholder), list? (suggestions)      password  set (has a value; never sent)
//   number / range  min, max, step?                            select    o:[[value, label]...]
//   check     posted as "1" when ticked, left out when not     color     "#rrggbb"
//   info      read-only text                                   tz        the browser's UTC offset, minutes
//   file      post, accept?, del?, del_l? (upload/remove: GIF slots, restore)
//   view      src (plain text shown in a box, e.g. the log)    link      src (opens/downloads)
//   firmware  repo, asset (the plugin downloads the release and sends it to POST /update)
// ro: shown but not editable (and not posted).

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "knomi.h"
#include "knomi_ble.h"
#include "knomi_gif.h"
#include "knomi_coaster.h"
#include "config.h"
#include "psram_json.h"

const char * knomi_wifi_policy(void);   // wifi_setup.cpp

#ifdef KNOMIV1
#define SCHEMA_BOARD "knomiv1"
#else
#define SCHEMA_BOARD "knomiv2"
#endif

static JsonObject section(JsonArray secs, const char * id, const char * title, const char * post, const char * save) {
    JsonObject s = secs.createNestedObject();
    s["id"] = id;
    s["title"] = title;
    s["post"] = post;
    if (save) s["save"] = save;
    s.createNestedArray("fields");
    return s;
}

// ArduinoJson keeps a const char * as a pointer and copies a char *: names and labels here are often built in
// a reused buffer, so they're copied
#define COPY(x) ((char *)(x))

static JsonObject field(JsonObject s, const char * n, const char * t, const char * l, const char * h = nullptr) {
    JsonObject f = s["fields"].as<JsonArray>().createNestedObject();
    f["n"] = COPY(n);
    f["t"] = t;
    f["l"] = COPY(l);
    if (h) f["h"] = COPY(h);
    return f;
}

static JsonObject num(JsonObject s, const char * n, const char * l, long v, long lo, long hi, const char * h = nullptr,
                      const char * t = "number") {
    JsonObject f = field(s, n, t, l, h);
    f["v"] = v;
    f["min"] = lo;
    f["max"] = hi;
    return f;
}

static void opt(JsonObject f, const char * value, const char * label) {
    if (!f.containsKey("o")) f.createNestedArray("o");
    JsonArray o = f["o"];
    JsonArray p = o.createNestedArray();
    p.add(COPY(value));
    p.add(COPY(label));
}

static JsonObject action(JsonObject s, const char * l, const char * post, const char * confirm = nullptr, bool danger = false) {
    if (!s.containsKey("actions")) s.createNestedArray("actions");
    JsonObject a = s["actions"].as<JsonArray>().createNestedObject();
    a["l"] = l;
    a["post"] = post;
    if (confirm) a["confirm"] = confirm;
    if (danger) a["danger"] = true;
    return a;
}

String knomi_settings_schema(void) {
    const knomi_config_t & c = knomi_config;
    PsJsonDocument d(49152);   // in PSRAM (malloc above 4 KB); about 20 KB used
    d["v"] = 1;
    d["fw"] = FW_VERSION;
    d["board"] = SCHEMA_BOARD;
    JsonArray secs = d.createNestedArray("sections");
    char buf[96];

    // ---- printer connection ----
    {
        JsonObject s = section(secs, "printer", "Printer connection", "", "Save connection");
        s["h"] = "Over Bluetooth the plugin sends the KNOMI everything; these are for its WiFi connection.";
        JsonObject f = field(s, "backend", "select", "Backend");
        opt(f, "moonraker", "Moonraker (Klipper)");
        opt(f, "octoprint", "OctoPrint");
        f["v"] = knomi_backend_is_octoprint() ? "octoprint" : "moonraker";
        f = field(s, "ip", "text", "Host"); f["v"] = c.moonraker_ip; f["max"] = 64; f["ph"] = "192.168.1.20 or octopi.local"; f["g"] = "host";
        f = field(s, "port", "text", "Port"); f["v"] = c.moonraker_port; f["max"] = 5; f["ph"] = "80"; f["g"] = "host";
        f = field(s, "tool", "text", "Tool"); f["v"] = c.moonraker_tool; f["max"] = 6; f["ph"] = "tool0";
        f = field(s, "api_key", "password", "OctoPrint API key", "OctoPrint > User Settings > Application Keys");
        f["set"] = c.api_key[0] != 0; f["max"] = 64;
    }

    // ---- display ----
    {
        JsonObject s = section(secs, "display", "Display", "display", "Save display");
        num(s, "bl", "Brightness", c.backlight, 1, 16, nullptr, "range")["g"] = "bright";
        num(s, "dim_lvl", "Dimmed brightness", c.dim_level, 1, 16, nullptr, "range")["g"] = "bright";
        num(s, "dim_min", "Dim after (minutes)", c.dim_after_min, 0, 1440, "0 = never")["g"] = "time";
        num(s, "sleep_min", "Screen off after (minutes)", c.sleep_after_min, 0, 1440, "0 = never")["g"] = "time";
        JsonObject f = field(s, "awake_print", "select", "While the printer is busy");
        opt(f, "1", "Stay awake"); opt(f, "0", "Dim and sleep as usual");
        f["v"] = (int)(c.awake_printing ? 1 : 0);
        f = field(s, "print_view", "select", "Printing screen");
        opt(f, "0", "Your layout (Print screen designer)"); opt(f, "1", "Accelerometer bars (stock)");
        f["v"] = (int)(c.print_view);
        f = field(s, "gif_tint", "select", "Animations follow the UI color");
        opt(f, "0", "Off: stock colors"); opt(f, "2", "On: built-in animations");
        f["v"] = (int)(c.gif_tint ? GIF_TINT_ALL : GIF_TINT_OFF);
    }

    // ---- screen ----
    {
        JsonObject s = section(secs, "screen", "Screens", "screen", "Save screens");
        lv_color32_t col;
        col.full = lv_color_to32(c.theme_color);
        snprintf(buf, sizeof(buf), "#%02x%02x%02x", col.ch.red, col.ch.green, col.ch.blue);
        JsonObject f = field(s, "theme", "color", "UI color", "Buttons, rings and (if turned on) the animations.");
        f["v"] = buf;
        num(s, "touch_idle", "Back to Coaster after (seconds)", c.touch_idle_s, 0, 3600, "When you leave a menu open. 0 = stay on the menu.");
        f = field(s, "hs_n", "check", "Heat-up screen while the nozzle heats"); f["v"] = (bool)(c.heat_screens & 1); f["g"] = "hs";
        f = field(s, "hs_b", "check", "Heat-up screen while the bed heats"); f["v"] = (bool)(c.heat_screens & 2); f["g"] = "hs";
        num(s, "heated_s", "Coaster gets ready (s)", c.heated_s, 0, 600, nullptr)["g"] = "anim";
        num(s, "print_ok_s", "Print finished (s)", c.print_ok_s, 0, 600, nullptr)["g"] = "anim";
        num(s, "printed_s", "After the print (s)", c.printed_s, 0, 3600, "How long each animation plays. 0 = skip.")["g"] = "anim";
        JsonObject a = action(s, "Default red", "screen");
        a["data"]["theme_default"] = "1";
        a["fields"] = true;   // posts the section's fields too (the handler reads them all)
    }

    // ---- presets ----
    {
        JsonObject s = section(secs, "presets", "Presets", "presets", "Save presets");
        s["h"] = "Preheat: Temperature > Preheat on the KNOMI. Extrude: the Extruder screen.";
        char n[8], g[16];
        for (int i = 0; i < PREHEAT_NUM; i++) {
            snprintf(g, sizeof(g), "Preheat %d", i + 1);
            snprintf(n, sizeof(n), "pl%d", i);
            JsonObject f = field(s, n, "text", "Name"); f["v"] = c.preheat[i].label; f["max"] = 9; f["g"] = g;
            snprintf(n, sizeof(n), "pn%d", i);
            num(s, n, "Nozzle °C", c.preheat[i].nozzle, 0, 500)["g"] = g;
            snprintf(n, sizeof(n), "pb%d", i);
            num(s, n, "Bed °C", c.preheat[i].bed, 0, 200)["g"] = g;
        }
        for (int i = 0; i < EXTRUDE_NUM; i++) {
            snprintf(g, sizeof(g), "Extrude %d", i + 1);
            snprintf(n, sizeof(n), "em%d", i);
            num(s, n, "Length mm", c.extrude_mm[i], 1, 1000)["g"] = g;
            snprintf(n, sizeof(n), "es%d", i);
            num(s, n, "Speed mm/s", c.extrude_mms[i], 1, 300)["g"] = g;
        }
        JsonObject dl = field(s, "em_def", "select", "Default length", "Selected when the KNOMI starts.");
        JsonObject ds = field(s, "es_def", "select", "Default speed");
        for (int i = 0; i < EXTRUDE_NUM; i++) {
            char v[4];
            snprintf(v, sizeof(v), "%d", i);
            snprintf(buf, sizeof(buf), "%u mm", c.extrude_mm[i]);
            opt(dl, v, buf);
            snprintf(buf, sizeof(buf), "%u mm/s", c.extrude_mms[i]);
            opt(ds, v, buf);
        }
        dl["v"] = (int)c.extrude_mm_def; dl["g"] = "def";
        ds["v"] = (int)c.extrude_mms_def; ds["g"] = "def";
        JsonObject a = action(s, "Stock presets", "presets", "Put the stock presets back?");
        a["data"]["reset"] = "1";
    }

    // ---- Bluetooth ----
    {
        JsonObject s = section(secs, "bluetooth", "Bluetooth", "bluetooth", "Save Bluetooth");
        s["status"] = !knomi_ble_running() ? "off" : knomi_ble_link_active() ? "connected"
                    : knomi_ble_connected() ? "paired, waiting for data" : "advertising";
        JsonObject f = field(s, "bt_enabled", "select", "Bluetooth link to OctoPrint", "Takes effect after a restart.");
        opt(f, "0", "Off"); opt(f, "1", "On: prefer Bluetooth, WiFi as fallback");
        f["v"] = (int)(c.bt_enabled ? 1 : 0);
        f = field(s, "bt_wifi_off", "select", "WiFi while Bluetooth is connected");
        opt(f, "0", "Keep WiFi on"); opt(f, "1", "Turn WiFi off");
        f["v"] = (int)(c.bt_wifi_off ? 1 : 0);
        if (!c.bt_wifi_off && !knomi_ble_link_active()) f["ro"] = true;
        f["h"] = c.bt_wifi_off ? "WiFi turns off 10 s after Bluetooth connects." :
                 knomi_ble_link_active() ? "Bluetooth is connected, so WiFi can be turned off." :
                 "Available once the plugin is connected over Bluetooth.";
        num(s, "bt_fallback", "WiFi fallback (seconds)", c.bt_fallback_s, 15, 3600,
            "If Bluetooth isn't connected for this long, including after boot, WiFi turns back on.");
        f = field(s, "bt_addr", "info", "KNOMI Bluetooth address"); f["v"] = knomi_ble_address();
        f = field(s, "wifi_policy", "info", "WiFi right now"); f["v"] = knomi_wifi_policy();
        action(s, "Forget paired devices", "bluetooth/forget", "Forget all paired devices? The Pi will need to pair again.", true);
    }

    // ---- WiFi: join a network ----
    {
        JsonObject s = section(secs, "wifi", "WiFi network", "", "Connect");
        s["h"] = "Pick a network from the last scan or type one (hidden networks too).";
        JsonObject f = field(s, "ssid", "text", "Network");
        f["v"] = c.sta_ssid; f["max"] = 32;
        JsonArray list = f.createNestedArray("list");
        for (uint8_t i = 0; i < wifi_scan.count; i++) {
            JsonArray e = list.createNestedArray();
            e.add(wifi_scan.ssid[i]);
            snprintf(buf, sizeof(buf), "%d dBm%s", (int)wifi_scan.rssi[i], wifi_scan.connected[i] ? ", connected" : "");
            e.add(buf);
        }
        f = field(s, "password", "password", "Password");
        f["set"] = c.sta_pwd[0] != 0; f["max"] = 64;
        f = field(s, "sta_ip", "info", "Address");
        if (WiFi.status() == WL_CONNECTED) f["v"] = WiFi.localIP().toString();
        else f["v"] = "not on WiFi";
        JsonObject a = action(s, "Rescan", "");
        a["data"]["refresh"] = "1";
    }

    // ---- KNOMI network ----
    {
        JsonObject s = section(secs, "network", "KNOMI network", "", "Save network");
        JsonObject f = field(s, "mode", "select", "WiFi mode");
        opt(f, "ap", "Access point"); opt(f, "sta", "Station (join network)"); opt(f, "apsta", "Access point + station");
        f["v"] = c.mode;
        f = field(s, "hostname", "text", "Hostname", "Takes effect after a restart."); f["v"] = c.hostname; f["max"] = 15;
        f = field(s, "ap_ssid", "text", "AP name"); f["v"] = c.ap_ssid; f["max"] = 32; f["g"] = "ap";
        f = field(s, "ap_password", "password", "AP password", "6 or more characters."); f["set"] = c.ap_pwd[0] != 0; f["max"] = 64; f["g"] = "ap";
    }

    // ---- Coaster ----
    {
        JsonObject s = section(secs, "coaster", "Coaster", "coaster.json", "Save Coaster");
        s["json"] = true;
        PsJsonDocument t(1024);
        deserializeJson(t, coaster_tuning_json());
        static const struct { const char * n, * l, * h; double lo, hi, step; } tune[] = {
            {"wobble", "Wobble (Hz)", "How fast the head bounces back. Low is floppy, high is stiff.", 0.8, 6, 0.1},
            {"settle", "Settle", "Damping. Low keeps it wobbling after a move; 1 stops dead.", 0.05, 1, 0.01},
            {"sense", "Sensitivity", "How far a 1 g shove throws the head and eyes.", 0.2, 3, 0.05},
            {"habit", "Gets used to it (s)", "How long before steady shaking stops being exciting.", 2, 60, 1},
            {"scare", "Scream at (g)", "Motion above the used-to level that makes it scream.", 0.3, 2.5, 0.05},
            {"dizzy", "Dizzy after (s)", "Seconds of screaming before it gets dizzy.", 1, 20, 0.5},
            {"sleep", "Sleepy after (s)", "Seconds of stillness before it dozes off.", 5, 120, 1},
        };
        for (auto & m : tune) {
            JsonObject f = field(s, m.n, "range", m.l, m.h);
            f["v"] = t[m.n]; f["min"] = m.lo; f["max"] = m.hi; f["step"] = m.step; f["g"] = "tune";
        }
        JsonObject f = field(s, "deco", "select", "Decorations", "By season changes with the seasons; holidays get a few days.");
        opt(f, "auto", "By season (automatic)"); opt(f, "off", "Off"); opt(f, "holidays", "Holidays: lights, snow, Santa hat");
        opt(f, "newyear", "New Year: party hat, fireworks"); opt(f, "winter", "Winter: snow"); opt(f, "valentine", "Valentine's: hearts");
        opt(f, "spring", "Spring: petals, a flower"); opt(f, "summer", "Summer: sunglasses"); opt(f, "july4", "4th of July: fireworks");
        opt(f, "autumn", "Autumn: falling leaves"); opt(f, "halloween", "Halloween: witch hat, bats"); opt(f, "birthday", "Coaster's birthday");
        f["v"] = t["deco"]; f["g"] = "deco";
        f = field(s, "hemi", "select", "Seasons");
        opt(f, "n", "Northern hemisphere"); opt(f, "s", "Southern hemisphere");
        f["v"] = t["hemi"]; f["g"] = "deco";
        f = field(s, "lights", "select", "Holiday lights");
        opt(f, "classic", "Classic colors"); opt(f, "warm", "Warm white"); opt(f, "theme", "UI color");
        opt(f, "candy", "Candy cane"); opt(f, "rainbow", "Rainbow fade");
        f["v"] = t["lights"]; f["g"] = "lights";
        f = field(s, "anim", "select", "Lights effect");
        opt(f, "twinkle", "Twinkle"); opt(f, "chase", "Chase"); opt(f, "breathe", "Slow fade"); opt(f, "steady", "Steady");
        f["v"] = t["anim"]; f["g"] = "lights";
        f = field(s, "bday", "text", "Coaster's birthday (MM-DD)", "Party hat and confetti all day.");
        f["v"] = t["bday"]; f["max"] = 5; f["ph"] = "09-28"; f["g"] = "talk";
        f = field(s, "talk", "select", "Coaster talks");
        opt(f, "2", "Often"); opt(f, "1", "Sometimes"); opt(f, "0", "Never");
        f["v"] = t["talk"].as<int>(); f["g"] = "talk";
        f = field(s, "clock", "select", "Clock when idle");
        opt(f, "1", "12 hour"); opt(f, "2", "24 hour"); opt(f, "0", "Off");
        f["v"] = t["clock"].as<int>(); f["g"] = "talk";
        field(s, "tz", "tz", "Time zone");
        field(s, "idle", "hidden", "")["v"] = t["idle"];
    }

    // ---- animations ----
    {
        JsonObject s = section(secs, "gifs", "Animations", "", nullptr);
        size_t used = knomi_fs_used(), total = LittleFS.totalBytes();
        snprintf(buf, sizeof(buf), "Upload a GIF to play it there instead of Coaster acting it out. %u / %u KB of flash used, 1.5 MB max per GIF.",
                 (unsigned)(used / 1024), (unsigned)(total / 1024));
        s["h"] = buf;
        for (int i = 0; i < GIF_SLOT_NUM; i++) {
            if (knomi_gif_is_face(i)) continue;   // faces are Coaster, drawn live
            knomi_gif_info_t info;
            knomi_gif_get_info((knomi_gif_slot_t)i, &info);
            JsonObject f = field(s, "gif", "file", info.label);
            f["accept"] = "image/gif";
            snprintf(buf, sizeof(buf), "gif/upload?slot=%s", info.name);
            f["post"] = buf;
            if (info.has_custom) {
                snprintf(buf, sizeof(buf), "gif/delete?slot=%s", info.name);
                f["del"] = buf;
                f["del_l"] = info.has_builtin ? "Restore built-in" : "Back to Coaster";
                snprintf(buf, sizeof(buf), info.loaded ? "custom, %u KB" : "custom, not loaded (too big or not a GIF)",
                         (unsigned)(info.custom_size / 1024));
                f["v"] = buf;
            } else {
                f["v"] = info.has_builtin ? "built-in" : "Coaster acts this out";
            }
        }
    }

    // ---- system ----
    {
        JsonObject s = section(secs, "system", "System", "", nullptr);
        JsonObject f = field(s, "fw", "info", "Firmware"); f["v"] = FW_VERSION;
        f = field(s, "update", "firmware", "Update", "Installs the latest release from GitHub; settings stay.");
        f["repo"] = UPDATE_REPO;
#ifdef KNOMIV1
        f["asset"] = "knomiv1-octoprint-firmware.bin";
#else
        f["asset"] = "knomiv2-octoprint-firmware.bin";
#endif
        f = field(s, "backup", "link", "Download backup", "Holds all settings and custom animations, including your WiFi password and API key. Keep it private.");
        f["src"] = "backup";
        f = field(s, "backup", "file", "Restore a backup", "Replaces all settings and custom animations, then restarts.");
        f["accept"] = ".knomi"; f["post"] = "restore"; f["confirm"] = "Restore this backup? It replaces all settings and custom animations, then restarts the KNOMI.";
        JsonObject a = action(s, "Restart", "", "Restart the KNOMI?", true);
        a["data"]["restart"] = "1";
    }

    // ---- log ----
    {
        JsonObject s = section(secs, "log", "Log", "", nullptr);
        JsonObject f = field(s, "log", "view", "");
        f["src"] = "log.txt?tail=8000";
        action(s, "Clear log", "log/clear");
    }

    return json_string(d);
}
