#include "knomi_perf.h"
#include <esp_freertos_hooks.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include "moonraker.h"
#undef delay

/* ---- CPU per core: the idle task calls our hook over and over while nothing else runs.
 * Time between two calls that are close together is idle time; a long gap means some
 * task ran in between. ---- */
static volatile int64_t idle_last[2], idle_us[2];
static volatile uint32_t measuring_until;   // millis(); only measure while someone reads /perf
static bool idle_hook(int core) {
    int64_t now = esp_timer_get_time(), gap = now - idle_last[core];
    if (gap > 0 && gap < 300) idle_us[core] += gap;
    idle_last[core] = now;
    // true lets the core sleep until the next interrupt (normal); while measuring keep the
    // idle task spinning so the gaps between calls show exactly when other tasks ran
    return (int32_t)(millis() - measuring_until) > 0;
}
static bool idle0(void) { return idle_hook(0); }
static bool idle1(void) { return idle_hook(1); }

/* ---- our tasks: time spent between delay() calls is time working ---- */
#define PERF_TASKS 10
typedef struct { TaskHandle_t h; int64_t woke, busy_us; } task_perf_t;
static task_perf_t tasks[PERF_TASKS];
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void knomi_perf_delay(uint32_t ms) {
    TaskHandle_t me = xTaskGetCurrentTaskHandle();
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&mux);
    task_perf_t * t = NULL;
    for (int i = 0; i < PERF_TASKS; i++) {
        if (tasks[i].h == me) { t = &tasks[i]; break; }
        if (!tasks[i].h) { tasks[i].h = me; tasks[i].woke = now; t = &tasks[i]; break; }
    }
    if (t) t->busy_us += now - t->woke;
    portEXIT_CRITICAL(&mux);
    delay(ms);
    if (t) t->woke = esp_timer_get_time();
}

/* ---- display ---- */
static volatile uint32_t frames, frame_ms_sum, frame_ms_max, px_sum;
static volatile uint32_t faces, face_us_sum, face_us_max;
static volatile uint32_t flushes, flush_us_sum, flush_us_max;
void knomi_perf_flush(uint32_t us) {
    flushes++; flush_us_sum += us; if (us > flush_us_max) flush_us_max = us;
}
void knomi_perf_frame(uint32_t render_ms, uint32_t px) {
    frames++; frame_ms_sum += render_ms; if (render_ms > frame_ms_max) frame_ms_max = render_ms; px_sum += px;
}
void knomi_perf_face(uint32_t us) {
    faces++; face_us_sum += us; if (us > face_us_max) face_us_max = us;
}

void knomi_perf_init(void) {
    esp_register_freertos_idle_hook_for_cpu(idle0, 0);
    esp_register_freertos_idle_hook_for_cpu(idle1, 1);
}

String knomi_perf_json(void) {
    static int64_t last = 0;
    int64_t now = esp_timer_get_time();
    float win = last ? (now - last) / 1e6f : 0;
    last = now;
    measuring_until = millis() + 30000;
    String o = "{\"window_s\":" + String(win, 2);
    if (win > 0.2f) {
        o += ",\"fps\":" + String(frames / win, 1);
        o += ",\"frame_ms\":" + String(frames ? (float)frame_ms_sum / frames : 0, 1) + ",\"frame_ms_max\":" + String(frame_ms_max);
        o += ",\"px_per_frame\":" + String(frames ? px_sum / frames : 0);
        o += ",\"face_ms\":" + String(faces ? face_us_sum / 1000.0f / faces : 0, 2) + ",\"face_ms_max\":" + String(face_us_max / 1000.0f, 2);
        o += ",\"faces_per_s\":" + String(faces / win, 1);
        o += ",\"flush_ms\":" + String(flushes ? flush_us_sum / 1000.0f / flushes : 0, 2) + ",\"flush_ms_max\":" + String(flush_us_max / 1000.0f, 2);
        if (win > 25) o += ",\"cpu_note\":\"CPU numbers are only exact when /perf is read every few seconds\"";
        o += ",\"cpu\":[" + String(100 - min(100.0f, idle_us[0] / 1e4f / win), 0) + "," + String(100 - min(100.0f, idle_us[1] / 1e4f / win), 0) + "]";
        o += ",\"tasks\":{";
        bool first = true;
        portENTER_CRITICAL(&mux);
        task_perf_t snap[PERF_TASKS];
        memcpy(snap, tasks, sizeof(snap));
        for (int i = 0; i < PERF_TASKS; i++) tasks[i].busy_us = 0;
        portEXIT_CRITICAL(&mux);
        for (int i = 0; i < PERF_TASKS; i++) if (snap[i].h) {
            o += String(first ? "" : ",") + "\"" + pcTaskGetName(snap[i].h) + "\":" + String(snap[i].busy_us / 1e4f / win, 1);
            first = false;
        }
        o += "}";
    }
    frames = frame_ms_sum = frame_ms_max = px_sum = 0;
    faces = face_us_sum = face_us_max = 0;
    flushes = flush_us_sum = flush_us_max = 0;
    idle_us[0] = idle_us[1] = 0;
    size_t free_int = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t big_int = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    o += ",\"heap\":" + String(free_int) + ",\"heap_min\":" + String(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    o += ",\"heap_block\":" + String(big_int) + ",\"frag\":" + String(free_int ? 100 - big_int * 100.0f / free_int : 0, 0);
    o += ",\"psram\":" + String(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) + ",\"uptime\":" + String(millis() / 1000);
    o += ",\"printing\":" + String(moonraker.data.printing ? "true" : "false") + "}";
    return o;
}
