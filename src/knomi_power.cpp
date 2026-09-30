#include <Arduino.h>
#include "knomi.h"
#include "moonraker.h"
#include "lvgl_hal.h"
#include "knomi_power.h"

typedef enum { PWR_AWAKE = 0, PWR_DIM, PWR_SLEEP } pwr_state_t;

static pwr_state_t state = PWR_AWAKE;
static uint32_t last_activity = 0;
static int8_t level_now = -1;
static bool swallowing = false;

static void set_level(uint8_t level) {
    if (level_now == level) return;
    level_now = level;
    tft_set_backlight(level);
}

static uint8_t awake_level(void) {
    return knomi_config.backlight;
}

void knomi_power_wake(void) {
    last_activity = millis();
    if (state != PWR_AWAKE) {
        state = PWR_AWAKE;
        set_level(awake_level());
    }
}

void knomi_power_init(void) {
    last_activity = millis();
    state = PWR_AWAKE;
    set_level(awake_level());
}

void knomi_power_set_brightness(uint8_t level) {
    if (level < 1) level = 1;
    if (level > 16) level = 16;
    knomi_config.backlight = level;
    knomi_power_wake();
    set_level(level);
}

bool knomi_power_filter_touch(bool pressed) {
    if (!pressed) {
        swallowing = false;
        return false;
    }
    if (swallowing) return true;
    if (state == PWR_SLEEP) {
        swallowing = true;     // this finger only wakes the screen
        knomi_power_wake();
        return true;
    }
    knomi_power_wake();        // dimmed screens are still readable: wake and pass through
    return false;
}

// Anything the printer is doing that someone might want to watch
static bool printer_busy(void) {
    const moonraker_data_t &d = moonraker.data;
    return d.printing || d.pause || d.paused_ext || d.homing || d.probing || d.qgling ||
           d.shaping || d.pid_tuning || d.cleaning || d.filament ||
           d.heating_nozzle || d.heating_bed || d.nozzle_target > 0 || d.bed_target > 0;
}

void knomi_power_loop(void) {
    // wake when the screen changes on its own (print done, popup, heating...)
    static lv_obj_t * last_scr = NULL;
    lv_obj_t * scr = lv_scr_act();
    if (scr != last_scr) {
        last_scr = scr;
        knomi_power_wake();
    }
    // and when the printer becomes busy / changes what it's doing
    static bool was_busy = false;
    bool busy = printer_busy();
    if (busy != was_busy) {
        was_busy = busy;
        knomi_power_wake();
    }
    if (busy && knomi_config.awake_printing) {
        last_activity = millis();
    }

    uint32_t idle_ms = millis() - last_activity;
    uint32_t dim_ms = (uint32_t)knomi_config.dim_after_min * 60000UL;
    uint32_t sleep_ms = (uint32_t)knomi_config.sleep_after_min * 60000UL;

    pwr_state_t want = PWR_AWAKE;
    if (sleep_ms && idle_ms >= sleep_ms) want = PWR_SLEEP;
    else if (dim_ms && idle_ms >= dim_ms) want = PWR_DIM;

    state = want;
    switch (state) {
        case PWR_AWAKE: set_level(awake_level()); break;
        case PWR_DIM:   set_level(min(knomi_config.dim_level, awake_level())); break;
        case PWR_SLEEP: set_level(0); break;
    }
}

bool knomi_power_dozing(void) { return state != PWR_AWAKE; }
bool knomi_power_screen_off(void) { return level_now == 0; }
