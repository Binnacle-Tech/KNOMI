// Settings + custom GIF backup and restore.
//
// File format (streamed both ways, so a few MB of GIFs never sits in RAM):
//   KNOMI-BACKUP 1\n
//   C <len>\n<settings JSON>
//   G <slot> <len>\n<gif bytes>        (one per custom GIF)
//   E\n
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "knomi.h"
#include "knomi_gif.h"
#include "backup.h"

#define BACKUP_MAGIC "KNOMI-BACKUP 1\n"

/* ---------------- settings <-> JSON ---------------- */

String backup_config_json(void) {
    DynamicJsonDocument d(4096);
    const knomi_config_t &c = knomi_config;
    d["v"] = 1;
    d["fw"] = FW_VERSION;
    d["sta_ssid"] = c.sta_ssid;
    d["sta_pwd"] = c.sta_pwd;
    d["ap_ssid"] = c.ap_ssid;
    d["ap_pwd"] = c.ap_pwd;
    d["hostname"] = c.hostname;
    d["mode"] = c.mode;
    d["backend"] = c.backend;
    d["host"] = c.moonraker_ip;
    d["port"] = c.moonraker_port;
    d["tool"] = c.moonraker_tool;
    d["api_key"] = c.api_key;
    d["theme"] = c.theme_color.full;
    d["backlight"] = c.backlight;
    d["dim_level"] = c.dim_level;
    d["dim_after_min"] = c.dim_after_min;
    d["sleep_after_min"] = c.sleep_after_min;
    d["awake_printing"] = c.awake_printing;
    d["print_view"] = c.print_view;
    d["gif_tint"] = c.gif_tint;
    d["bt_enabled"] = c.bt_enabled;
    d["bt_wifi_off"] = c.bt_wifi_off;
    d["bt_fallback_s"] = c.bt_fallback_s;
    d["idle_rotate_s"] = c.idle_rotate_s;
    d["idle_mask"] = c.idle_mask;
    d["heat_screens"] = c.heat_screens;
    d["touch_idle_s"] = c.touch_idle_s;
    d["heated_s"] = c.heated_s;
    d["print_ok_s"] = c.print_ok_s;
    d["printed_s"] = c.printed_s;
    JsonArray pre = d.createNestedArray("preheat");
    for (int i = 0; i < PREHEAT_NUM; i++) {
        JsonArray e = pre.createNestedArray();
        e.add(c.preheat[i].label);
        e.add(c.preheat[i].nozzle);
        e.add(c.preheat[i].bed);
    }
    JsonArray em = d.createNestedArray("extrude_mm");
    JsonArray es = d.createNestedArray("extrude_mms");
    for (int i = 0; i < EXTRUDE_NUM; i++) {
        em.add(c.extrude_mm[i]);
        es.add(c.extrude_mms[i]);
    }
    d["extrude_mm_def"] = c.extrude_mm_def;
    d["extrude_mms_def"] = c.extrude_mms_def;
    String out;
    serializeJson(d, out);
    return out;
}

static void copy_str(JsonVariantConst v, char *dst, size_t n) {
    if (v.is<const char *>()) strlcpy(dst, v.as<const char *>(), n);
}

// Missing keys keep their current value. Ranges are re-checked on the next boot
// (knomi_config_sanitize_*), and a restore always restarts the KNOMI.
bool backup_apply_config_json(const char *json, size_t len) {
    DynamicJsonDocument d(6144);
    if (deserializeJson(d, json, len) != DeserializationError::Ok) return false;
    if ((d["v"] | 0) != 1) return false;
    knomi_config_t &c = knomi_config;
    copy_str(d["sta_ssid"], c.sta_ssid, sizeof(c.sta_ssid));
    copy_str(d["sta_pwd"], c.sta_pwd, sizeof(c.sta_pwd));
    copy_str(d["ap_ssid"], c.ap_ssid, sizeof(c.ap_ssid));
    copy_str(d["ap_pwd"], c.ap_pwd, sizeof(c.ap_pwd));
    copy_str(d["hostname"], c.hostname, sizeof(c.hostname));
    copy_str(d["mode"], c.mode, sizeof(c.mode));
    copy_str(d["backend"], c.backend, sizeof(c.backend));
    copy_str(d["host"], c.moonraker_ip, sizeof(c.moonraker_ip));
    copy_str(d["port"], c.moonraker_port, sizeof(c.moonraker_port));
    copy_str(d["tool"], c.moonraker_tool, sizeof(c.moonraker_tool));
    copy_str(d["api_key"], c.api_key, sizeof(c.api_key));
    if (d.containsKey("theme")) c.theme_color.full = d["theme"];
    c.backlight = d["backlight"] | c.backlight;
    c.dim_level = d["dim_level"] | c.dim_level;
    c.dim_after_min = d["dim_after_min"] | c.dim_after_min;
    c.sleep_after_min = d["sleep_after_min"] | c.sleep_after_min;
    c.awake_printing = d["awake_printing"] | c.awake_printing;
    c.print_view = d["print_view"] | c.print_view;
    c.gif_tint = d["gif_tint"] | c.gif_tint;
    c.bt_enabled = d["bt_enabled"] | c.bt_enabled;
    c.bt_wifi_off = d["bt_wifi_off"] | c.bt_wifi_off;
    c.bt_fallback_s = d["bt_fallback_s"] | c.bt_fallback_s;
    c.idle_rotate_s = d["idle_rotate_s"] | c.idle_rotate_s;
    c.idle_mask = d["idle_mask"] | c.idle_mask;
    c.heat_screens = d["heat_screens"] | c.heat_screens;
    c.touch_idle_s = d["touch_idle_s"] | c.touch_idle_s;
    c.heated_s = d["heated_s"] | c.heated_s;
    c.print_ok_s = d["print_ok_s"] | c.print_ok_s;
    c.printed_s = d["printed_s"] | c.printed_s;
    JsonArrayConst pre = d["preheat"];
    for (int i = 0; i < PREHEAT_NUM && i < (int)pre.size(); i++) {
        copy_str(pre[i][0], c.preheat[i].label, sizeof(c.preheat[i].label));
        c.preheat[i].nozzle = pre[i][1] | c.preheat[i].nozzle;
        c.preheat[i].bed = pre[i][2] | c.preheat[i].bed;
    }
    JsonArrayConst em = d["extrude_mm"], es = d["extrude_mms"];
    for (int i = 0; i < EXTRUDE_NUM; i++) {
        c.extrude_mm[i] = em[i] | c.extrude_mm[i];
        c.extrude_mms[i] = es[i] | c.extrude_mms[i];
    }
    c.extrude_mm_def = d["extrude_mm_def"] | c.extrude_mm_def;
    c.extrude_mms_def = d["extrude_mms_def"] | c.extrude_mms_def;
    knomi_config_sanitize_screen();
    if (strcmp(c.mode, "ap") && strcmp(c.mode, "sta") && strcmp(c.mode, "apsta")) strlcpy(c.mode, "ap", sizeof(c.mode));
    return true;
}

/* ---------------- download (chunked) ---------------- */

struct backup_gen_t {
    int phase = 0;        // 0 magic, 1 config, 2 gifs, 3 end, 4 done
    int slot = 0;
    String pending;       // header/config bytes still to send
    size_t pending_off = 0;
    File file;
};

static size_t backup_fill(backup_gen_t *g, uint8_t *buf, size_t max_len) {
    size_t out = 0;
    while (out < max_len) {
        if (g->pending_off < g->pending.length()) {
            size_t n = min(max_len - out, (size_t)(g->pending.length() - g->pending_off));
            memcpy(buf + out, g->pending.c_str() + g->pending_off, n);
            g->pending_off += n;
            out += n;
            continue;
        }
        if (g->file) {
            size_t n = g->file.read(buf + out, max_len - out);
            if (n == 0) g->file.close();
            out += n;
            continue;
        }
        g->pending = "";
        g->pending_off = 0;
        if (g->phase == 0) {
            g->pending = BACKUP_MAGIC;
            g->phase = 1;
        } else if (g->phase == 1) {
            String cfg = backup_config_json();
            g->pending = "C " + String(cfg.length()) + "\n" + cfg;
            g->phase = 2;
        } else if (g->phase == 2) {
            // next custom GIF, if any
            while (g->slot < GIF_SLOT_NUM) {
                knomi_gif_slot_t s = (knomi_gif_slot_t)g->slot++;
                String path = knomi_gif_path(s);
                if (!LittleFS.exists(path)) continue;
                g->file = LittleFS.open(path, "r");
                if (!g->file) continue;
                knomi_gif_info_t info;
                knomi_gif_get_info(s, &info);
                g->pending = String("G ") + info.name + " " + String((unsigned)g->file.size()) + "\n";
                break;
            }
            if (g->pending.isEmpty() && !g->file) g->phase = 3;
        } else if (g->phase == 3) {
            g->pending = "E\n";
            g->phase = 4;
        } else {
            break; // done
        }
    }
    return out;
}

/* ---------------- restore (streamed upload) ---------------- */

struct restore_t {
    int state = 0;        // 0 magic, 1 record header, 2 config body, 3 gif body, 4 done, -1 error
    String line;          // header line being read
    String cfg;           // config JSON being read
    size_t remaining = 0;
    File file;
    bool cfg_ok = false;
    int gifs = 0;
    char err[80] = {0};
};

// A restore replaces all custom GIFs, but only after the complete file arrived:
// GIFs are written as <slot>.gif.rst and swapped in at the end record.
static void restore_clear_staged(void) {
    for (int s = 0; s < GIF_SLOT_NUM; s++) LittleFS.remove(knomi_gif_path((knomi_gif_slot_t)s) + ".rst");
}

static void restore_commit_staged(void) {
    for (int s = 0; s < GIF_SLOT_NUM; s++) {
        String live = knomi_gif_path((knomi_gif_slot_t)s);
        LittleFS.remove(live);
        if (LittleFS.exists(live + ".rst")) LittleFS.rename(live + ".rst", live);
    }
}

static void restore_fail(restore_t *r, const char *msg) {
    if (r->file) r->file.close();
    restore_clear_staged();
    r->state = -1;
    strlcpy(r->err, msg, sizeof(r->err));
}

static void restore_feed(restore_t *r, const uint8_t *data, size_t len) {
    size_t i = 0;
    while (i < len && r->state >= 0 && r->state != 4) {
        if (r->state == 0 || r->state == 1) {
            char ch = (char)data[i++];
            if (ch != '\n') {
                if (r->line.length() > 80) { restore_fail(r, "Not a KNOMI backup file"); return; }
                r->line += ch;
                continue;
            }
            String l = r->line;
            r->line = "";
            if (r->state == 0) {
                if (l + "\n" != BACKUP_MAGIC) { restore_fail(r, "Not a KNOMI backup file"); return; }
                restore_clear_staged();
                r->state = 1;
            } else if (l.startsWith("C ")) {
                r->remaining = l.substring(2).toInt();
                if (r->remaining == 0 || r->remaining > 4096) { restore_fail(r, "Bad settings block"); return; }
                r->cfg = "";
                r->state = 2;
            } else if (l.startsWith("G ")) {
                int sp = l.indexOf(' ', 2);
                String name = l.substring(2, sp);
                r->remaining = l.substring(sp + 1).toInt();
                int slot = knomi_gif_slot_by_name(name.c_str());
                if (sp < 0 || r->remaining > GIF_MAX_FILE_SIZE) { restore_fail(r, "Bad animation block"); return; }
                if (slot >= 0) {
                    // staged next to the live file; swapped in only once the whole backup arrived
                    r->file = LittleFS.open(knomi_gif_path((knomi_gif_slot_t)slot) + ".rst", "w");
                    if (!r->file) { restore_fail(r, "Could not write to flash"); return; }
                    r->gifs++;
                }
                r->state = 3;   // unknown slots (from a newer firmware) are skipped
            } else if (l == "E") {
                restore_commit_staged();
                r->state = 4;
            } else {
                restore_fail(r, "Bad record in backup");
                return;
            }
        } else if (r->state == 2) {
            size_t n = min(r->remaining, len - i);
            r->cfg.concat((const char *)(data + i), n);
            i += n;
            r->remaining -= n;
            if (r->remaining == 0) r->state = 1;
        } else if (r->state == 3) {
            size_t n = min(r->remaining, len - i);
            if (r->file && r->file.write(data + i, n) != n) { restore_fail(r, "Flash is full"); return; }
            i += n;
            r->remaining -= n;
            if (r->remaining == 0) {
                if (r->file) r->file.close();
                r->state = 1;
            }
        }
    }
}

// Test hooks (host builds feed bytes through the same parser)
void *backup_restore_begin(void) { return new restore_t(); }
void backup_restore_feed(void *r, const uint8_t *d, size_t n) { restore_feed((restore_t *)r, d, n); }

/* ---------------- routes ---------------- */

String message_page(const String &title, const String &body_html);  // webserver.cpp

void backup_routes(AsyncWebServer &server) {
    server.on("/backup", HTTP_GET, [](AsyncWebServerRequest *request){
        backup_gen_t *g = new backup_gen_t();
        AsyncWebServerResponse *resp = request->beginChunkedResponse("application/octet-stream",
            [g](uint8_t *buf, size_t max_len, size_t index) -> size_t {
                size_t n = backup_fill(g, buf, max_len);
                return n;
            });
        String fname = String("knomi-") + knomi_config.hostname + "-backup.knomi";
        resp->addHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
        request->onDisconnect([g](){ if (g->file) g->file.close(); delete g; });
        request->send(resp);
    });

    server.on("/restore", HTTP_POST, [](AsyncWebServerRequest *request){
        restore_t *r = (restore_t *)request->_tempObject;
        String body;
        bool ok = false;
        if (!r) {
            body = "<p><span class='pill bad'>no file received</span></p>";
        } else if (r->state != 4) {
            const char *why = r->err[0] ? r->err : (r->state == 0 ? "Not a KNOMI backup file" : "Backup file is incomplete");
            if (r->file) r->file.close();
            restore_clear_staged();
            body = String("<p><span class='pill bad'>") + why + "</span></p><p>Nothing was changed.</p>";
        } else if (!r->cfg_ok) {
            body = "<p><span class='pill bad'>settings in the backup couldn't be read</span></p>";
        } else {
            ok = true;
            body = "<p><span class='pill ok'>restored</span> Settings and " + String(r->gifs) +
                   " custom animation(s). The KNOMI is restarting to apply them. If the WiFi settings "
                   "changed, reconnect on the restored network.</p>";
        }
        if (r) {
            if (r->file) r->file.close();
            delete r;
            request->_tempObject = nullptr;
        }
        request->send(200, "text/html", message_page(ok ? "Restored" : "Restore failed", body));
        if (ok) knomi_config_require_change(LOCAL_POST_SETTINGS | WEB_POST_RESTART);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        restore_t *r = (restore_t *)request->_tempObject;
        if (index == 0 && !r) {
            r = new restore_t();
            request->_tempObject = r;
            // the server free()s _tempObject on teardown; delete it properly if the
            // client drops before the handler above runs
            request->onDisconnect([request](){
                restore_t *left = (restore_t *)request->_tempObject;
                if (left) {
                    if (left->file) left->file.close();
                    delete left;
                    request->_tempObject = nullptr;
                }
            });
        }
        if (!r) return;
        restore_feed(r, data, len);
        if (final && r->state == 4) {
            r->cfg_ok = backup_apply_config_json(r->cfg.c_str(), r->cfg.length());
        }
    });
}
