// Printing screen: "info" view (file, %, time left, temps, Z/layer) or the stock
// accelerometer view, plus the paused animation overlay.
#include "ui/ui.h"
#include "knomi.h"
#include "moonraker.h"
#include "lv_overlay.h"
#include "knomi_gif.h"

static lv_obj_t * lbl_file;
static lv_obj_t * lbl_time;
static lv_obj_t * lbl_temps;
static lv_obj_t * lbl_pos;
static lv_obj_t * pause_gif_obj;
static bool paused_shown = false;

#define COLOR_MUTED 0x93A4B2
#define COLOR_TEXT  0xE7EEF4

static lv_obj_t * make_label(const lv_font_t * font, uint32_t color, lv_coord_t y) {
    lv_obj_t * l = lv_label_create(ui_ScreenPrinting);
    lv_obj_set_align(l, LV_ALIGN_CENTER);
    lv_obj_set_y(l, y);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(l, "");
    return l;
}

void lv_print_info_init(void) {
    lbl_file = make_label(&ui_font_InterSemiBold14, COLOR_MUTED, -62);
    lv_obj_set_width(lbl_file, 150);
    lv_label_set_long_mode(lbl_file, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lbl_time = make_label(&ui_font_InterSemiBold18, COLOR_TEXT, 26);
    lbl_temps = make_label(&ui_font_InterSemiBold14, COLOR_MUTED, 54);
    lbl_pos = make_label(&ui_font_InterSemiBold14, COLOR_MUTED, 76);

    pause_gif_obj = lv_gif_create(ui_ScreenPrinting);
    lv_obj_align(pause_gif_obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(pause_gif_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pause_gif_obj, LV_OBJ_FLAG_CLICKABLE); // swipes still reach the screen

    lv_print_info_apply();
}

static bool info_view(void) {
#ifdef LIS2DW_SUPPORT
    return knomi_config.print_view == PRINT_VIEW_INFO;
#else
    return true; // no accelerometer on this board
#endif
}

static void set_hidden(lv_obj_t * o, bool hidden) {
    if (!o) return;
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// Show/hide objects for the chosen view (and the pause overlay)
void lv_print_info_apply(void) {
    bool info = info_view() && !paused_shown;
    bool accel = !info_view() && !paused_shown;
    set_hidden(lbl_file, !info);
    set_hidden(lbl_time, !info);
    set_hidden(lbl_temps, !info);
    set_hidden(lbl_pos, !info);
    set_hidden(ui_label_printing_progress, paused_shown);
#ifdef LIS2DW_SUPPORT
    set_hidden(ui_slider_printing_acc_x, !accel);
    set_hidden(ui_slider_printing_acc_y, !accel);
    set_hidden(ui_slider_printing_acc_z, !accel);
    set_hidden(ui_label_printing_acc_x, !accel);
    set_hidden(ui_label_printing_acc_y, !accel);
    set_hidden(ui_label_printing_acc_z, !accel);
#endif
    if (info) {
        lv_obj_set_align(ui_label_printing_progress, LV_ALIGN_CENTER);
        lv_obj_set_y(ui_label_printing_progress, -18);
    } else if (accel) {
        lv_obj_set_align(ui_label_printing_progress, LV_ALIGN_TOP_MID); // stock layout
        lv_obj_set_y(ui_label_printing_progress, 50);
    }
}

static void fmt_duration(char * out, size_t n, uint32_t s) {
    if (s < 60) snprintf(out, n, "<1m");
    else if (s < 3600) snprintf(out, n, "%um", (unsigned)(s / 60));
    else snprintf(out, n, "%uh %02um", (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
}

void lv_print_info_update(void) {
    const moonraker_data_t &d = moonraker.data;

    // pause overlay
    bool paused = d.printing && (d.pause || d.paused_ext);
    if (paused != paused_shown) {
        paused_shown = paused;
        if (paused) {
            knomi_gif_show(pause_gif_obj, GIF_SLOT_PAUSED);
            lv_obj_clear_flag(pause_gif_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(pause_gif_obj, LV_OBJ_FLAG_HIDDEN);
        }
        lv_print_info_apply();
    }
    // a reload/retint restarts the GIF timer; keep it stopped while hidden
    if (!paused_shown) {
        lv_gif_t * g = (lv_gif_t *)pause_gif_obj;
        if (g->timer && !g->timer->paused) lv_timer_pause(g->timer);
    }

    if (!info_view() || lv_scr_act() != ui_ScreenPrinting) return;

    char buf[48];
    if (strcmp(lv_label_get_text(lbl_file), d.file_path) != 0) {
        lv_label_set_text(lbl_file, d.file_path);
    }

    char t[16];
    if (d.time_left >= 0) {
        fmt_duration(t, sizeof(t), (uint32_t)d.time_left);
        snprintf(buf, sizeof(buf), "%s left", t);
    } else if (d.print_time > 0) {
        fmt_duration(t, sizeof(t), d.print_time);
        snprintf(buf, sizeof(buf), "%s elapsed", t);
    } else {
        snprintf(buf, sizeof(buf), "starting");
    }
    if (strcmp(lv_label_get_text(lbl_time), buf) != 0) lv_label_set_text(lbl_time, buf);

    snprintf(buf, sizeof(buf), "%d/%d\xe2\x84\x83   %d/%d\xe2\x84\x83",
             d.nozzle_actual, d.nozzle_target, d.bed_actual, d.bed_target);
    if (strcmp(lv_label_get_text(lbl_temps), buf) != 0) lv_label_set_text(lbl_temps, buf);

    if (d.layer_total > 0) {
        snprintf(buf, sizeof(buf), "Layer %u/%u", d.layer, d.layer_total);
    } else if (d.z_um != INT32_MIN) {
        snprintf(buf, sizeof(buf), "Z %d.%02d", (int)(d.z_um / 1000), (int)(abs(d.z_um) % 1000) / 10);
    } else {
        buf[0] = 0;
    }
    if (strcmp(lv_label_get_text(lbl_pos), buf) != 0) lv_label_set_text(lbl_pos, buf);
}
