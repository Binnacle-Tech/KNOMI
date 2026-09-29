#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <AsyncElegantOTA.h>
#include <esp_ota_ops.h>
#include "knomi_health.h"
#include <ESPmDNS.h>

#include "knomi.h"
#include "knomi_gif.h"
#include "discover.h"
#include "binnacle_css.h"
#include "knomi_ble.h"
#include "backup.h"
#include "layout_html.h"
#include "coaster_html.h"
#include "log_html.h"
#include "knomi_update.h"
#include "knomi_coaster.h"
#include "ui_overlay/lv_overlay.h"
#include <LittleFS.h>

static AsyncWebServer server(SERVER_PORT);

const char captive_html[] PROGMEM = R"rawliteral(<html>
<head>
<meta http-equiv="refresh" content="2;url=/" />
<title>For makers! By makers!</title>
</head>
<body>
You've successfully connected to the BTT KNOMI Screen. Click <a href="/">here</a> to go to the homepage.
</body>
</html>)rawliteral";

// This is a wrapper for a normal Async handler that allows the captive portal to intercept
// all requests regardless of their destination.
class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", captive_html);
  }
};

// This function will populate variables on the main html index page.
// Variables include WiFi SSIDs/RSSIs and the current mode.
static String html_escape(const String &in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

// value="..." attribute, or nothing when empty (so the placeholder shows)
static String value_attr(const char *v) {
    if (!v || !v[0]) return "";
    return "value=\"" + html_escape(v) + "\"";
}

// This function will populate variables on the main html index page.
// Variables include WiFi SSIDs/RSSIs and the current mode.
String knomi_html_processor(const String& var){
    String value = "";
    if (var == "wifi_list") {
        for (uint8_t i = 0; i < wifi_scan.count; i++) {
            value += "<tr class=\"showpop\" data-modal=\"modalOne\"><td class=\"ssid\">";
            value += html_escape(wifi_scan.ssid[i]);
            value += "</td><td class=\"num\">";
            value += wifi_scan.rssi[i];
            value += " dBm</td><td>";
            value += wifi_scan.connected[i] ? "<span class=\"pill ok\">connected</span>" : "";
            value += "</td></tr>";
        }
        if (wifi_scan.count == 0) {
            value = "<tr><td colspan=\"3\" style=\"color:var(--muted-2)\">No networks yet. Press Rescan.</td></tr>";
        }
    } else if (var == "ip") {
        value = value_attr(knomi_config.moonraker_ip);
    } else if (var == "port") {
        value = value_attr(knomi_config.moonraker_port);
    } else if (var == "tool") {
        value = value_attr(knomi_config.moonraker_tool);
    } else if (var == "api_key") {
        value = value_attr(knomi_config.api_key);
    } else if (var == "be_moonraker") {
        value = knomi_backend_is_octoprint() ? "" : "selected";
    } else if (var == "be_octoprint") {
        value = knomi_backend_is_octoprint() ? "selected" : "";
    } else if (var == "backend_pill") {
        value = knomi_backend_is_octoprint() ? "<span class=\"pill now\">OctoPrint</span>"
                                             : "<span class=\"pill now\">Moonraker</span>";
    } else if (var == "bl") {
        value = String(knomi_config.backlight);
    } else if (var == "dim_lvl") {
        value = String(knomi_config.dim_level);
    } else if (var == "dim_min") {
        value = String(knomi_config.dim_after_min);
    } else if (var == "sleep_min") {
        value = String(knomi_config.sleep_after_min);
    } else if (var == "aw_1" || var == "aw_0") {
        value = (knomi_config.awake_printing == (var == "aw_1" ? 1 : 0)) ? "selected" : "";
    } else if (var == "pv_0" || var == "pv_1") {
        value = (knomi_config.print_view == (uint8_t)(var[3] - '0')) ? "selected" : "";
    } else if (var.startsWith("tint_")) {
        uint8_t t = knomi_config.gif_tint ? GIF_TINT_ALL : GIF_TINT_OFF;   // "idle faces only" is gone: faces are Coaster
        value = (t == (uint8_t)(var[5] - '0')) ? "selected" : "";
    } else if (var == "bt_on" || var == "bt_off") {
        value = (knomi_config.bt_enabled == (var == "bt_on" ? 1 : 0)) ? "selected" : "";
    } else if (var == "wo_1" || var == "wo_0") {
        value = (knomi_config.bt_wifi_off == (var == "wo_1" ? 1 : 0)) ? "selected" : "";
    } else if (var == "wo_lock") {
        // turning WiFi off is only offered while a Bluetooth link is up (so you can't lock yourself out)
        value = (!knomi_config.bt_wifi_off && !knomi_ble_link_active()) ? "disabled" : "";
    } else if (var == "wo_hint") {
        if (knomi_config.bt_wifi_off) value = "WiFi turns off 10 s after Bluetooth connects.";
        else if (knomi_ble_link_active()) value = "Bluetooth is connected, so WiFi can be turned off.";
        else value = "Available once the plugin is connected over Bluetooth.";
    } else if (var == "bt_fb") {
        value = String(knomi_config.bt_fallback_s);
    } else if (var == "bt_addr") {
        value = knomi_ble_address();
    } else if (var == "bt_state") {
        if (!knomi_ble_running()) value = "<span class=\"pill held\">off</span>";
        else if (knomi_ble_link_active()) value = "<span class=\"pill ok\">connected</span>";
        else if (knomi_ble_connected()) value = "<span class=\"pill now\">paired, waiting for data</span>";
        else value = "<span class=\"pill held\">advertising</span>";
    } else if (var == "theme") {
        lv_color32_t c;
        c.full = lv_color_to32(knomi_config.theme_color);
        char hex[8];
        snprintf(hex, sizeof(hex), "#%02x%02x%02x", c.ch.red, c.ch.green, c.ch.blue);
        value = hex;
    } else if (var == "if_0" || var == "if_1") {
        value = (coaster_idle_enabled() == (var == "if_1")) ? "selected" : "";
    } else if (var == "idle_rot") {
        value = String(knomi_config.idle_rotate_s);
    } else if (var.startsWith("idle_c")) {
        value = (knomi_config.idle_mask & (1 << (var[6] - '1'))) ? "checked" : "";
    } else if (var == "hs_n" || var == "hs_b") {
        value = (knomi_config.heat_screens & (var == "hs_n" ? 1 : 2)) ? "checked" : "";
    } else if (var == "touch_idle") {
        value = String(knomi_config.touch_idle_s);
    } else if (var == "heated_s") {
        value = String(knomi_config.heated_s);
    } else if (var == "print_ok_s") {
        value = String(knomi_config.print_ok_s);
    } else if (var == "printed_s") {
        value = String(knomi_config.printed_s);
    } else if (var == "preset_rows") {
        for (int i = 0; i < PREHEAT_NUM; i++) {
            const knomi_preheat_t &pr = knomi_config.preheat[i];
            String n(i);
            value += "<div class=\"preset\"><input type=\"text\" name=\"pl" + n + "\" maxlength=\"9\" " +
                     value_attr(pr.label) + " aria-label=\"Preset " + String(i + 1) + " name\">"
                     "<input type=\"number\" class=\"mono\" name=\"pn" + n + "\" min=\"0\" max=\"500\" value=\"" +
                     String(pr.nozzle) + "\" aria-label=\"Nozzle\">"
                     "<input type=\"number\" class=\"mono\" name=\"pb" + n + "\" min=\"0\" max=\"200\" value=\"" +
                     String(pr.bed) + "\" aria-label=\"Bed\"></div>";
        }
    } else if (var == "extrude_rows") {
        for (int i = 0; i < EXTRUDE_NUM; i++) {
            String n(i);
            value += "<div class=\"preset ex\"><input type=\"number\" class=\"mono\" name=\"em" + n +
                     "\" min=\"1\" max=\"1000\" value=\"" + String(knomi_config.extrude_mm[i]) +
                     "\" aria-label=\"Length\"><input type=\"number\" class=\"mono\" name=\"es" + n +
                     "\" min=\"1\" max=\"300\" value=\"" + String(knomi_config.extrude_mms[i]) +
                     "\" aria-label=\"Speed\"><label class=\"def\"><input type=\"radio\" name=\"em_def\" value=\"" + n + "\"" +
                     (knomi_config.extrude_mm_def == i ? " checked" : "") + ">len</label>"
                     "<label class=\"def\"><input type=\"radio\" name=\"es_def\" value=\"" + n + "\"" +
                     (knomi_config.extrude_mms_def == i ? " checked" : "") + ">speed</label></div>";
        }
    } else if (var == "repo") {
        value = UPDATE_REPO;
    } else if (var == "board") {
#ifdef KNOMIV1
        value = "knomiv1";
#else
        value = "knomiv2";
#endif
    } else if (var == "fw") {
        value = FW_VERSION;
    } else if (var == "sta_ip") {
        value = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("not on WiFi");
    } else if (var == "ap_ssid") {
        value = value_attr(knomi_config.ap_ssid);
    } else if (var == "ap_password") {
        value = value_attr(knomi_config.ap_pwd);
    } else if (var == "hostname") {
        value = value_attr(knomi_config.hostname);
    } else  if (var == knomi_config.mode) {
        value = "selected";
    }
    return value;    // Could just be something between two normal $ signs in the HTML...
}

// Small Binnacle-styled page for POST results
String message_page(const String &title, const String &body_html) {
    return String("<!DOCTYPE html><html lang='en'><head><title>KNOMI</title>") + BINNACLE_HEAD +
        "</head><body><header class='rail'><div class='wrap rail-in'><div class='brand'>"
        "<a class='n' href='/'>" KNOMI_MARK "<span>KNOMI<span class='dot'>.</span></span></a><span class='f'>Printer display</span></div></div></header>"
        "<main class='wrap'><section class='mast'><h1>" + title + "<span class='dot'>.</span></h1></section>"
        "<section class='card'><div class='card-b'>" + body_html + "</div>"
        "<div class='card-f'><a class='btn-primary' href='/'>Back to settings</a></div></section></main></body></html>";
}

/*
 * mDNS
 * OTA
 */
#include "favicon.h"
#include "index_html.h"
typedef struct {
    const char * name;
    char * value;
    uint8_t v_len;
    uint8_t require;
} web_post_info_t;

web_post_info_t web_post_info[] = {
    {
        .name = "ssid",
        .value = knomi_config.sta_ssid,
        .v_len = sizeof(knomi_config.sta_ssid),
        .require = WEB_POST_WIFI_CONFIG_STA,
    },
    {
        .name = "password",
        .value = knomi_config.sta_pwd,
        .v_len = sizeof(knomi_config.sta_pwd),
        .require = WEB_POST_WIFI_CONFIG_STA,
    },
    {
        .name = "mode",
        .value = knomi_config.mode,
        .v_len = sizeof(knomi_config.mode),
        .require = WEB_POST_WIFI_CONFIG_MODE,
    },
    {
        .name = "ap_ssid",
        .value = knomi_config.ap_ssid,
        .v_len = sizeof(knomi_config.ap_ssid),
        .require = WEB_POST_WIFI_CONFIG_AP,
    },
    {
        .name = "ap_password",
        .value = knomi_config.ap_pwd,
        .v_len = sizeof(knomi_config.ap_pwd),
        .require = WEB_POST_WIFI_CONFIG_AP,
    },
    {
        .name = "hostname",
        .value = knomi_config.hostname,
        .v_len = sizeof(knomi_config.hostname),
        .require = WEB_POST_LOCAL_HOSTNAME,
    },
    {
        .name = "ip",
        .value = knomi_config.moonraker_ip,
        .v_len = sizeof(knomi_config.moonraker_ip),
        .require = WEB_POST_MOONRAKER,
    },
    {
        .name = "port",
        .value = knomi_config.moonraker_port,
        .v_len = sizeof(knomi_config.moonraker_port),
        .require = WEB_POST_MOONRAKER,
    },
    {
        .name = "tool",
        .value = knomi_config.moonraker_tool,
        .v_len = sizeof(knomi_config.moonraker_tool),
        .require = WEB_POST_MOONRAKER,
    },
    {
        .name = "backend",
        .value = knomi_config.backend,
        .v_len = sizeof(knomi_config.backend),
        .require = WEB_POST_MOONRAKER,
    },
    {
        .name = "api_key",
        .value = knomi_config.api_key,
        .v_len = sizeof(knomi_config.api_key),
        .require = WEB_POST_MOONRAKER,
    },
    {
        .name = "refresh",
        .value = NULL,
        .v_len = 0,
        .require = WEB_POST_WIFI_REFRESH,
    },
    {
        .name = "restart",
        .value = NULL,
        .v_len = 0,
        .require = WEB_POST_RESTART,
    },
};

static AsyncWebServerRequest * wifi_refresh_request = NULL;
void webserver_wifi_refresh_callback(void) {
    if (wifi_refresh_request == NULL) return;
    wifi_refresh_request->send_P(200, "text/html", index_html, knomi_html_processor);
    wifi_refresh_request = NULL;
}


/* ---------------- custom GIFs ---------------- */

static String gifs_page(void) {
    String page = String("<!DOCTYPE html><html lang='en'><head><title>KNOMI · Animations</title>") + BINNACLE_HEAD +
        "</head><body><header class='rail'><div class='wrap rail-in'><div class='brand'>"
        "<a class='n' href='/'>" KNOMI_MARK "<span>KNOMI<span class='dot'>.</span></span></a><span class='f'>Printer display</span></div>"
        "<span class='rail-sp'></span><nav><a href='/'>Settings</a><a class='on' href='/gifs'>Animations</a><a href='/layout'>Print screen</a><a href='/coaster'>Coaster face</a>"
        "<a href='/update'>Firmware</a><a href='/log'>Log</a></nav>" BINNACLE_MODES "</div></header><main class='wrap'>"
        "<section class='mast'><span class='label'>Animations</span><h1>Animations<span class='dot'>.</span></h1>"
        "<p class='lede'>Coaster acts out every state live. Upload a GIF to a slot to play your own animation there instead; it shows on the display right away. "
        "The screen is a 240&times;240 circle, so keep the subject centered.</p>";
    size_t used = LittleFS.usedBytes(), total = LittleFS.totalBytes();
    unsigned pct = total ? (unsigned)(used * 100 / total) : 0;
    page += "<div class='strip'><span class='pill'>" + String((unsigned)(used / 1024)) + " / " +
            String((unsigned)(total / 1024)) + " KB used</span><span class='pill'>max 1.5 MB per GIF</span>"
            "<span class='pill'>5 MB loaded total</span></div>"
            "<div class='meter' style='max-width:320px'><i style='width:" + String(pct) + "%'></i></div>"
            "<div class='rule'></div></section><div class='slots'>";
    for (int i = 0; i < GIF_SLOT_NUM; i++) {
        if (knomi_gif_is_face(i)) continue;   // faces are Coaster, drawn live
        knomi_gif_info_t info;
        knomi_gif_get_info((knomi_gif_slot_t)i, &info);
        String n = info.name;
        char idx[4];
        snprintf(idx, sizeof(idx), "%02d", i + 1);
        page += "<section class='card'><div class='card-h'><span class='idx'>" + String(idx) +
                "</span><span class='k'>" + String(info.label) + "</span></div><div class='card-b' style='display:flex;flex-direction:column;flex:1'>";
        if (info.has_custom || info.has_builtin) {
            page += "<div class='screen'><img loading='lazy' alt='' src='/gif/file?slot=" + n + "&t=" + String(millis()) + "'></div>";
        } else {
            page += "<div class='screen empty' style='flex-direction:column;gap:8px'><span style='display:block;width:72px;height:72px'>" KNOMI_MARK "</span>Coaster</div>";
        }
        page += "<div style='text-align:center;margin-bottom:12px'>";
        if (info.has_custom && info.loaded) {
            page += "<span class='pill ok'>custom · " + String((unsigned)(info.custom_size / 1024)) + " KB</span>";
        } else if (info.has_custom) {
            page += "<span class='pill bad'>not loaded: too big or not a GIF</span>";
        } else if (info.has_builtin) {
            page += "<span class='pill held'>built-in</span>";
        } else {
            page += "<span class='pill held'>Coaster acts this out</span>";
        }
        page += "</div><div class='slot-actions'>"
                "<form method='POST' action='/gif/upload?slot=" + n + "' enctype='multipart/form-data'>"
                "<input type='file' name='gif' accept='image/gif' required>"
                "<button type='submit' class='btn-ghost' style='justify-content:center'>Upload</button></form>";
        if (info.has_custom) {
            page += "<form method='POST' action='/gif/delete?slot=" + n + "'><button type='submit' class='btn-ghost' style='justify-content:center'>" +
                    String(info.has_builtin ? "Restore built-in" : "Back to Coaster") + "</button></form>";
        }
        page += "</div></div></section>";
    }
    page += "</div><div class='foot'>Files are kept in flash and survive firmware updates.</div></main></body></html>";
    return page;
}

static int slot_param(AsyncWebServerRequest *request) {
    if (!request->hasParam("slot")) return -1;
    return knomi_gif_slot_by_name(request->getParam("slot")->value().c_str());
}

typedef struct { int slot; bool error; char msg[80]; } gif_upload_state_t;

static void gif_upload_chunk(AsyncWebServerRequest *request, String filename, size_t index,
                             uint8_t *data, size_t len, bool final) {
    gif_upload_state_t *st = (gif_upload_state_t *)request->_tempObject;
    if (index == 0) {
        if (!st) {
            st = (gif_upload_state_t *)calloc(1, sizeof(gif_upload_state_t));
            request->_tempObject = st;
        }
        st->slot = slot_param(request);
        if (st->slot < 0) { st->error = true; strlcpy(st->msg, "Unknown slot", sizeof(st->msg)); return; }
        if (len < 4 || memcmp(data, "GIF8", 4) != 0) {
            st->error = true; strlcpy(st->msg, "That file is not a GIF", sizeof(st->msg)); return;
        }
        String tmp = "/gif/upload.tmp";
        LittleFS.remove(tmp);
        request->_tempFile = LittleFS.open(tmp, "w");
        if (!request->_tempFile) { st->error = true; strlcpy(st->msg, "Could not open file on flash", sizeof(st->msg)); return; }
    }
    if (!st || st->error) return;
    if (index + len > GIF_MAX_FILE_SIZE) {
        st->error = true; strlcpy(st->msg, "GIF is larger than 1.5 MB", sizeof(st->msg));
        request->_tempFile.close();
        LittleFS.remove("/gif/upload.tmp");
        return;
    }
    if (request->_tempFile.write(data, len) != len) {
        st->error = true; strlcpy(st->msg, "Flash is full", sizeof(st->msg));
        request->_tempFile.close();
        LittleFS.remove("/gif/upload.tmp");
        return;
    }
    if (final) {
        request->_tempFile.close();
        String path = knomi_gif_path((knomi_gif_slot_t)st->slot);
        LittleFS.remove(path);
        LittleFS.rename("/gif/upload.tmp", path);
        knomi_gif_request_reload((knomi_gif_slot_t)st->slot);
    }
}

static String upload_error_page(const String &msg) {
    String p = message_page("Upload failed", "<p><span class='pill bad'>" + html_escape(msg) + "</span></p>");
    p.replace("href='/'>Back to settings", "href='/gifs'>Back to animations");
    return p;
}

extern volatile bool knomi_display_settings_dirty;

static long int_param(AsyncWebServerRequest *request, const char *name, long lo, long hi, long fallback) {
    if (!request->hasParam(name, true)) return fallback;
    long v = request->getParam(name, true)->value().toInt();
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

// ESPAsyncWebServer matches "/log" for "/log/info" too (any "/log/..." path), and the first
// handler registered wins. Pages that have sub-paths only answer their exact URL.
static ArRequestFilterFunction exact(const char * uri) {
    return [uri](AsyncWebServerRequest * r) { return r->url() == uri; };
}

static void bluetooth_routes(void) {
    server.on("/bluetooth", HTTP_POST, [](AsyncWebServerRequest *request){
        uint8_t was_enabled = knomi_config.bt_enabled;
        uint8_t want_enabled = int_param(request, "bt_enabled", 0, 1, knomi_config.bt_enabled);
        // a disabled <select> isn't submitted, so a missing field means "unchanged"
        uint8_t want_off = int_param(request, "bt_wifi_off", 0, 1, knomi_config.bt_wifi_off);
        uint16_t fallback = int_param(request, "bt_fallback", 15, 3600, knomi_config.bt_fallback_s);

        String note;
        if (want_off && !knomi_config.bt_wifi_off && !knomi_ble_link_active()) {
            want_off = 0;
            note += "<p><span class='pill bad'>WiFi kept on</span> Bluetooth isn't connected right now. "
                    "Connect the plugin over Bluetooth first, then turn WiFi off.</p>";
        }
        if (!want_enabled) want_off = 0;
        knomi_config.bt_enabled = want_enabled;
        knomi_config.bt_wifi_off = want_off;
        knomi_config.bt_fallback_s = fallback;
        knomi_config_require_change(LOCAL_POST_SETTINGS);

        if (want_enabled != was_enabled) {
            note += String("<p><span class='pill now'>restart needed</span> Bluetooth turns ") +
                    (want_enabled ? "on" : "off") + " after a restart (System &rsaquo; Restart).</p>";
        }
        if (want_off) {
            note += "<p><span class='pill now'>WiFi turning off</span> In about 10 seconds WiFi goes off and this page "
                    "stops loading. It comes back if Bluetooth is disconnected for " + String(fallback) +
                    " seconds, or when you press <b>Turn KNOMI WiFi on</b> in the plugin settings.</p>";
        }
        if (note.isEmpty()) {
            request->redirect("/#bluetooth");
            return;
        }
        request->send(200, "text/html", message_page("Bluetooth saved", note));
    }).setFilter(exact("/bluetooth"));
    server.on("/bluetooth/forget", HTTP_POST, [](AsyncWebServerRequest *request){
        knomi_ble_forget_bonds();
        request->send(200, "text/html", message_page("Paired devices forgotten",
            "<p>Pair the Pi again with <span class='mono'>bluetoothctl</span>. Remove the old pairing there first "
            "(<span class='mono'>remove " + html_escape(knomi_ble_address()) + "</span>).</p>"));
    });
}

static void display_routes(void) {
    server.on("/display", HTTP_POST, [](AsyncWebServerRequest *request){
        knomi_config.backlight = int_param(request, "bl", 1, 16, knomi_config.backlight);
        knomi_config.dim_level = int_param(request, "dim_lvl", 1, 16, knomi_config.dim_level);
        knomi_config.dim_after_min = int_param(request, "dim_min", 0, 1440, knomi_config.dim_after_min);
        knomi_config.sleep_after_min = int_param(request, "sleep_min", 0, 1440, knomi_config.sleep_after_min);
        knomi_config.awake_printing = int_param(request, "awake_print", 0, 1, knomi_config.awake_printing);
        knomi_config.print_view = int_param(request, "print_view", 0, PRINT_VIEW_ACCEL, knomi_config.print_view);
        knomi_config.gif_tint = int_param(request, "gif_tint", 0, GIF_TINT_ALL, knomi_config.gif_tint);
        knomi_config_require_change(LOCAL_POST_SETTINGS);  // save to EEPROM (WiFi task)
        knomi_display_settings_dirty = true;               // apply on screen (LVGL task)
        request->redirect("/#display");
    });
}

static uint8_t hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c |= 0x20;
    return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0;
}

static void screen_routes(void) {
    server.on("/screen", HTTP_POST, [](AsyncWebServerRequest *request){
        knomi_config_t &c = knomi_config;
        if (request->hasParam("theme_default", true)) {
            c.theme_color = lv_color_hex(LV_DEFAULT_COLOR);
        } else if (request->hasParam("theme", true)) {
            String h = request->getParam("theme", true)->value();
            if (h.length() == 7 && h[0] == '#') {
                uint32_t rgb = 0;
                for (int i = 1; i < 7; i++) rgb = (rgb << 4) | hex_nibble(h[i]);
                c.theme_color = lv_color_hex(rgb);
            }
        }
        c.idle_rotate_s = int_param(request, "idle_rot", 0, 3600, c.idle_rotate_s);
        uint8_t mask = 0;
        for (int i = 0; i < 4; i++) {
            if (request->hasParam(String("idle_m") + (i + 1), true)) mask |= 1 << i;
        }
        c.idle_mask = mask ? mask : 0x01;
        c.heat_screens = (request->hasParam("hs_n", true) ? 1 : 0) | (request->hasParam("hs_b", true) ? 2 : 0);
        c.touch_idle_s = int_param(request, "touch_idle", 0, 3600, c.touch_idle_s);
        c.heated_s = int_param(request, "heated_s", 0, 600, c.heated_s);
        c.print_ok_s = int_param(request, "print_ok_s", 0, 600, c.print_ok_s);
        c.printed_s = int_param(request, "printed_s", 0, 3600, c.printed_s);
        knomi_config_sanitize_screen();
        knomi_config_require_change(LOCAL_POST_SETTINGS);
        knomi_display_settings_dirty = true;
        request->redirect("/#screen");
    });
    server.on("/presets", HTTP_POST, [](AsyncWebServerRequest *request){
        knomi_config_t &c = knomi_config;
        if (request->hasParam("reset", true)) {
            knomi_config_default_presets();
        } else {
            for (int i = 0; i < PREHEAT_NUM; i++) {
                String n(i);
                if (request->hasParam("pl" + n, true)) {
                    String l = request->getParam("pl" + n, true)->value();
                    l.trim();
                    strlcpy(c.preheat[i].label, l.c_str(), sizeof(c.preheat[i].label));
                }
                c.preheat[i].nozzle = int_param(request, ("pn" + n).c_str(), 0, 500, c.preheat[i].nozzle);
                c.preheat[i].bed = int_param(request, ("pb" + n).c_str(), 0, 200, c.preheat[i].bed);
            }
            for (int i = 0; i < EXTRUDE_NUM; i++) {
                String n(i);
                c.extrude_mm[i] = int_param(request, ("em" + n).c_str(), 1, 1000, c.extrude_mm[i]);
                c.extrude_mms[i] = int_param(request, ("es" + n).c_str(), 1, 300, c.extrude_mms[i]);
            }
            c.extrude_mm_def = int_param(request, "em_def", 0, EXTRUDE_NUM - 1, c.extrude_mm_def);
            c.extrude_mms_def = int_param(request, "es_def", 0, EXTRUDE_NUM - 1, c.extrude_mms_def);
        }
        knomi_config_sanitize_screen();
        knomi_config_require_change(LOCAL_POST_SETTINGS);
        knomi_display_settings_dirty = true;
        request->redirect("/#presets");
    });
}

extern const char layout_default_json[];

static void layout_routes(void) {
    server.on("/layout", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", layout_html);
    }).setFilter(exact("/layout"));
    server.on("/layout.json", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("default") && LittleFS.exists("/layout.json")) {
            request->send(LittleFS, "/layout.json", "application/json");
        } else {
            request->send(200, "application/json", layout_default_json);
        }
    });
    // body collected in a malloc'd buffer: the server free()s _tempObject if the request is aborted
    server.on("/layout.json", HTTP_POST, [](AsyncWebServerRequest *request){
        char *body = (char *)request->_tempObject;
        size_t len = body ? strlen(body) : 0;
        const char *err = !body ? "Layout too large or empty" : print_layout_validate(body, len);
        if (!err) {
            File f = LittleFS.open("/layout.tmp", "w");
            if (!f || f.write((const uint8_t *)body, len) != len) err = "Couldn't write to flash";
            if (f) f.close();
            if (!err) {
                LittleFS.remove("/layout.json");
                LittleFS.rename("/layout.tmp", "/layout.json");
                print_layout_request_reload();
            }
        }
        if (err) request->send(400, "text/plain", err);
        else request->send(200, "text/plain", "ok");
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (total == 0 || total > 12288) return;
        if (index == 0) {
            char *b = (char *)malloc(total + 1);
            if (b) b[0] = 0;
            request->_tempObject = b;
        }
        char *b = (char *)request->_tempObject;
        if (!b || index + len > total) return;
        memcpy(b + index, data, len);
        b[index + len] = 0;
    });
    server.on("/layout/reset", HTTP_POST, [](AsyncWebServerRequest *request){
        LittleFS.remove("/layout.json");
        print_layout_request_reload();
        request->send(200, "text/plain", "ok");
    });
    server.on("/layout/preview", HTTP_POST, [](AsyncWebServerRequest *request){
        print_layout_preview(20);
        request->send(200, "text/plain", "ok");
    });
    server.on("/status.json", HTTP_GET, [](AsyncWebServerRequest *request){
        lv_color32_t c;
        c.full = lv_color_to32(knomi_config.theme_color);
        char hex[8];
        snprintf(hex, sizeof(hex), "#%02x%02x%02x", c.ch.red, c.ch.green, c.ch.blue);
        String s = print_layout_status_json();   // {"printing":..,"tokens":{..}}
        s.remove(s.length() - 1);
        s += ",\"theme\":\"";
        s += hex;
        s += "\",\"gifs\":[";
        for (int i = 0; i < GIF_SLOT_NUM; i++) {
            knomi_gif_info_t info;
            knomi_gif_get_info((knomi_gif_slot_t)i, &info);
            if (!info.has_builtin && !info.has_custom) continue;
            if (knomi_gif_is_face(i)) continue;
            if (s[s.length() - 1] != '[') s += ",";
            s += "{\"name\":\"" + String(info.name) + "\",\"label\":\"" + html_escape(info.label) + "\"}";
        }
        s += "]}";
        AsyncWebServerResponse *r = request->beginResponse(200, "application/json", s);
        r->addHeader("Cache-Control", "no-store");
        request->send(r);
    });
}

static const char * reset_reason_text(void) {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "power on";
        case ESP_RST_SW: return "restart";
        case ESP_RST_PANIC: return "crash";
        case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "watchdog";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_DEEPSLEEP: return "deep sleep";
        default: return "other";
    }
}

static void update_routes(void) {
    server.on("/update/github", HTTP_POST, [](AsyncWebServerRequest *request){
        bool force = request->hasParam("force", true);
        knomi_update_start(force);
        request->send(200, "application/json", knomi_update_status_json());
    });
    server.on("/update/progress", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *r = request->beginResponse(200, "application/json", knomi_update_status_json());
        r->addHeader("Cache-Control", "no-store");
        request->send(r);
    });
}

static void log_routes(void) {
    server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", log_html);
    }).setFilter(exact("/log"));
    server.on("/log.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *r = request->beginResponse(200, "text/plain; charset=utf-8", knomi_log_text());
        r->addHeader("Cache-Control", "no-store");
        if (request->hasParam("dl")) {
            r->addHeader("Content-Disposition", String("attachment; filename=\"knomi-") + knomi_config.hostname + "-log.txt\"");
        }
        request->send(r);
    });
    server.on("/log/clear", HTTP_POST, [](AsyncWebServerRequest *request){
        knomi_log_clear();
        request->send(200, "text/plain", "ok");
    });
    server.on("/log/info", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonDocument d(1536);
        d["fw"] = FW_VERSION;
#ifdef KNOMIV1
        d["board"] = "KNOMI 1";
#else
        d["board"] = "KNOMI 2";
#endif
        d["uptime"] = millis() / 1000;
        d["reset"] = reset_reason_text();
        d["heap"] = ESP.getFreeHeap();
        d["heap_min"] = ESP.getMinFreeHeap();
        d["psram"] = ESP.getFreePsram();
        if (WiFi.status() == WL_CONNECTED)
            d["wifi"] = WiFi.SSID() + " · " + String(WiFi.RSSI()) + " dBm · " + WiFi.localIP().toString();
        else d["wifi"] = knomi_ble_link_active() ? "off (Bluetooth link)" : "not connected";
        d["backend"] = knomi_config.backend;
        d["host"] = String(knomi_config.moonraker_ip) + ":" + knomi_config.moonraker_port;
        String st = coaster_state_json();
        DynamicJsonDocument cs(3072);
        deserializeJson(cs, st);
        d["mood"] = cs["mood"] | "?";
        // free stack per task (bytes never used so far): tools/check.py warns when one runs low
        static const char * tn[KNOMI_TASKS] = {"ui", "accel", "wifi", "printer", "post"};
        JsonObject stacks = d.createNestedObject("stacks");
        for (int i = 0; i < KNOMI_TASKS; i++) if (knomi_tasks[i]) stacks[tn[i]] = uxTaskGetStackHighWaterMark(knomi_tasks[i]);
        const esp_partition_t * run = esp_ota_get_running_partition();
        d["slot"] = run ? run->label : "?";
        d["fs_used"] = LittleFS.usedBytes();
        d["fs_total"] = LittleFS.totalBytes();
        String out;
        serializeJson(d, out);
        AsyncWebServerResponse *r = request->beginResponse(200, "application/json", out);
        r->addHeader("Cache-Control", "no-store");
        request->send(r);
    });
}

static void coaster_routes(void) {
    server.on("/coaster", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", coaster_html);
    }).setFilter(exact("/coaster"));
    server.on("/coaster.json", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", coaster_tuning_json());
    });
    server.on("/coaster.json", HTTP_POST, [](AsyncWebServerRequest *request){
        char *body = (char *)request->_tempObject;
        const char *err = body ? coaster_save_json(body, strlen(body)) : "Empty";
        if (err) request->send(400, "text/plain", err);
        else request->send(200, "text/plain", "ok");
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (total == 0 || total > 1024) return;
        if (index == 0) { char *b = (char *)malloc(total + 1); if (b) b[0] = 0; request->_tempObject = b; }
        char *b = (char *)request->_tempObject;
        if (!b || index + len > total) return;
        memcpy(b + index, data, len);
        b[index + len] = 0;
    });
    server.on("/coaster/card", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *r = request->beginResponse(200, "application/json", coaster_plugin_json());
        r->addHeader("Cache-Control", "no-store");
        request->send(r);
    });
    server.on("/coaster/album", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *r = request->beginResponse(200, "application/json", coaster_album_json());
        r->addHeader("Cache-Control", "no-store");
        request->send(r);
    });
    server.on("/coaster/state", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *r = request->beginResponse(200, "application/json", coaster_state_json());
        r->addHeader("Cache-Control", "no-store");
        request->send(r);
    });
}

static void gif_routes(void) {
    server.on("/binnacle.css", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", binnacle_css);
        response->addHeader("Cache-Control", "max-age=86400");
        request->send(response);
    });
    server.on("/gifs", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", gifs_page());
    });
    server.on("/gif/file", HTTP_GET, [](AsyncWebServerRequest *request){
        int slot = slot_param(request);
        if (slot < 0) { request->send(404); return; }
        String path = knomi_gif_path((knomi_gif_slot_t)slot);
        if (LittleFS.exists(path)) {
            AsyncWebServerResponse *r = request->beginResponse(LittleFS, path, "image/gif");
            r->addHeader("Cache-Control", "max-age=300"); // pages add a version parameter
            request->send(r);
            return;
        }
        const lv_img_dsc_t *b = knomi_gif_builtin((knomi_gif_slot_t)slot);
        if (b) {
            AsyncWebServerResponse *r = request->beginResponse_P(200, "image/gif", b->data, b->data_size);
            r->addHeader("Cache-Control", "max-age=300");
            request->send(r);
        } else {
            request->send(404);
        }
    });
    server.on("/gif/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        gif_upload_state_t *st = (gif_upload_state_t *)request->_tempObject;
        if (!st) { request->send(400, "text/html", upload_error_page("No file received")); return; }
        if (st->error) { request->send(400, "text/html", upload_error_page(st->msg)); return; }
        request->redirect("/gifs");
    }, gif_upload_chunk);
    server.on("/gif/delete", HTTP_POST, [](AsyncWebServerRequest *request){
        int slot = slot_param(request);
        if (slot >= 0) {
            LittleFS.remove(knomi_gif_path((knomi_gif_slot_t)slot));
            knomi_gif_request_reload((knomi_gif_slot_t)slot);
        }
        request->redirect("/gifs");
    });
    server.on("/discover", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("start")) octoprint_discover_start();
        request->send(200, "application/json", octoprint_discover_json());
    });
}

void webserver_setup(void) {

    if(!MDNS.begin(knomi_config.hostname)) {
        Serial.println("Error starting mDNS");
    } else {
        Serial.println("mDNS start ok!");
    }
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", btt_logo_only_ico, sizeof(btt_logo_only_ico));
        request->send(response);
    });

    gif_routes();
    display_routes();
    screen_routes();
    layout_routes();
    coaster_routes();
    log_routes();
    update_routes();
    AsyncElegantOTA.begin(&server);   // after /update/github and /update/progress, or its /update grabs them
    bluetooth_routes();
    backup_routes(server);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html, knomi_html_processor);
    });

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
        uint8_t post_require = WEB_POST_NULL;
        int paramsNr = request->params();
        for (int i = 0; i < paramsNr; i++) {
            AsyncWebParameter* p = request->getParam(i);
            // never log secrets (the log is readable from the web page)
            const String & pn = p->name();
            bool secret = pn.indexOf("password") >= 0 || pn.indexOf("pwd") >= 0 || pn == "api_key";
            Serial.printf("setting %s = %s\r\n", pn.c_str(), secret ? "(hidden)" : p->value().c_str());

            for (uint8_t i = 0; i < ACOUNT(web_post_info); i++) {
                if (strcmp(web_post_info[i].name, p->name().c_str()) == 0) {
                    if (web_post_info[i].value == NULL) {
                        post_require |= web_post_info[i].require;
                        break;
                    }
                    if (strcmp(web_post_info[i].value, p->value().c_str()) != 0) {
                        post_require |= web_post_info[i].require;
                        strlcpy(web_post_info[i].value, p->value().c_str(), web_post_info[i].v_len);
                    }
                    break;
                }
            }
        }
        if ((post_require & WEB_POST_WIFI_CONFIG_STA)) {
            if (strcmp(knomi_config.mode, "ap") == 0) {
                strlcpy(knomi_config.mode, "sta", sizeof(knomi_config.mode));
                post_require |= WEB_POST_WIFI_CONFIG_MODE;
            }
            knomi_config.sta_auth = wifi_get_ahth_mode_from_scanned_list();
        }

        knomi_config_require_change(post_require);

        if (post_require & WEB_POST_WIFI_CONFIG_STA) {
            request->send(200, "text/html", message_page("Connecting",
                "<dl class='kv'><dt>Network</dt><dd>" + html_escape(knomi_config.sta_ssid) + "</dd></dl>"
                "<p>KNOMI is joining this network now. If that fails within 15 seconds, it brings its own "
                "access point back up so you can try again.</p>"));
        } else if (post_require & WEB_POST_LOCAL_HOSTNAME){
            request->send(200, "text/html", message_page("Saved",
                "<p><span class='pill ok'>saved</span></p><p>The new hostname takes effect after a restart. "
                "Restart from the System section on the settings page.</p>"));
        } else if (post_require & WEB_POST_RESTART){
            request->send(200, "text/html", message_page("Restarting",
                "<p><span class='pill now'>restarting</span></p><p>KNOMI is restarting. Reconnect once it's back, "
                "usually 10 to 20 seconds.</p>"));
        } else if (post_require & WEB_POST_WIFI_REFRESH) {
            wifi_refresh_request = request;
            wifi_scan_refresh_set_callback(webserver_wifi_refresh_callback);
        } else {
            request->send_P(200, "text/html", index_html, knomi_html_processor);
        }
    });

    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    server.begin();
}
