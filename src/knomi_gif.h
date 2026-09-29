#ifndef KNOMI_GIF_H
#define KNOMI_GIF_H

#include "lvgl.h"

// Animation slots. Each one plays its built-in GIF unless a custom GIF was
// uploaded through the web page (stored in LittleFS as /gif/<name>.gif).
typedef enum {
    GIF_SLOT_IDLE1 = 0,
    GIF_SLOT_IDLE2,
    GIF_SLOT_IDLE3,
    GIF_SLOT_IDLE4,
    GIF_SLOT_WELCOME,
    GIF_SLOT_HOMING,
    GIF_SLOT_PROBING,
    GIF_SLOT_QGLING,
    GIF_SLOT_SHAPING,
    GIF_SLOT_PID,
    GIF_SLOT_CLEANING,
    GIF_SLOT_FILAMENT,
    GIF_SLOT_PAUSED,
    GIF_SLOT_HEATED,
    GIF_SLOT_PRINT,
    GIF_SLOT_PRINT_OK,
    GIF_SLOT_PRINTED,
    GIF_SLOT_NUM,
} knomi_gif_slot_t;

#define GIF_MAX_FILE_SIZE  (1536 * 1024)   // per GIF
#define GIF_MAX_TOTAL_SIZE (5 * 1024 * 1024) // all custom GIFs loaded in PSRAM

typedef struct {
    const char * name;    // file / URL name
    const char * label;   // shown on the web page
    bool has_builtin;
    bool has_custom;      // custom file present on flash
    bool loaded;          // custom file currently loaded in PSRAM
    size_t custom_size;
} knomi_gif_info_t;

// Mount the filesystem. Call once from setup(), before tasks start.
void knomi_fs_init(void);
// Load custom GIFs into PSRAM. Call from the LVGL task before creating the UI.
void knomi_gif_init(void);

// Active GIF for a slot: custom if uploaded, else built-in (NULL for empty optional slots)
const lv_img_dsc_t * knomi_gif(knomi_gif_slot_t slot);
// lv_gif_set_src() wrapper that remembers which slot each object shows (for live reload)
void knomi_gif_show(lv_obj_t * obj, knomi_gif_slot_t slot);

// Idle rotation: the n-th enabled idle slot (has a GIF, ticked on the web page), and how many there are
uint8_t knomi_gif_idle_count(void);
knomi_gif_slot_t knomi_gif_idle_slot(uint8_t n);
bool knomi_gif_idle_enabled(int slot);
int knomi_gif_shown_slot(lv_obj_t * obj); // slot last shown on obj, -1 if none
void knomi_gif_forget(lv_obj_t * obj);
bool knomi_gif_is_face(int slot);          // idle / heated / printed / paused: always Coaster
bool knomi_gif_coaster_shows(int slot);    // Coaster acts this slot out (no uploaded GIF for it)     // call before deleting an object shown with knomi_gif_show

// Recolor built-in GIFs to the UI color (GIF_TINT_* from knomi.h). LVGL task only.
// Call after the theme color or the tint setting changes.
void knomi_gif_apply_tint(void);
// Palette recolor used by the above; exposed for testing
void knomi_gif_tint_buffer(uint8_t * buf, size_t size, uint8_t r, uint8_t g, uint8_t b);

// Web side (any task): flag a slot for reload after its file changed
void knomi_gif_request_reload(knomi_gif_slot_t slot);
// LVGL task: apply pending reloads
void knomi_gif_process(void);

int knomi_gif_slot_by_name(const char * name);
void knomi_gif_get_info(knomi_gif_slot_t slot, knomi_gif_info_t * info);
String knomi_gif_path(knomi_gif_slot_t slot);
const lv_img_dsc_t * knomi_gif_builtin(knomi_gif_slot_t slot);

#endif
