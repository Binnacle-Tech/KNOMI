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
    uint8_t hat;   // 0 seasonal, 1 off, 2 party, 3 santa, 4 witch
} coaster_tune_t;
static const coaster_tune_t TUNE_DEF = {2.4f, 0.28f, 1.0f, 12.0f, 0.9f, 5.0f, 20.0f, true, 0};  // Coaster is the mascot: on by default
static coaster_tune_t T = TUNE_DEF;
static volatile bool reload_pending = false;

static void load_tuning(void) {
    T = TUNE_DEF;
    File f = LittleFS.open(COASTER_PATH, "r");
    if (!f) return;
    StaticJsonDocument<512> d;
    if (deserializeJson(d, f) == DeserializationError::Ok) {
        T.wobble = constrain(d["wobble"] | T.wobble, 0.8f, 6.0f);
        T.settle = constrain(d["settle"] | T.settle, 0.05f, 1.0f);
        T.sense  = constrain(d["sense"]  | T.sense,  0.2f, 3.0f);
        T.habit  = constrain(d["habit"]  | T.habit,  2.0f, 60.0f);
        T.scare  = constrain(d["scare"]  | T.scare,  0.3f, 2.5f);
        T.dizzy  = constrain(d["dizzy"]  | T.dizzy,  1.0f, 20.0f);
        T.sleep  = constrain(d["sleep"]  | T.sleep,  5.0f, 120.0f);
        T.idle   = d["idle"] | true;
        T.hat    = constrain((int)(d["hat"] | 0), 0, 4);
    }
    f.close();
}

bool coaster_idle_enabled(void) { return true; }   // every face is Coaster now

// Web task: save tuning from the /coaster page (unknown keys dropped, values clamped on load)
const char * coaster_save_json(const char * json, size_t len) {
    if (len > 1024) return "Too large";
    StaticJsonDocument<512> in;
    if (deserializeJson(in, json, len) != DeserializationError::Ok) return "Not valid JSON";
    StaticJsonDocument<384> out;
    static const char * keys[] = {"wobble", "settle", "sense", "habit", "scare", "dizzy", "sleep"};
    for (const char * k : keys) if (in[k].is<float>()) out[k] = in[k].as<float>();
    if (in["hat"].is<int>()) out["hat"] = constrain(in["hat"].as<int>(), 0, 4);
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
    StaticJsonDocument<384> d;
    File f = LittleFS.open(COASTER_PATH, "r");
    if (f) { deserializeJson(d, f); f.close(); }
    d["idle"] = on;
    f = LittleFS.open(COASTER_PATH, "w");
    if (f) { serializeJson(d, f); f.close(); }
    T.idle = on;   // takes effect right away; the rest reloads on the LVGL task
    coaster_request_reload();
}

String coaster_tuning_json(void) {
    char buf[200];
    snprintf(buf, sizeof(buf),
             "{\"wobble\":%.2f,\"settle\":%.2f,\"sense\":%.2f,\"habit\":%.0f,\"scare\":%.2f,\"dizzy\":%.1f,\"sleep\":%.0f,\"idle\":%s,\"hat\":%d}",
             T.wobble, T.settle, T.sense, T.habit, T.scare, T.dizzy, T.sleep, T.idle ? "true" : "false", T.hat);
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
    M_HUNGRY, M_WINDY, M_NERVOUS, M_BRACE, M_LEVEL, M_SCRUB, M_COUNT
};
static const char * MOOD_NAMES[M_COUNT] = {
    "calm", "riding", "excited", "screaming", "startled", "elevator", "sleepy",
    "bored", "shivering", "dizzy", "giggle", "celebrate", "ready",
    "sad", "shocked", "lonely", "confused", "heating up", "cooling off", "focused", "almost there",
    "hungry", "windy", "hanging on", "bracing", "leveling", "scrubbing",
};

// sensing
static float lp[3], mfl[3], hist[6][3];
static uint8_t hist_i = 0;
static float env = 0, base = 0, vib = 0, step = 0, thrill = 0, scream_t = 0, dizzy_meter = 0, still_t = 0;
static bool from_rest = false;
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
    /* bracing   */ {0.08f, 1.0f,  0, 0, 10, 0.0f, 0.0f, 0.0f, 0.5f, 0, -0.3f},
    /* leveling  */ {0.5f,  1.0f,  0, 0, 12, 0.0f, 0.0f, 0.0f, 0, 0, 0},
    /* scrubbing */ {0.8f,  1.0f,  0.6f, 0, 12, 0.5f, 0.0f, 0.0f, 0, 0.6f, 0},
    /* nervous   */ {0.9f,  0.9f,  0, 0, 10, 0.0f, 0.0f, 0.0f, 0.6f, 0, -0.3f},
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
    env = max(mag, env * expf(-dt / 0.6f));
    vib += (sqrtf(hp) - vib) * (1 - expf(-dt / 0.6f));
    // sudden step: change of the 40 Hz signal over 25 ms, starting from rest (endstop hits)
    float k40 = 1 - expf(-2 * PI * 40 * dt);
    for (int i = 0; i < 3; i++) mfl[i] += (a[i] - mfl[i]) * k40;
    const float * o = hist[hist_i];   // oldest of the last 6 samples
    step = sqrtf(sq(mfl[0] - o[0]) + sq(mfl[1] - o[1]) + sq(mfl[2] - o[2]));
    from_rest = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]) < 0.3f;
    memcpy(hist[hist_i], mfl, sizeof(mfl));
    hist_i = (hist_i + 1) % 6;
    base += (env - base) * (1 - expf(-dt / max(1.0f, T.habit)));
    thrill = max(0.0f, env - 0.7f * base) / T.scare;
    still_t = (env < 0.03f && vib < 0.03f) ? still_t + dt : 0;
}

static void head_kick(float vx, float vy) { hvx += vx; hvy += vy; }

static void pick_mood(float dt, const moonraker_data_t & d) {
    t_startle = max(0.0f, t_startle - dt); t_dizzy = max(0.0f, t_dizzy - dt);
    t_giggle = max(0.0f, t_giggle - dt); t_celebrate = max(0.0f, t_celebrate - dt);
    t_ready = max(0.0f, t_ready - dt); t_sad = max(0.0f, t_sad - dt); t_phew = max(0.0f, t_phew - dt);
    if (step > 1.8f && from_rest && vib < 0.35f && t_startle <= 0 && t_celebrate <= 0) {
        t_startle = 0.7f; blink_now(); head_kick(0, -40);
    }
    if (thrill > 1) { scream_t += dt; dizzy_meter += dt * min(2.0f, thrill); }
    else { scream_t = max(0.0f, scream_t - dt * 2); dizzy_meter = max(0.0f, dizzy_meter - dt * 0.6f); }
    if (dizzy_meter > T.dizzy && t_dizzy <= 0) { t_dizzy = 3.5f; dizzy_meter = 0; }
    bool paused = d.printing && (d.pause || d.paused_ext);
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
    else if (lonely) m = M_LONELY;
    else if (confused) m = M_CONFUSED;
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
    else if (thrill > 0.35f) m = M_EXCITED;
    else if (t_sad > 0) m = M_SAD;
    else if (d.runout && d.printing) m = M_HUNGRY;
    else if (fabsf(vz) > 2.0f) m = M_ELEVATOR;
    else if (paused) m = M_BORED;
    else if (heating) m = M_HEATING;
    else if (t_phew > 0) m = M_COOLING;                   // phew, made it
    else if (knomi_power_dozing()) m = M_SLEEPY;          // the screen dims: Coaster dozes off with it
    else if (first_layer) m = M_FOCUS;
    else if (d.printing && d.progress >= 90) m = M_ANTICIPATE;
    else if (d.printing && d.speed >= 130) m = M_NERVOUS;
    else if (d.printing && d.fan >= 80) m = M_WINDY;
    else if (still_t > T.sleep && !d.printing) m = M_SLEEPY;
    else if (cooling) m = M_COOLING;
    else if (env > 0.05f || vib > 0.05f) m = M_RIDING;
    else m = M_CALM;
    if (m != mood) {
        if (d.printing) {
            if (m == M_SCREAM) stats.screams++;
            if (m == M_DIZZY) stats.dizzies++;
            if (m == M_STARTLED) stats.jolts++;
        }
        mood = m; mood_t = 0;
    } else mood_t += dt;
    if (d.printing && env > stats.peak) stats.peak = env;
}

static void step_body(float dt) {
    float w = 2 * PI * T.wobble, k = w * w, c = 2 * T.settle * w, gain = T.sense * k * 18;
    // inertia: the head lags opposite the acceleration. screen x <- X, screen y <- Z (up = -y), depth <- Y
    hvx += (-k * hx - c * hvx - gain * lp[0]) * dt;
    hvy += (-k * hy - c * hvy + gain * lp[2]) * dt;
    hvs += (-k * hs - c * hvs - T.sense * k * 0.16f * lp[1]) * dt;
    hx = clampf(hx + hvx * dt, -34, 34); hy = clampf(hy + hvy * dt, -30, 30); hs = clampf(hs + hvs * dt, -0.3f, 0.3f);
    float w2 = w * 1.35f, k2 = w2 * w2, c2 = 2 * T.settle * 0.6f * w2, g2 = T.sense * k2 * 11;
    pvx += (-k2 * px_ - c2 * pvx - g2 * lp[0]) * dt;
    pvy += (-k2 * py_ - c2 * pvy + g2 * lp[2]) * dt;
    px_ += pvx * dt; py_ += pvy * dt;
    float r = sqrtf(px_ * px_ + py_ * py_);
    if (r > 13) { px_ *= 13 / r; py_ *= 13 / r; pvx *= 0.4f; pvy *= 0.4f; }
}

static void step_expr(float dt) {
    wander_t -= dt;
    if (wander_t <= 0) { wander_tx = frand(-12, 12); wander_t = frand(1.2f, 3.5f); }
    wander_x += (wander_tx - wander_x) * (1 - expf(-dt * 5));
    const expr_t & t = MOODS[mood];
    float kk = 1 - expf(-dt * 9);
    float * e = (float *)&E; const float * tt = (const float *)&t;
    for (unsigned i = 0; i < sizeof(expr_t) / sizeof(float); i++) e[i] += (tt[i] - e[i]) * kk;
    float want = mood == M_BORED ? sinf(now_s * 1.3f) * 12
               : mood == M_LONELY ? sinf(now_s * 0.7f) * 14                        // looking around for OctoPrint
               : mood == M_CONFUSED ? (fmodf(now_s, 2.4f) < 1.2f ? -10 : 10)       // glancing left, right
               : mood == M_HEATING ? 0
               : (mood == M_CALM || mood == M_RIDING || mood == M_SLEEPY || mood == M_COOLING) ? wander_x : 0;
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
    if (d.printing) last_progress = d.progress;
    if (!was_printing && d.printing) stats = {};
    if (was_printing && !d.printing) {
        report = stats;
        report.valid = true;
        report.done = last_progress >= 98;
        report.progress = last_progress;
        report.secs = d.print_time;
        Serial.printf("coaster: print over, %u screams, %u dizzy, %u jolts, peak %.2f g\r\n",
                      report.screams, report.dizzies, report.jolts, report.peak);
        if (last_progress >= 98) { t_celebrate = 4.5f; spawn_confetti(); }
        else t_sad = 8;                                   // cancelled or failed
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

static void draw_eye(const pen_t & p, float ex, float ey, int side, float sx, float open) {
    const float LINE = 26, R = 11;
    float r = R * E.size, L = LINE * sx;
    float slide = clampf(px_ * 1.6f + look, -(L - r), L - r);
    float squash = clampf(1 + py_ / 22, 0.55f, 1.5f);
    float ox = cosf(now_s * 6 * side) * 9 * E.orbit, oy = sinf(now_s * 6 * side) * 4 * E.orbit;
    float cx = ex + slide + ox, cy = ey + oy, ry = r * squash;
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

// which hat today: party on New Year, Santa in December, witch at the end of October
static int hat_today(void) {
    if (T.hat == 1) return 0;
    if (T.hat >= 2) return T.hat - 1;
    time_t now = time(NULL);
    if (now < 1700000000) return 0;   // clock not set yet
    struct tm t;
    gmtime_r(&now, &t);
    int mon = t.tm_mon + 1, day = t.tm_mday;
    if ((mon == 12 && day == 31) || (mon == 1 && day == 1)) return 1;
    if (mon == 12 && day <= 26) return 2;
    if (mon == 10 && day >= 20) return 3;
    return 0;
}

static void fill_poly(const pen_t & p, const float * xy, int n, lv_color_t col) {
    lv_point_t pts[6];
    for (int i = 0; i < n && i < 6; i++) pts[i] = {X(p, xy[2 * i]), Y(p, xy[2 * i + 1])};
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_color = col;
    lv_draw_polygon(p.ctx, &d, pts, n);
}

static void draw_hat(const pen_t & p, float cx, float cy, float sx, float sy) {
    int h = hat_today();
    if (!h) return;
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
    float jit = min(3.0f, vib * 6) * E.zig;
    if (mood == M_ERROR) jit = 1.5f;                  // trembling
    if (mood == M_HEATING) jit = 0.4f + heat_effort * 1.8f;   // straining, harder as it gets close
    if (mood == M_NERVOUS) jit = max(jit, 0.8f);
    if (mood == M_WINDY) jit = max(jit, 0.6f);
    float cx = 120 + hx + frand(-jit, jit), cy = 118 + hy + frand(-jit, jit);
    float sx = 1 - hs * 0.5f, sy = 1 + hs;
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
    draw_eye(p, cx - 54 * sx, cy - 14 * sy + tiltL, -1, sx, open);
    draw_eye(p, cx + 54 * sx, cy - 14 * sy + tiltR, 1, sx, open);
    float talk = 0;
    if (bubble && millis() - bubble_since < 2000) talk = 0.6f * fabsf(sinf(now_s * 11));
    float gape0 = E.gape;
    E.gape = max(E.gape, talk);
    draw_mouth(p, cx, cy + 22 * sy);
    E.gape = gape0;
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
        sense(a, SAMPLE_DT); pick_mood(SAMPLE_DT, d); step_body(SAMPLE_DT); step_expr(SAMPLE_DT);
        steps++;
    }
    if (!steps) {
        float el = (ms - last_ms) / 1000.0f;
        static const float zero[3] = {0, 0, 0};
        int n = min(20, (int)(el / SAMPLE_DT));
        if (n > 0 && (ms - last_ms) > 30) {   // no sensor (KNOMI 1) or it stalled
            for (int i = 0; i < n; i++) {
                now_s += SAMPLE_DT;
                sense(zero, SAMPLE_DT); pick_mood(SAMPLE_DT, d); step_body(SAMPLE_DT); step_expr(SAMPLE_DT);
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
    char buf[200];
    snprintf(buf, sizeof(buf),
             "{\"mood\":\"%s\",\"thrill\":%.2f,\"buzz\":%.2f,\"dizzy\":%.2f,\"used_to\":%.2f,\"motion\":%.2f,\"idle\":%s,\"sensor\":%s}",
             MOOD_NAMES[mood], thrill, vib, t_dizzy > 0 ? 1.0f : dizzy_meter / T.dizzy, base, env,
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
    int n = snprintf(buf, sizeof(buf), "{\"mood\":\"%s\",\"hat\":%d", MOOD_NAMES[mood], hat_today());
    if (report.valid)
        n += snprintf(buf + n, sizeof(buf) - n, ",\"report\":{\"done\":%s,\"progress\":%u,\"screams\":%u,\"dizzies\":%u,\"jolts\":%u,\"peak\":%.2f,\"secs\":%u}",
                      report.done ? "true" : "false", report.progress, report.screams, report.dizzies, report.jolts, report.peak, (unsigned)report.secs);
    snprintf(buf + n, sizeof(buf) - n, "}");
    return String(buf);
}
