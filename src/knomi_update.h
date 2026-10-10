#ifndef KNOMI_UPDATE_H
#define KNOMI_UPDATE_H
#include <Arduino.h>
// One-click update: the KNOMI fetches the latest GitHub release of UPDATE_REPO,
// downloads its own .bin over verified HTTPS and flashes it (then restarts).
bool knomi_update_start(bool force);   // web task; false if one is already running
String knomi_update_status_json(void); // {"state":"downloading","pct":42,"msg":"..."}
bool knomi_update_busy(void);          // other network work should pause
String knomi_update_latest(void);      // newest release tag known ("v1.0.2-op59"), "" if not known yet
bool knomi_update_newer_out(void);     // ...and it's newer than this firmware
void knomi_update_set_latest(const char * tag);   // OctoPrint build: from the plugin
void knomi_update_loop(void);          // WiFi task (Moonraker build: checks GitHub now and then)
#ifdef __cplusplus
void knomi_update_remote(const char * state, int pct, const char * msg);   // OctoPrint build: the plugin's install
#endif
#endif
