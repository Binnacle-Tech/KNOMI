#include "ui/ui.h"
#include "knomi.h"
#include "lv_overlay.h"
#include "moonraker.h"

static uint32_t touch_idle_sec = 0;

void touch_idle_time_clear(void) {
    touch_idle_sec = 0;
}

void lv_loop_auto_idle(wifi_status_t status) {
    lv_obj_t * ui_now = lv_scr_act();
    if ((ui_now == ui_ScreenMainGif) || (ui_now == ui_ScreenPopup)
         || (ui_now == ui_ScreenHeatingNozzle) || (ui_now == ui_ScreenHeatingBed)
         || (moonraker.data.printing) || (status != WIFI_STATUS_CONNECTED)) {
        touch_idle_sec = 0;
        return;
    }
    static uint32_t ms;
    if (ms > millis()) return;
    ms = millis() + 1000; // 1 sec;
    if (ms < millis()) return; // Avoid goes straight to sleep when the cycle overflow.
    touch_idle_sec++;

    // back to the idle face after touch_idle_s without a touch (0 = never)
    if (knomi_config.touch_idle_s && touch_idle_sec >= knomi_config.touch_idle_s) {
        knomi_gif_show(ui_img_main_gif, knomi_gif_idle_slot(0));
        lv_obj_add_flag(ui_ScreenMainGif, LV_OBJ_FLAG_CLICKABLE);
        _ui_screen_change(&ui_ScreenMainGif, LV_SCR_LOAD_ANIM_NONE, 500, 0, NULL);
        touch_idle_sec = 0;
    }
}
