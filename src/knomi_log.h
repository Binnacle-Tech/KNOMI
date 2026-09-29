#ifndef KNOMI_LOG_H
#define KNOMI_LOG_H
// Everything the firmware prints with Serial.* also goes into a RAM ring buffer
// (with uptime stamps) that the web page shows at /log. Force-included into every
// project source (build_src_flags = -include knomi_log.h), after Arduino.h, so
// "Serial" below means the tee.
#ifdef __cplusplus
#include <Arduino.h>

class KnomiLog : public Print {
public:
    void begin(unsigned long baud);
    size_t write(uint8_t c) override;
    size_t write(const uint8_t * buf, size_t n) override;
    void flush(void) override;
    operator bool() const { return true; }
};
extern KnomiLog knomi_log;
String knomi_log_text(void);   // the ring buffer, oldest first
void knomi_log_clear(void);

#define Serial knomi_log
#endif
#endif
