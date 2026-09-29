// Boot / WiFi screens, restyled to match the rest of the UI:
//  - Connecting: thin spinner in the UI color, WiFi waves, "Connecting to <ssid>"
//  - Setup (welcome): WiFi waves + a QR code. Before a phone joins the KNOMI's
//    access point the QR joins that network; once someone is connected it opens
//    the setup page instead.
//  - WiFi lost: dimmed, slashed WiFi icon and "Reconnecting to <ssid>"
#include <WiFi.h>
#include "ui/ui.h"
#include "knomi.h"
#include "knomi_gif.h"
#include "lv_overlay.h"
#include "../knomi_coaster.h"


#define COLOR_BG    0x000000
#define COLOR_TEXT  0xE7EEF4
#define COLOR_MUTED 0x93A4B2
#define COLOR_TRACK 0x1B2731

static lv_obj_t * setup_qr, * setup_l1, * setup_l2;
static lv_obj_t * conn_label, * lost_label;
static String qr_text;

static lv_obj_t * label(lv_obj_t * parent, const lv_font_t * font, uint32_t color, lv_coord_t y) {
    lv_obj_t * l = lv_label_create(parent);
    lv_obj_set_align(l, LV_ALIGN_CENTER);
    lv_obj_set_y(l, y);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(l, "");
    return l;
}

static void dark(lv_obj_t * scr) {
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(scr, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void lv_setup_screens_init(void) {
    // --- setup (welcome) ---
    dark(ui_ScreenWelcome);
    lv_obj_add_flag(ui_label_welcome, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t * waves = lv_gif_create(ui_ScreenWelcome);
    knomi_gif_show(waves, GIF_SLOT_WELCOME);
    lv_obj_align(waves, LV_ALIGN_CENTER, 0, -83);
    // quiet zone: white rounded tile behind the code so phones read it reliably
    lv_obj_t * tile = lv_obj_create(ui_ScreenWelcome);
    lv_obj_remove_style_all(tile);
    lv_obj_set_size(tile, 99, 99);
    lv_obj_align(tile, LV_ALIGN_CENTER, 0, 2);
    lv_obj_set_style_bg_color(tile, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(tile, 255, 0);
    lv_obj_set_style_radius(tile, 8, 0);
    setup_qr = lv_qrcode_create(ui_ScreenWelcome, 87, lv_color_hex(0x000000), lv_color_hex(0xFFFFFF));
    lv_obj_align(setup_qr, LV_ALIGN_CENTER, 0, 2);
    setup_l1 = label(ui_ScreenWelcome, &ui_font_InterSemiBold16, COLOR_TEXT, 66);
    setup_l2 = label(ui_ScreenWelcome, &ui_font_InterSemiBold14, COLOR_MUTED, 87);

    // --- connecting ---
    dark(ui_ScreenWIFIConnecting);
    lv_obj_add_flag(ui_Image7, LV_OBJ_FLAG_HIDDEN);   // stock full-screen logo
    lv_obj_set_style_arc_width(ui_spinner_wifi_connecting, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_spinner_wifi_connecting, 5, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui_spinner_wifi_connecting, lv_color_hex(COLOR_TRACK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui_spinner_wifi_connecting, lv_theme_color(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_t * cwaves = lv_gif_create(ui_ScreenWIFIConnecting);
    knomi_gif_show(cwaves, GIF_SLOT_WELCOME);
    lv_obj_align(cwaves, LV_ALIGN_CENTER, 0, -18);
    conn_label = label(ui_ScreenWIFIConnecting, &ui_font_InterSemiBold14, COLOR_MUTED, 38);
    lv_obj_set_width(conn_label, 150);
    lv_label_set_long_mode(conn_label, LV_LABEL_LONG_WRAP);

    // --- WiFi lost ---
    dark(ui_ScreenWIFIDisconnect);
    lv_obj_add_flag(ui_img_wifi_disconnect, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_label_wifi_disconnect, LV_OBJ_FLAG_HIDDEN);
    // Coaster looks around for the network (lonely)
    lv_obj_t * off = coaster_create(ui_ScreenWIFIDisconnect, 130);
    lv_obj_align(off, LV_ALIGN_CENTER, 0, -34);
    lv_obj_t * lost = label(ui_ScreenWIFIDisconnect, &ui_font_InterSemiBold18, COLOR_TEXT, 26);
    lv_label_set_text(lost, "WiFi lost");
    lost_label = label(ui_ScreenWIFIDisconnect, &ui_font_InterSemiBold14, COLOR_MUTED, 52);
    lv_obj_set_width(lost_label, 160);
    lv_label_set_long_mode(lost_label, LV_LABEL_LONG_WRAP);
}

// spinner follows the UI color picked on the KNOMI
void lv_setup_screens_theme(void) {
    if (ui_spinner_wifi_connecting)
        lv_obj_set_style_arc_color(ui_spinner_wifi_connecting, lv_theme_color(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

// WIFI:... payload phones understand (special characters escaped)
static String wifi_qr_escape(const char * s) {
    String out;
    for (; *s; s++) {
        if (*s == '\\' || *s == ';' || *s == ',' || *s == ':' || *s == '"') out += '\\';
        out += *s;
    }
    return out;
}

static void set_text(lv_obj_t * l, const String & t) {
    if (strcmp(lv_label_get_text(l), t.c_str()) != 0) lv_label_set_text(l, t.c_str());
}

bool knomi_wifi_sta_fallback(void);

void lv_setup_screens_loop(void) {
    static uint32_t next = 0;
    if ((int32_t)(millis() - next) < 0) return;
    next = millis() + 500;

    lv_obj_t * scr = lv_scr_act();
    String ssid = knomi_config.sta_ssid;

    if (scr == ui_ScreenWIFIConnecting) {
        set_text(conn_label, ssid.isEmpty() ? String("Connecting") : "Connecting to\n" + ssid);
    } else if (scr == ui_ScreenWIFIDisconnect) {
        set_text(lost_label, ssid.isEmpty() ? String("Reconnecting") : "Reconnecting to\n" + ssid);
    } else if (scr == ui_ScreenWelcome) {
        String qr, l1, l2;
        if (WiFi.softAPgetStationNum() > 0) {
            String ip = WiFi.softAPIP().toString();
            qr = "http://" + ip + "/";
            l1 = "Scan to open setup";
            l2 = ip;
        } else {
            bool secured = knomi_config.ap_pwd[0] != 0;
            qr = String("WIFI:T:") + (secured ? "WPA" : "nopass") + ";S:" + wifi_qr_escape(knomi_config.ap_ssid) + ";";
            if (secured) qr += String("P:") + wifi_qr_escape(knomi_config.ap_pwd) + ";";
            qr += ";";
            // if a saved network is being retried, say so every other few seconds
            bool retrying = knomi_wifi_sta_fallback() && !ssid.isEmpty() && (millis() / 4000) % 2;
            l1 = retrying ? String("Can't reach ") + ssid : String("Scan to set up");
            l2 = String("or join ") + knomi_config.ap_ssid;
        }
        if (qr != qr_text) {
            qr_text = qr;
            lv_qrcode_update(setup_qr, qr.c_str(), qr.length());
        }
        set_text(setup_l1, l1);
        set_text(setup_l2, l2);
    }
}
