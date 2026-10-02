// JSON documents whose pool lives in PSRAM. A DynamicJsonDocument under 4 KB would come from internal RAM,
// which is short once Bluetooth is on.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct PsramAllocator {
    void * allocate(size_t n) {
        void * p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return p ? p : malloc(n);
    }
    void deallocate(void * p) { heap_caps_free(p); }
    void * reallocate(void * p, size_t n) {
        void * q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return q ? q : realloc(p, n);
    }
};
typedef BasicJsonDocument<PsramAllocator> PsJsonDocument;

// serialize into a String reserved once (a String over 4 KB lands in PSRAM, and it isn't regrown 32 bytes at a time)
template <typename D> String json_string(const D & d) {
    String out;
    out.reserve(measureJson(d) + 1);
    serializeJson(d, out);
    return out;
}
