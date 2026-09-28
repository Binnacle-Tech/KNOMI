#include "stdio.h" // sprintf
#include "ui/ui.h"
#include "knomi.h"
#include "moonraker.h"

/*!
 *  @param  symbol
 *          "+": extrude
 *          "-": retract
 */
void extruder_action(const char* symbol) {
    uint16_t len_id = lv_roller_get_selected(ui_roller_set_extrude_length);
    uint16_t speed_id = lv_roller_get_selected(ui_roller_set_extrude_speed);
    if (len_id >= EXTRUDE_NUM) len_id = EXTRUDE_NUM - 1;
    if (speed_id >= EXTRUDE_NUM) speed_id = EXTRUDE_NUM - 1;
    uint16_t length = knomi_config.extrude_mm[len_id];
    uint16_t speed = knomi_config.extrude_mms[speed_id];
    moonraker.post_gcode_to_queue("M83");
    char buf[100];
    snprintf(buf, sizeof(buf), "G1 E%s%u F%u", symbol, (unsigned)length, (unsigned)MMS_TO_MMM(speed));
    moonraker.post_gcode_to_queue(buf);
}

void lv_btn_extruder_extrude(lv_event_t * e) {
    extruder_action("+");
}
void lv_btn_extruder_retract(lv_event_t * e) {
    extruder_action("-");
}
