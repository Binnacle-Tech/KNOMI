#ifndef KNOMI_PERF_H
#define KNOMI_PERF_H
// Performance numbers for /perf and tools/check.py --perf: frame rate and draw time,
// how busy each task and each CPU core is, and memory. Cheap enough to leave on.
#include <Arduino.h>

void knomi_perf_init(void);                          // setup(): idle hooks on both cores
void knomi_perf_frame(uint32_t render_ms, uint32_t px);   // LVGL finished a screen refresh
void knomi_perf_face(uint32_t us);                   // one Coaster face drawn
void knomi_perf_flush(uint32_t us);
void knomi_perf_logic(uint32_t us);
void knomi_perf_accel(bool ok, const int32_t raw[3]);   // one accelerometer read (mg)
void knomi_perf_samples(uint32_t n);                 // samples Coaster used this pass                  // one pass of coaster_loop()                  // one area sent to the screen over SPI
void knomi_perf_delay(uint32_t ms);                  // delay() in our tasks: counts busy time
String knomi_perf_json(void);                        // everything since the last call
#endif
