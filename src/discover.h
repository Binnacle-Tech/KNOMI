#ifndef DISCOVER_H
#define DISCOVER_H
#include <WString.h>
// OctoPrint auto-discovery over mDNS (_octoprint._tcp, announced by OctoPrint's
// built-in discovery plugin). Scans run in the WiFi task; the web page polls.
void octoprint_discover_start(void);   // any task
void octoprint_discover_loop(void);    // WiFi task
String octoprint_discover_json(void);  // {"scanning":bool,"results":[{name,ip,port,path}]}
#endif
