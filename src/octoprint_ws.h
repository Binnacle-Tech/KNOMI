#ifndef OCTOPRINT_WS_H
#define OCTOPRINT_WS_H
// Push updates from OctoPrint's websocket (/sockjs/websocket, raw JSON).
// Runs in the moonraker task. HTTP polling stays as the fallback whenever
// the socket is down or quiet.
void octoprint_ws_loop(void);
bool octoprint_ws_healthy(void);
#endif
