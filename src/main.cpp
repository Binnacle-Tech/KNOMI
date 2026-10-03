#include <Arduino.h>
#include "lvgl_hal.h"
#include "pinout.h"

#include "ui/ui.h"
#include "knomi_gif.h"
#include "knomi_ble.h"
#include "knomi_health.h"
#include "knomi_perf.h"
#include <esp_ota_ops.h>


// Hardware init
#ifdef I2C0_SUPPORT
TwoWire i2c0 = TwoWire(0);
// TwoWire i2c1 = TwoWire(1);
#endif

void lvgl_ui_task(void * parameter);
void lis2dw12_task(void * parameter);
void wifi_task(void * parameter);
void moonraker_task(void * parameter);
void eeprom_init(void);

/* ---- self-rescue: crashing right after starting, again and again, means the new firmware is
 * broken. After 3 crashes in a row, each within a minute of starting, switch back to the
 * firmware in the other OTA slot (the one before the last update). Once only, so two broken
 * slots don't ping-pong. The counters live in RTC memory: they survive a crash but not a
 * power cut, so unplugging always starts fresh. ---- */
#define RESCUE_MAGIC 0xC0A57E12u
RTC_NOINIT_ATTR static uint32_t rescue_magic, rescue_crashes, rescue_done;
RTC_NOINIT_ATTR static char rescue_note[80];
TaskHandle_t knomi_tasks[KNOMI_TASKS];

static void self_rescue(void) {
    esp_reset_reason_t r = esp_reset_reason();
    bool crash = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
    if (rescue_magic != RESCUE_MAGIC || r == ESP_RST_POWERON) {
        rescue_magic = RESCUE_MAGIC; rescue_crashes = 0; rescue_done = 0; rescue_note[0] = 0;
    }
    if (rescue_note[0]) { Serial.printf("rescue: %s\r\n", rescue_note); rescue_note[0] = 0; }
    if (!crash) { rescue_crashes = 0; return; }
    rescue_crashes++;
    Serial.printf("rescue: crashed right after starting (%u in a row)\r\n", rescue_crashes);
    if (rescue_crashes < 3 || rescue_done) return;
    const esp_partition_t * running = esp_ota_get_running_partition();
    const esp_partition_t * other = esp_ota_get_next_update_partition(NULL);
    if (!other || other == running) return;
    if (esp_ota_set_boot_partition(other) != ESP_OK) {   // the other slot has no valid firmware
        Serial.println("rescue: no older firmware to go back to");
        return;
    }
    rescue_done = 1; rescue_crashes = 0;
    snprintf(rescue_note, sizeof(rescue_note), "went back to the older firmware in %s after 3 crashes", other->label);
    esp_restart();
}

// called once a minute after boot: made it, so this firmware is fine
void knomi_rescue_ok(void) { rescue_crashes = 0; rescue_done = 0; }

void setup() {
    Serial.begin(115200);
    while (!Serial)
        delay(10);
    Serial.println("\r\n\r\n------------- Knomi startup -----------\r\n");
    self_rescue();
    knomi_perf_init();
    Serial.println("SPI Flash: ");
    Serial.print("  Size: ");
    Serial.println(ESP.getFlashChipSize());
    Serial.print("  Speed: ");
    Serial.println(ESP.getFlashChipSpeed());
    Serial.print("  Mode: ");
    Serial.println(ESP.getFlashChipMode());
    Serial.println("SPI PSRAM: ");
    Serial.print("  Found: ");
    Serial.println(psramFound());
    Serial.print("  Size: ");
    Serial.println(ESP.getPsramSize());
    Serial.println("\r\n\r\n------------------------------------------\r\n");

#ifdef I2C0_SUPPORT
    i2c0.begin(I2C0_SDA_PIN, I2C0_SCL_PIN, I2C0_SPEED);
    // i2c1.begin(I2C1_SDA_PIN, I2C1_SCL_PIN, I2C1_SPEED);
#endif

    eeprom_init();   // settings, before any task reads knomi_config
    knomi_fs_init(); // custom GIF storage
    knomi_ble_init(); // Bluetooth link to the OctoPrint plugin, if enabled, before the UI and web server use it

    xTaskCreatePinnedToCore(lvgl_ui_task, "lvgl ui",
        16384, // Stack size (bytes): Coaster's drawing, settings and feelings run here
        NULL,  // Parameter to pass
        10,     // Task priority
        &knomi_tasks[KT_LVGL],  // Task handle
        1);    // core 1; frames are sent to the screen from core 0 (lvgl_hal.cpp)

#ifdef LIS2DW_SUPPORT
    // on core 0: left to float it often waited behind the screen work on core 1 (/perf during a print with
    // Coaster on screen: 40-54 readings a second instead of 140)
    xTaskCreatePinnedToCore(lis2dw12_task, "lis2dw12",
        4096,  // Stack size (bytes)
        NULL,  // Parameter to pass
        9,     // Task priority
        &knomi_tasks[KT_ACCEL],  // Task handle
        0);
#endif

    xTaskCreate(wifi_task, "wifi",
        8192,  // tools/check.py: HTTP / WiFi AP setup go deep  // Stack size (bytes)
        NULL,  // Parameter to pass
        8,     // Task priority
        &knomi_tasks[KT_WIFI]   // Task handle
        );

    xTaskCreate(moonraker_task, "moonraker",
        8192,  // tools/check.py: HTTP / WiFi AP setup go deep  // Stack size (bytes)
        NULL,  // Parameter to pass
        7,     // Task priority
        &knomi_tasks[KT_PRINTER]   // Task handle
        );
}

void loop() {
    static bool ok = false;
    if (!ok && millis() > 60000) { ok = true; knomi_rescue_ok(); }
    delay(1000);
}
