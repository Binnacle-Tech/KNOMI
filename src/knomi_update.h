#ifndef KNOMI_UPDATE_H
#define KNOMI_UPDATE_H
#include <Arduino.h>
// One-click update: the KNOMI fetches the latest GitHub release of UPDATE_REPO,
// downloads its own .bin over verified HTTPS and flashes it (then restarts).
bool knomi_update_start(bool force);   // web task; false if one is already running
String knomi_update_status_json(void); // {"state":"downloading","pct":42,"msg":"..."}
bool knomi_update_busy(void);          // other network work should pause
#endif
