// Printing screen: the layout from the web designer (lv_print_layout.cpp) or the
// stock accelerometer view, plus the paused animation overlay.
#include "ui/ui.h"
#include "knomi.h"
#include "moonraker.h"
#include "lv_overlay.h"
#include "knomi_gif.h"

static lv_obj_t * pause_gif_obj;
static bool paused_shown = false;

void lv_print_info_init(void) {
    print_layout_init();

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
    bool layout = info_view() && !paused_shown;
    bool accel = !info_view() && !paused_shown;
    print_layout_set_visible(layout);
    // stock printing screen: accelerometer view only (the designer layout draws its own ring)
    lv_obj_t * const stock[] = {
        ui_label_printing_progress, ui_arc_printing_progress,
        ui_arc_bg_1, ui_arc_bg_2_1, ui_arc_bg_2_2, ui_arc_bg_2_3,
    };
    for (lv_obj_t * o : stock) set_hidden(o, !accel);
#ifdef LIS2DW_SUPPORT
    set_hidden(ui_slider_printing_acc_x, !accel);
    set_hidden(ui_slider_printing_acc_y, !accel);
    set_hidden(ui_slider_printing_acc_z, !accel);
    set_hidden(ui_label_printing_acc_x, !accel);
    set_hidden(ui_label_printing_acc_y, !accel);
    set_hidden(ui_label_printing_acc_z, !accel);
#endif
    if (accel) {
        lv_obj_set_align(ui_label_printing_progress, LV_ALIGN_TOP_MID); // stock layout
        lv_obj_set_y(ui_label_printing_progress, 50);
    }
    lv_obj_move_foreground(pause_gif_obj);
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

    print_layout_update();
    lv_obj_move_foreground(pause_gif_obj); // layout pages rebuilt after a save land on top
}
