#include <stdio.h> // sprintf
#include <Arduino.h>

#include "lvgl_hal.h"
#include "lvgl_usr.h"
#include "ui/ui.h"
#include "moonraker.h"
#include "knomi.h"
#include "ui_overlay/lv_overlay.h"
#include "knomi_power.h"
#include "knomi_ble.h"


/****************** lvgl ui call function ******************/
//
void lv_tft_set_backlight(lv_event_t * e) {
    int32_t light = lv_slider_get_value(ui_slider_backlight);
    knomi_power_set_brightness(light);
}

// save brightness once the finger lifts, not on every slider step
static void backlight_released_cb(lv_event_t * e) {
    knomi_config_require_change(LOCAL_POST_SETTINGS);
}

// Web page changed display settings (set from the web server task)
volatile bool knomi_display_settings_dirty = false;
static void extrude_roller_fill(lv_obj_t *roller, const uint16_t *v, const char *unit, uint16_t sel) {
    String opts;
    for (int i = 0; i < EXTRUDE_NUM; i++) {
        if (i) opts += "\n";
        opts += String(v[i]) + unit;
    }
    lv_roller_set_options(roller, opts.c_str(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(roller, sel, LV_ANIM_OFF);
}

// boot: select the saved defaults; later rebuilds keep what's selected on the KNOMI
void lv_extrude_rollers_rebuild(bool use_defaults) {
    uint16_t len_sel = use_defaults ? knomi_config.extrude_mm_def : lv_roller_get_selected(ui_roller_set_extrude_length);
    uint16_t spd_sel = use_defaults ? knomi_config.extrude_mms_def : lv_roller_get_selected(ui_roller_set_extrude_speed);
    extrude_roller_fill(ui_roller_set_extrude_length, knomi_config.extrude_mm, "mm", len_sel);
    extrude_roller_fill(ui_roller_set_extrude_speed, knomi_config.extrude_mms, "mm/s", spd_sel);
    lv_btn_set_extrude(NULL);  // labels on the extruder screen
}

static void apply_display_settings(void) {
    lv_slider_set_value(ui_slider_backlight, knomi_config.backlight, LV_ANIM_OFF);
    knomi_power_wake();
    lv_print_info_apply();
    // UI color (may have been changed on the web page)
    lv_btn_add_style();
    lv_theme_color_style();
    lv_setup_screens_theme();
    knomi_gif_apply_tint();
    lv_extrude_rollers_rebuild(false);
    lv_roller_preheat_rebuild();
    print_layout_request_reload();  // "UI color" elements
}

// extruder speed
void lv_btn_set_extrude(lv_event_t * e) {
    // Initialize parameter values from roller settings
    char roller_str[10];
    lv_roller_get_selected_str(ui_roller_set_extrude_length, roller_str, sizeof(roller_str));
    lv_label_set_text(ui_label_extruder_length, roller_str);
    uint32_t sel = lv_roller_get_selected(ui_roller_set_extrude_length);
    lv_obj_set_user_data(ui_label_extruder_length, (void *)sel);
    lv_roller_get_selected_str(ui_roller_set_extrude_speed, roller_str, sizeof(roller_str));
    lv_label_set_text(ui_label_extruder_speed, roller_str);
    sel = lv_roller_get_selected(ui_roller_set_extrude_speed);
    lv_obj_set_user_data(ui_label_extruder_speed, (void *)sel);
}

// set extruder roller
void lv_roller_set_extrude(lv_event_t * e) {
    // Initialize parameter values from roller settings
    uint32_t sel = (uint32_t)lv_obj_get_user_data(ui_label_extruder_length);
    lv_roller_set_selected(ui_roller_set_extrude_length, sel, LV_ANIM_OFF);

    sel = (uint32_t)lv_obj_get_user_data(ui_label_extruder_speed);
    lv_roller_set_selected(ui_roller_set_extrude_speed, sel, LV_ANIM_OFF);
}
/***********************************************************/


void lv_popup_warning(const char * warning, bool clickable);
void lv_popup_remove(lv_event_t * e) ;
// lvgl ui
void lvgl_ui_task(void * parameter) {
    lv_btn_init();
    lvgl_hal_init();
    knomi_gif_init(); // custom GIFs must be in memory before the UI uses them
    ui_init();

#ifndef LIS2DW_SUPPORT
    // progress in center if no lis2dw accelerometer data to display
    lv_obj_set_y(ui_label_printing_progress, 0);
    lv_obj_set_align(ui_label_printing_progress, LV_ALIGN_CENTER);
    // delete unused accelerometer data
    lv_obj_del(ui_slider_printing_acc_x);
    lv_obj_del(ui_slider_printing_acc_y);
    lv_obj_del(ui_slider_printing_acc_z);
    lv_obj_del(ui_label_printing_acc_x);
    lv_obj_del(ui_label_printing_acc_y);
    lv_obj_del(ui_label_printing_acc_z);
#endif

    lv_obj_t * label = lv_label_create(ui_ScreenTestImg);
    lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_label_set_text_static(label, FW_VERSION);

    // Add all button style
    lv_btn_add_style();

    // Set theme color
    lv_theme_color_style();

    // Printing screen info view + paused overlay
    lv_print_info_init();

    // Saved brightness, and persist slider changes
    lv_slider_set_value(ui_slider_backlight, knomi_config.backlight, LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_slider_backlight, backlight_released_cb, LV_EVENT_RELEASED, NULL);
    knomi_power_init();

    // Add logo gif
    ui_img_main_gif = lv_gif_create(ui_ScreenMainGif);
    knomi_gif_show(ui_img_main_gif, GIF_SLOT_IDLE1);
    lv_obj_align(ui_img_main_gif, LV_ALIGN_CENTER, 0, 0);

    // Boot / WiFi setup / WiFi lost screens (dark, QR code setup)
    lv_setup_screens_init();
    // built-in animations follow the UI color if enabled
    knomi_gif_apply_tint();

    // Create a QR Code
    lv_obj_t * qr = lv_qrcode_create(ui_ScreenQRCode, 130, LV_COLOR_MAKE(0xff, 0xff, 0xff), LV_COLOR_MAKE(0, 0, 0));
    const char * data = "https://bigtreetech.github.io/docs/KNOMI2.html";
    lv_qrcode_update(qr, data, strlen(data));
    lv_obj_center(qr);

    // Extruder length/speed rollers and preheat presets (editable on the web page)
    lv_extrude_rollers_rebuild(true);
    lv_roller_preheat_rebuild();
    // Initialize extruder speed/length values from roller settings
    lv_btn_set_extrude(NULL);

    for(;;) {
        // lvgl task, must run in loop first.
        lv_timer_handler();

        wifi_status_t status = wifi_get_connect_status();
        // a live Bluetooth link to the plugin counts as connected (WiFi may be off)
        if (knomi_ble_link_active()) status = WIFI_STATUS_CONNECTED;

        uint32_t passkey = knomi_ble_take_passkey();
        if (passkey) {
            static char pk_msg[64];
            snprintf(pk_msg, sizeof(pk_msg), "Bluetooth pairing\n%03u %03u\nEnter this on the Pi",
                     (unsigned)(passkey / 1000), (unsigned)(passkey % 1000));
            knomi_power_wake();
            lv_popup_warning(pk_msg, true);
        }
        int paired = knomi_ble_take_pair_result();
        if (paired) {
            knomi_power_wake();
            lv_popup_warning(paired > 0 ? "Bluetooth paired" : "Bluetooth pairing failed", true);
        }

        lv_loop_wifi_change_screen(status);

        if (status == WIFI_STATUS_CONNECTED) {

            lv_loop_popup_screen();
            lv_loop_set_temp_screen();

            if (!moonraker.unconnected && !moonraker.unready) {
                if (moonraker.data_unlock) {
                    lv_loop_moonraker_change_screen();
                }
                lv_loop_moonraker_change_screen_value();
            }
        }

        lv_loop_auto_idle(status);
        lv_loop_btn_event();
        knomi_gif_process();
        knomi_power_loop();
        lv_setup_screens_loop();
        if (knomi_display_settings_dirty) {
            knomi_display_settings_dirty = false;
            apply_display_settings();
        }

        delay(5);
    }
}
