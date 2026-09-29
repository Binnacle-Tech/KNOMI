#pragma once
// Task handles for the stack report on /log/info (tools/check.py reads it) and the self-rescue.
#include <Arduino.h>
enum { KT_LVGL, KT_ACCEL, KT_WIFI, KT_PRINTER, KT_POST, KNOMI_TASKS };
extern TaskHandle_t knomi_tasks[KNOMI_TASKS];
void knomi_rescue_ok(void);
