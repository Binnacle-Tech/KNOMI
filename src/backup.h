#ifndef BACKUP_H
#define BACKUP_H
#include <WString.h>
class AsyncWebServer;
// GET /backup -> .knomi file (settings + custom GIFs); POST /restore <- same file
void backup_routes(AsyncWebServer &server);
String backup_config_json(void);
bool backup_apply_config_json(const char *json, size_t len);
#endif
