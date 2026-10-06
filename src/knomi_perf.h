#ifndef KNOMI_PERF_H
#define KNOMI_PERF_H
// Performance numbers for /perf and tools/check.py --perf: frame rate and draw time,
// how busy each task and each CPU core is, and memory. Cheap enough to leave on.
#include <Arduino.h>

void knomi_perf_init(void);                          // setup(): idle hooks on both cores
void knomi_perf_frame(uint32_t render_ms, uint32_t px);   // LVGL finished a screen refresh
void knomi_perf_face(uint32_t us);                   // one Coaster face drawn
void knomi_perf_flush(uint32_t us);
void knomi_perf_flush_area(int x1, int y1, int x2, int y2);   // which parts of the screen get redrawn (/perf "areas")
void knomi_perf_logic(uint32_t us);
void knomi_perf_accel(bool ok, const int32_t raw[3]);   // one accelerometer read (mg)
void knomi_perf_samples(uint32_t n);                 // samples Coaster used this pass                  // one pass of coaster_loop()                  // one area sent to the screen over SPI
void knomi_perf_delay(uint32_t ms);                  // delay() in our tasks: counts busy time
// where the UI task's time goes (/perf "parts_pct"): Coaster's thinking by stage, LVGL, and what LVGL
// draws outside the face, by kind
enum { PP_SENSE, PP_MOOD, PP_BODY, PP_EXPR, PP_DECO, PP_MEASURE, PP_LVGL, PP_LOOP,
       PP_RECT, PP_ARC, PP_TEXT, PP_IMG, PP_LINE, PP_POLY, PP_TOUCH, PP_WAIT,
       PP_F_RECT, PP_F_ARC, PP_F_TEXT, PP_F_IMG, PP_F_LINE, PP_F_POLY, PP_COUNT };   // PP_F_*: inside the face
void knomi_perf_part(int id, uint32_t us);
extern volatile bool knomi_perf_in_face;            // Coaster's face is being drawn (its parts aren't split out)
void knomi_perf_draw_hooks(void * draw_ctx);         // time LVGL's drawing by kind (lv_draw_ctx_t *)
void knomi_perf_membench(void * disp);                        // once at start: drawing speed in internal RAM vs PSRAM
String knomi_perf_json(void);                        // everything since the last call
#endif
