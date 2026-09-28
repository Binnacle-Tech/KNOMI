#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include "knomi.h"
#include "knomi_gif.h"
#include "ui_overlay/lv_overlay.h"

typedef struct {
    const char * name;
    const char * label;
    const lv_img_dsc_t * builtin;
} gif_slot_def_t;

// Fallbacks for the new states reuse stock animations until you upload your own
static const gif_slot_def_t slot_def[GIF_SLOT_NUM] = {
    {"idle1",    "Idle 1",                     &gif_voron},
    {"idle2",    "Idle 2",                     &gif_standby},
    {"idle3",    "Idle 3 (optional)",          NULL},
    {"idle4",    "Idle 4 (optional)",          NULL},
    {"welcome",  "Boot / WiFi setup",          &gif_welcome},
    {"homing",   "Homing",                     &gif_homing},
    {"probing",  "Probing / bed mesh",         &gif_probing},
    {"qgling",   "Gantry leveling (QGL)",      &gif_qgling},
    {"shaping",  "Input shaper calibration",   &gif_qgling},
    {"pid",      "PID tuning",                 &gif_heated},
    {"cleaning", "Nozzle cleaning",            &gif_probing},
    {"filament", "Filament load / unload",     &gif_print},
    {"paused",   "Paused (plays on the print screen)", &gif_standby},
    {"heated",   "Heated, print starting",     &gif_heated},
    {"print",    "Printing",                   &gif_print},
    {"print_ok", "Print finished",             &gif_print_ok},
    {"printed",  "After print finished",       &gif_printed},
};

static lv_img_dsc_t * custom[GIF_SLOT_NUM];  // PSRAM copies of uploaded GIFs
static lv_img_dsc_t * tinted[GIF_SLOT_NUM];  // PSRAM recolored copies of built-ins
static size_t custom_total = 0;
static bool fs_ok = false;
static volatile uint32_t pending_reload = 0;

// objects showing a slot, so a reload can swap them before freeing memory
#define GIF_OBJ_MAX 4
static struct { lv_obj_t * obj; int slot; } shown[GIF_OBJ_MAX];

String knomi_gif_path(knomi_gif_slot_t slot) {
    return String("/gif/") + slot_def[slot].name + ".gif";
}

const lv_img_dsc_t * knomi_gif_builtin(knomi_gif_slot_t slot) {
    return slot_def[slot].builtin;
}

int knomi_gif_slot_by_name(const char * name) {
    for (int i = 0; i < GIF_SLOT_NUM; i++) {
        if (strcmp(slot_def[i].name, name) == 0) return i;
    }
    return -1;
}

void knomi_fs_init(void) {
    // "spiffs" partition in spiffs_16MB.csv (~7MB), formatted as LittleFS on first boot
    fs_ok = LittleFS.begin(true, "/littlefs", 10, "spiffs");
    Serial.printf("LittleFS: %s, %u/%u bytes used\r\n", fs_ok ? "ok" : "FAILED",
                  fs_ok ? (unsigned)LittleFS.usedBytes() : 0, fs_ok ? (unsigned)LittleFS.totalBytes() : 0);
    if (fs_ok && !LittleFS.exists("/gif")) LittleFS.mkdir("/gif");
}

static void free_custom(lv_img_dsc_t * d) {
    if (!d) return;
    free((void *)d->data);
    free(d);
}

static lv_img_dsc_t * load_custom(knomi_gif_slot_t slot, size_t budget) {
    if (!fs_ok) return NULL;
    String path = knomi_gif_path(slot);
    if (!LittleFS.exists(path)) return NULL;
    File f = LittleFS.open(path, "r");
    if (!f) return NULL;
    size_t size = f.size();
    if (size < 16 || size > GIF_MAX_FILE_SIZE || size > budget) {
        Serial.printf("gif %s: size %u rejected\r\n", slot_def[slot].name, (unsigned)size);
        f.close();
        return NULL;
    }
    uint8_t * buf = (uint8_t *)ps_malloc(size);
    lv_img_dsc_t * d = (lv_img_dsc_t *)ps_malloc(sizeof(lv_img_dsc_t));
    if (!buf || !d) {
        free(buf); free(d); f.close();
        return NULL;
    }
    size_t got = f.read(buf, size);
    f.close();
    if (got != size || memcmp(buf, "GIF8", 4) != 0) {
        Serial.printf("gif %s: bad file\r\n", slot_def[slot].name);
        free(buf); free(d);
        return NULL;
    }
    memset(d, 0, sizeof(*d));
    d->header.cf = LV_IMG_CF_RAW_CHROMA_KEYED;
    d->header.w = buf[6] | (buf[7] << 8);
    d->header.h = buf[8] | (buf[9] << 8);
    d->data_size = size;
    d->data = buf;
    Serial.printf("gif %s: custom %ux%u, %u bytes\r\n", slot_def[slot].name,
                  d->header.w, d->header.h, (unsigned)size);
    return d;
}

void knomi_gif_init(void) {
    for (int i = 0; i < GIF_SLOT_NUM; i++) {
        custom[i] = load_custom((knomi_gif_slot_t)i, GIF_MAX_TOTAL_SIZE - custom_total);
        if (custom[i]) custom_total += custom[i]->data_size;
    }
}

const lv_img_dsc_t * knomi_gif(knomi_gif_slot_t slot) {
    if (slot >= GIF_SLOT_NUM) return NULL;
    if (custom[slot]) return custom[slot];
    if (tinted[slot]) return tinted[slot];
    return slot_def[slot].builtin;
}

// Point every tracked object at its slot's current source. Run after swapping
// buffers and before freeing old ones (objects may show another slot's GIF as a fallback).
static void reshow_all(void) {
    for (int i = 0; i < GIF_OBJ_MAX; i++) {
        if (shown[i].obj) knomi_gif_show(shown[i].obj, (knomi_gif_slot_t)shown[i].slot);
    }
}

/* ---- tint: recolor the saturated palette entries of a GIF ----
 * Stock animations are BTT red (~#C02F30) plus black/white/grey. Neutral colors
 * are kept; colored entries become the UI color, keeping their relative
 * brightness so anti-aliased edges stay smooth. Only palettes change, so the
 * LZW image data (and file size) is untouched. */
static void tint_table(uint8_t * t, size_t n, uint8_t tr, uint8_t tg, uint8_t tb) {
    const float REF = 192.0f; // brightness of the stock accent red
    for (size_t i = 0; i < n; i++, t += 3) {
        uint8_t mx = max(t[0], max(t[1], t[2]));
        uint8_t mn = min(t[0], min(t[1], t[2]));
        if (mx < 24) continue;                        // black / near black
        if ((mx - mn) * 100 < mx * 30) continue;      // low saturation: white/grey
        float k = mx / REF;
        if (k > 1.0f) k = 1.0f;
        t[0] = (uint8_t)(tr * k + 0.5f);
        t[1] = (uint8_t)(tg * k + 0.5f);
        t[2] = (uint8_t)(tb * k + 0.5f);
    }
}

static bool skip_sub_blocks(const uint8_t * buf, size_t size, size_t * p) {
    while (*p < size && buf[*p]) *p += buf[*p] + 1;
    if (*p >= size) return false;
    (*p)++; // terminator
    return true;
}

void knomi_gif_tint_buffer(uint8_t * buf, size_t size, uint8_t tr, uint8_t tg, uint8_t tb) {
    if (size < 13 || memcmp(buf, "GIF8", 4) != 0) return;
    size_t p = 13;
    uint8_t flags = buf[10];
    if (flags & 0x80) {                       // global color table
        size_t n = (size_t)1 << ((flags & 7) + 1);
        if (p + n * 3 > size) return;
        tint_table(buf + p, n, tr, tg, tb);
        p += n * 3;
    }
    while (p < size) {
        uint8_t b = buf[p++];
        if (b == 0x21) {                      // extension: label + sub-blocks
            p++;
            if (!skip_sub_blocks(buf, size, &p)) return;
        } else if (b == 0x2C) {               // image descriptor
            if (p + 9 > size) return;
            uint8_t f = buf[p + 8];
            p += 9;
            if (f & 0x80) {                   // local color table
                size_t n = (size_t)1 << ((f & 7) + 1);
                if (p + n * 3 > size) return;
                tint_table(buf + p, n, tr, tg, tb);
                p += n * 3;
            }
            p++;                              // LZW minimum code size
            if (!skip_sub_blocks(buf, size, &p)) return;
        } else {
            return;                           // trailer (0x3B) or unknown
        }
    }
}

static lv_img_dsc_t * make_tinted(const lv_img_dsc_t * src, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t * buf = (uint8_t *)ps_malloc(src->data_size);
    lv_img_dsc_t * d = (lv_img_dsc_t *)ps_malloc(sizeof(lv_img_dsc_t));
    if (!buf || !d) { free(buf); free(d); return NULL; }
    memcpy(buf, src->data, src->data_size);
    knomi_gif_tint_buffer(buf, src->data_size, r, g, b);
    *d = *src;
    d->data = buf;
    return d;
}

void knomi_gif_apply_tint(void) {
    lv_img_dsc_t * old[GIF_SLOT_NUM];
    memcpy(old, tinted, sizeof(old));
    memset(tinted, 0, sizeof(tinted));

    bool default_color = knomi_config.theme_color.full == lv_color_hex(LV_DEFAULT_COLOR).full;
    uint8_t mode = default_color ? GIF_TINT_OFF : knomi_config.gif_tint;
    if (mode != GIF_TINT_OFF) {
        lv_color32_t c;
        c.full = lv_color_to32(knomi_config.theme_color);
        for (int s = 0; s < GIF_SLOT_NUM; s++) {
            if (!slot_def[s].builtin) continue;
            bool idle = s >= GIF_SLOT_IDLE1 && s <= GIF_SLOT_IDLE4;
            if (mode == GIF_TINT_IDLE && !idle) continue;
            if (s == GIF_SLOT_PRINT_OK) continue; // keep the success check green
            tinted[s] = make_tinted(slot_def[s].builtin, c.ch.red, c.ch.green, c.ch.blue);
        }
    }
    reshow_all();
    for (int s = 0; s < GIF_SLOT_NUM; s++) free_custom(old[s]);
}

void knomi_gif_show(lv_obj_t * obj, knomi_gif_slot_t slot) {
    const lv_img_dsc_t * src = knomi_gif(slot);
    if (!src) src = knomi_gif(GIF_SLOT_IDLE1);
    lv_gif_set_src(obj, src);
    int free_i = -1;
    for (int i = 0; i < GIF_OBJ_MAX; i++) {
        if (shown[i].obj == obj) { shown[i].slot = slot; return; }
        if (!shown[i].obj && free_i < 0) free_i = i;
    }
    if (free_i >= 0) { shown[free_i].obj = obj; shown[free_i].slot = slot; }
}

uint8_t knomi_gif_idle_count(void) {
    uint8_t n = 0;
    for (int s = GIF_SLOT_IDLE1; s <= GIF_SLOT_IDLE4; s++)
        if (knomi_gif((knomi_gif_slot_t)s)) n++;
    return n;
}

knomi_gif_slot_t knomi_gif_idle_slot(uint8_t n) {
    for (int s = GIF_SLOT_IDLE1; s <= GIF_SLOT_IDLE4; s++) {
        if (knomi_gif((knomi_gif_slot_t)s)) {
            if (n == 0) return (knomi_gif_slot_t)s;
            n--;
        }
    }
    return GIF_SLOT_IDLE1;
}

void knomi_gif_request_reload(knomi_gif_slot_t slot) {
    __atomic_fetch_or(&pending_reload, (uint32_t)1 << slot, __ATOMIC_SEQ_CST);
}

void knomi_gif_process(void) {
    uint32_t pending = __atomic_exchange_n(&pending_reload, 0, __ATOMIC_SEQ_CST);
    if (!pending) return;
    for (int s = 0; s < GIF_SLOT_NUM; s++) {
        if (!(pending & ((uint32_t)1 << s))) continue;
        lv_img_dsc_t * old = custom[s];
        if (old) custom_total -= old->data_size;
        custom[s] = load_custom((knomi_gif_slot_t)s, GIF_MAX_TOTAL_SIZE - custom_total);
        if (custom[s]) custom_total += custom[s]->data_size;
        // re-point everything before the old data goes away
        reshow_all();
        free_custom(old);
    }
}

void knomi_gif_get_info(knomi_gif_slot_t slot, knomi_gif_info_t * info) {
    info->name = slot_def[slot].name;
    info->label = slot_def[slot].label;
    info->has_builtin = slot_def[slot].builtin != NULL;
    info->loaded = custom[slot] != NULL;
    info->has_custom = false;
    info->custom_size = 0;
    if (fs_ok) {
        String path = knomi_gif_path(slot);
        if (LittleFS.exists(path)) {
            File f = LittleFS.open(path, "r");
            if (f) { info->has_custom = true; info->custom_size = f.size(); f.close(); }
        }
    }
}
