#include <Arduino.h>
#include <LIS2DW12Sensor.h>
#include "pinout.h"
#include "knomi_coaster.h"
#include "knomi_perf.h"

#ifdef LIS2DW_SUPPORT

// #define LIS2DW12_DEBUG

extern TwoWire i2c0;
LIS2DW12Sensor lis2dw12 = LIS2DW12Sensor(&i2c0, LIS2DW12_I2C_ADD_H);

extern LIS2DW12Sensor lis2dw12;

// Printer-frame motion (X, Y, Z) in mg with gravity removed, as a decaying peak
// so short moves are visible on the 10 Hz UI. Used by the printing screen bars.
int32_t lis2dw12_acc[3];

/*
 * The stock code swapped Y/Z for one mounting and subtracted a fixed 980 mg from
 * Z, so on other mounts (or flipped boards) Z sat at a full bar and Y/Z looked
 * reversed (#42, #57, #62). Instead:
 *  - gravity is tracked with a slow low-pass per axis and subtracted (any mount),
 *  - the axis gravity pulls on is the printer's Z; the axis through the screen is
 *    Y (the KNOMI faces the front); the remaining in-plane axis is X.
 */
#define SAMPLE_MS     5       // 200 Hz (the coaster face wants the motion, not just peaks)
#define GRAVITY_ALPHA 0.0025f // ~2 s time constant for the gravity estimate
#define PEAK_DECAY    0.96f   // per sample (same fall-off as 0.85 at 50 Hz)

void lis2dw12_task(void * parameter) {
    Serial.println("\r\n******** LIS2DW12 startup *****\r\n");
    if (lis2dw12.begin()) {   // change this to 0x19 for alternative i2c address
        while (1) {
            Serial.println("Couldn't find lis2dw12");
            delay(1000);
        }
    }
    Serial.println("LIS2DW12 found!");
    lis2dw12.Set_X_ODR(200.0f);
    lis2dw12.Set_X_FS(4.0f);
    lis2dw12.Enable_X();
    Serial.println("\r\n******** LIS2DW12 init ok *****\r\n");

    // Back to the stock Get_X_Axes() per sample (OP35-38 read the range separately and Coaster stopped
    // feeling motion). /perf reports reads, failures and the last values so a stuck sensor shows up.
    int32_t raw[3] = {0, 0, 0}, last_raw[3] = {0, 0, 0};
    float g[3] = {0, 0, 0};
    float peak[3] = {0, 0, 0};
    bool first = true;
    uint8_t z_axis = 1;   // raw index gravity is on (stock mount: raw Y)
    uint32_t same_n = 0;

    for(;;) {
        uint32_t t0 = millis();
        if (lis2dw12.Get_X_Axes(raw) == LIS2DW12_STATUS_OK) knomi_perf_accel(true, raw);
        else knomi_perf_accel(false, raw);
        // exactly the same reading for 2 s is a sensor that stopped measuring: wake it up again
        if (raw[0] == last_raw[0] && raw[1] == last_raw[1] && raw[2] == last_raw[2]) {
            if (++same_n == 400) {
                Serial.println("LIS2DW12: readings stopped changing, restarting it");
                lis2dw12.Disable_X(); lis2dw12.Set_X_ODR(200.0f); lis2dw12.Set_X_FS(4.0f); lis2dw12.Enable_X();
                same_n = 0;
            }
        } else same_n = 0;
        memcpy(last_raw, raw, sizeof(raw));
        if (first) {
            for (int i = 0; i < 3; i++) g[i] = raw[i];
            first = false;
        }
        float dyn[3];
        for (int i = 0; i < 3; i++) {
            g[i] += GRAVITY_ALPHA * (raw[i] - g[i]);
            dyn[i] = raw[i] - g[i];
        }
        // which raw axis carries gravity (with hysteresis so it doesn't flicker)
        uint8_t best = 0;
        for (uint8_t i = 1; i < 3; i++) if (fabsf(g[i]) > fabsf(g[best])) best = i;
        if (best != z_axis && fabsf(g[best]) > fabsf(g[z_axis]) + 200) z_axis = best;

        uint8_t ax, ay, az;
        if (z_axis == 2) {          // lying flat: board axes are the printer axes
            ax = 0; ay = 1; az = 2;
        } else {                    // upright on the toolhead: screen normal is printer Y
            az = z_axis; ay = 2; ax = (z_axis == 0) ? 1 : 0;
        }
        const uint8_t map[3] = {ax, ay, az};
        // signed printer-frame motion in g for the coaster face; +Z is up (against gravity)
        float up = g[az] >= 0 ? 1.0f : -1.0f;
        coaster_push_sample(dyn[ax] / 1000.0f, dyn[ay] / 1000.0f, up * dyn[az] / 1000.0f);
        for (int i = 0; i < 3; i++) {
            float v = fabsf(dyn[map[i]]);
            peak[i] = max(v, peak[i] * PEAK_DECAY);
            lis2dw12_acc[i] = (int32_t)peak[i];
        }
#ifdef LIS2DW12_DEBUG
        Serial.printf("raw %d %d %d  g-axis %d  xyz %d %d %d\r\n", raw[0], raw[1], raw[2], z_axis,
                      lis2dw12_acc[0], lis2dw12_acc[1], lis2dw12_acc[2]);
#endif
        // aim for a sample every 5 ms: the read itself takes a few (3 I2C transactions at 100 kHz)
        uint32_t spent = millis() - t0;
        delay(spent >= SAMPLE_MS ? 1 : SAMPLE_MS - spent);
    }
}

#endif
