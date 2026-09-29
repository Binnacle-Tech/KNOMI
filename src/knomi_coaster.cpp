// Coaster face engine and renderer. Port of the web mock-up (coaster-face.html):
// same springs, same moods, same one-face morphing expressions.
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "knomi.h"
#include "moonraker.h"
#include "knomi_power.h"
#include "knomi_ble.h"
#include "knomi_gif.h"
#include <time.h>
#include "knomi_coaster.h"
#include "ui_overlay/lv_overlay.h"
#include "ui/ui.h"

#define SAMPLE_DT   0.005f   // sensor runs at 200 Hz
#define FRAME_MS    33       // redraw ~30 fps
#define MAX_FACES   6

/* ---------------- tuning ---------------- */

typedef struct {
    float wobble, settle, sense, habit, scare, dizzy, sleep;
    bool idle;
    uint8_t deco;       // DECO_AUTO, DECO_OFF or one decoration (see DECO_KEYS)
    uint8_t lights;     // holiday light colors (LIGHT_KEYS)
    uint8_t anim;       // holiday light effect (ANIM_KEYS)
    bool south;         // southern hemisphere: weather seasons flip
    uint8_t bday_m, bday_d;
    int16_t tz_min;     // offset from UTC, from the browser that saved the settings
} coaster_tune_t;
// decorations: "auto" follows the date, "off", or one picked by hand
enum { DECO_AUTO, DECO_OFF, D_HOLIDAYS, D_NEWYEAR, D_WINTER, D_VALENTINE, D_SPRING, D_SUMMER, D_JULY4,
       D_AUTUMN, D_HALLOWEEN, D_BIRTHDAY, DECO_COUNT };
static const char * DECO_KEYS[DECO_COUNT] = {"auto", "off", "holidays", "newyear", "winter", "valentine", "spring",
                                             "summer", "july4", "autumn", "halloween", "birthday"};
static const char * LIGHT_KEYS[] = {"classic", "warm", "theme", "candy", "rainbow"};
static const char * ANIM_KEYS[] = {"twinkle", "chase", "breathe", "steady"};
static int key_index(const char * v, const char * const * keys, int n, int fallback) {
    if (v) for (int i = 0; i < n; i++) if (strcmp(v, keys[i]) == 0) return i;
    return fallback;
}
static const coaster_tune_t TUNE_DEF = {2.0f, 0.2f, 1.0f, 20.0f, 0.6f, 5.0f, 20.0f, true, DECO_AUTO, 0, 0, false, 9, 28, 0}; // Coaster is the mascot: on by default
// OP18: sensitivity 1.0 now moves the head as much as 3.0 did before (tuned on recorded prints)
#define SENSE_K  3.0f
#define BOUNCE   6.0f    // px per compressed g: the head jiggles along with the toolhead
static coaster_tune_t T = TUNE_DEF;
static volatile bool reload_pending = false;

static void load_tuning(void) {
    T = TUNE_DEF;
    File f = LittleFS.open(COASTER_PATH, "r");
    if (!f) return;
    StaticJsonDocument<768> d;
    if (deserializeJson(d, f) == DeserializationError::Ok) {
        T.wobble = constrain(d["wobble"] | T.wobble, 0.8f, 6.0f);
        T.settle = constrain(d["settle"] | T.settle, 0.05f, 1.0f);
        float sn = d["sense"] | T.sense;
        if (!d.containsKey("v") && d.containsKey("sense")) sn /= SENSE_K;   // saved before OP18
        T.sense  = constrain(sn, 0.2f, 3.0f);
        T.habit  = constrain(d["habit"]  | T.habit,  2.0f, 60.0f);
        T.scare  = constrain(d["scare"]  | T.scare,  0.3f, 2.5f);
        T.dizzy  = constrain(d["dizzy"]  | T.dizzy,  1.0f, 20.0f);
        T.sleep  = constrain(d["sleep"]  | T.sleep,  5.0f, 120.0f);
        T.idle   = d["idle"] | true;
        T.deco   = key_index(d["deco"] | (const char *)NULL, DECO_KEYS, DECO_COUNT, DECO_AUTO);
        if (!d.containsKey("deco") && d.containsKey("hat")) {   // saved before decorations: 1 off, 2 party, 3 Santa, 4 witch
            static const uint8_t from_hat[] = {DECO_AUTO, DECO_OFF, D_NEWYEAR, D_HOLIDAYS, D_HALLOWEEN};
            T.deco = from_hat[constrain((int)(d["hat"] | 0), 0, 4)];
        }
        T.lights = key_index(d["lights"] | (const char *)NULL, LIGHT_KEYS, 5, 0);
        T.anim   = key_index(d["anim"] | (const char *)NULL, ANIM_KEYS, 4, 0);
        T.south  = strcmp(d["hemi"] | "n", "s") == 0;
        int bm = 0, bd = 0;
        if (sscanf(d["bday"] | "", "%d-%d", &bm, &bd) == 2 && bm >= 1 && bm <= 12 && bd >= 1 && bd <= 31) { T.bday_m = bm; T.bday_d = bd; }
        T.tz_min = constrain((int)(d["tz"] | 0), -840, 840);
    }
    f.close();
}

bool coaster_idle_enabled(void) { return true; }   // every face is Coaster now

// Web task: save tuning from the /coaster page (unknown keys dropped, values clamped on load)
const char * coaster_save_json(const char * json, size_t len) {
    if (len > 1024) return "Too large";
    StaticJsonDocument<512> in;
    if (deserializeJson(in, json, len) != DeserializationError::Ok) return "Not valid JSON";
    StaticJsonDocument<768> out;
    static const char * keys[] = {"wobble", "settle", "sense", "habit", "scare", "dizzy", "sleep"};
    for (const char * k : keys) if (in[k].is<float>()) out[k] = in[k].as<float>();
    // decorations (strings are checked against the known keys when loaded)
    for (const char * k : {"deco", "lights", "anim", "hemi", "bday"})
        if (in[k].is<const char *>()) out[k] = String(in[k].as<const char *>()).substring(0, 12);
    if (in["tz"].is<int>()) out["tz"] = constrain(in["tz"].as<int>(), -840, 840);
    out["v"] = 2;
    out["idle"] = in.containsKey("idle") ? (bool)(in["idle"] | false) : T.idle;
    File f = LittleFS.open(COASTER_PATH, "w");
    if (!f) return "Couldn't write to flash";
    serializeJson(out, f);
    f.close();
    coaster_request_reload();
    return NULL;
}

// Web task: the settings page's "Idle screen" choice
void coaster_set_idle(bool on) {
    StaticJsonDocument<768> d;
    File f = LittleFS.open(COASTER_PATH, "r");
    if (f) { deserializeJson(d, f); f.close(); }
    d["idle"] = on;
    if (!d.containsKey("v") && d.containsKey("sense")) d["sense"] = (d["sense"].as<float>()) / SENSE_K;
    d["v"] = 2;
    f = LittleFS.open(COASTER_PATH, "w");
    if (f) { serializeJson(d, f); f.close(); }
    T.idle = on;   // takes effect right away; the rest reloads on the LVGL task
    coaster_request_reload();
}

String coaster_tuning_json(void) {
    char buf[320];
    snprintf(buf, sizeof(buf),
             "{\"wobble\":%.2f,\"settle\":%.2f,\"sense\":%.2f,\"habit\":%.0f,\"scare\":%.2f,\"dizzy\":%.1f,\"sleep\":%.0f,\"idle\":%s,"
             "\"deco\":\"%s\",\"lights\":\"%s\",\"anim\":\"%s\",\"hemi\":\"%s\",\"bday\":\"%02u-%02u\",\"tz\":%d}",
             T.wobble, T.settle, T.sense, T.habit, T.scare, T.dizzy, T.sleep, T.idle ? "true" : "false",
             DECO_KEYS[T.deco], LIGHT_KEYS[T.lights], ANIM_KEYS[T.anim], T.south ? "s" : "n", T.bday_m, T.bday_d, T.tz_min);
    return String(buf);
}
void coaster_request_reload(void) { reload_pending = true; }

/* ---------------- samples from the sensor task ---------------- */

#define RING 64
static float ring[RING][3];
static volatile uint32_t ring_w = 0;
static uint32_t ring_r = 0;

void coaster_push_sample(float x, float y, float z) {
    uint32_t w = ring_w;
    ring[w % RING][0] = x; ring[w % RING][1] = y; ring[w % RING][2] = z;
    ring_w = w + 1;
}

/* ---------------- state ---------------- */

static float frand(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

enum {
    M_CALM, M_RIDING, M_EXCITED, M_SCREAM, M_STARTLED, M_ELEVATOR, M_SLEEPY,
    M_BORED, M_SHIVER, M_DIZZY, M_GIGGLE, M_CELEBRATE, M_READY,
    M_SAD, M_ERROR, M_LONELY, M_CONFUSED, M_HEATING, M_COOLING, M_FOCUS, M_ANTICIPATE,
    M_HUNGRY, M_WINDY, M_NERVOUS, M_BRACE, M_LEVEL, M_SCRUB, M_WHEE, M_MAD, M_COUNT
};
static const char * MOOD_NAMES[M_COUNT] = {
    "calm", "riding", "excited", "screaming", "startled", "elevator", "sleepy",
    "bored", "shivering", "dizzy", "giggle", "celebrate", "ready",
    "sad", "shocked", "lonely", "confused", "heating up", "cooling off", "focused", "almost there",
    "hungry", "windy", "hanging on", "bracing", "leveling", "scrubbing", "whee", "mad",
};

// sensing
static float lp[3], mfl[3], hist[6][3];
static uint8_t hist_i = 0;
static float env = 0, base = 0, vib = 0, step = 0, thrill = 0, scream_t = 0, dizzy_meter = 0, still_t = 0;
static bool from_rest = false;
static float act2 = 0, motion = 0, quiet_t = 0, t_whee = 0;   // short RMS of motion, time since it was quiet
static float bx[3];              // compressed motion for the head bounce
// mood
static int mood = M_CALM;
static float mood_t = 0;
static float t_startle = 0, t_dizzy = 0, t_giggle = 0, t_celebrate = 0, t_ready = 0, t_sad = 0;
static uint32_t cool_until = 0;   // "cooling off" after a print, up to 10 minutes
// body
static float hx = 0, hy = 0, hvx = 0, hvy = 0, hs = 0, hvs = 0;
static float px_ = 0, py_ = 0, pvx = 0, pvy = 0;
// expression
typedef struct { float open, size, cheek, orbit, w, curve, omega, gape, zig, wave, tilt; } expr_t;
// same order as the enum above
static const expr_t MOODS[M_COUNT] = {
    /* calm      */ {0.5f,  1.0f,  0, 0, 14, 0.0f, 1.0f, 0.0f, 0, 0, 0},
    /* riding    */ {0.56f, 1.0f,  0, 0, 14, 0.3f, 0.7f, 0.0f, 0, 0, 0},
    /* excited   */ {0.85f, 1.05f, 0, 0, 12, 0.7f, 0.0f, 0.9f, 0, 0, 0},
    /* screaming */ {1.0f,  1.3f,  0, 0,  8, 0.0f, 0.0f, 1.7f, 0, 0, 0},
    /* startled  */ {1.0f,  1.2f,  0, 0,  6, 0.0f, 0.0f, 1.1f, 0, 0, 0},
    /* elevator  */ {0.75f, 1.05f, 0, 0,  7, 0.0f, 0.0f, 0.6f, 0, 0, 0},
    /* sleepy    */ {0.02f, 1.0f,  0, 0, 11, 0.0f, 1.0f, 0.0f, 0, 0, 0},
    /* bored     */ {0.32f, 1.0f,  0, 0,  9, 0.0f, 0.0f, 0.0f, 0, 0, 0},
    /* shivering */ {0.42f, 1.0f,  0, 0, 14, 0.0f, 0.0f, 0.0f, 1, 0, 0},
    /* dizzy     */ {0.5f,  1.0f,  0, 1, 14, 0.0f, 0.0f, 0.0f, 0, 1, 0},
    /* giggle    */ {1.0f,  1.05f, 1, 0, 12, 0.8f, 0.0f, 0.8f, 0, 0, 0},
    /* celebrate */ {1.0f,  1.1f,  1, 0, 15, 0.9f, 0.0f, 1.0f, 0, 0, 0},
    /* ready     */ {1.0f,  1.1f,  0, 0, 13, 0.8f, 0.0f, 0.5f, 0, 0, 0},
    /* sad       */ {0.35f, 1.0f,  0, 0, 12, -0.9f, 0.0f, 0.0f, 0, 0, -0.7f},
    /* shocked   */ {1.0f,  0.75f, 0, 0, 10, 0.0f, 0.0f, 0.35f, 0, 0.8f, -0.4f},
    /* lonely    */ {0.45f, 1.0f,  0, 0,  9, -0.6f, 0.0f, 0.0f, 0, 0, -0.5f},
    /* confused  */ {0.55f, 1.0f,  0, 0, 10, 0.0f, 0.0f, 0.0f, 0, 0.5f, 0.3f},
    /* impatient */ {0.42f, 0.95f, 0, 0, 13, 0.0f, 0.0f, 0.0f, 0.8f, 0, 1.0f},  // straining to heat the hotend
    /* cooling   */ {0.3f,  1.0f,  0, 0, 12, 0.5f, 0.4f, 0.0f, 0, 0, 0},
    /* focused   */ {0.28f, 0.9f,  0, 0,  7, 0.0f, 0.0f, 0.0f, 0, 0, 0},
    /* almost    */ {0.8f,  1.05f, 0, 0, 12, 0.5f, 0.0f, 0.2f, 0, 0, 0},
    /* hungry    */ {0.7f,  1.0f,  0, 0,  8, 0.0f, 0.0f, 0.9f, 0, 0, 0},
    /* windy     */ {0.3f,  1.0f,  0, 0, 10, 0.0f, 0.0f, 0.0f, 0, 0.6f, 0},
    /* nervous   */ {0.9f,  0.9f,  0, 0, 10, 0.0f, 0.0f, 0.0f, 0.6f, 0, -0.3f},   // "hanging on"
    /* bracing   */ {0.08f, 1.0f,  0, 0, 10, 0.0f, 0.0f, 0.0f, 0.5f, 0, -0.3f},
    /* leveling  */ {0.5f,  1.0f,  0, 0, 12, 0.0f, 0.0f, 0.0f, 0, 0, 0},
    /* scrubbing */ {0.8f,  1.0f,  0.6f, 0, 12, 0.5f, 0.0f, 0.0f, 0, 0.6f, 0},
    /* whee      */ {1.0f,  1.15f, 0, 0, 13, 0.8f, 0.0f, 1.3f, 0, 0, 0},   // a burst of motion after a pause
    /* mad       */ {0.36f, 0.95f, 0, 0, 12, -0.35f, 0.0f, 0.0f, 0.3f, 0, 1.0f},   // brows down, narrow eyes
};
static expr_t E = MOODS[M_CALM];
static float look = 0, wander_x = 0, wander_tx = 0, wander_t = 0;
static float blink_t = 3, blink_closing = 0;
static float now_s = 0;
// printer
static bool was_printing = false;
static uint8_t last_progress = 0;
static float vz = 0;              // Z speed from the printer's reported Z, mm/s
static int32_t zhist[7];
static uint32_t zhist_ms = 0;
static uint8_t zhist_n = 0;
// confetti
typedef struct { float x, y, vx, vy, r, vr, life; } bit_t;
static bit_t confetti[40];
static uint8_t confetti_n = 0;

static void blink_now(void) { blink_closing = 0.16f; }
static float heat_effort = 0;     // 0..1 how hard it's working (nozzle heating counts double)
static float t_phew = 0;          // relief when the heater reaches temperature
static bool was_heating = false;
// this print's thrills, for the report card after it
typedef struct { bool valid, done; uint8_t progress; uint16_t screams, dizzies, jolts; float peak; uint32_t secs; } report_t;
static report_t stats = {}, report = {};
// busy state Coaster is acting out (set from the screen logic)
enum { ACT_NONE, ACT_HOMING, ACT_PROBING, ACT_QGL, ACT_SHAPING, ACT_PID, ACT_CLEANING, ACT_FILAMENT, ACT_START, ACT_DONE, ACT_REPORT };
static int act = ACT_NONE;
static const char * ACT_LABEL[] = {"", "Homing", "Probing", "Leveling gantry", "Input shaping", "PID tuning",
                                   "Cleaning nozzle", "Filament", "Starting print", "Done!", ""};
// speech bubble for display messages (M117 and friends)
static uint16_t bubble_msg_id = 0;
static uint32_t bubble_since = 0, bubble_until = 0;
static float bubble_k = 0;        // 0 hidden .. 1 shown (eased)

/* ---------------- simulation ---------------- */

static void sense(const float a[3], float dt) {
    float k15 = 1 - expf(-2 * PI * 15 * dt);
    float hp = 0;
    for (int i = 0; i < 3; i++) { lp[i] += (a[i] - lp[i]) * k15; float h = a[i] - lp[i]; hp += h * h; }
    float mag = sqrtf(lp[0] * lp[0] + lp[1] * lp[1] + lp[2] * lp[2]);
    env = max(mag, env * expf(-dt / 0.3f));
    act2 += (mag * mag - act2) * (1 - expf(-dt / 0.12f));
    motion = sqrtf(act2);
    vib += (sqrtf(hp) - vib) * (1 - expf(-dt / 0.6f));
    // sudden step: change of the 40 Hz signal over 25 ms, starting from rest (endstop hits)
    float k40 = 1 - expf(-2 * PI * 40 * dt);
    for (int i = 0; i < 3; i++) mfl[i] += (a[i] - mfl[i]) * k40;
    const float * o = hist[hist_i];   // oldest of the last 6 samples
    step = sqrtf(sq(mfl[0] - o[0]) + sq(mfl[1] - o[1]) + sq(mfl[2] - o[2]));
    from_rest = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]) < 0.3f;
    memcpy(hist[hist_i], mfl, sizeof(mfl));
    hist_i = (hist_i + 1) % 6;
    base += (motion - base) * (1 - expf(-dt / max(1.0f, T.habit)));
    // a burst of motion after a pause (a travel, a new perimeter): "whee!"
    t_whee = max(0.0f, t_whee - dt);
    if (motion < 0.1f) quiet_t += dt;
    else if (motion > 0.15f) {
        if (quiet_t > 0.3f && t_whee <= 0) { t_whee = 0.5f; hvy -= 60 * T.sense; }
        quiet_t = 0;
    }
    thrill = max(0.0f, env - 0.7f * base) / T.scare;
    still_t = (env < 0.03f && vib < 0.03f) ? still_t + dt : 0;
}

static void head_kick(float vx, float vy) { hvx += vx; hvy += vy; }

/* ---------------- feelings: how Coaster feels over hours and days ---------------- */
// Moods above come and go in seconds. This is the slow layer under them: one number from
// -1 (miserable) to 1 (happy) that printing, finishing prints and attention raise, and being
// left off, getting dizzy, failed prints and heating up for nothing lower. It tints the calm
// face (smile or droop), changes which quirks it picks, and survives restarts.
#define FEEL_PATH "/coaster_feel.json"
static float H = 0.3f;
static int16_t streak = 0;          // prints finished in a row
static uint32_t prints_done = 0;
static time_t last_seen = 0;        // saved every 10 minutes: how long it was off
static bool feel_dirty = false, boot_judged = false, night = false;
static uint32_t feel_save_ms = 0;
static int reset_kind = 0;          // 0 plugged in, 1 restarted on purpose, 2 crashed / power dip
static float t_confused = 0, t_mad = 0, feel_acc = 0;
static uint32_t heat_idle_s = 0, idle_s = 0, unlinked_s = 0;
static bool mad_heat = false, was_runout = false;
typedef struct { char why[30]; int8_t d; } feel_note_t;   // d in hundredths
static feel_note_t fnotes[5];
static uint8_t fnotes_n = 0;

static void quirk_start(int q);
static void feel_boot_quirk(bool happy);
static int deco_for(int m, int d);

// ---- likes and dislikes: rolled once per Coaster, kept in its own file, not editable ----
// The first nine follow the decoration seasons (same order as D_HOLIDAYS..D_HALLOWEEN).
enum { LK_HOLIDAYS, LK_NEWYEAR, LK_WINTER, LK_VALENTINE, LK_SPRING, LK_SUMMER, LK_JULY4, LK_AUTUMN, LK_HALLOWEEN,
       LK_LONG, LK_SHORT, LK_FAST, LK_POKES, LK_NIGHT, LK_FANS, LK_HEAT, LK_QUIET, LK_COUNT };
static const char * LIKE_NAMES[LK_COUNT] = {"the holidays", "New Year", "winter", "Valentine's", "spring", "summer",
    "the 4th of July", "autumn", "Halloween", "long prints", "quick prints", "fast moves", "being poked",
    "late nights", "fans", "heat", "quiet time"};
static int8_t like[LK_COUNT];      // -100 hates .. 100 loves
static bool likes_rolled = false;
static int8_t season_seen = -1;     // last season it noticed, so a new one is an event once
static int16_t bday_year = 0;       // last birthday it celebrated
// ---- what it learns: habits from the prints you do, and tastes that grow from them ----
static int8_t learned[LK_COUNT];    // -50..50 on top of what it was born with
static float openness = 0;          // rolled once: 1 loves a change of pace, -1 creature of habit
static uint16_t habit_n = 0;        // prints it has learned from
static float habit_len = 0;         // average log2(print minutes)
static float habit_hx = 0, habit_hy = 0;   // average start hour, as a point on a 24 h circle
static float habit_peak = 0;        // average hardest move, g
static int eff(int k) { return constrain((int)like[k] + learned[k], -100, 100); }
static float L(int k) { return eff(k) / 100.0f; }
static void learn(int k, int delta) { learned[k] = (int8_t)constrain((int)learned[k] + delta, -50, 50); }

static void roll_likes(void) {
    for (int i = 0; i < LK_COUNT; i++) like[i] = (int8_t)(esp_random() % 41) - 20;   // mostly "don't mind"
    auto pick = [](int from, int to, int lo, int hi) {
        for (int tries = 0; tries < 20; tries++) {
            int k = from + esp_random() % (to - from + 1);
            if (abs(like[k]) > 40) continue;          // already a strong feeling
            like[k] = (int8_t)(lo + (int)(esp_random() % (hi - lo + 1)));
            return;
        }
    };
    pick(LK_HOLIDAYS, LK_HALLOWEEN, 80, 100);               // a favorite time of year
    if (esp_random() & 1) pick(LK_HOLIDAYS, LK_HALLOWEEN, 55, 80);
    pick(LK_HOLIDAYS, LK_HALLOWEEN, -90, -55);              // one it could do without
    pick(LK_LONG, LK_QUIET, 60, 100); pick(LK_LONG, LK_QUIET, 60, 100);
    pick(LK_LONG, LK_QUIET, -90, -60);
    if (esp_random() & 1) pick(LK_LONG, LK_QUIET, -80, -50);
    likes_rolled = true;
    Serial.print("coaster: likes");
    for (int i = 0; i < LK_COUNT; i++) if (like[i] >= 50) Serial.printf(" %s", LIKE_NAMES[i]);
    Serial.print(", dislikes");
    for (int i = 0; i < LK_COUNT; i++) if (like[i] <= -50) Serial.printf(" %s", LIKE_NAMES[i]);
    Serial.println();
}

static const char * feel_name(void) {
    return H > 0.6f ? "happy" : H > 0.25f ? "content" : H > -0.15f ? "okay" : H > -0.45f ? "down" : H > -0.75f ? "unhappy" : "miserable";
}

static void feel(float delta, const char * why) {
    float before = H;
    H = clampf(H + delta, -1, 1);
    feel_dirty = true;
    if (!why) return;
    memmove(fnotes + 1, fnotes, sizeof(fnotes) - sizeof(fnotes[0]));
    strlcpy(fnotes[0].why, why, sizeof(fnotes[0].why));
    fnotes[0].d = (int8_t)lroundf(clampf(delta * 100, -99, 99));
    if (fnotes_n < 5) fnotes_n++;
    Serial.printf("coaster: %s (%+.2f), feels %s (%.2f -> %.2f)\r\n", why, delta, feel_name(), before, H);
}

static void feel_save(void) {
    time_t now = time(NULL);
    if (now > 1700000000) last_seen = now;
    File f = LittleFS.open(FEEL_PATH, "w");
    if (!f) return;
    f.printf("{\"h\":%.3f,\"streak\":%d,\"prints\":%u,\"seen\":%ld,\"season\":%d,\"bday\":%d,\"like\":[",
             H, streak, prints_done, (long)last_seen, season_seen, bday_year);
    for (int i = 0; i < LK_COUNT; i++) f.printf("%s%d", i ? "," : "", like[i]);
    f.printf("],\"learned\":[");
    for (int i = 0; i < LK_COUNT; i++) f.printf("%s%d", i ? "," : "", learned[i]);
    f.printf("],\"open\":%.2f,\"hn\":%u,\"hlen\":%.3f,\"hx\":%.3f,\"hy\":%.3f,\"hpeak\":%.3f}",
             openness, habit_n, habit_len, habit_hx, habit_hy, habit_peak);
    f.close();
    feel_dirty = false;
    feel_save_ms = millis();
}

static void feel_load(void) {
    File f = LittleFS.open(FEEL_PATH, "r");
    if (f) {
        StaticJsonDocument<1536> d;
        if (deserializeJson(d, f) == DeserializationError::Ok) {
            JsonArrayConst lk = d["like"];
            if (lk.size() == LK_COUNT) { for (int i = 0; i < LK_COUNT; i++) like[i] = constrain((int)(lk[i] | 0), -100, 100); likes_rolled = true; }
            JsonArrayConst ln = d["learned"];
            if (ln.size() == LK_COUNT) for (int i = 0; i < LK_COUNT; i++) learned[i] = constrain((int)(ln[i] | 0), -50, 50);
            openness = clampf(d["open"] | 0.0f, -1, 1);
            habit_n = d["hn"] | 0; habit_len = d["hlen"] | 0.0f;
            habit_hx = d["hx"] | 0.0f; habit_hy = d["hy"] | 0.0f; habit_peak = d["hpeak"] | 0.0f;
            season_seen = d["season"] | -1;
            bday_year = d["bday"] | 0;
            H = clampf(d["h"] | 0.3f, -1, 1);
            streak = d["streak"] | 0;
            prints_done = d["prints"] | 0;
            last_seen = (time_t)(d["seen"] | 0L);
        }
        f.close();
    }
    esp_reset_reason_t r = esp_reset_reason();
    reset_kind = (r == ESP_RST_SW || r == ESP_RST_DEEPSLEEP) ? 1
               : (r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT || r == ESP_RST_BROWNOUT) ? 2 : 0;
    if (!likes_rolled) {
        roll_likes();
        openness = (esp_random() % 201) / 100.0f - 1;
        feel_dirty = true;
    }
    Serial.printf("coaster: feels %s (%.2f), %u prints, streak %d\r\n", feel_name(), H, prints_done, streak);
}


// how it woke up: judged once the clock is set (or after 90 s without one)
static void feel_judge_boot(void) {
    time_t now = time(NULL);
    bool clock = now > 1700000000;
    if (!clock && millis() < 90000) return;
    boot_judged = true;
    long off = (clock && last_seen > 1700000000) ? (long)(now - last_seen) : -1;
    if (reset_kind == 2) { t_confused = 15; feel(-0.05f, "woke up suddenly"); }
    else if (reset_kind == 0 && off >= 0 && off < 120) { t_confused = 12; feel(-0.02f, "power blinked"); }
    else if (off > 3 * 86400L) {
        char w[30]; snprintf(w, sizeof(w), "alone for %ld days", off / 86400);
        feel(-min(0.6f, 0.08f * off / 86400.0f), w);
        t_sad = 20;
    } else if (off > 12 * 3600L) {
        feel_boot_quirk(false);   // oh, you're back: a yawn
    } else if (reset_kind == 0) {
        feel_boot_quirk(H > 0);   // good morning
    }
    feel_save();
}


// ---- habits: is this print like the ones it's used to? ----
static bool novelty_pending = false;
static float t_uneasy = 0;
static int local_hour(void) {
    time_t now = time(NULL);
    if (now < 1700000000) return -1;
    now += T.tz_min * 60;
    struct tm t; gmtime_r(&now, &t);
    return t.tm_hour;
}
static float habit_hour(void) {   // the usual start hour, or -1 if it has no habit yet
    if (habit_n < 5 || habit_hx * habit_hx + habit_hy * habit_hy < 0.15f) return -1;
    float h = atan2f(habit_hy, habit_hx) / (2 * PI) * 24;
    return h < 0 ? h + 24 : h;
}
static void change_of_pace(float how_new, const char * novel, const char * uneasy) {
    // loves surprises: excited. Creature of habit: uneasy for a bit
    if (openness > 0.15f) {
        feel(0.12f * openness * how_new, novel);
        t_ready = 3; feel_boot_quirk(true);   // a happy bounce
    } else if (openness < -0.15f) {
        feel(0.1f * openness * how_new, uneasy);
        t_uneasy = 6;
    }
}
// a couple of minutes into a print, when the time estimate has settled
static void judge_new_print(const moonraker_data_t & d) {
    novelty_pending = false;
    if (habit_n < 5) return;   // still learning what "usual" is
    float est_min = (d.print_time + d.time_left) / 60.0f;
    if (d.time_left > 0 && est_min > 1) {
        float diff = log2f(est_min) - habit_len;          // +1 = twice as long as usual
        if (diff > 1.3f) change_of_pace(min(2.0f, diff - 0.3f), "a long one, a change of pace!", "a long one... not used to that");
        else if (diff < -1.3f) change_of_pace(min(2.0f, -diff - 0.3f), "a quick one for once!", "that's short... odd");
    }
    int hr = local_hour();
    float hh = habit_hour();
    if (hr >= 0 && hh >= 0) {
        float dh = fabsf(hr + 0.5f - hh); dh = min(dh, 24 - dh);
        if (dh > 5) change_of_pace(1, "printing at a new time!", "printing at this hour?");
    }
}
// a print finished: learn from it
static void learn_from_print(const moonraker_data_t & d, float peak) {
    float mins = max(1.0f, d.print_time / 60.0f);
    float lg = log2f(mins);
    float k = habit_n < 10 ? 1.0f / (habit_n + 1) : 0.1f;   // average over roughly the last 10 prints
    habit_len += (lg - habit_len) * k;
    int hr = local_hour();
    if (hr >= 0) {
        float a = (hr + 0.5f) / 24 * 2 * PI;
        habit_hx += (cosf(a) - habit_hx) * k; habit_hy += (sinf(a) - habit_hy) * k;
        if (hr >= 23 || hr < 6) learn(LK_NIGHT, 2);        // late prints: a night owl in the making
    }
    if (habit_n >= 5 && peak > habit_peak * 1.8f && peak > 0.6f)
        feel(0.1f * L(LK_FAST), eff(LK_FAST) >= 0 ? "that was a wild ride!" : "too wild for me");
    habit_peak += (peak - habit_peak) * k;
    if (habit_n < 60000) habit_n++;
    // what you print a lot, it gets used to, and a little fond of
    if (mins <= 30) { learn(LK_SHORT, 3); learn(LK_LONG, -1); }
    else if (mins >= 240) { learn(LK_LONG, 3); learn(LK_SHORT, -1); }
    if (d.fan >= 60) learn(LK_FANS, 1);
    Serial.printf("coaster: used to %.0f min prints around %.0f:00, %.2f g (%u prints)\r\n",
                  exp2f(habit_len), habit_hour(), habit_peak, habit_n);
}

// once a second: slow drifts, heating for nothing, being left alone
static void feel_tick(const moonraker_data_t & d) {
    if (!boot_judged) feel_judge_boot();
    if (novelty_pending && d.printing && d.print_time > 120) judge_new_print(d);
    if (!d.printing) novelty_pending = false;
    // the time of year: its favorite season lifts it, the one it dislikes wears on it
    time_t now = time(NULL);
    float base_h = 0.15f;
    if (now > 1700000000) {
        time_t lt = now + T.tz_min * 60;
        struct tm t; gmtime_r(&lt, &t);
        night = t.tm_hour >= 23 || t.tm_hour < 6;
        int m = t.tm_mon + 1, dd = t.tm_mday;
        int k = deco_for(m, dd);
        if (k == D_BIRTHDAY) {
            base_h = 0.6f;
            if (bday_year != t.tm_year + 1900) { bday_year = t.tm_year + 1900; feel(0.3f, "it's my birthday!"); }
            T.bday_m = 0;  // look past the birthday for the season underneath
            k = deco_for(m, dd);
            T.bday_m = m;
        }
        if (k >= D_HOLIDAYS && k <= D_HALLOWEEN) {
            int lk = k - D_HOLIDAYS;
            base_h += 0.3f * L(lk);
            if (season_seen != k) {
                if (season_seen >= 0) {   // not the very first boot
                    char w[30];
                    if (eff(lk) >= 50) { snprintf(w, sizeof(w), "yay, %s!", LIKE_NAMES[lk]); feel(0.15f, w); }
                    else if (eff(lk) <= -50) { snprintf(w, sizeof(w), "ugh, %s", LIKE_NAMES[lk]); feel(-0.1f, w); }
                }
                season_seen = k;
                feel_dirty = true;
            }
        }
    }
    if (night) base_h += 0.1f * L(LK_NIGHT);
    H += (base_h - H) / 21600;                     // drifts toward its baseline over ~6 h
    bool fans = d.printing && d.fan >= 60;
    if (d.printing && !(d.pause || d.paused_ext)) {
        H = min(1.0f, H + (0.1f + (fans ? 0.05f * L(LK_FANS) : 0)) / 3600);   // printing is fun
        idle_s = 0;
    } else {
        idle_s++;
        // left alone: some like the quiet, most get bored after a while
        float per_h = L(LK_QUIET) > 0.3f ? 0 : L(LK_QUIET) < -0.3f && idle_s > 2 * 3600UL ? 0.05f : idle_s > 12 * 3600UL ? 0.02f : 0;
        H = max(-1.0f, H - per_h / 3600);
    }
    // heated up and then nothing happened
    bool at_temp = (d.nozzle_target > 0 && d.nozzle_actual >= d.nozzle_target - 3) || (d.bed_target > 0 && d.bed_actual >= d.bed_target - 2);
    if (at_temp && !d.printing && act == ACT_NONE && !d.homing && !d.probing && !d.qgling && still_t > 20) heat_idle_s++;
    else if (!at_temp || d.printing || env > 0.1f) heat_idle_s = 0;
    bool mad_now = heat_idle_s > (uint32_t)(180 + 120 * L(LK_HEAT));   // heat lovers put up with it longer
    if (mad_now && !mad_heat) feel(-0.06f, "heated up for nothing");
    if (mad_now) H = max(-1.0f, H - 0.03f / 60);
    mad_heat = mad_now;
    // lost OctoPrint for a while
    if (moonraker.unconnected || wifi_get_connect_status() != WIFI_STATUS_CONNECTED) {
        if (++unlinked_s == 60 && !knomi_ble_link_active()) feel(-0.03f, "lost OctoPrint");
    } else unlinked_s = 0;
    if (d.runout && d.printing && !was_runout) feel(-0.05f, "ran out of filament");
    was_runout = d.runout && d.printing;
    if ((feel_dirty && millis() - feel_save_ms > 20000) || millis() - feel_save_ms > 600000UL) feel_save();
}

static void pick_mood(float dt, const moonraker_data_t & d) {
    t_startle = max(0.0f, t_startle - dt); t_dizzy = max(0.0f, t_dizzy - dt);
    t_giggle = max(0.0f, t_giggle - dt); t_celebrate = max(0.0f, t_celebrate - dt);
    t_ready = max(0.0f, t_ready - dt); t_sad = max(0.0f, t_sad - dt); t_phew = max(0.0f, t_phew - dt);
    t_confused = max(0.0f, t_confused - dt); t_mad = max(0.0f, t_mad - dt); t_uneasy = max(0.0f, t_uneasy - dt);
    feel_acc += dt;
    if (feel_acc >= 1) { feel_acc -= 1; feel_tick(d); }
    if (step > 1.8f && from_rest && vib < 0.35f && t_startle <= 0 && t_celebrate <= 0) {
        t_startle = 0.7f; blink_now(); head_kick(0, -40);
    }
    if (thrill > 1) { scream_t += dt; dizzy_meter += dt * min(2.0f, thrill); }
    else { scream_t = max(0.0f, scream_t - dt * 2); dizzy_meter = max(0.0f, dizzy_meter - dt * 0.6f); }
    if (dizzy_meter > T.dizzy && t_dizzy <= 0) { t_dizzy = 3.5f; dizzy_meter = 0; feel(-0.08f, "got dizzy"); }
    bool paused = d.printing && (d.pause || d.paused_ext);
    // the screen woke up in the middle of the night: huh? what?
    static bool was_dozing = false;
    bool dozing = knomi_power_dozing();
    if (was_dozing && !dozing && night && !d.printing) { t_confused = 4; blink_now(); }
    was_dozing = dozing;
    // connection trouble (these show on the WiFi-lost screen and the error popups)
    bool link = wifi_get_connect_status() == WIFI_STATUS_CONNECTED || knomi_ble_link_active();
    bool lonely = !link || moonraker.unconnected;
    bool confused = !lonely && moonraker.auth_failed;
    bool error = !lonely && !confused && moonraker.unready;
    bool noz_heat = d.nozzle_target > 0 && d.nozzle_actual < d.nozzle_target - 3;
    bool bed_heat = d.bed_target > 0 && d.bed_actual < d.bed_target - 2;
    bool heating = noz_heat || bed_heat;
    // effort grows as the temperature climbs: pushing hardest right before it gets there
    float e_noz = noz_heat ? 0.4f + 0.6f * clampf((float)d.nozzle_actual / d.nozzle_target, 0, 1) : 0;
    float e_bed = bed_heat ? 0.2f + 0.3f * clampf((float)d.bed_actual / d.bed_target, 0, 1) : 0;
    heat_effort = max(e_noz, e_bed);
    if (was_heating && !heating && (d.nozzle_target > 0 || d.bed_target > 0)) t_phew = 1.8f;
    was_heating = heating;
    bool first_layer = d.printing && !paused && d.print_time > 0 &&
                       (d.layer_total > 0 ? d.layer <= 1 : (d.z_um != INT32_MIN && d.z_um <= 400));
    bool cooling = !d.printing && d.nozzle_target == 0 && d.nozzle_actual >= 60 &&
                   (int32_t)(cool_until - millis()) > 0;
    int m;
    if (t_celebrate > 0 || act == ACT_DONE) m = M_CELEBRATE;
    else if (t_giggle > 0) m = M_GIGGLE;
    else if (t_mad > 0) m = M_MAD;                        // poked one too many times
    else if (lonely) m = M_LONELY;
    else if (confused || t_confused > 0) m = M_CONFUSED;  // also: woke up out of nowhere
    else if (error) m = M_ERROR;
    else if (t_startle > 0 && act == ACT_HOMING) m = M_STARTLED;   // the endstop hit
    else if (act == ACT_HOMING) m = M_BRACE;
    else if (act == ACT_PROBING || act == ACT_START) m = M_FOCUS;
    else if (act == ACT_QGL) m = M_LEVEL;
    else if (act == ACT_SHAPING) m = M_SHIVER;
    else if (act == ACT_PID) m = M_HEATING;
    else if (act == ACT_CLEANING) m = M_SCRUB;
    else if (act == ACT_FILAMENT) m = M_HUNGRY;
    else if (t_ready > 0) m = M_READY;
    else if (t_dizzy > 0) m = M_DIZZY;
    else if (t_startle > 0) m = M_STARTLED;
    else if (vib > 0.28f) m = M_SHIVER;
    else if (thrill > 1 && scream_t > 0.4f) m = M_SCREAM;
    else if (t_whee > 0) m = M_WHEE;
    else if (thrill > 0.35f) m = M_EXCITED;
    else if (t_sad > 0) m = M_SAD;
    else if (t_uneasy > 0) m = M_NERVOUS;                 // not used to this kind of print
    else if (d.runout && d.printing) m = M_HUNGRY;
    else if (fabsf(vz) > 2.0f) m = M_ELEVATOR;
    else if (paused) m = M_BORED;
    else if (heating) m = M_HEATING;
    else if (mad_heat) m = M_MAD;                         // hot and nothing to print
    else if (t_phew > 0) m = M_COOLING;                   // phew, made it
    else if (knomi_power_dozing()) m = M_SLEEPY;          // the screen dims: Coaster dozes off with it
    else if (first_layer) m = M_FOCUS;
    else if (d.printing && d.progress >= 90) m = M_ANTICIPATE;
    else if (d.printing && d.speed >= 130) m = M_NERVOUS;
    else if (d.printing && d.fan >= 80) m = M_WINDY;
    else if (still_t > T.sleep * (night ? (eff(LK_NIGHT) >= 50 ? 1.5f : 0.5f) : 1.0f) && !d.printing) m = M_SLEEPY;   // night owls stay up
    else if (cooling) m = M_COOLING;
    else if (env > 0.05f || vib > 0.05f) m = M_RIDING;
    else m = M_CALM;
    if (m != mood) {
        if (m == M_SCREAM || m == M_WHEE) feel(0.02f * L(LK_FAST), NULL);   // thrill seekers love it, others don't
        if (d.printing) {
            if (m == M_SCREAM) stats.screams++;
            if (m == M_DIZZY) stats.dizzies++;
            if (m == M_STARTLED) stats.jolts++;
        }
        mood = m; mood_t = 0;
        static uint32_t said_ms = 0;
        static int said_mood = -1;
        if (mood != said_mood && millis() - said_ms > 2000) {
            Serial.printf("coaster: %s\r\n", MOOD_NAMES[mood]);
            said_mood = mood; said_ms = millis();
        }
    } else mood_t += dt;
    if (d.printing && env > stats.peak) stats.peak = env;
}

// soft compression: small moves still show, big ones don't just pin the head to the edge
static inline float squash(float v) { return copysignf(powf(fabsf(v), 0.6f), v); }

static void step_body(float dt) {
    float q[3] = {squash(lp[0]), squash(lp[1]), squash(lp[2])};
    float sn = T.sense * SENSE_K;
    float w = 2 * PI * T.wobble, k = w * w, c = 2 * T.settle * w, gain = sn * k * 18;
    // inertia: the head lags opposite the acceleration. screen x <- X, screen y <- Z (up = -y), depth <- Y
    hvx += (-k * hx - c * hvx - gain * q[0]) * dt;
    hvy += (-k * hy - c * hvy + gain * q[2]) * dt;
    hvs += (-k * hs - c * hvs - sn * k * 0.16f * q[1]) * dt;
    // the spring can't follow 3-10 Hz toolhead motion, so the head also jiggles with it directly
    float k8 = 1 - expf(-2 * PI * 8 * dt);
    for (int i = 0; i < 3; i++) bx[i] += (q[i] - bx[i]) * k8;
    hx = clampf(hx + hvx * dt, -34, 34); hy = clampf(hy + hvy * dt, -30, 30); hs = clampf(hs + hvs * dt, -0.3f, 0.3f);
    float w2 = w * 1.35f, k2 = w2 * w2, c2 = 2 * T.settle * 0.6f * w2, g2 = sn * k2 * 11;
    pvx += (-k2 * px_ - c2 * pvx - g2 * q[0]) * dt;
    pvy += (-k2 * py_ - c2 * pvy + g2 * q[2]) * dt;
    px_ += pvx * dt; py_ += pvy * dt;
    float r = sqrtf(px_ * px_ + py_ * py_);
    if (r > 13) { px_ *= 13 / r; py_ *= 13 / r; pvx *= 0.4f; pvy *= 0.4f; }
}

/* ---------------- quirks: little things it does just because ---------------- */
// Every few seconds, when nothing else is going on, Coaster does something small: glances
// around, winks, yawns, hums, sneezes, stretches. Each KNOMI gets its own personality
// (from its chip ID), so one is curious, another sleepy, another hums a lot.

enum { Q_NONE, Q_GLANCE, Q_DBLINK, Q_SLOWBLINK, Q_WINK, Q_YAWN, Q_HUM, Q_SNEEZE, Q_LOOKUP,
       Q_STRETCH, Q_ROLL, Q_NOD, Q_CHEER, Q_SIGH, Q_HUFF, Q_COUNT };
static const char * QUIRK_NAMES[Q_COUNT] = {"", "glance", "double blink", "slow blink", "wink", "yawn", "hum",
                                            "sneeze", "look up", "stretch", "eye roll", "nod", "cheer", "sigh", "huff"};
static const float QUIRK_DUR[Q_COUNT] = {0, 1.6f, 0.5f, 1.3f, 0.8f, 2.4f, 3.6f, 1.7f, 1.9f, 1.8f, 1.3f, 0.7f, 1.3f, 1.9f, 0.9f};
static int quirk = Q_NONE, quirk_side = 1;
static float quirk_t = 0, quirk_next = 6;
static uint8_t quirk_fired = 0;                 // one-shot steps inside a quirk (bit per step)
// personality 0.5..1.5: curious (glances, looking up), sleepy (yawns, slow blinks),
// musical (humming), silly (winks, sneezes, eye rolls)
static float tr_curious = 1, tr_sleepy = 1, tr_musical = 1, tr_silly = 1;
// what the quirk does to the face this instant
typedef struct { float look, look_y, open_l, open_r, gape, curve, w, cheek, dx, dy, sq; } quirk_fx_t;
static quirk_fx_t QF = {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0};
static float sacc_x = 0, sacc_y = 0, sacc_tx = 0, sacc_ty = 0, sacc_t = 1;   // tiny eye darts
typedef struct { float x, y, life; } hum_note_t;
static hum_note_t notes[3];

static void personality_init(void) {
    uint64_t id = ESP.getEfuseMac();
    uint32_t h = (uint32_t)(id ^ (id >> 32)) * 2654435761u;
    float * tr[4] = {&tr_curious, &tr_sleepy, &tr_musical, &tr_silly};
    for (int i = 0; i < 4; i++) { *tr[i] = 0.5f + ((h >> (i * 8)) & 0xFF) / 255.0f; }
    Serial.printf("coaster: curious %.1f, sleepy %.1f, musical %.1f, silly %.1f\r\n", tr_curious, tr_sleepy, tr_musical, tr_silly);
}

// 0 -> 1 -> 0 over the quirk, with soft edges of `edge` seconds
static float bump(float t, float dur, float edge) {
    return clampf(min(t / edge, (dur - t) / edge), 0, 1);
}

static bool quirk_step(int n) {   // true once, the first time the quirk reaches step n
    if (quirk_fired & (1 << n)) return false;
    quirk_fired |= 1 << n;
    return true;
}

static void quirk_start(int q) {
    quirk = q; quirk_t = 0; quirk_fired = 0; quirk_side = esp_random() & 1 ? 1 : -1;
}

static void feel_boot_quirk(bool happy) { quirk_start(happy ? Q_CHEER : Q_YAWN); }

// a layer change or a progress milestone: small reactions that can interrupt idling
void coaster_quirk_nod(void) { if (quirk == Q_NONE || quirk == Q_GLANCE) quirk_start(Q_NOD); }
static void quirk_cheer(void) { if (quirk != Q_YAWN && quirk != Q_SNEEZE) quirk_start(Q_CHEER); }

static int quirk_pick(void) {
    // weights per mood; the personality scales them
    float w[Q_COUNT] = {0};
    bool printing = moonraker.data.printing;
    switch (mood) {
        case M_CALM:
            w[Q_GLANCE] = 3 * tr_curious; w[Q_LOOKUP] = 1.5f * tr_curious; w[Q_DBLINK] = 1.5f;
            w[Q_SLOWBLINK] = 1.5f * tr_sleepy; w[Q_YAWN] = (still_t > 60 ? 2.5f : 0.8f) * tr_sleepy;
            w[Q_HUM] = 1.5f * tr_musical; w[Q_WINK] = 0.8f * tr_silly; w[Q_SNEEZE] = 0.35f * tr_silly;
            w[Q_STRETCH] = 0.8f * tr_sleepy;
            break;
        case M_RIDING:
            w[Q_GLANCE] = 2 * tr_curious; w[Q_DBLINK] = 1.5f; w[Q_WINK] = 0.8f * tr_silly;
            w[Q_HUM] = (printing ? 2.0f : 1.0f) * tr_musical; w[Q_LOOKUP] = 0.8f * tr_curious;
            w[Q_SLOWBLINK] = 0.6f * tr_sleepy;
            break;
        case M_FOCUS:
            w[Q_DBLINK] = 1; w[Q_SLOWBLINK] = 0.3f * tr_sleepy;
            break;
        case M_BORED:
            w[Q_ROLL] = 2 * tr_silly; w[Q_YAWN] = 2 * tr_sleepy; w[Q_GLANCE] = 1 * tr_curious; w[Q_HUM] = 1 * tr_musical;
            break;
        case M_COOLING:
            w[Q_STRETCH] = 2 * tr_sleepy; w[Q_YAWN] = 1.5f * tr_sleepy; w[Q_SLOWBLINK] = 1.5f; w[Q_HUM] = 1.5f * tr_musical;
            break;
        case M_SLEEPY:
            w[Q_YAWN] = 1 * tr_sleepy;
            break;
        case M_ANTICIPATE:
            w[Q_GLANCE] = 1; w[Q_HUM] = 1.5f * tr_musical; w[Q_WINK] = 1 * tr_silly;
            break;
        case M_SCREAM: case M_STARTLED: case M_DIZZY: case M_WHEE: case M_SHIVER:
            w[Q_DBLINK] = 1; w[Q_GLANCE] = 0.5f * tr_curious;   // busy hanging on: only quick ones
            break;
        case M_HEATING:
            w[Q_GLANCE] = 1.5f * tr_curious; w[Q_DBLINK] = 1; w[Q_LOOKUP] = 1 * tr_curious; w[Q_SNEEZE] = 0.6f * tr_silly;
            break;
        default:   // any other mood, printing or not
            w[Q_GLANCE] = 2 * tr_curious; w[Q_DBLINK] = 1.5f; w[Q_WINK] = 0.8f * tr_silly; w[Q_HUM] = 1.2f * tr_musical;
            w[Q_LOOKUP] = 0.8f * tr_curious; w[Q_SLOWBLINK] = 0.6f * tr_sleepy;
            if (printing) w[Q_YAWN] = 0.4f * tr_sleepy;
            break;
    }
    // heating smells: the odd sneeze while the nozzle is hot
    if (moonraker.data.nozzle_actual > 180) w[Q_SNEEZE] += 0.3f * tr_silly;
    // how it feels changes what it does: sighs when down, more humming and winking when happy
    if (H < -0.2f) { w[Q_SIGH] += 3 * -H; w[Q_HUM] *= 1 + H; w[Q_WINK] *= 1 + H; w[Q_CHEER] = 0; }
    if (H > 0.4f) { w[Q_HUM] *= 1.6f; w[Q_WINK] *= 1.4f; }
    if (mood == M_MAD) { for (int i = 0; i < Q_COUNT; i++) w[i] = 0; w[Q_HUFF] = 3; w[Q_ROLL] = 1; w[Q_GLANCE] = 1; }
    float sum = 0;
    for (int i = 0; i < Q_COUNT; i++) sum += w[i];
    if (sum <= 0) return Q_NONE;
    float r = frand(0, sum);
    for (int i = 0; i < Q_COUNT; i++) { r -= w[i]; if (r <= 0 && w[i] > 0) return i; }
    return Q_NONE;
}

static void step_quirks(float dt) {
    QF = {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0};
    // quirks happen any time, printing or not; only a speech bubble (it's talking) holds them off
    bool busy = (int32_t)(bubble_until - millis()) > 0;
    // tiny eye darts, like it's actually looking at things
    sacc_t -= dt;
    if (sacc_t <= 0) {
        sacc_t = frand(0.4f, 2.2f);
        sacc_tx = frand(-4, 4) * tr_curious; sacc_ty = frand(-2, 2);
    }
    float ks = 1 - expf(-dt * 30);
    sacc_x += (sacc_tx - sacc_x) * ks; sacc_y += (sacc_ty - sacc_y) * ks;
    if (mood != M_SLEEPY && mood != M_DIZZY && act != ACT_PROBING) { QF.look = sacc_x; QF.look_y = sacc_y; }

    if (quirk != Q_NONE && busy && quirk != Q_CHEER && quirk != Q_NOD) quirk = Q_NONE;
    if (quirk == Q_NONE) {
        if (busy) { quirk_next = max(quirk_next, 2.0f); return; }
        quirk_next -= dt;
        if (quirk_next > 0) return;
        quirk_next = frand(4, 13) * (H < -0.5f ? 1.8f : 1.0f) * (mood == M_MAD ? 0.5f : 1.0f);
        int q = quirk_pick();
        if (q == Q_NONE) return;
        quirk_start(q);
    }
    quirk_t += dt;
    float t = quirk_t, D = QUIRK_DUR[quirk];
    if (t >= D) {
        if (quirk == Q_YAWN || quirk == Q_SNEEZE) blink_now();
        quirk = Q_NONE;
        return;
    }
    float e;
    switch (quirk) {
        case Q_GLANCE:      // a look to one side, then back
            e = bump(t, D, 0.15f);
            QF.look += quirk_side * 18 * e; QF.look_y -= 2 * e;
            if (t > 0.9f && quirk_step(0) && (esp_random() & 1)) blink_now();
            break;
        case Q_DBLINK:
            if (quirk_step(0)) blink_now();
            if (t > 0.26f && quirk_step(1)) blink_now();
            break;
        case Q_SLOWBLINK:   // content: eyes close slowly, a small smile
            e = bump(t, D, 0.45f);
            QF.open_l = QF.open_r = 1 - 0.97f * e; QF.curve = 0.5f * e;
            break;
        case Q_WINK:
            e = bump(t, D, 0.12f);
            (quirk_side > 0 ? QF.open_r : QF.open_l) = 1 - e;
            QF.curve = 0.7f * e; QF.dx = quirk_side * 2 * e;
            break;
        case Q_YAWN:        // mouth opens wide, eyes squeeze, head tips back
            e = bump(t, D, 0.8f);
            QF.gape = 1.6f * e; QF.w = -5 * e; QF.open_l = QF.open_r = 1 - 0.85f * e; QF.dy = -6 * e;
            QF.sq = 0.08f * e;
            break;
        case Q_HUM: {       // little "o" mouth, swaying, notes drifting up
            e = bump(t, D, 0.3f);
            QF.w = -8 * e; QF.gape = 0.45f * e; QF.dx = sinf(t * 4.2f) * 4 * e; QF.dy = -fabsf(sinf(t * 4.2f)) * 2 * e;
            QF.open_l = QF.open_r = 1 - 0.4f * e;
            for (int n = 0; n < 3; n++) {
                if (t > 0.3f + n * 1.0f && quirk_step(n)) notes[n] = {150 + frand(-6, 10), 128, 1.6f};
            }
            break;
        }
        case Q_SNEEZE:      // ah... ah... CHOO
            if (t < 1.1f) {
                e = clampf(t / 1.1f, 0, 1);
                QF.open_l = QF.open_r = 1 - 0.75f * e; QF.dy = -7 * e; QF.gape = 0.7f * e; QF.w = -4 * e;
                QF.dx = sinf(t * 30) * e;    // twitching nose
            } else {
                if (quirk_step(0)) { head_kick(frand(-30, 30), 170); blink_now(); }
                e = 1 - clampf((t - 1.1f) / 0.6f, 0, 1);
                QF.open_l = QF.open_r = 1 - e; QF.gape = 0.2f * e;
            }
            break;
        case Q_LOOKUP:      // something up there?
            e = bump(t, D, 0.3f);
            QF.look_y -= 7 * e; QF.look += quirk_side * 7 * e; QF.open_l = QF.open_r = 1 + 0.2f * e;
            break;
        case Q_STRETCH:     // grows tall, eyes happy-shut, then settles
            e = bump(t, D, 0.6f);
            QF.sq = 0.16f * e; QF.cheek = 0.9f * e; QF.curve = 0.5f * e; QF.dy = -4 * e;
            QF.open_l = QF.open_r = 1 + e;      // eyes up into happy "^"
            break;
        case Q_ROLL: {      // eyes go up and around
            float a = clampf(t / D, 0, 1) * 2 * PI;
            e = bump(t, D, 0.15f);
            QF.look += sinf(a) * 16 * e; QF.look_y -= (1 - cosf(a)) * 4 * e; QF.open_l = QF.open_r = 1 - 0.25f * e;
            break;
        }
        case Q_NOD:         // one little nod: another layer done
            QF.dy = sinf(PI * clampf(t / D, 0, 1)) * 6;
            break;
        case Q_SIGH:        // eyes drop, head sinks, a little breath out
            e = bump(t, D, 0.5f);
            QF.open_l = QF.open_r = 1 - 0.6f * e; QF.dy = 5 * e; QF.curve = -0.3f * e; QF.gape = 0.25f * e; QF.w = -6 * e;
            break;
        case Q_HUFF:        // mad: a sharp puff, head jerks down
            e = bump(t, D, 0.12f);
            QF.dy = 4 * e; QF.gape = 0.4f * e; QF.w = -7 * e; QF.dx = sinf(t * 40) * 1.5f * e;
            if (quirk_step(0)) head_kick(0, 60);
            break;
        case Q_CHEER:       // a milestone: happy bounce
            e = bump(t, D, 0.2f);
            QF.cheek = e; QF.curve = 0.8f * e; QF.gape = 0.6f * e; QF.dy = -fabsf(sinf(t * 10)) * 6 * e;
            QF.open_l = QF.open_r = 1 + e;
            break;
    }
}

static void step_notes(float dt) {
    for (int i = 0; i < 3; i++) {
        if (notes[i].life <= 0) continue;
        notes[i].life -= dt;
        notes[i].y -= 22 * dt;
        notes[i].x += sinf(notes[i].life * 5) * 12 * dt;
    }
}

static void step_expr(float dt) {
    wander_t -= dt;
    if (wander_t <= 0) { wander_tx = frand(-12, 12); wander_t = frand(1.2f, 3.5f); }
    wander_x += (wander_tx - wander_x) * (1 - expf(-dt * 5));
    expr_t t = MOODS[mood];
    // the slow feeling shows through the everyday faces: a smile, or droopy worried brows
    // (a print running cheers it up right away, on top of the slow feeling)
    float hv = clampf(H + (moonraker.data.printing ? 0.25f : 0), -1, 1);
    if (mood == M_CALM || mood == M_RIDING || mood == M_FOCUS || mood == M_COOLING || mood == M_WINDY ||
        mood == M_ANTICIPATE || mood == M_BORED) {
        t.curve = clampf(t.curve + 0.55f * hv, -0.9f, 1);
        t.open = clampf(t.open + 0.08f * hv, 0.2f, 1);
        if (hv < 0) { t.tilt += 0.7f * hv; t.omega *= 1 + hv; }
    }
    float kk = 1 - expf(-dt * 9);
    float * e = (float *)&E; const float * tt = (const float *)&t;
    for (unsigned i = 0; i < sizeof(expr_t) / sizeof(float); i++) e[i] += (tt[i] - e[i]) * kk;
    float want = mood == M_BORED ? sinf(now_s * 1.3f) * 12
               : mood == M_LONELY ? sinf(now_s * 0.7f) * 14                        // looking around for OctoPrint
               : mood == M_CONFUSED ? (fmodf(now_s, 2.4f) < 1.2f ? -10 : 10)       // glancing left, right
               : mood == M_HEATING ? 0
               : (mood == M_CALM || mood == M_RIDING || mood == M_SLEEPY || mood == M_COOLING) ? (H < -0.5f ? -14 : wander_x)   // sulking: won't look at you
               : mood == M_MAD ? -10 : 0;
    look += (want - look) * kk;
    if (mood == M_HUNGRY) E.gape = 0.9f * fabsf(sinf(now_s * 5));   // chomp chomp
    if (act == ACT_PROBING) look = 0;
    if (blink_closing > 0) blink_closing -= dt;
    else {
        blink_t -= dt;
        if (blink_t <= 0 && mood != M_SLEEPY && mood != M_DIZZY) { blink_now(); blink_t = frand(2.2f, 6); }
    }
    for (int i = 0; i < confetti_n; i++) {
        bit_t & p = confetti[i];
        p.vy += 420 * dt; p.x += p.vx * dt; p.y += p.vy * dt; p.r += p.vr * dt; p.life -= dt;
    }
    while (confetti_n && confetti[confetti_n - 1].life <= 0) confetti_n--;
    step_quirks(dt);
    step_notes(dt);
}

static void spawn_confetti(void) {
    confetti_n = sizeof(confetti) / sizeof(confetti[0]);
    for (int i = 0; i < confetti_n; i++)
        confetti[i] = {120 + frand(-20, 20), 120, frand(-160, 160), frand(-260, -80), frand(0, 6.28f), frand(-8, 8), frand(1.8f, 3.2f)};
}

// printer events: print finished, Z moving (elevator)
static void watch_printer(const moonraker_data_t & d) {
    if (d.msg_id != bubble_msg_id) {
        bubble_msg_id = d.msg_id;
        if (d.msg[0]) {
            bubble_since = millis();
            bubble_until = millis() + 5000 + strlen(d.msg) * 80;   // longer messages stay longer
        } else {
            bubble_until = millis();
        }
    }
    // small reactions to the print going well: a nod per layer, a cheer at 25/50/75 %
    static int16_t seen_layer = 0;
    if (d.printing && d.layer > 1 && seen_layer && d.layer != seen_layer) coaster_quirk_nod();
    seen_layer = d.printing ? d.layer : 0;
    if (d.printing && was_printing && d.progress != last_progress &&
        (d.progress / 25) > (last_progress / 25) && d.progress < 100) quirk_cheer();
    if (d.printing) last_progress = d.progress;
    if (!was_printing && d.printing) { stats = {}; novelty_pending = true; }
    if (was_printing && !d.printing) {
        report = stats;
        report.valid = true;
        report.done = last_progress >= 98;
        report.progress = last_progress;
        report.secs = d.print_time;
        Serial.printf("coaster: print over, %u screams, %u dizzy, %u jolts, peak %.2f g\r\n",
                      report.screams, report.dizzies, report.jolts, report.peak);
        if (last_progress >= 98) {
            t_celebrate = 4.5f; spawn_confetti();
            streak++; prints_done++;
            learn_from_print(d, stats.peak);
            float extra = d.print_time > 4 * 3600 ? 0.1f * L(LK_LONG) : d.print_time < 1800 ? 0.1f * L(LK_SHORT) : 0;
            feel(0.2f + 0.04f * min((int)streak, 5) + extra,
                 streak >= 3 ? "print streak!" : d.print_time > 4 * 3600 ? "finished a long print" : "finished a print");
        } else {
            t_sad = 8;                                    // cancelled or failed
            streak = 0;
            if (last_progress < 5) feel(-0.05f, "print stopped early");
            else feel(-0.15f, "print failed");
        }
        cool_until = millis() + 10 * 60000UL;
    }
    was_printing = d.printing;
    // Z speed over the last 1.5 s from the printer's reported Z (the accelerometer can't feel slow Z moves)
    if (millis() - zhist_ms >= 250) {
        zhist_ms = millis();
        if (d.z_um == INT32_MIN) { zhist_n = 0; vz = 0; return; }
        memmove(zhist + 1, zhist, sizeof(zhist) - sizeof(zhist[0]));
        zhist[0] = d.z_um;
        if (zhist_n < 7) zhist_n++;
        vz = zhist_n == 7 ? (zhist[0] - zhist[6]) / 1000.0f / 1.5f : 0;
    }
}

/* ---------------- faces on screen ---------------- */

static lv_obj_t * faces[MAX_FACES];

/* ---------------- drawing ---------------- */

typedef struct {
    lv_draw_ctx_t * ctx;
    lv_coord_t ox, oy;   // object origin
    float s;             // scale from the 240 px design
    lv_color_t fc;
} pen_t;

static lv_coord_t X(const pen_t & p, float x) { return p.ox + (lv_coord_t)lroundf(x * p.s); }
static lv_coord_t Y(const pen_t & p, float y) { return p.oy + (lv_coord_t)lroundf(y * p.s); }

static void fill_ellipse(const pen_t & p, float cx, float cy, float rx, float ry, lv_color_t col) {
    if (rx < 0.5f || ry < 0.5f) return;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col; d.radius = LV_RADIUS_CIRCLE;
    lv_area_t a = {X(p, cx - rx), Y(p, cy - ry), X(p, cx + rx), Y(p, cy + ry)};
    lv_draw_rect(p.ctx, &d, &a);
}

static void fill_rect(const pen_t & p, float x0, float y0, float x1, float y1, lv_color_t col) {
    if (x1 < x0 || y1 < y0) return;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col;
    lv_area_t a = {X(p, x0), Y(p, y0), X(p, x1), Y(p, y1)};
    lv_draw_rect(p.ctx, &d, &a);
}

static void line(const pen_t & p, float x0, float y0, float x1, float y1, float w, lv_opa_t opa = LV_OPA_COVER) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = p.fc; d.width = max(1, (int)lroundf(w * p.s)); d.round_start = 1; d.round_end = 1; d.opa = opa;
    lv_point_t a = {X(p, x0), Y(p, y0)}, b = {X(p, x1), Y(p, y1)};
    lv_draw_line(p.ctx, &d, &a, &b);
}

static void draw_eye(const pen_t & p, float ex, float ey, int side, float sx, float open, float look_y) {
    const float LINE = 26, R = 11;
    float r = R * E.size, L = LINE * sx;
    float slide = clampf(px_ * 1.6f + look + QF.look, -(L - r), L - r);
    float squash = clampf(1 + py_ / 22, 0.55f, 1.5f);
    float ox = cosf(now_s * 6 * side) * 9 * E.orbit, oy = sinf(now_s * 6 * side) * 4 * E.orbit;
    float cx = ex + slide + ox, cy = ey + oy + look_y, ry = r * squash;
    float lid = cy + ry - open * 2 * ry;       // lid height: below, through or above the pupil
    lv_color_t black = lv_color_black();
    // brow tilt: + inner ends down (angry, straining), - inner ends up (worried)
    float slope = -side * 0.28f * E.tilt;
    fill_ellipse(p, cx, cy, r, ry, p.fc);
    if (fabsf(E.tilt) < 0.02f) {
        fill_rect(p, cx - r - 2, cy - ry - 2, cx + r + 2, lid, black);    // lid covers the top
    } else {
        float x0 = cx - r - 3, x1 = cx + r + 3, top = min(cy - ry - 4, lid - 12);
        lv_point_t q[4] = {{X(p, x0), Y(p, top)}, {X(p, x1), Y(p, top)},
                           {X(p, x1), Y(p, lid + slope * (x1 - ex))}, {X(p, x0), Y(p, lid + slope * (x0 - ex))}};
        lv_draw_rect_dsc_t bd; lv_draw_rect_dsc_init(&bd); bd.bg_color = black;
        lv_draw_polygon(p.ctx, &bd, q, 4);
    }
    if (E.cheek > 0.01f) {                                                // cheek pushes up: "^" eyes
        float cyc = cy + r * 2.4f - E.cheek * 1.55f * r;
        fill_ellipse(p, cx, cyc, r * 1.5f, r * 1.5f, black);
    }
    float k = clampf((open - 0.5f) / 0.3f, 0, 1);   // lid line shrinks into the pupil as it opens
    if (k < 0.98f) {
        float x0 = ex - L + (cx - (ex - L)) * k, x1 = ex + L + (cx - (ex + L)) * k;
        line(p, x0, lid + slope * (x0 - ex), x1, lid + slope * (x1 - ex), 5, (lv_opa_t)(255 * (1 - k * 0.6f)));
    }
}

static float tri(float u) { return 2 * fabsf(2 * (u - floorf(u + 0.5f))) - 1; }

static void draw_mouth(const pen_t & p, float mx, float my) {
    const int N = 20;
    float w = E.w, prevx = 0, prevy = 0;
    float zsign = (mood == M_SHIVER && sinf(now_s * 38) > 0) ? 0.5f : 0;   // chattering only when shivering
    for (int i = 0; i <= N; i++) {
        float t = -1 + 2.0f * i / N, x = mx + t * w, envl = 1 - t * t;
        float y = my + E.curve * 7 * envl + E.omega * 5 * fabsf(sinf(PI * t))
                + E.zig * 3 * tri(t * 3 + zsign) + E.wave * 3 * sinf(t * 5 + now_s * 9);
        if (E.gape > 0.06f) {   // open mouth: fill down to the lower lip in vertical strokes
            float yb = y + E.gape * 9 * powf(max(envl, 0.0f), 0.7f);
            if (yb - y > 1) line(p, x, y, x, yb, 2.5f * 2 * w / N);
        }
        if (i) line(p, prevx, prevy, x, y, 4);
        prevx = x; prevy = y;
    }
}

/* ---------------- hats ---------------- */

static bool ntp_started = false;

// ---- decorations: seasons are windows, not single days ----
// Holidays go by date anywhere; the weather ones (snow, petals, sunglasses, leaves)
// flip with the hemisphere.
static bool in_window(int m, int d, int m0, int d0, int m1, int d1) {
    int v = m * 100 + d, a = m0 * 100 + d0, b = m1 * 100 + d1;
    return a <= b ? (v >= a && v <= b) : (v >= a || v <= b);
}

static int deco_for(int m, int d) {
    if (m == T.bday_m && d == T.bday_d) return D_BIRTHDAY;
    if (in_window(m, d, 12, 31, 1, 1)) return D_NEWYEAR;
    if (in_window(m, d, 12, 1, 12, 30)) return D_HOLIDAYS;
    if (in_window(m, d, 10, 20, 10, 31)) return D_HALLOWEEN;
    if (in_window(m, d, 2, 10, 2, 14)) return D_VALENTINE;
    if (in_window(m, d, 7, 1, 7, 5)) return D_JULY4;
    static const uint8_t north[12] = {D_WINTER, D_WINTER, D_SPRING, D_SPRING, D_SPRING, D_SUMMER,
                                      D_SUMMER, D_SUMMER, D_AUTUMN, D_AUTUMN, D_AUTUMN, D_WINTER};
    int k = north[m - 1];
    if (T.south) k = k == D_WINTER ? D_SUMMER : k == D_SUMMER ? D_WINTER : k == D_SPRING ? D_AUTUMN : D_SPRING;
    return k;
}

static int deco_today(void) {
    if (T.deco == DECO_OFF) return 0;
    if (T.deco != DECO_AUTO) return T.deco;
    time_t now = time(NULL);
    if (now < 1700000000) return 0;   // clock not set yet
    now += T.tz_min * 60;
    struct tm t;
    gmtime_r(&now, &t);
    return deco_for(t.tm_mon + 1, t.tm_mday);
}

// southern Christmas is summer: lights, no snow
static bool deco_snowy(int k) { return k == D_WINTER || (k == D_HOLIDAYS && !(T.deco == DECO_AUTO && T.south)); }

// the old hat numbers, for the OctoPrint sidebar: 1 party, 2 Santa, 3 witch
static int hat_today(void) {
    int k = deco_today();
    return (k == D_NEWYEAR || k == D_BIRTHDAY) ? 1 : k == D_HOLIDAYS ? 2 : k == D_HALLOWEEN ? 3 : 0;
}

enum { P_SNOW, P_PETAL, P_LEAF, P_HEART };
typedef struct { float x, y, ph, rot, vr, vy, r; uint8_t kind; uint32_t col; } deco_part_t;
static deco_part_t dparts[36];
static uint8_t dparts_n = 0;
typedef struct { float x, y, vx, vy; } spark_t;
typedef struct { float x, y, ty, life; uint32_t col; bool burst; spark_t sp[24]; } rocket_t;
static rocket_t rockets[3];
static uint8_t rockets_n = 0;
static int deco_kind = -1;
static float deco_spawn = 0, rocket_t_next = 1, shades = 0, shades_t = 40, bday_conf_t = 6;
static bool shades_on = false;
static uint32_t deco_check_ms = 0;
static int deco_cached = 0;

static void step_deco(float dt) {
    if (millis() - deco_check_ms > 5000 || deco_kind < 0) { deco_check_ms = millis(); deco_cached = deco_today(); }
    int k = deco_cached;
    if (k != deco_kind) { deco_kind = k; dparts_n = 0; rockets_n = 0; }
    // falling / floating things
    bool snow = deco_snowy(k);
    int want = snow ? (k == D_WINTER ? 30 : 24) : k == D_SPRING ? 14 : k == D_AUTUMN ? 12 : k == D_VALENTINE ? 10 : 0;
    if (!want) dparts_n = 0;
    deco_spawn -= dt;
    if (dparts_n < want && deco_spawn <= 0) {
        deco_spawn = k == D_VALENTINE ? 0.5f : 0.25f;
        deco_part_t q = {frand(10, 230), -8, frand(0, 6.28f), frand(0, 6.28f), frand(-2, 2), 0, 0, 0, 0};
        if (snow) { q.kind = P_SNOW; q.r = frand(1, 2.4f); q.vy = frand(14, 30); q.col = 0xE7EEF4; }
        else if (k == D_SPRING) { q.kind = P_PETAL; q.vy = frand(10, 18); q.col = (esp_random() & 1) ? 0xF8BBD0 : 0xF48FB1; }
        else if (k == D_AUTUMN) { static const uint32_t c[] = {0xE65100, 0xF9A825, 0xBF360C, 0xA1887F}; q.kind = P_LEAF; q.vy = frand(16, 26); q.col = c[esp_random() % 4]; }
        else { q.kind = P_HEART; q.y = 250; q.vy = -frand(10, 18); q.r = frand(3.5f, 6); q.col = (esp_random() & 1) ? 0xE53935 : 0xF48FB1; }
        dparts[dparts_n++] = q;
    }
    for (int i = 0; i < dparts_n; i++) {
        deco_part_t & q = dparts[i];
        q.ph += dt; q.y += q.vy * dt; q.rot += q.vr * dt;
        q.x += sinf(q.ph * (q.kind == P_LEAF ? 2.2f : 1.3f)) * (q.kind == P_SNOW ? 8 : 16) * dt;
        if (q.y > 250 || q.y < -20 || q.x < -20 || q.x > 260) dparts[i--] = dparts[--dparts_n];
    }
    // fireworks
    if (k == D_NEWYEAR || k == D_JULY4) {
        rocket_t_next -= dt;
        if (rocket_t_next <= 0 && rockets_n < 3) {
            rocket_t_next = frand(0.9f, 2.2f);
            static const uint32_t ny[] = {0xFFD54F, 0xFFB300, 0xF5F5F5, 0xE53935, 0x4FC3F7}, us[] = {0xE53935, 0xF5F5F5, 0x42A5F5};
            rocket_t & r = rockets[rockets_n++];
            r = {frand(50, 190), 250, frand(30, 95), 0, k == D_JULY4 ? us[esp_random() % 3] : ny[esp_random() % 5], false};
        }
        for (int i = 0; i < rockets_n; i++) {
            rocket_t & r = rockets[i];
            if (!r.burst) {
                r.y -= 180 * dt;
                if (r.y <= r.ty) {
                    r.burst = true; r.life = 1.1f;
                    for (int j = 0; j < 24; j++) {
                        float a = j / 24.0f * 2 * PI + frand(-0.05f, 0.05f), v = frand(62, 70);
                        r.sp[j] = {r.x, r.y, cosf(a) * v, sinf(a) * v};
                    }
                }
            } else {
                r.life -= dt;
                float dr = expf(-dt * 2.2f);
                for (int j = 0; j < 24; j++) {
                    spark_t & sp = r.sp[j];
                    sp.vx *= dr; sp.vy = sp.vy * dr + 28 * dt; sp.x += sp.vx * dt; sp.y += sp.vy * dt;
                }
                if (r.life <= 0) rockets[i--] = rockets[--rockets_n];
            }
        }
    } else rockets_n = 0;
    // summer: puts sunglasses on now and then and keeps them on a while
    if (k == D_SUMMER || k == D_JULY4) {
        shades_t -= dt;
        if (shades_t <= 0 && (mood == M_CALM || mood == M_RIDING || shades_on)) {
            shades_on = !shades_on;
            shades_t = shades_on ? frand(15, 30) : frand(40, 120);
        }
    } else shades_on = false;
    shades += ((shades_on ? 1.0f : 0.0f) - shades) * (1 - expf(-dt * 5));
    // birthday: confetti every so often
    if (k == D_BIRTHDAY) {
        bday_conf_t -= dt;
        if (bday_conf_t <= 0) { bday_conf_t = frand(10, 20); spawn_confetti(); }
    }
}

static void fill_poly(const pen_t & p, const float * xy, int n, lv_color_t col, lv_opa_t opa = LV_OPA_COVER) {
    lv_point_t pts[8];
    n = min(n, 8);
    for (int i = 0; i < n; i++) pts[i] = {X(p, xy[2 * i]), Y(p, xy[2 * i + 1])};
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_color = col; d.bg_opa = opa;
    lv_draw_polygon(p.ctx, &d, pts, n);
}

static void dot(const pen_t & p, float cx, float cy, float rx, float ry, lv_color_t col, lv_opa_t opa) {
    if (rx * p.s < 0.5f) return;
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = col; d.bg_opa = opa; d.radius = LV_RADIUS_CIRCLE;
    lv_area_t a = {X(p, cx - rx), Y(p, cy - ry), X(p, cx + rx), Y(p, cy + ry)};
    lv_draw_rect(p.ctx, &d, &a);
}

static void cline(const pen_t & p, float x0, float y0, float x1, float y1, float w, lv_color_t col, lv_opa_t opa) {
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.color = col; d.width = max(1, (int)lroundf(w * p.s)); d.round_start = 1; d.round_end = 1; d.opa = opa;
    lv_point_t a = {X(p, x0), Y(p, y0)}, b = {X(p, x1), Y(p, y1)};
    lv_draw_line(p.ctx, &d, &a, &b);
}

// a rotated ellipse as a hexagon (petals, leaves, bulbs)
static void blob(const pen_t & p, float cx, float cy, float rx, float ry, float rot, lv_color_t col, lv_opa_t opa) {
    float xy[12], c = cosf(rot), sn = sinf(rot);
    for (int i = 0; i < 6; i++) {
        float a = i * PI / 3, ex = cosf(a) * rx, ey = sinf(a) * ry;
        xy[2 * i] = cx + ex * c - ey * sn; xy[2 * i + 1] = cy + ex * sn + ey * c;
    }
    fill_poly(p, xy, 6, col, opa);
}

/* decorations behind the face */
static void draw_deco_back(const pen_t & p) {
    for (int i = 0; i < dparts_n; i++) {
        const deco_part_t & q = dparts[i];
        lv_color_t c = lv_color_hex(q.col);
        switch (q.kind) {
            case P_SNOW: dot(p, q.x, q.y, q.r, q.r, c, 215); break;
            case P_PETAL: blob(p, q.x, q.y, 3.6f, 2, q.rot, c, LV_OPA_COVER); break;
            case P_LEAF:
                blob(p, q.x, q.y, 5.5f, 2.8f, q.rot, c, LV_OPA_COVER);
                cline(p, q.x + cosf(q.rot) * 5, q.y + sinf(q.rot) * 5, q.x + cosf(q.rot) * 8, q.y + sinf(q.rot) * 8, 1.2f, c, LV_OPA_COVER);
                break;
            case P_HEART: {
                lv_opa_t op = (lv_opa_t)(230 * clampf((q.y - 10) / 60, 0, 1));
                float r = q.r;
                dot(p, q.x - r * 0.5f, q.y, r * 0.55f, r * 0.55f, c, op);
                dot(p, q.x + r * 0.5f, q.y, r * 0.55f, r * 0.55f, c, op);
                float tri_[] = {q.x - r * 1.03f, q.y + r * 0.1f, q.x + r * 1.03f, q.y + r * 0.1f, q.x, q.y + r * 1.1f};
                fill_poly(p, tri_, 3, c, op);
                break;
            }
        }
    }
    for (int i = 0; i < rockets_n; i++) {
        const rocket_t & r = rockets[i];
        lv_color_t c = lv_color_hex(r.col);
        if (!r.burst) { dot(p, r.x, r.y, 1.8f, 1.8f, c, LV_OPA_COVER); cline(p, r.x, r.y + 3, r.x, r.y + 10, 1.6f, c, 100); continue; }
        lv_opa_t op = (lv_opa_t)(255 * clampf(r.life / 0.7f, 0, 1));
        for (int j = 0; j < 24; j++) {
            const spark_t & sp = r.sp[j];
            cline(p, sp.x, sp.y, sp.x - sp.vx * 0.12f, sp.y - sp.vy * 0.12f, 1.4f, c, op / 2);
            dot(p, sp.x, sp.y, 1.9f, 1.9f, c, op);
        }
    }
}

// the string of holiday lights along the top
static lv_color_t bulb_color(int i, lv_color_t theme) {
    static const uint32_t classic[] = {0xE53935, 0x43A047, 0x1E88E5, 0xFDD835, 0xFB8C00}, candy[] = {0xE53935, 0xF5F5F5};
    switch (T.lights) {
        case 1: return lv_color_hex(0xFFD27A);
        case 2: return theme;
        case 3: return lv_color_hex(candy[i % 2]);
        case 4: return lv_color_hsv_to_rgb((uint16_t)fmodf(i * 40 + now_s * 40, 360), 75, 100);
        default: return lv_color_hex(classic[i % 5]);
    }
}

static float bulb_level(int i, int n) {
    switch (T.anim) {
        case 3: return 1;
        case 1: { float c = max(0.0f, cosf((float)i / n * 2 * PI * 2 - now_s * 4)); return 0.25f + 0.75f * c * c; }
        case 2: return 0.35f + 0.65f * (0.5f + 0.5f * sinf(now_s * 1.6f + (i % 2) * PI));
        default: {
            float h = sinf(i * 91.7f + floorf(now_s * 3 + i * 0.37f) * 13.1f) * 43758.5f;
            h -= floorf(h);
            return h < 0.22f ? 0.25f : 1;
        }
    }
}

static void draw_lights(const pen_t & p) {
    const int N = 6;
    float hk[N + 1][2];
    for (int i = 0; i <= N; i++) { float a = PI * (1.16f + 0.68f * i / N); hk[i][0] = 120 + cosf(a) * 116; hk[i][1] = 122 + sinf(a) * 116; }
    lv_color_t wire = lv_color_hex(0x2E3B2F);
    int bi = 0;
    for (int j = 0; j < N; j++) {
        float mx = (hk[j][0] + hk[j + 1][0]) / 2, my = (hk[j][1] + hk[j + 1][1]) / 2, dx = 120 - mx, dy = 122 - my, dl = sqrtf(dx * dx + dy * dy);
        float c0 = mx + dx / dl * 11, c1 = my + dy / dl * 11;
        float px = hk[j][0], py = hk[j][1];
        for (int s2 = 1; s2 <= 8; s2++) {   // the sagging wire
            float t = s2 / 8.0f, u = 1 - t;
            float x = u * u * hk[j][0] + 2 * u * t * c0 + t * t * hk[j + 1][0], y = u * u * hk[j][1] + 2 * u * t * c1 + t * t * hk[j + 1][1];
            cline(p, px, py, x, y, 1.6f, wire, LV_OPA_COVER);
            px = x; py = y;
        }
        float rot = atan2f(dy, dx) - PI / 2;
        for (float t : {0.3f, 0.7f}) {
            float u = 1 - t;
            float x = u * u * hk[j][0] + 2 * u * t * c0 + t * t * hk[j + 1][0], y = u * u * hk[j][1] + 2 * u * t * c1 + t * t * hk[j + 1][1];
            float lv = bulb_level(bi, N * 2);
            lv_color_t col = bulb_color(bi, p.fc);
            float bxp = x - sinf(rot) * 6.5f, byp = y + cosf(rot) * 6.5f;   // bulb hangs off the wire
            cline(p, x, y, x - sinf(rot) * 2, y + cosf(rot) * 2, 3.4f, wire, LV_OPA_COVER);
            dot(p, bxp, byp, 8.5f, 8.5f, col, (lv_opa_t)(56 * lv));        // glow
            blob(p, bxp, byp, 4.8f, 3.4f, rot + PI / 2, col, (lv_opa_t)(90 + 165 * lv));
            bi++;
        }
    }
}

static void draw_bats(const pen_t & p) {
    for (int i = 0; i < 3; i++) {
        float t = fmodf(now_s * 0.09f + i / 3.0f, 1), x = -20 + t * 280, y = 46 + i * 16 + sinf(now_s * 1.7f + i * 2) * 10, f = sinf(now_s * 14 + i * 3);
        dot(p, x, y, 2.4f, 2.4f, p.fc, LV_OPA_COVER);
        cline(p, x - 2, y, x - 6, y - 3 - f * 4, 1.8f, p.fc, LV_OPA_COVER); cline(p, x - 6, y - 3 - f * 4, x - 11, y - f * 2, 1.8f, p.fc, LV_OPA_COVER);
        cline(p, x + 2, y, x + 6, y - 3 - f * 4, 1.8f, p.fc, LV_OPA_COVER); cline(p, x + 6, y - 3 - f * 4, x + 11, y - f * 2, 1.8f, p.fc, LV_OPA_COVER);
    }
}

static void draw_flower(const pen_t & p, float x, float y) {
    for (int i = 0; i < 5; i++) { float a = i / 5.0f * 2 * PI + 0.3f; dot(p, x + cosf(a) * 4.6f, y + sinf(a) * 4.6f, 3.4f, 3.4f, lv_color_hex(0xF48FB1), LV_OPA_COVER); }
    dot(p, x, y, 2.8f, 2.8f, lv_color_hex(0xFDD835), LV_OPA_COVER);
}

static void draw_shades(const pen_t & p, float cx, float ey, float sx) {
    if (shades < 0.02f) return;
    float drop = (1 - shades) * -40;
    lv_opa_t op = (lv_opa_t)(255 * clampf(shades * 1.5f, 0, 1));
    for (int s2 = -1; s2 <= 1; s2 += 2) {
        float x = cx + s2 * 54 * sx, t = ey - 11 + drop;
        float lens[] = {x - 31, t, x + 31, t, x + 28, t + 14, x + 14, t + 25, x - 14, t + 25, x - 28, t + 14};
        fill_poly(p, lens, 6, p.fc, op);
        cline(p, x - 14, ey - 5 + drop, x - 6, ey - 5 + drop, 2, lv_color_black(), op * 7 / 10);   // glint
    }
    cline(p, cx - 24 * sx, ey - 8 + drop, cx, ey - 12 + drop, 3, p.fc, op);
    cline(p, cx, ey - 12 + drop, cx + 24 * sx, ey - 8 + drop, 3, p.fc, op);
}


static void draw_hat(const pen_t & p, float cx, float cy, float sx, float sy) {
    int h = hat_today();
    if (!h) return;
    if (h == 2) {               // Santa hat: white trim and pompom
        float top = cy - 50 * sy;
        float c[] = {cx - 30, top, cx + 30, top, cx + 44, top - 34};
        fill_poly(p, c, 3, p.fc);
        fill_rect(p, cx - 34, top - 2, cx + 34, top + 8, lv_color_hex(0xE7EEF4));
        fill_ellipse(p, cx + 46, top - 34, 7, 7, lv_color_hex(0xE7EEF4));
        return;
    }
    lv_color_t black = lv_color_black();
    float top = cy - 50 * sy;   // sits above the eyes
    if (h == 1) {               // party hat with stripes and a pompom
        float c[] = {cx - 22, top, cx + 22, top, cx + 6, top - 46};
        fill_poly(p, c, 3, p.fc);
        {
            lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d); d.color = black; d.width = max(1, (int)lroundf(3 * p.s));
            lv_point_t a = {X(p, cx - 14), Y(p, top - 14)}, b = {X(p, cx + 15), Y(p, top - 14)};
            lv_draw_line(p.ctx, &d, &a, &b);
            lv_point_t e2 = {X(p, cx - 6), Y(p, top - 30)}, f2 = {X(p, cx + 11), Y(p, top - 30)};
            lv_draw_line(p.ctx, &d, &e2, &f2);
        }
        fill_ellipse(p, cx + 6, top - 48, 5, 5, p.fc);
    } else if (h == 2) {        // Santa hat flopping to the side
        float c[] = {cx - 30, top, cx + 30, top, cx + 44, top - 34};
        fill_poly(p, c, 3, p.fc);
        fill_rect(p, cx - 34, top - 2, cx + 34, top + 8, p.fc);
        {
            lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d); d.color = black; d.width = max(1, (int)lroundf(2 * p.s));
            lv_point_t a = {X(p, cx - 34), Y(p, top - 3)}, b = {X(p, cx + 34), Y(p, top - 3)};
            lv_draw_line(p.ctx, &d, &a, &b);
        }
        fill_ellipse(p, cx + 46, top - 34, 7, 7, p.fc);
    } else if (h == 3) {        // witch hat
        float c[] = {cx - 16, top - 2, cx + 16, top - 2, cx + 12, top - 58};
        fill_poly(p, c, 3, p.fc);
        fill_ellipse(p, cx, top, 42, 6, p.fc);
        lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d); d.color = black; d.width = max(1, (int)lroundf(3 * p.s));
        lv_point_t a = {X(p, cx - 15), Y(p, top - 10)}, b = {X(p, cx + 15), Y(p, top - 10)};
        lv_draw_line(p.ctx, &d, &a, &b);
    }
}

static void draw_face(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    pen_t p;
    p.ctx = lv_event_get_draw_ctx(e);
    p.ox = obj->coords.x1; p.oy = obj->coords.y1;
    p.s = lv_obj_get_width(obj) / 240.0f;
    p.fc = lv_theme_color();

    // speech bubble: the face drops a little to make room, and talks for the first two seconds
    bool bubble = p.s > 0.8f && (int32_t)(bubble_until - millis()) > 0 && moonraker.data.msg[0];
    bubble_k += ((bubble ? 1.0f : 0.0f) - bubble_k) * 0.25f;
    bool big = p.s > 0.8f;                 // decorations only on the full-size face
    if (big) draw_deco_back(p);
    float jit = min(3.0f, vib * 6) * E.zig;
    if (mood == M_ERROR) jit = 1.5f;                  // trembling
    if (mood == M_HEATING) jit = 0.4f + heat_effort * 1.8f;   // straining, harder as it gets close
    if (mood == M_NERVOUS) jit = max(jit, 0.8f);
    if (mood == M_WINDY) jit = max(jit, 0.6f);
    float bs = BOUNCE * T.sense * SENSE_K;
    float cx = 120 + clampf(hx - bs * bx[0], -34, 34) + frand(-jit, jit), cy = 118 + clampf(hy + bs * bx[2], -30, 30) + frand(-jit, jit);
    float hsq = hs + QF.sq;
    float sx = 1 - hsq * 0.5f, sy = 1 + hsq;
    cx += QF.dx; cy += QF.dy;
    cy += 14 * bubble_k;
    if (act == ACT_REPORT && p.s > 0.8f) cy -= 34;
    if (t_giggle > 0) cy -= fabsf(sinf(now_s * 14)) * 6;
    if (t_ready > 0) cy -= fabsf(sinf(now_s * 6)) * 4;   // bouncing, ready to go
    if (t_celebrate > 0) cy -= fabsf(sinf(now_s * 9)) * 8;
    cy += sinf(now_s * 1.6f) * 1.5f * (1 - clampf(E.open * 4, 0, 1));   // breathing while asleep
    float blinkk = blink_closing > 0 ? sinf(PI * (1 - blink_closing / 0.16f)) : 0;
    float open = E.open * (1 - blinkk);
    // long prints wear it out: heavier lids after 2 h, up to 35 % lower by 8 h (big moves still wake it up)
    const moonraker_data_t & dd = moonraker.data;
    if (dd.printing && (mood == M_CALM || mood == M_RIDING || mood == M_FOCUS || mood == M_WINDY)) {
        float tired = clampf((dd.print_time / 3600.0f - 2) / 6, 0, 0.35f);
        open *= 1 - tired;
    }
    // acting out busy states
    float tiltL = 0, tiltR = 0;
    if (act == ACT_QGL) { float t = sinf(now_s * 1.4f) * 7; tiltL = t; tiltR = -t; }   // corners leveling out
    if (act == ACT_PROBING) cy += fabsf(sinf(now_s * PI * 1.6f)) * 5;                  // tap, tap, tap
    if (act == ACT_CLEANING) cx += sinf(now_s * 14) * 6;                                // scrub scrub
    // quirks layer on top of the mood (the mood's own expression stays underneath)
    expr_t keep = E;
    E.curve += QF.curve; E.w = max(4.0f, E.w + QF.w); E.cheek = max(E.cheek, QF.cheek);
    E.gape = max(E.gape, QF.gape);
    E.omega *= 1 - clampf(QF.gape / 0.4f, 0, 1);   // an open "o" mouth, not the cat "w"
    draw_eye(p, cx - 54 * sx, cy - 14 * sy + tiltL, -1, sx, clampf(open * QF.open_l, 0, 1.1f), QF.look_y);
    draw_eye(p, cx + 54 * sx, cy - 14 * sy + tiltR, 1, sx, clampf(open * QF.open_r, 0, 1.1f), QF.look_y);
    float talk = 0;
    if (bubble && millis() - bubble_since < 2000) talk = 0.6f * fabsf(sinf(now_s * 11));
    E.gape = max(E.gape, talk);
    draw_mouth(p, cx, cy + 22 * sy);
    E = keep;
    for (int i = 0; i < 3; i++) {       // humming: little notes floating up
        if (notes[i].life <= 0) continue;
        lv_opa_t op = (lv_opa_t)(255 * clampf(notes[i].life / 0.6f, 0, 1));
        float nx = notes[i].x, ny = notes[i].y;
        lv_draw_rect_dsc_t nd; lv_draw_rect_dsc_init(&nd); nd.bg_color = p.fc; nd.bg_opa = op; nd.radius = LV_RADIUS_CIRCLE;
        lv_area_t na = {X(p, nx - 4), Y(p, ny - 3), X(p, nx + 4), Y(p, ny + 3)};
        lv_draw_rect(p.ctx, &nd, &na);
        line(p, nx + 3, ny, nx + 3, ny - 13, 2, op);
        line(p, nx + 3, ny - 13, nx + 8, ny - 9, 2, op);
    }
    if (bubble_k > 0.05f) {
        lv_opa_t op = (lv_opa_t)(255 * clampf(bubble_k, 0, 1));
        float top = 26 - (1 - bubble_k) * 10;
        lv_draw_rect_dsc_t bd; lv_draw_rect_dsc_init(&bd);
        bd.bg_color = lv_color_black(); bd.bg_opa = op;
        bd.border_color = p.fc; bd.border_width = max(1, (int)lroundf(2 * p.s)); bd.border_opa = op;
        bd.radius = (lv_coord_t)(12 * p.s);
        lv_area_t a = {X(p, 44), Y(p, top), X(p, 196), Y(p, top + 48)};
        lv_draw_rect(p.ctx, &bd, &a);
        // tail toward Coaster
        line(p, 128, top + 48, 122, top + 58, 2, op);
        line(p, 122, top + 58, 118, top + 48, 2, op);
        lv_draw_label_dsc_t ld; lv_draw_label_dsc_init(&ld);
        ld.color = lv_color_hex(0xE7EEF4); ld.opa = op; ld.font = &ui_font_InterSemiBold14; ld.align = LV_TEXT_ALIGN_CENTER;
        ld.line_space = -1;
        lv_area_t ta = {X(p, 52), Y(p, top + 6), X(p, 188), Y(p, top + 43)};
        lv_draw_label(p.ctx, &ld, &ta, moonraker.data.msg, NULL);
    }

    // sweat drop while the nozzle is hot
    const moonraker_data_t & d = moonraker.data;
    // sweat drop: appears from 120 °C and grows with the nozzle temperature
    if (d.nozzle_actual >= 120 && mood != M_SLEEPY) {
        float sz = clampf((d.nozzle_actual - 120) / 130.0f, 0, 1) * 0.8f + 0.5f;
        float q = fmodf(now_s * 0.5f, 1), dx = cx + 86 * sx, dy = cy - 40 + q * 26;
        lv_opa_t op = (lv_opa_t)(255 * (1 - q * 0.8f));
        lv_draw_rect_dsc_t rd; lv_draw_rect_dsc_init(&rd);
        rd.bg_color = p.fc; rd.bg_opa = op; rd.radius = LV_RADIUS_CIRCLE;
        lv_area_t a = {X(p, dx - 4 * sz), Y(p, dy - 3 * sz), X(p, dx + 4 * sz), Y(p, dy + 5 * sz)};
        lv_draw_rect(p.ctx, &rd, &a);
        line(p, dx, dy - 8 * sz, dx, dy - 2 * sz, 3 * sz, op);
    }
    if (big) {
        draw_shades(p, cx, cy - 14 * sy, sx);
        if (deco_kind == D_SPRING) draw_flower(p, cx - 38 * sx, cy - 50 * sy);
        if (deco_kind == D_HALLOWEEN) draw_bats(p);
        if (deco_kind == D_HOLIDAYS) draw_lights(p);
    }
    draw_hat(p, cx, cy, sx, sy);
    if (act == ACT_REPORT && p.s > 0.8f) {
        char l1[40], l2[48], l3[40];
        if (report.done) snprintf(l1, sizeof(l1), "Print done!");
        else snprintf(l1, sizeof(l1), "Stopped at %u%%", report.progress);
        snprintf(l2, sizeof(l2), "%u scream%s \xc2\xb7 peak %.1f g", report.screams, report.screams == 1 ? "" : "s", report.peak);
        snprintf(l3, sizeof(l3), "dizzy %ux \xc2\xb7 %u jolt%s", report.dizzies, report.jolts, report.jolts == 1 ? "" : "s");
        lv_draw_label_dsc_t ld; lv_draw_label_dsc_init(&ld);
        ld.color = p.fc; ld.align = LV_TEXT_ALIGN_CENTER;
        ld.font = &ui_font_InterSemiBold18;
        lv_area_t a1 = {X(p, 30), Y(p, 160), X(p, 210), Y(p, 182)};
        lv_draw_label(p.ctx, &ld, &a1, l1, NULL);
        ld.font = &ui_font_InterSemiBold14; ld.color = lv_color_hex(0xE7EEF4);
        lv_area_t a2 = {X(p, 30), Y(p, 184), X(p, 210), Y(p, 202)};
        lv_draw_label(p.ctx, &ld, &a2, l2, NULL);
        lv_area_t a3 = {X(p, 40), Y(p, 202), X(p, 200), Y(p, 220)};
        lv_draw_label(p.ctx, &ld, &a3, l3, NULL);
    }
    if (act != ACT_NONE && act != ACT_PID && p.s > 0.8f && bubble_k < 0.5f) {
        lv_draw_label_dsc_t ld; lv_draw_label_dsc_init(&ld);
        ld.color = p.fc; ld.font = &ui_font_InterSemiBold18; ld.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t a = {X(p, 30), Y(p, 184), X(p, 210), Y(p, 208)};
        lv_draw_label(p.ctx, &ld, &a, ACT_LABEL[act], NULL);
    }
    if (mood == M_HEATING) {
        // steam puffing off the top, faster and bigger the harder it works
        for (int i = 0; i < 3; i++) {
            float q = fmodf(now_s * (0.8f + heat_effort) + i / 3.0f, 1);
            float px = cx + (i - 1) * 26 * sx + sinf(now_s * 3 + i) * 3, py = cy - 52 - q * 26;
            float len = 4 + 6 * heat_effort * (1 - q);
            line(p, px, py, px, py - len, 3, (lv_opa_t)(255 * sinf(q * PI) * (0.4f + 0.6f * heat_effort)));
        }
        // what it's heating, on the full-size face
        if (p.s > 0.8f) {
            char t[24];
            bool noz = d.nozzle_target > 0 && d.nozzle_actual < d.nozzle_target - 3;
            if (noz) snprintf(t, sizeof(t), "%d / %d\xe2\x84\x83", d.nozzle_actual, d.nozzle_target);
            else snprintf(t, sizeof(t), "Bed %d / %d\xe2\x84\x83", d.bed_actual, d.bed_target);
            lv_draw_label_dsc_t ld; lv_draw_label_dsc_init(&ld);
            ld.color = p.fc; ld.font = &ui_font_InterSemiBold18; ld.align = LV_TEXT_ALIGN_CENTER;
            lv_area_t a = {X(p, 40), Y(p, 180), X(p, 200), Y(p, 204)};
            lv_draw_label(p.ctx, &ld, &a, t, NULL);
        }
    }
    if (mood == M_SLEEPY) {
        lv_draw_label_dsc_t ld; lv_draw_label_dsc_init(&ld);
        ld.color = p.fc; ld.font = p.s > 0.7f ? &ui_font_InterSemiBold18 : &ui_font_InterSemiBold14;
        for (int i = 0; i < 3; i++) {
            float z = fmodf(now_s * 0.45f + i / 3.0f, 1);
            ld.opa = (lv_opa_t)(255 * sinf(z * PI) * clampf(mood_t / 1.5f, 0, 1));
            lv_area_t a = {X(p, 160 + z * 26), Y(p, 80 - z * 36), X(p, 180 + z * 26), Y(p, 104 - z * 36)};
            lv_draw_label(p.ctx, &ld, &a, "z", NULL);
        }
    }
    for (int i = 0; i < confetti_n; i++) {
        const bit_t & b = confetti[i];
        if (b.life <= 0) continue;
        float dx = cosf(b.r) * 3, dy = sinf(b.r) * 3;
        line(p, b.x - dx, b.y - dy, b.x + dx, b.y + dy, 3, (lv_opa_t)(255 * clampf(b.life, 0, 1)));
    }
}

static void face_event(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DRAW_MAIN) draw_face(e);
    else if (code == LV_EVENT_DELETE) coaster_forget(lv_event_get_target(e));
}

lv_obj_t * coaster_create(lv_obj_t * parent, int size) {
    lv_obj_t * o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, size, size);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(o, face_event, LV_EVENT_ALL, NULL);
    for (int i = 0; i < MAX_FACES; i++) if (!faces[i]) { faces[i] = o; break; }
    return o;
}

void coaster_forget(lv_obj_t * obj) {
    for (int i = 0; i < MAX_FACES; i++) if (faces[i] == obj) faces[i] = NULL;
}

/* ---------------- loop ---------------- */

void coaster_init(void) {
    load_tuning();
    blink_t = frand(2, 5);
    personality_init();
    feel_load();
}

void coaster_loop(void) {
    if (reload_pending) { reload_pending = false; load_tuning(); }
    if (!ntp_started && WiFi.status() == WL_CONNECTED) {   // the date, for seasonal hats
        configTime(0, 0, "pool.ntp.org", "time.google.com");
        ntp_started = true;
    }
    static uint32_t last_ms = 0, frame_ms = 0;
    uint32_t ms = millis();
    const moonraker_data_t & d = moonraker.data;
    watch_printer(d);

    // physics at the sensor rate: consume every queued sample; without samples keep time moving
    uint32_t w = ring_w;
    if (w - ring_r > RING) ring_r = w - RING;   // fell behind: drop the oldest
    int steps = 0;
    while (ring_r != w) {
        float a[3] = {ring[ring_r % RING][0], ring[ring_r % RING][1], ring[ring_r % RING][2]};
        ring_r++;
        now_s += SAMPLE_DT;
        sense(a, SAMPLE_DT); pick_mood(SAMPLE_DT, d); step_body(SAMPLE_DT); step_expr(SAMPLE_DT); step_deco(SAMPLE_DT);
        steps++;
    }
    if (!steps) {
        float el = (ms - last_ms) / 1000.0f;
        static const float zero[3] = {0, 0, 0};
        int n = min(20, (int)(el / SAMPLE_DT));
        if (n > 0 && (ms - last_ms) > 30) {   // no sensor (KNOMI 1) or it stalled
            for (int i = 0; i < n; i++) {
                now_s += SAMPLE_DT;
                sense(zero, SAMPLE_DT); pick_mood(SAMPLE_DT, d); step_body(SAMPLE_DT); step_expr(SAMPLE_DT); step_deco(SAMPLE_DT);
            }
            last_ms = ms;
        }
    } else {
        last_ms = ms;
    }

    if (ms - frame_ms >= FRAME_MS) {
        frame_ms = ms;
        lv_obj_t * scr = lv_scr_act();
        for (int i = 0; i < MAX_FACES; i++) {
            lv_obj_t * f = faces[i];
            if (f && lv_obj_get_screen(f) == scr && lv_obj_is_visible(f)) lv_obj_invalidate(f);
        }
    }
}

String coaster_state_json(void) {
    char buf[1400];
    int n = snprintf(buf, sizeof(buf), "{\"feel\":%.2f,\"feeling\":\"%s\",\"streak\":%d,\"prints\":%u,\"why\":[",
                     H, feel_name(), streak, prints_done);
    for (int i = 0; i < fnotes_n && n < 440; i++)
        n += snprintf(buf + n, sizeof(buf) - n, "%s{\"t\":\"%s\",\"d\":%d}", i ? "," : "", fnotes[i].why, fnotes[i].d);
    n += snprintf(buf + n, sizeof(buf) - n, "],\"likes\":[");
    bool first = true;
    for (int i = 0; i < LK_COUNT; i++) if (like[i] >= 50) { n += snprintf(buf + n, sizeof(buf) - n, "%s\"%s\"", first ? "" : ",", LIKE_NAMES[i]); first = false; }
    n += snprintf(buf + n, sizeof(buf) - n, "],\"dislikes\":[");
    first = true;
    for (int i = 0; i < LK_COUNT; i++) if (like[i] <= -50) { n += snprintf(buf + n, sizeof(buf) - n, "%s\"%s\"", first ? "" : ",", LIKE_NAMES[i]); first = false; }
    n += snprintf(buf + n, sizeof(buf) - n, "],\"learned\":[");
    first = true;
    for (int i = 0; i < LK_COUNT; i++) if (abs(learned[i]) >= 10) {
        n += snprintf(buf + n, sizeof(buf) - n, "%s{\"t\":\"%s\",\"d\":%d}", first ? "" : ",", LIKE_NAMES[i], learned[i]); first = false;
    }
    n += snprintf(buf + n, sizeof(buf) - n, "],\"open\":%.2f,\"habit\":{\"n\":%u,\"min\":%.0f,\"hour\":%.1f,\"peak\":%.2f},",
                  openness, habit_n, habit_n ? exp2f(habit_len) : 0.0f, habit_hour(), habit_peak);
    snprintf(buf + n, sizeof(buf) - n,
             "\"mood\":\"%s\",\"quirk\":\"%s\",\"thrill\":%.2f,\"buzz\":%.2f,\"dizzy\":%.2f,\"used_to\":%.2f,\"motion\":%.2f,\"idle\":%s,\"sensor\":%s}",
             MOOD_NAMES[mood], QUIRK_NAMES[quirk], thrill, vib, t_dizzy > 0 ? 1.0f : dizzy_meter / T.dizzy, base, env,
             T.idle ? "true" : "false",
#ifdef LIS2DW_SUPPORT
             "true"
#else
             "false"
#endif
             );
    return String(buf);
}

void coaster_event_ready(float secs) { t_ready = secs > 0 ? secs : 0; }

// touch: a tap pokes it, holding keeps tickling (LVGL task)
void coaster_poke(void) {
    // likes a little attention; too much gets annoying
    static uint32_t pokes[8]; static uint8_t pi = 0;
    uint32_t now = millis(), oldest = pokes[pi];
    pokes[pi] = now; pi = (pi + 1) % 8;
    int limit_ms = eff(LK_POKES) <= -50 ? 40000 : eff(LK_POKES) >= 50 ? 6000 : 15000;
    if (oldest && now - oldest < (uint32_t)limit_ms && t_mad <= 0) { t_mad = 4; t_giggle = 0; feel(-0.03f, "poked too much"); head_kick(0, 50); return; }
    if (t_mad > 0) return;
    feel(0.005f + 0.01f * L(LK_POKES), NULL);
    static uint32_t poke_day_ms = 0;   // poked on many days: gets used to it
    if (millis() - poke_day_ms > 86400000UL || !poke_day_ms) { poke_day_ms = millis(); learn(LK_POKES, 2); feel_dirty = true; }
    t_giggle = max(t_giggle, 1.6f);
    head_kick(frand(-50, 50), -45);
}

// the screen logic tells Coaster which state it's standing in for (GIF slot shown on the main screen)
void coaster_set_act(int slot) {
    int a = ACT_NONE;
    switch (slot) {
        case GIF_SLOT_HOMING:   a = ACT_HOMING; break;
        case GIF_SLOT_PROBING:  a = ACT_PROBING; break;
        case GIF_SLOT_QGLING:   a = ACT_QGL; break;
        case GIF_SLOT_SHAPING:  a = ACT_SHAPING; break;
        case GIF_SLOT_PID:      a = ACT_PID; break;
        case GIF_SLOT_CLEANING: a = ACT_CLEANING; break;
        case GIF_SLOT_FILAMENT: a = ACT_FILAMENT; break;
        case GIF_SLOT_PRINT:    a = ACT_START; break;
        case GIF_SLOT_PRINT_OK: a = ACT_DONE; break;
        case GIF_SLOT_PRINTED:  a = report.valid ? ACT_REPORT : ACT_NONE; break;
    }
    if (a != act) Serial.printf("coaster: %s\r\n", a ? ACT_LABEL[a] : "idle");
    act = a;
}

// compact state for the OctoPrint plugin's sidebar Coaster
String coaster_plugin_json(void) {
    char buf[240];
    int n = snprintf(buf, sizeof(buf), "{\"mood\":\"%s\",\"feel\":\"%s\",\"hat\":%d", MOOD_NAMES[mood], feel_name(), hat_today());
    if (report.valid)
        n += snprintf(buf + n, sizeof(buf) - n, ",\"report\":{\"done\":%s,\"progress\":%u,\"screams\":%u,\"dizzies\":%u,\"jolts\":%u,\"peak\":%.2f,\"secs\":%u}",
                      report.done ? "true" : "false", report.progress, report.screams, report.dizzies, report.jolts, report.peak, (unsigned)report.secs);
    snprintf(buf + n, sizeof(buf) - n, "}");
    return String(buf);
}
