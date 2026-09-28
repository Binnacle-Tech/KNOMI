#ifndef KNOMI_POWER_H
#define KNOMI_POWER_H
#include <stdint.h>
// Backlight, auto-dim and sleep. All functions run in the LVGL task.
void knomi_power_init(void);             // apply saved brightness
void knomi_power_loop(void);             // dim/sleep state machine
void knomi_power_wake(void);             // any activity
void knomi_power_set_brightness(uint8_t level); // from the backlight slider
// Touch filter: returns true if this touch should be swallowed
// (the tap that wakes a sleeping screen must not press a button)
bool knomi_power_filter_touch(bool pressed);
#endif
