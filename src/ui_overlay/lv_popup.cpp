#include "ui/ui.h"
#include "knomi.h"
#include "moonraker.h"
#include "lv_overlay.h"
#include "../knomi_coaster.h"

typedef enum {
    LV_POPUP_NULL = 0,
    LV_POPUP_UNCONNECT,
    LV_POPUP_PRINTER_ERR,
    LV_POPUP_ACTION_ERR,
    LV_POPUP_AUTH_ERR,
} lv_popup_status_t;

static lv_popup_status_t lv_popup_status = LV_POPUP_NULL;
static lv_obj_t * previous_menu;

static void popup_face_show(bool show);
void lv_popup_warning(const char * warning, bool clickable) {
    popup_face_show(false);   // other popups (pairing codes, action errors) have no face
    lv_textarea_set_text(ui_textarea_popup, warning);

    if (clickable) {
        lv_obj_clear_flag(ui_btn_popup_ok, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(ui_img_popup, -15);
    } else {
        lv_obj_add_flag(ui_btn_popup_ok, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(ui_img_popup, 0);
    }

    if (lv_scr_act() != ui_ScreenPopup) {
        previous_menu = lv_scr_act();
        _ui_screen_change(&ui_ScreenPopup, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, NULL);
    }
}

static void lv_popup_remove(lv_event_t * e) {
    if (previous_menu && (previous_menu != lv_scr_act())) {
        _ui_screen_change(&previous_menu, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, NULL);
    }
    previous_menu = NULL;
}

void ui_event_popup_ok(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        lv_popup_remove(e);
    }
}

// Coaster on the connection / printer error popups (lonely, confused, shocked)
static lv_obj_t * popup_face = NULL;
static void popup_face_show(bool show) {
    if (!popup_face) {
        popup_face = coaster_create(ui_ScreenPopup, 84);
        lv_obj_align(popup_face, LV_ALIGN_TOP_MID, 0, 2);
    }
    if (show) lv_obj_clear_flag(popup_face, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(popup_face, LV_OBJ_FLAG_HIDDEN);
}

static void lv_goto_popup_screen(lv_popup_status_t state, const char * warning) {
    if (lv_popup_status == state) return;
    lv_popup_status = state;
    lv_popup_warning(warning, false);
    popup_face_show(true);
}

static void lv_remove_popup_screen(void) {
    if (lv_popup_status == LV_POPUP_NULL) return;
    lv_popup_status = LV_POPUP_NULL;
    lv_popup_remove(NULL);
}

void lv_loop_popup_screen(void) {
    bool octo = knomi_backend_is_octoprint();
    if (moonraker.unconnected) {
        lv_goto_popup_screen(LV_POPUP_UNCONNECT, octo ?
            "OctoPrint Connect failed\nPlease check Your printer or KNOMI IP" :
            "Moonraker Connect failed\nPlease check Your printer or KNOMI IP");
        return;
    }
    if (octo && moonraker.auth_failed) {
        lv_goto_popup_screen(LV_POPUP_AUTH_ERR, "OctoPrint API key rejected\nSet it in the KNOMI web page");
        return;
    }
    if (moonraker.unready) {
        lv_goto_popup_screen(LV_POPUP_PRINTER_ERR, "Printer is Unoperational\nPlease check your Printer");
        return;
    }
    lv_remove_popup_screen();
}
