#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ESPmDNS.h>

#include "knomi.h"
#include "knomi_gif.h"
#include "discover.h"
#include "binnacle_css.h"
#include "knomi_ble.h"
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
        value = (knomi_config.gif_tint == (uint8_t)(var[5] - '0')) ? "selected" : "";
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
static String message_page(const String &title, const String &body_html) {
    return String("<!DOCTYPE html><html lang='en'><head><title>KNOMI</title>") + BINNACLE_HEAD +
        "</head><body><header class='rail'><div class='wrap rail-in'><div class='brand'>"
        "<a class='n' href='/'>KNOMI<span class='dot'>.</span></a><span class='f'>Printer display</span></div></div></header>"
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
        "<a class='n' href='/'>KNOMI<span class='dot'>.</span></a><span class='f'>Printer display</span></div>"
        "<span class='rail-sp'></span><nav><a href='/'>Settings</a><a class='on' href='/gifs'>Animations</a>"
        "<a href='/update'>Firmware</a></nav>" BINNACLE_MODES "</div></header><main class='wrap'>"
        "<section class='mast'><span class='label'>Animations</span><h1>Animations<span class='dot'>.</span></h1>"
        "<p class='lede'>Upload a GIF to any slot to replace it. It shows on the display right away. "
        "The screen is a 240&times;240 circle, so keep the subject centered.</p>";
    size_t used = LittleFS.usedBytes(), total = LittleFS.totalBytes();
    unsigned pct = total ? (unsigned)(used * 100 / total) : 0;
    page += "<div class='strip'><span class='pill'>" + String((unsigned)(used / 1024)) + " / " +
            String((unsigned)(total / 1024)) + " KB used</span><span class='pill'>max 1.5 MB per GIF</span>"
            "<span class='pill'>5 MB loaded total</span></div>"
            "<div class='meter' style='max-width:320px'><i style='width:" + String(pct) + "%'></i></div>"
            "<div class='rule'></div></section><div class='slots'>";
    for (int i = 0; i < GIF_SLOT_NUM; i++) {
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
            page += "<div class='screen empty'>empty</div>";
        }
        page += "<div style='text-align:center;margin-bottom:12px'>";
        if (info.has_custom && info.loaded) {
            page += "<span class='pill ok'>custom · " + String((unsigned)(info.custom_size / 1024)) + " KB</span>";
        } else if (info.has_custom) {
            page += "<span class='pill bad'>not loaded: too big or not a GIF</span>";
        } else if (info.has_builtin) {
            page += "<span class='pill held'>built-in</span>";
        } else {
            page += "<span class='pill held'>not set</span>";
        }
        page += "</div><div class='slot-actions'>"
                "<form method='POST' action='/gif/upload?slot=" + n + "' enctype='multipart/form-data'>"
                "<input type='file' name='gif' accept='image/gif' required>"
                "<button type='submit' class='btn-ghost' style='justify-content:center'>Upload</button></form>";
        if (info.has_custom) {
            page += "<form method='POST' action='/gif/delete?slot=" + n + "'><button type='submit' class='btn-ghost' style='justify-content:center'>" +
                    String(info.has_builtin ? "Restore built-in" : "Remove") + "</button></form>";
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
    });
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
            request->send(LittleFS, path, "image/gif");
            return;
        }
        const lv_img_dsc_t *b = knomi_gif_builtin((knomi_gif_slot_t)slot);
        if (b) request->send_P(200, "image/gif", b->data, b->data_size);
        else request->send(404);
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
    AsyncElegantOTA.begin(&server);

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", btt_logo_only_ico, sizeof(btt_logo_only_ico));
        request->send(response);
    });

    gif_routes();
    display_routes();
    bluetooth_routes();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html, knomi_html_processor);
    });

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
        uint8_t post_require = WEB_POST_NULL;
        int paramsNr = request->params();
        for (int i = 0; i < paramsNr; i++) {
            AsyncWebParameter* p = request->getParam(i);
            Serial.printf("name: %s\r\n", p->name().c_str());
            Serial.printf("value: %s\r\n", p->value().c_str());

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
