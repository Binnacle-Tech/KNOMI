#include "knomi_log.h"
#undef Serial   // the real port

#define LOG_SIZE (32 * 1024)

KnomiLog knomi_log;
static char * ring = NULL;
static size_t head = 0;        // next write position
static bool wrapped = false;
static bool line_start = true;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static void ring_put(char c) {
    if (!ring) {
        ring = (char *)ps_malloc(LOG_SIZE + 1);   // +1: String::concat reads one past what it copies
        if (!ring) return;
    }
    ring[head++] = c;
    if (head >= LOG_SIZE) { head = 0; wrapped = true; }
}

static void ring_write(const uint8_t * buf, size_t n) {
    char stamp[16];
    portENTER_CRITICAL(&mux);
    for (size_t i = 0; i < n; i++) {
        char c = (char)buf[i];
        if (c == '\r') continue;
        if (line_start) {
            uint32_t ms = millis();
            int k = snprintf(stamp, sizeof(stamp), "[%6u.%02u] ", (unsigned)(ms / 1000), (unsigned)(ms % 1000) / 10);
            for (int j = 0; j < k; j++) ring_put(stamp[j]);
            line_start = false;
        }
        ring_put(c);
        if (c == '\n') line_start = true;
    }
    portEXIT_CRITICAL(&mux);
}

void KnomiLog::begin(unsigned long baud) { Serial.begin(baud); }

size_t KnomiLog::write(uint8_t c) { return write(&c, 1); }

size_t KnomiLog::write(const uint8_t * buf, size_t n) {
    ring_write(buf, n);
    return Serial.write(buf, n);
}

void KnomiLog::flush(void) { Serial.flush(); }

String knomi_log_text(size_t tail) {
    String out;
    if (!ring) return out;
    portENTER_CRITICAL(&mux);
    size_t h = head; bool w = wrapped;
    portEXIT_CRITICAL(&mux);
    // copy outside the lock (a concurrent write can tear one line, which is fine for a log)
    size_t len = w ? LOG_SIZE : h;
    if (tail && tail < len) len = tail;
    // the text is the last len bytes before head (wrapping), starting at the next full line
    size_t start = (h + LOG_SIZE - len) % LOG_SIZE, n = len;
    if (w || len < h) {
        while (n && ring[start] != '\n') { start = (start + 1) % LOG_SIZE; n--; }
        if (n) { start = (start + 1) % LOG_SIZE; n--; }
    }
    out.reserve(n + 1);
    if (start + n > LOG_SIZE) {
        out.concat(ring + start, LOG_SIZE - start);
        out.concat(ring, n - (LOG_SIZE - start));
    } else {
        out.concat(ring + start, n);
    }
    return out;
}

void knomi_log_clear(void) {
    portENTER_CRITICAL(&mux);
    head = 0; wrapped = false; line_start = true;
    portEXIT_CRITICAL(&mux);
}
