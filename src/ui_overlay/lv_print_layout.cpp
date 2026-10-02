// Printing screen built from a layout designed on the web page (/layout).
//
// The layout is JSON in LittleFS (/layout.json), or the built-in default:
//   {"v":1,"pages":[{"s":30,"el":[ ... ]}, ...]}
// Pages rotate every "s" seconds (a tap skips to the next one). A page with "m":"e" is left out
// of the rotation and shown for "s" seconds when one of its triggers fires, or for as long as a
// condition holds:  "tr":{"pe":10, "pa":[25,50,75], "le":1, "st":1, "lm":10, "fl":1}
//   pe every N %, pa at these %, le every N layer changes, st print started   (events)
//   lm while less than N minutes left, fl during the first layer              (conditions)
// Elements:
//   text: {"t":"text","x":120,"y":80,"f":18,"c":"x","w":0,"a":"c","sc":0,"txt":"{pct}%"}
//   arc:  {"t":"arc","x":120,"y":120,"d":212,"w":20,"s":0,"e":360,"c":"t","b":"","p":1,"rd":0}
//   bar:  {"t":"bar","x":120,"y":200,"w":120,"h":8,"c":"t","b":"#333333","rd":1}
//   gif:  {"t":"gif","x":120,"y":120,"g":"print"}
//   face: {"t":"face","x":120,"y":120,"d":240}   (the coaster face, d px square)
// x/y are the element's center on the 240x240 screen. Angles: 0 = top, clockwise.
// Colors: "t" UI color, "x" text, "m" muted, "a" amber, or "#rrggbb".
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ui/ui.h"
#include "knomi.h"
#include "moonraker.h"
#include "knomi_gif.h"
#include "lv_overlay.h"
#include "../knomi_coaster.h"

#define LAYOUT_PATH      "/layout.json"
#define LAYOUT_MAX_PAGES 4
#define LAYOUT_MAX_EL    24   // per page
#define LAYOUT_MAX_BYTES 12288

extern const char layout_default_json[];
const char layout_default_json[] = R"json({"v":1,"pages":[{"s":10,"el":[
{"t":"arc","x":120,"y":120,"d":240,"w":2,"s":0,"e":360,"c":"t","b":"","p":0},
{"t":"arc","x":120,"y":120,"d":230,"w":4,"s":2,"e":132,"c":"t","b":"","p":0},
{"t":"arc","x":120,"y":120,"d":230,"w":4,"s":137,"e":224,"c":"t","b":"","p":0},
{"t":"arc","x":120,"y":120,"d":230,"w":4,"s":228,"e":358,"c":"t","b":"","p":0},
{"t":"arc","x":120,"y":120,"d":212,"w":20,"s":0,"e":360,"c":"t","b":"","p":1},
{"t":"text","x":120,"y":58,"f":14,"c":"m","w":150,"a":"c","sc":1,"txt":"{file}"},
{"t":"text","x":120,"y":102,"f":48,"c":"a","w":0,"a":"c","txt":"{pct}%"},
{"t":"text","x":120,"y":146,"f":18,"c":"x","w":0,"a":"c","txt":"{time}"},
{"t":"text","x":120,"y":174,"f":14,"c":"m","w":0,"a":"c","txt":"{noz}/{noz_t}{deg}   {bed}/{bed_t}{deg}"},
{"t":"text","x":120,"y":196,"f":14,"c":"m","w":0,"a":"c","txt":"{pos}"}]}]})json";

enum { EL_TEXT, EL_ARC, EL_BAR, EL_GIF, EL_FACE };

// parse in PSRAM, internal RAM is tight
struct SpiRamAllocator {
    void * allocate(size_t n) { return ps_malloc(n); }
    void deallocate(void * p) { free(p); }
    void * reallocate(void * p, size_t n) { return ps_realloc(p, n); }
};
typedef BasicJsonDocument<SpiRamAllocator> PsJsonDocument;

typedef struct {
    uint8_t type;
    bool progress;      // arc bound to progress
    lv_obj_t * obj;
    char * tpl;         // text template (PSRAM)
} layout_el_t;

typedef struct {
    lv_obj_t * cont;
    uint16_t secs;
    bool on_event;          // not in the rotation; shown by triggers
    uint8_t pct_every;      // 0 = off
    uint8_t pct_at[8];
    uint8_t pct_at_n;
    uint8_t layer_every;    // 0 = off
    bool on_start;
    uint16_t left_min;      // condition: time left below N minutes (0 = off)
    bool first_layer;       // condition: still on the first layer
    uint8_t n;
    layout_el_t el[LAYOUT_MAX_EL];
} layout_page_t;

static layout_page_t pages[LAYOUT_MAX_PAGES];
static uint8_t page_n = 0;
static uint8_t page_cur = 0;
static bool layout_visible = false;
static volatile bool reload_pending = false;
static uint32_t preview_until = 0;
static uint32_t preview_start = 0;
static uint32_t update_tick = 0;

// page scheduling
static int8_t rot_cur = -1;          // current rotation page
static uint32_t rot_next_ms = 0;
static int8_t event_page = -1;       // page shown because a trigger fired
static uint32_t event_until = 0;
// print tracking for triggers
static bool was_printing = false;
static int16_t last_pct = -1;
static uint16_t last_layer = 0;
static uint32_t layer_changes = 0;   // since the print started
static int32_t stable_z = INT32_MIN, cand_z = INT32_MIN;
static uint32_t cand_since = 0;

/* ---------------- tokens ---------------- */

static void fmt_duration(char * out, size_t n, uint32_t s) {
    if (s < 60) snprintf(out, n, "<1m");
    else if (s < 3600) snprintf(out, n, "%um", (unsigned)(s / 60));
    else snprintf(out, n, "%uh %02um", (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
}

// sample print for "preview on KNOMI": it moves (2 %/s, a layer every 1.5 s) so triggers can be seen
static moonraker_data_t demo_data(void) {
    moonraker_data_t d = {};
    uint32_t t = (millis() - preview_start) / 1000;
    d.printing = true;
    d.progress = (uint8_t)(40 + t * 2 > 100 ? 100 : 40 + t * 2);
    strlcpy(d.file_path, "benchy_0.2mm_PLA.gcode", sizeof(d.file_path));
    d.print_time = 2520 + t;
    d.time_left = 3480 - (int32_t)t * 90 > 0 ? 3480 - (int32_t)t * 90 : 0;
    d.nozzle_actual = 215; d.nozzle_target = 215;
    d.bed_actual = 60; d.bed_target = 60;
    d.layer = 42 + (millis() - preview_start) / 1500; d.layer_total = 240;
    d.z_um = d.layer * 200;
    return d;
}

bool print_layout_preview_active(void) {
    return preview_until && (int32_t)(preview_until - millis()) > 0;
}

// the data the screen shows: real, or sample values while previewing and not printing
static moonraker_data_t shown_data(void) {
    if (print_layout_preview_active() && !moonraker.data.printing) return demo_data();
    return moonraker.data;
}

static const char * token_names[] = {
    "pct", "left", "elapsed", "total", "time", "file", "noz", "noz_t", "bed", "bed_t",
    "z", "layer", "layers", "pos", "state", "deg", "msg", "eta",
};

// value of one token, false if the name is unknown
static bool token_value(const moonraker_data_t &d, const char * name, size_t len, char * out, size_t n) {
    #define IS(s) (len == strlen(s) && strncmp(name, s, len) == 0)
    char t[16];
    if (IS("pct")) snprintf(out, n, "%u", d.progress);
    else if (IS("left")) {
        if (d.time_left >= 0) fmt_duration(out, n, (uint32_t)d.time_left);
        else snprintf(out, n, "--");
    } else if (IS("elapsed")) fmt_duration(out, n, d.print_time);
    else if (IS("total")) {
        if (d.time_left >= 0) fmt_duration(out, n, d.print_time + (uint32_t)d.time_left);
        else snprintf(out, n, "--");
    } else if (IS("time")) {
        if (d.time_left >= 0) {   // "58m left, done 9:40 pm" once the clock is set (the fonts have no middle dot)
            char at[12];
            fmt_duration(t, sizeof(t), (uint32_t)d.time_left);
            if (coaster_clock_text(time(NULL) + d.time_left, at, sizeof(at))) snprintf(out, n, "%s left, done %s", t, at);
            else snprintf(out, n, "%s left", t);
        }
        else if (d.print_time > 0) { fmt_duration(t, sizeof(t), d.print_time); snprintf(out, n, "%s elapsed", t); }
        else snprintf(out, n, "starting");
    } else if (IS("file")) strlcpy(out, d.file_path, n);
    else if (IS("noz")) snprintf(out, n, "%d", d.nozzle_actual);
    else if (IS("noz_t")) snprintf(out, n, "%d", d.nozzle_target);
    else if (IS("bed")) snprintf(out, n, "%d", d.bed_actual);
    else if (IS("bed_t")) snprintf(out, n, "%d", d.bed_target);
    else if (IS("z")) {
        if (d.z_um == INT32_MIN) snprintf(out, n, "--");
        else snprintf(out, n, "%s%d.%02d", d.z_um < 0 ? "-" : "", (int)(abs(d.z_um) / 1000), (int)(abs(d.z_um) % 1000) / 10);
    } else if (IS("layer")) {
        if (d.layer_total) snprintf(out, n, "%u", d.layer); else snprintf(out, n, "--");
    } else if (IS("layers")) {
        if (d.layer_total) snprintf(out, n, "%u", d.layer_total); else snprintf(out, n, "--");
    } else if (IS("pos")) {
        if (d.layer_total > 0) snprintf(out, n, "Layer %u/%u", d.layer, d.layer_total);
        else if (d.z_um != INT32_MIN) snprintf(out, n, "Z %d.%02d", (int)(d.z_um / 1000), (int)(abs(d.z_um) % 1000) / 10);
        else out[0] = 0;
    } else if (IS("state")) {
        snprintf(out, n, "%s", !d.printing ? "Idle" : (d.pause || d.paused_ext) ? "Paused" : "Printing");
    } else if (IS("deg")) strlcpy(out, "\xe2\x84\x83", n);
    else if (IS("msg")) strlcpy(out, d.msg, n);
    else if (IS("eta")) {   // clock time the print should finish
        if (d.time_left < 0 || !coaster_clock_text(time(NULL) + d.time_left, out, n)) snprintf(out, n, "--");
    }
    else return false;
    #undef IS
    return true;
}

static void expand(const moonraker_data_t &d, const char * tpl, char * out, size_t n) {
    size_t o = 0;
    for (const char * p = tpl; *p && o + 1 < n; ) {
        if (*p == '{') {
            const char * e = strchr(p + 1, '}');
            char v[40];
            if (e && e - p <= 12 && token_value(d, p + 1, e - p - 1, v, sizeof(v))) {
                size_t l = strlen(v);
                if (o + l >= n) l = n - 1 - o;
                memcpy(out + o, v, l);
                o += l;
                p = e + 1;
                continue;
            }
        }
        out[o++] = *p++;
    }
    out[o] = 0;
}

// {"printing":true,"preview":false,"tokens":{"pct":"42",...}} for the designer's live preview
String print_layout_status_json(void) {
    moonraker_data_t d = moonraker.data;
    String s;
    s.reserve(1024);
    s += "{\"printing\":";
    s += d.printing ? "true" : "false";
    s += ",\"tokens\":{";
    for (size_t i = 0; i < sizeof(token_names) / sizeof(token_names[0]); i++) {
        char v[40];
        token_value(d, token_names[i], strlen(token_names[i]), v, sizeof(v));
        if (i) s += ",";
        s += "\"";
        s += token_names[i];
        s += "\":\"";
        for (const char * c = v; *c; c++) {
            if ((uint8_t)*c < 0x20) { s += ' '; continue; }   // a tab or newline in a name would break the JSON
            if (*c == '"' || *c == '\\') s += '\\';
            s += *c;
        }
        s += "\"";
    }
    s += "}}";
    return s;
}

/* ---------------- build ---------------- */

static lv_color_t parse_color(const char * c, lv_color_t def) {
    if (!c || !*c) return def;
    if (c[0] == '#' && strlen(c) == 7) return lv_color_hex(strtoul(c + 1, NULL, 16));
    switch (c[0]) {
        case 't': return lv_theme_color();
        case 'x': return lv_color_hex(0xE7EEF4);
        case 'm': return lv_color_hex(0x93A4B2);
        case 'a': return lv_color_hex(0xFFD164);
    }
    return def;
}

static const lv_font_t * pick_font(int size) {
    if (size >= 40) return &ui_font_InterSemiBold48;
    if (size >= 28) return &ui_font_InterSeimiBold32;
    if (size >= 22) return &ui_font_InterSemiBold24;
    if (size >= 19) return &ui_font_InterSemiBold20;
    if (size >= 17) return &ui_font_InterSemiBold18;
    if (size >= 15) return &ui_font_InterSemiBold16;
    return &ui_font_InterSemiBold14;
}

static void no_click(lv_obj_t * o) {
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE);
}

static void place(lv_obj_t * o, JsonObjectConst e) {
    int x = constrain((int)(e["x"] | 120), -40, 280);
    int y = constrain((int)(e["y"] | 120), -40, 280);
    lv_obj_align(o, LV_ALIGN_CENTER, x - 120, y - 120);
}

static void build_el(layout_page_t & pg, JsonObjectConst e) {
    if (pg.n >= LAYOUT_MAX_EL) return;
    const char * t = e["t"] | "";
    layout_el_t & el = pg.el[pg.n];
    el = {};
    lv_color_t col = parse_color(e["c"] | "x", lv_color_hex(0xE7EEF4));

    if (strcmp(t, "text") == 0) {
        lv_obj_t * l = lv_label_create(pg.cont);
        no_click(l);
        lv_obj_set_style_text_font(l, pick_font(e["f"] | 18), 0);
        lv_obj_set_style_text_color(l, col, 0);
        const char * a = e["a"] | "c";
        lv_obj_set_style_text_align(l, a[0] == 'l' ? LV_TEXT_ALIGN_LEFT : a[0] == 'r' ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER, 0);
        int w = constrain((int)(e["w"] | 0), 0, 240);
        if (w > 0) {
            lv_obj_set_width(l, w);
            lv_label_set_long_mode(l, (e["sc"] | 0) ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_DOT);
        }
        lv_label_set_text(l, "");
        const char * txt = e["txt"] | "";
        size_t len = min(strlen(txt), (size_t)120);
        el.tpl = (char *)ps_malloc(len + 1);
        if (el.tpl) { memcpy(el.tpl, txt, len); el.tpl[len] = 0; }
        place(l, e);
        el.type = EL_TEXT;
        el.obj = l;
    } else if (strcmp(t, "arc") == 0) {
        lv_obj_t * a = lv_arc_create(pg.cont);
        no_click(a);
        int d = constrain((int)(e["d"] | 212), 10, 240);
        int w = constrain((int)(e["w"] | 10), 1, d / 2);
        lv_obj_set_size(a, d, d);
        lv_obj_remove_style(a, NULL, LV_PART_KNOB);
        lv_obj_set_style_pad_all(a, 0, LV_PART_KNOB);
        lv_obj_set_style_pad_all(a, 0, LV_PART_MAIN);       // stroke touches the edge, like the web preview
        lv_obj_set_style_pad_all(a, 0, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(a, 0, LV_PART_KNOB);
        lv_arc_set_rotation(a, 270);   // 0 = top
        int s = constrain((int)(e["s"] | 0), 0, 360);
        int en = constrain((int)(e["e"] | 360), 0, 360);
        lv_arc_set_bg_angles(a, s, en);
        lv_arc_set_range(a, 0, 100);
        bool rd = e["rd"] | 0;
        const char * b = e["b"] | "";
        lv_obj_set_style_arc_width(a, w, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(a, rd, LV_PART_MAIN);
        lv_obj_set_style_arc_color(a, parse_color(b, lv_color_black()), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(a, *b ? 255 : 0, LV_PART_MAIN);
        lv_obj_set_style_arc_width(a, w, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(a, rd, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(a, col, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(a, 255, LV_PART_INDICATOR);
        el.progress = e["p"] | 0;
        lv_arc_set_value(a, el.progress ? 0 : 100);
        place(a, e);
        el.type = EL_ARC;
        el.obj = a;
    } else if (strcmp(t, "bar") == 0) {
        lv_obj_t * br = lv_bar_create(pg.cont);
        no_click(br);
        lv_obj_set_size(br, constrain((int)(e["w"] | 120), 4, 240), constrain((int)(e["h"] | 8), 2, 120));
        lv_bar_set_range(br, 0, 100);
        const char * b = e["b"] | "#2A3440";
        bool rd = e["rd"] | 1;
        lv_coord_t r = rd ? LV_RADIUS_CIRCLE : 0;
        lv_obj_set_style_radius(br, r, LV_PART_MAIN);
        lv_obj_set_style_radius(br, r, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(br, parse_color(b, lv_color_hex(0x2A3440)), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(br, *b ? 255 : 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(br, col, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(br, 255, LV_PART_INDICATOR);
        lv_obj_set_style_anim_time(br, 0, LV_PART_MAIN);
        place(br, e);
        el.type = EL_BAR;
        el.progress = true;
        el.obj = br;
    } else if (strcmp(t, "gif") == 0) {
        int slot = knomi_gif_slot_by_name(e["g"] | "print");
        if (slot < 0) slot = GIF_SLOT_PRINT;
        if (!knomi_gif((knomi_gif_slot_t)slot)) {
            // no GIF in that slot any more (Coaster acts it out): put Coaster there instead
            lv_obj_t * f = coaster_create(pg.cont, 150);
            place(f, e);
            el.type = EL_FACE;
            el.obj = f;
            pg.n++;
            return;
        }
        lv_obj_t * g = lv_gif_create(pg.cont);
        no_click(g);
        knomi_gif_show(g, (knomi_gif_slot_t)slot);
        place(g, e);
        el.type = EL_GIF;
        el.obj = g;
    } else if (strcmp(t, "face") == 0) {
        lv_obj_t * f = coaster_create(pg.cont, constrain((int)(e["d"] | 240), 60, 240));
        place(f, e);
        el.type = EL_FACE;
        el.obj = f;
    } else {
        return;
    }
    pg.n++;
}

static void destroy(void) {
    for (int p = 0; p < page_n; p++) {
        for (int i = 0; i < pages[p].n; i++) {
            layout_el_t & el = pages[p].el[i];
            if (el.type == EL_GIF) knomi_gif_forget(el.obj);
            free(el.tpl);
            el.tpl = NULL;
        }
        if (pages[p].cont) lv_obj_del(pages[p].cont);
        pages[p].cont = NULL;
        pages[p].n = 0;
    }
    page_n = 0;
}

static bool build_from(const char * json, size_t len) {
    PsJsonDocument doc(49152);
    if (deserializeJson(doc, json, len) != DeserializationError::Ok) return false;
    JsonArrayConst ps = doc["pages"];
    if (ps.isNull() || ps.size() == 0) return false;
    destroy();
    for (JsonObjectConst p : ps) {
        if (page_n >= LAYOUT_MAX_PAGES) break;
        layout_page_t & pg = pages[page_n];
        pg.n = 0;
        pg.secs = constrain((int)(p["s"] | 10), 1, 3600);
        pg.on_event = strcmp(p["m"] | "r", "e") == 0;
        JsonObjectConst tr = p["tr"];
        pg.pct_every = constrain((int)(tr["pe"] | 0), 0, 50);
        pg.pct_at_n = 0;
        for (JsonVariantConst v : tr["pa"].as<JsonArrayConst>()) {
            if (pg.pct_at_n < sizeof(pg.pct_at)) pg.pct_at[pg.pct_at_n++] = constrain(v.as<int>(), 1, 100);
        }
        pg.layer_every = constrain((int)(tr["le"] | 0), 0, 250);
        pg.on_start = tr["st"] | 0;
        pg.left_min = constrain((int)(tr["lm"] | 0), 0, 1440);
        pg.first_layer = tr["fl"] | 0;
        pg.cont = lv_obj_create(ui_ScreenPrinting);
        lv_obj_remove_style_all(pg.cont);
        lv_obj_set_size(pg.cont, 240, 240);
        lv_obj_center(pg.cont);
        no_click(pg.cont);
        lv_obj_add_flag(pg.cont, LV_OBJ_FLAG_HIDDEN);
        for (JsonObjectConst e : p["el"].as<JsonArrayConst>()) build_el(pg, e);
        page_n++;
    }
    page_cur = 0;
    rot_cur = -1;
    rot_next_ms = 0;
    event_page = -1;
    return true;
}

// Check a layout before saving it (web task). Returns an error message or NULL.
const char * print_layout_validate(const char * json, size_t len) {
    if (len > LAYOUT_MAX_BYTES) return "Layout too large";
    PsJsonDocument doc(49152);
    if (deserializeJson(doc, json, len) != DeserializationError::Ok) return "Not valid JSON";
    JsonArrayConst ps = doc["pages"];
    if (ps.isNull() || ps.size() == 0) return "No pages";
    if (ps.size() > LAYOUT_MAX_PAGES) return "Too many pages (max 4)";
    for (JsonObjectConst p : ps) {
        if (p["el"].as<JsonArrayConst>().size() > LAYOUT_MAX_EL) return "Too many elements on a page (max 24)";
    }
    return NULL;
}

static void load(void) {
    bool ok = false;
    if (LittleFS.exists(LAYOUT_PATH)) {
        File f = LittleFS.open(LAYOUT_PATH, "r");
        size_t n = f ? f.size() : 0;
        if (n > 0 && n <= LAYOUT_MAX_BYTES) {
            char * buf = (char *)ps_malloc(n);
            if (buf && f.read((uint8_t *)buf, n) == n) ok = build_from(buf, n);
            free(buf);
        }
        if (f) f.close();
        if (!ok) Serial.println("layout.json invalid, using the default layout");
    }
    if (!ok) build_from(layout_default_json, strlen(layout_default_json));
}

/* ---------------- runtime ---------------- */

static void set_hidden(lv_obj_t * o, bool hidden) {
    if (!o) return;
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// run the GIF timers only for the page on screen
static void sync_gif_timers(void) {
    bool on_screen = layout_visible && lv_scr_act() == ui_ScreenPrinting;
    for (int p = 0; p < page_n; p++) {
        bool run = on_screen && p == page_cur;
        for (int i = 0; i < pages[p].n; i++) {
            if (pages[p].el[i].type != EL_GIF) continue;
            lv_gif_t * g = (lv_gif_t *)pages[p].el[i].obj;
            if (!g->timer) continue;
            if (run && g->timer->paused) lv_timer_resume(g->timer);
            if (!run && !g->timer->paused) lv_timer_pause(g->timer);
        }
    }
}

static void show_page(uint8_t p) {
    page_cur = p < page_n ? p : 0;
    for (int i = 0; i < page_n; i++) set_hidden(pages[i].cont, !layout_visible || i != page_cur);
    update_tick = millis();  // fill in the new page's values on the next update
    sync_gif_timers();
}

void print_layout_set_visible(bool visible) {
    layout_visible = visible;
    show_page(page_cur);
}

static int next_rotation_page(int from) {
    for (int i = 1; i <= page_n; i++) {
        int p = (from + i + page_n) % page_n;
        if (!pages[p].on_event) return p;
    }
    return -1;
}

// tap: drop any event page and go to the next page in the rotation
void print_layout_next_page(void) {
    event_page = -1;
    int n = next_rotation_page(rot_cur < 0 ? page_n - 1 : rot_cur);
    if (n >= 0) {
        rot_cur = n;
        rot_next_ms = millis() + pages[n].secs * 1000UL;
        show_page(n);
    }
}

static void fire(int p) {
    event_page = p;
    event_until = millis() + pages[p].secs * 1000UL;
}

// Layer changes: the printer's layer number when it has one (Moonraker, some OctoPrint
// setups), otherwise a new higher Z that holds for 1.5 s (ignores z-hops).
static bool layer_changed(const moonraker_data_t & d) {
    if (d.layer_total > 0 && d.layer > 0) {
        bool ch = last_layer && d.layer != last_layer;
        last_layer = d.layer;
        return ch;
    }
    if (d.z_um == INT32_MIN) return false;
    if (d.z_um != cand_z) { cand_z = d.z_um; cand_since = millis(); return false; }
    if (millis() - cand_since < 1500 || cand_z == stable_z) return false;
    bool ch = stable_z != INT32_MIN && cand_z > stable_z;
    stable_z = cand_z;
    return ch;
}

static void track_print(const moonraker_data_t & d) {
    if (!d.printing) { was_printing = false; return; }
    if (!was_printing) {
        was_printing = true;
        last_pct = -1; last_layer = 0; layer_changes = 0;
        stable_z = cand_z = INT32_MIN;
        for (int p = 0; p < page_n; p++) if (pages[p].on_event && pages[p].on_start) fire(p);
    }
    int pct = d.progress;
    if (last_pct >= 0 && pct > last_pct) {
        for (int p = 0; p < page_n; p++) {
            const layout_page_t & pg = pages[p];
            if (!pg.on_event) continue;
            if (pg.pct_every && pct / pg.pct_every > last_pct / pg.pct_every) fire(p);
            for (int i = 0; i < pg.pct_at_n; i++)
                if (last_pct < pg.pct_at[i] && pct >= pg.pct_at[i]) fire(p);
        }
    }
    last_pct = pct;   // also resets if the percentage went down (new job)
    if (layer_changed(d)) {
        layer_changes++;
        for (int p = 0; p < page_n; p++)
            if (pages[p].on_event && pages[p].layer_every && layer_changes % pages[p].layer_every == 0) fire(p);
    }
}

static bool condition_holds(const layout_page_t & pg, const moonraker_data_t & d) {
    if (!d.printing) return false;
    if (pg.left_min && d.time_left >= 0 && d.time_left < (int32_t)pg.left_min * 60) return true;
    if (pg.first_layer) {
        if (d.layer_total > 0 && d.layer > 0) return d.layer <= 1;
        // without a layer number we go by Z. No Z either (e.g. printing from Klipper's
        // virtual SD card, OctoPrint then reports no currentZ): can't tell, so don't hold
        // the page for the whole print. A first layer longer than 20 minutes is a guess gone wrong.
        if (d.z_um == INT32_MIN && stable_z == INT32_MIN) return false;
        return layer_changes == 0 && d.print_time > 0 && d.print_time < 20 * 60;
    }
    return false;
}

// which page should be on screen now
static int pick_page(const moonraker_data_t & d) {
    for (int p = 0; p < page_n; p++)
        if (pages[p].on_event && condition_holds(pages[p], d)) return p;
    if (event_page >= 0 && (int32_t)(millis() - event_until) < 0) return event_page;
    event_page = -1;
    if (rot_cur < 0 || pages[rot_cur].on_event || (int32_t)(millis() - rot_next_ms) >= 0) {
        int n = next_rotation_page(rot_cur < 0 ? page_n - 1 : rot_cur);
        if (n < 0) return 0;   // only event pages: fall back to the first one
        rot_next_ms = millis() + pages[n].secs * 1000UL;
        rot_cur = n;
    }
    return rot_cur;
}

static void screen_tap_cb(lv_event_t * e) {
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_SHORT_CLICKED) { if (layout_visible) print_layout_next_page(); }
    else coaster_poke();   // hold anywhere on the print screen: tickle Coaster
}

void print_layout_init(void) {
    load();
    lv_obj_add_event_cb(ui_ScreenPrinting, screen_tap_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ScreenPrinting, screen_tap_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(ui_ScreenPrinting, screen_tap_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
}

void print_layout_request_reload(void) { reload_pending = true; }

void print_layout_preview(uint16_t secs) {
    preview_start = millis();
    preview_until = millis() + secs * 1000UL;
    if (!preview_until) preview_until = 1;
}

void print_layout_update(void) {
    if (reload_pending) {
        reload_pending = false;
        load();
        show_page(0);
    }
    if (preview_until && !print_layout_preview_active()) preview_until = 0;

    if ((int32_t)(millis() - update_tick) < 0) return;
    update_tick = millis() + 250;

    sync_gif_timers();  // a GIF reload/retint restarts timers
    moonraker_data_t d = shown_data();
    track_print(d);     // triggers count even while another screen is showing
    if (!layout_visible || lv_scr_act() != ui_ScreenPrinting || !page_n) return;

    int want = pick_page(d);
    if (want != page_cur) show_page(want);
    layout_page_t & pg = pages[page_cur];
    char buf[160];
    for (int i = 0; i < pg.n; i++) {
        layout_el_t & el = pg.el[i];
        switch (el.type) {
            case EL_TEXT:
                if (!el.tpl) break;
                expand(d, el.tpl, buf, sizeof(buf));
                if (strcmp(lv_label_get_text(el.obj), buf) != 0) lv_label_set_text(el.obj, buf);
                break;
            case EL_ARC:
                if (el.progress && lv_arc_get_value(el.obj) != d.progress) lv_arc_set_value(el.obj, d.progress);
                break;
            case EL_BAR:
                if (lv_bar_get_value(el.obj) != d.progress) lv_bar_set_value(el.obj, d.progress, LV_ANIM_OFF);
                break;
        }
    }
}
