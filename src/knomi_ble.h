#ifndef KNOMI_BLE_H
#define KNOMI_BLE_H
#include <Arduino.h>

// BLE link to the OctoPrint-KNOMI plugin (KNOMI = peripheral, Pi = central).
// GATT service (all values UTF-8):
//   STATUS  write (encrypted+MITM)  plugin -> KNOMI, compact JSON status, ~2x/s + on change
//   FILES   write (encrypted+MITM)  plugin -> KNOMI, file list in frames: [flags][text], flags 1=start 2=end
//   CMD     notify                  KNOMI -> plugin, Moonraker-style paths (same strings the UI queues)
//   INFO    read                    firmware version / hostname
// Pairing: LE Secure Connections, passkey shown on the KNOMI screen, bonded.
#define KNOMI_BLE_SERVICE_UUID "4b4e4f4d-4900-4c69-6e6b-000000000001"
#define KNOMI_BLE_STATUS_UUID  "4b4e4f4d-4900-4c69-6e6b-000000000002"
#define KNOMI_BLE_FILES_UUID   "4b4e4f4d-4900-4c69-6e6b-000000000003"
#define KNOMI_BLE_CMD_UUID     "4b4e4f4d-4900-4c69-6e6b-000000000004"
#define KNOMI_BLE_INFO_UUID    "4b4e4f4d-4900-4c69-6e6b-000000000005"

void knomi_ble_init(void);              // setup(): starts advertising if enabled in config
bool knomi_ble_running(void);
bool knomi_ble_connected(void);         // a paired (encrypted + authenticated) central is connected
bool knomi_ble_link_active(void);       // ...and status arrived in the last few seconds
bool knomi_ble_process(void);           // moonraker task: apply the latest status; true if applied
bool knomi_ble_send_command(const String &path);
bool knomi_ble_file_list(String &out);  // last list the plugin sent
String knomi_ble_address(void);
void knomi_ble_forget_bonds(void);
// events for other tasks (each call consumes the event)
uint32_t knomi_ble_take_passkey(void);  // passkey to display, 0 = none
int knomi_ble_take_pair_result(void);   // 1 paired, -1 failed, 0 nothing
bool knomi_ble_take_wifi_request(void); // plugin asked for WiFi to come back on

// status decoding, split out so it can be tested off-device
bool knomi_ble_apply_status(const char *json, size_t len);
#endif
