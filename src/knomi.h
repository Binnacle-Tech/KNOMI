#ifndef KNOMI_H
#define KNOMI_H

#include "pinout.h"
#include "config.h"
#include <WiFi.h>
#include "lvgl.h"

typedef void (*p_function_t)(void);

typedef struct {
    char sta_ssid[33];
    char sta_pwd[65];
    wifi_auth_mode_t sta_auth;
    char ap_ssid[33];
    char ap_pwd[65];
    char hostname[16];
    char moonraker_ip[65]; // "192.168.255.255" or "<hostname>.local"
    char moonraker_port[6]; // "65536", max_len=6
    char moonraker_tool[7]; // "tool0"
    char mode[6]; // "ap":WIFI_MODE_AP, "sta":WIFI_MODE_STA, "apsta":WIFI_MODE_APSTA
    lv_color_t theme_color;
    // --- appended for OctoPrint support (keep at end: EEPROM layout migration) ---
    char backend[10];  // "moonraker" or "octoprint"
    char api_key[65];  // OctoPrint API key (X-Api-Key), unused for moonraker
    // --- appended for display settings (layout V3) ---
    uint8_t backlight;        // 1..16
    uint8_t dim_level;        // 1..16, brightness when dimmed
    uint16_t dim_after_min;   // 0 = never
    uint16_t sleep_after_min; // 0 = never (backlight off)
    uint8_t awake_printing;   // 1 = never dim/sleep while the printer is busy
    uint8_t print_view;       // PRINT_VIEW_*
    uint8_t gif_tint;         // GIF_TINT_*
    // --- appended for Bluetooth (layout V4) ---
    uint8_t bt_enabled;       // 1 = advertise + talk to the OctoPrint plugin over BLE
    uint8_t bt_wifi_off;      // 1 = WiFi off while the BLE link is up
    uint16_t bt_fallback_s;   // WiFi comes back if BLE isn't connected within this long
} knomi_config_t;

#define PRINT_VIEW_INFO  0   // time left, temps, file, Z/layer
#define PRINT_VIEW_ACCEL 1   // stock accelerometer bars
#define GIF_TINT_OFF  0
#define GIF_TINT_IDLE 1      // idle faces follow the UI color
#define GIF_TINT_ALL  2      // every built-in animation follows the UI color

#define BACKEND_MOONRAKER "moonraker"
#define BACKEND_OCTOPRINT "octoprint"
bool knomi_backend_is_octoprint(void);

typedef struct {
    int32_t rssi[SCAN_SSIDS_NUM]; // RSSI returned RSSI values
    char ssid[SCAN_SSIDS_NUM][65]; // SSIDs returned by the wifi scan.
    wifi_auth_mode_t authmode[SCAN_SSIDS_NUM]; // encryptionType
    uint8_t connected[SCAN_SSIDS_NUM];
    uint8_t count; // Number of WiFi Scanned
} knomi_wifi_scan_t;

extern knomi_config_t knomi_config;
extern knomi_wifi_scan_t wifi_scan;

#define WEB_POST_NULL                 (0)
#define WEB_POST_LOCAL_HOSTNAME       BIT(0)
#define WEB_POST_WIFI_CONFIG_MODE     BIT(1)
#define WEB_POST_WIFI_CONFIG_AP       BIT(2)
#define WEB_POST_WIFI_CONFIG_STA      BIT(3)
#define WEB_POST_MOONRAKER            BIT(4)
#define WEB_POST_WIFI_REFRESH         BIT(5)
#define WEB_POST_RESTART              BIT(6)

#define LOCAL_POST_LV_THEME_COLOR     BIT(7)
#define LOCAL_POST_SETTINGS           BIT(8)  // display settings changed (web or backlight slider)
#define EEPROM_PARA_CHANGED     (WEB_POST_WIFI_CONFIG_AP   | \
                                 WEB_POST_WIFI_CONFIG_STA  | \
                                 WEB_POST_WIFI_CONFIG_MODE | \
                                 WEB_POST_LOCAL_HOSTNAME   | \
                                 WEB_POST_MOONRAKER        | \
                                 LOCAL_POST_LV_THEME_COLOR | \
                                 LOCAL_POST_SETTINGS)


void knomi_config_require_change(uint16_t require);
void wifi_scan_refresh_set_callback(p_function_t cb);
wifi_auth_mode_t wifi_get_ahth_mode_from_scanned_list(void);

void knomi_factory_reset(void);

typedef enum {
    WIFI_STATUS_ERROR, // no station ssid or connect failed
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_DISCONNECT,
    WIFI_STATUS_INIT,
}wifi_status_t;

wifi_status_t wifi_get_connect_status(void);
bool knomi_wifi_suspended(void); // WiFi turned off because Bluetooth is carrying the link

extern int32_t lis2dw12_acc[3];

// Macros for array numbers
#define ACOUNT(n) (sizeof(n) / sizeof(n[0]))
// Macros to make a string from a macro
#define STRINGIFY_(M) #M
#define STRINGIFY(M) STRINGIFY_(M)

// Preheat
#define PREHEAT_0_NOZZLE 0
#define PREHEAT_0_BED    0
#define PREHEAT_0_LABEL "CoolDown"
#define PREHEAT_1_NOZZLE 200
#define PREHEAT_1_BED    60
#define PREHEAT_1_LABEL "PLA:" STRINGIFY(PREHEAT_1_NOZZLE) "/" STRINGIFY(PREHEAT_1_BED) "℃"
#define PREHEAT_2_NOZZLE 240
#define PREHEAT_2_BED    70
#define PREHEAT_2_LABEL "PETG:" STRINGIFY(PREHEAT_2_NOZZLE) "/" STRINGIFY(PREHEAT_2_BED) "℃"
#define PREHEAT_3_NOZZLE 230
#define PREHEAT_3_BED    90
#define PREHEAT_3_LABEL "ABS:" STRINGIFY(PREHEAT_3_NOZZLE) "/" STRINGIFY(PREHEAT_3_BED) "℃"
#define PREHEAT_4_NOZZLE 170
#define PREHEAT_4_BED    50
#define PREHEAT_4_LABEL "WOOD:" STRINGIFY(PREHEAT_4_NOZZLE) "/" STRINGIFY(PREHEAT_4_BED) "℃"
#define PREHEAT_5_NOZZLE 220
#define PREHEAT_5_BED    50
#define PREHEAT_5_LABEL "TPU:" STRINGIFY(PREHEAT_5_NOZZLE) "/" STRINGIFY(PREHEAT_5_BED) "℃"
#define PREHEAT_6_NOZZLE 250
#define PREHEAT_6_BED    90
#define PREHEAT_6_LABEL "NYLON:" STRINGIFY(PREHEAT_6_NOZZLE) "/" STRINGIFY(PREHEAT_6_BED) "℃"

// Extrude length, mm
#define EXTRUDE_MM_0 1
#define EXTRUDE_MM_0_LABEL STRINGIFY(EXTRUDE_MM_0)"mm"
#define EXTRUDE_MM_1 5
#define EXTRUDE_MM_1_LABEL STRINGIFY(EXTRUDE_MM_1)"mm"
#define EXTRUDE_MM_2 10
#define EXTRUDE_MM_2_LABEL STRINGIFY(EXTRUDE_MM_2)"mm"
#define EXTRUDE_MM_3 50
#define EXTRUDE_MM_3_LABEL STRINGIFY(EXTRUDE_MM_3)"mm"
#define EXTRUDE_MM_4 100
#define EXTRUDE_MM_4_LABEL STRINGIFY(EXTRUDE_MM_4)"mm"
// mm/s to mm/min
#define MMS_TO_MMM(n) ((n) * 60)
// Extrude speed, mm/s
#define EXTRUDE_MM_S_0 1
#define EXTRUDE_MM_S_0_LABEL STRINGIFY(EXTRUDE_MM_S_0)"mm/s"
#define EXTRUDE_MM_S_1 5
#define EXTRUDE_MM_S_1_LABEL STRINGIFY(EXTRUDE_MM_S_1)"mm/s"
#define EXTRUDE_MM_S_2 10
#define EXTRUDE_MM_S_2_LABEL STRINGIFY(EXTRUDE_MM_S_2)"mm/s"
#define EXTRUDE_MM_S_3 50
#define EXTRUDE_MM_S_3_LABEL STRINGIFY(EXTRUDE_MM_S_3)"mm/s"
#define EXTRUDE_MM_S_4 100
#define EXTRUDE_MM_S_4_LABEL STRINGIFY(EXTRUDE_MM_S_4)"mm/s"

#endif
