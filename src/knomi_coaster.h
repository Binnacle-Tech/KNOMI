#ifndef KNOMI_COASTER_H
#define KNOMI_COASTER_H
// Coaster face: a face drawn live from the accelerometer (and printer state) instead of a GIF.
// Its head and pupils hang on springs driven by toolhead motion, and it picks a mood from how
// hard and how long it's being thrown. Tuning lives in /coaster.json (web page /coaster).
#include "lvgl.h"
#ifdef __cplusplus
#include <WString.h>
extern "C" {
#endif

void coaster_init(void);                                  // LVGL task, once, after the UI exists
void coaster_loop(void);                                  // LVGL task, every loop pass
lv_obj_t * coaster_create(lv_obj_t * parent, int size);   // a face object, size px square (240 = full screen)
void coaster_forget(lv_obj_t * obj);                      // before deleting a face object
bool coaster_idle_enabled(void);                          // idle screen shows the face instead of GIFs
void coaster_request_reload(void);                        // any task: re-read /coaster.json
void coaster_push_sample(float x, float y, float z);      // sensor task: printer-frame accel in g, gravity removed
void coaster_event_ready(float secs);                     // LVGL task: heated up, print starting
void coaster_poke(void);                                  // LVGL task: tapped / tickled
void coaster_set_act(int slot);                           // LVGL task: busy state (GIF slot) Coaster stands in for

#ifdef __cplusplus
}
String coaster_state_json(void);   // live mood + meters for the web page
uint32_t coaster_ms_to_frame(void);
String coaster_tuning_json(void);  // current tuning (defaults if never saved)
String coaster_plugin_json(bool motion = false, size_t max = 640);   // max: keep the JSON under this many bytes
extern bool coaster_plugin_watched;
String coaster_album_json(void);   // the Coaster page's album  // mood, hat, last report for the OctoPrint sidebar
const char * coaster_save_json(const char * json, size_t len);  // web task; NULL or an error
void coaster_set_idle(bool on);    // web task
#include <time.h>
void coaster_host_tz(int minutes);                         // the Pi's UTC offset (used until a page saves one)
bool coaster_clock_text(time_t utc, char * out, size_t n, bool compact = false); // "9:40 pm" ("9:40pm" compact) / "21:40" as Coaster's clock shows it; false if the clock isn't set
#endif

#define COASTER_PATH "/coaster.json"
#endif
