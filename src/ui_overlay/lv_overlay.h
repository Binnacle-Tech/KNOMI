#ifndef _LV_OVERLAY_H
#define _LV_OVERLAY_H

#include "lvgl.h"

extern lv_obj_t * ui_img_main_gif;
#include "knomi_gif.h"

LV_IMG_DECLARE(gif_welcome);
LV_IMG_DECLARE(gif_setup);
LV_IMG_DECLARE(gif_voron);
LV_IMG_DECLARE(gif_standby);
LV_IMG_DECLARE(gif_homing);
LV_IMG_DECLARE(gif_probing);
LV_IMG_DECLARE(gif_qgling);
LV_IMG_DECLARE(gif_heated);
LV_IMG_DECLARE(gif_print);
LV_IMG_DECLARE(gif_print_ok);
LV_IMG_DECLARE(gif_printed);

// lv_setup_screens.cpp
void lv_setup_screens_init(void);
void lv_setup_screens_theme(void);
void lv_setup_screens_loop(void);
// lv_print_info.cpp
void lv_print_info_init(void);
void lv_print_info_apply(void);
void lv_print_info_update(void);
// lv_button_style.cpp
void lv_btn_add_style(void);
// lv_theme_color.cpp
lv_color_t lv_theme_color(void);
void lv_theme_update_color(lv_color_t c);
void lv_theme_color_style(void);
//lv_wifi_change_screen.cpp
void lv_loop_wifi_change_screen(wifi_status_t status);
// lv_moonraker_change_screen.cpp
void lv_loop_moonraker_change_screen(void);
void lv_loop_moonraker_change_screen_value(void);
// lv_set_temp.cpp
void lv_loop_set_temp_screen(void);
// lv_popup.cpp
void lv_loop_popup_screen(void);
void lv_popup_warning(const char * warning, bool clickable);
// lv_dialog.cpp
void lv_dialog_goto_print(void);
void lv_dialog_goto_reset_wifi(void);
// lv_btn_event.cpp
void lv_btn_init(void);
void lv_loop_btn_event(void);
// lv_auto_goto_idle.cpp
void touch_idle_time_clear(void);
void lv_loop_auto_idle(wifi_status_t status);

#endif
