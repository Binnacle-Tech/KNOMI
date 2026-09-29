// Coaster face engine and renderer. Port of the web mock-up (coaster-face.html):
// same springs, same moods, same one-face morphing expressions.
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "knomi.h"
#include "moonraker.h"
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
} coaster_tune_t;
static const coaster_tune_t TUNE_DEF = {2.4f, 0.28f, 1.0f, 12.0f, 0.9f, 5.0f, 20.0f, false};
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
        T.idle   = d["idle"] | false;
    }
    f.close();
}

bool coaster_idle_enabled(void) { return T.idle; }

// Web task: save tuning from the /coaster page (unknown keys dropped, values clamped on load)
const char * coaster_save_json(const char * json, size_t len) {
    if (len > 1024) return "Too large";
    StaticJsonDocument<512> in;
    if (deserializeJson(in, json, len) != DeserializationError::Ok) return "Not valid JSON";
    StaticJsonDocument<384> out;
    static const char * keys[] = {"wobble", "settle", "sense", "habit", "scare", "dizzy", "sleep"};
    for (const char * k : keys) if (in[k].is<float>()) out[k] = in[k].as<float>();
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
             "{\"wobble\":%.2f,\"settle\":%.2f,\"sense\":%.2f,\"habit\":%.0f,\"scare\":%.2f,\"dizzy\":%.1f,\"sleep\":%.0f,\"idle\":%s}",
             T.wobble, T.settle, T.sense, T.habit, T.scare, T.dizzy, T.sleep, T.idle ? "true" : "false");
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
    M_BORED, M_SHIVER, M_DIZZY, M_GIGGLE, M_CELEBRATE, M_COUNT
};
static const char * MOOD_NAMES[M_COUNT] = {
    "calm", "riding", "excited", "screaming", "startled", "elevator", "sleepy",
    "bored", "shivering", "dizzy", "giggle", "celebrate",
};

// sensing
static float lp[3], mfl[3], hist[6][3];
static uint8_t hist_i = 0;
static float env = 0, base = 0, vib = 0, step = 0, thrill = 0, scream_t = 0, dizzy_meter = 0, still_t = 0;
static bool from_rest = false;
// mood
static int mood = M_CALM;
static float mood_t = 0;
static float t_startle = 0, t_dizzy = 0, t_giggle = 0, t_celebrate = 0;
// body
static float hx = 0, hy = 0, hvx = 0, hvy = 0, hs = 0, hvs = 0;
static float px_ = 0, py_ = 0, pvx = 0, pvy = 0;
// expression
typedef struct { float open, size, cheek, orbit, w, curve, omega, gape, zig, wave; } expr_t;
static const expr_t MOODS[M_COUNT] = {
    /* calm      */ {0.5f,  1.0f,  0, 0, 14, 0.0f, 1.0f, 0.0f, 0, 0},
    /* riding    */ {0.56f, 1.0f,  0, 0, 14, 0.3f, 0.7f, 0.0f, 0, 0},
    /* excited   */ {0.85f, 1.05f, 0, 0, 12, 0.7f, 0.0f, 0.9f, 0, 0},
    /* screaming */ {1.0f,  1.3f,  0, 0,  8, 0.0f, 0.0f, 1.7f, 0, 0},
    /* startled  */ {1.0f,  1.2f,  0, 0,  6, 0.0f, 0.0f, 1.1f, 0, 0},
    /* elevator  */ {0.75f, 1.05f, 0, 0,  7, 0.0f, 0.0f, 0.6f, 0, 0},
    /* sleepy    */ {0.02f, 1.0f,  0, 0, 11, 0.0f, 1.0f, 0.0f, 0, 0},
    /* bored     */ {0.32f, 1.0f,  0, 0,  9, 0.0f, 0.0f, 0.0f, 0, 0},
    /* shivering */ {0.42f, 1.0f,  0, 0, 14, 0.0f, 0.0f, 0.0f, 1, 0},
    /* dizzy     */ {0.5f,  1.0f,  0, 1, 14, 0.0f, 0.0f, 0.0f, 0, 1},
    /* giggle    */ {1.0f,  1.05f, 1, 0, 12, 0.8f, 0.0f, 0.8f, 0, 0},
    /* celebrate */ {1.0f,  1.1f,  1, 0, 15, 0.9f, 0.0f, 1.0f, 0, 0},
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
    if (step > 1.8f && from_rest && vib < 0.35f && t_startle <= 0 && t_celebrate <= 0) {
        t_startle = 0.7f; blink_now(); head_kick(0, -40);
    }
    if (thrill > 1) { scream_t += dt; dizzy_meter += dt * min(2.0f, thrill); }
    else { scream_t = max(0.0f, scream_t - dt * 2); dizzy_meter = max(0.0f, dizzy_meter - dt * 0.6f); }
    if (dizzy_meter > T.dizzy && t_dizzy <= 0) { t_dizzy = 3.5f; dizzy_meter = 0; }
    bool paused = d.printing && (d.pause || d.paused_ext);
    int m;
    if (t_celebrate > 0) m = M_CELEBRATE;
    else if (t_giggle > 0) m = M_GIGGLE;
    else if (t_dizzy > 0) m = M_DIZZY;
    else if (t_startle > 0) m = M_STARTLED;
    else if (vib > 0.28f) m = M_SHIVER;
    else if (thrill > 1 && scream_t > 0.4f) m = M_SCREAM;
    else if (thrill > 0.35f) m = M_EXCITED;
    else if (fabsf(vz) > 2.0f) m = M_ELEVATOR;
    else if (paused) m = M_BORED;
    else if (still_t > T.sleep && !d.printing) m = M_SLEEPY;
    else if (env > 0.05f || vib > 0.05f) m = M_RIDING;
    else m = M_CALM;
    if (m != mood) { mood = m; mood_t = 0; } else mood_t += dt;
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
               : (mood == M_CALM || mood == M_RIDING || mood == M_SLEEPY) ? wander_x : 0;
    look += (want - look) * kk;
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
    if (d.printing) last_progress = d.progress;
    if (was_printing && !d.printing && last_progress >= 98) { t_celebrate = 4.5f; spawn_confetti(); }
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
    fill_ellipse(p, cx, cy, r, ry, p.fc);
    fill_rect(p, cx - r - 2, cy - ry - 2, cx + r + 2, lid, black);        // lid covers the top
    if (E.cheek > 0.01f) {                                                // cheek pushes up: "^" eyes
        float cyc = cy + r * 2.4f - E.cheek * 1.55f * r;
        fill_ellipse(p, cx, cyc, r * 1.5f, r * 1.5f, black);
    }
    float k = clampf((open - 0.5f) / 0.3f, 0, 1);   // lid line shrinks into the pupil as it opens
    if (k < 0.98f) {
        float x0 = ex - L + (cx - (ex - L)) * k, x1 = ex + L + (cx - (ex + L)) * k;
        line(p, x0, lid, x1, lid, 5, (lv_opa_t)(255 * (1 - k * 0.6f)));
    }
}

static float tri(float u) { return 2 * fabsf(2 * (u - floorf(u + 0.5f))) - 1; }

static void draw_mouth(const pen_t & p, float mx, float my) {
    const int N = 20;
    float w = E.w, prevx = 0, prevy = 0;
    float zsign = sinf(now_s * 38) > 0 ? 0.5f : 0;
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

static void draw_face(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    pen_t p;
    p.ctx = lv_event_get_draw_ctx(e);
    p.ox = obj->coords.x1; p.oy = obj->coords.y1;
    p.s = lv_obj_get_width(obj) / 240.0f;
    p.fc = lv_theme_color();

    float jit = min(3.0f, vib * 6) * E.zig;
    float cx = 120 + hx + frand(-jit, jit), cy = 118 + hy + frand(-jit, jit);
    float sx = 1 - hs * 0.5f, sy = 1 + hs;
    if (t_giggle > 0) cy -= fabsf(sinf(now_s * 14)) * 6;
    if (t_celebrate > 0) cy -= fabsf(sinf(now_s * 9)) * 8;
    cy += sinf(now_s * 1.6f) * 1.5f * (1 - clampf(E.open * 4, 0, 1));   // breathing while asleep
    float blinkk = blink_closing > 0 ? sinf(PI * (1 - blink_closing / 0.16f)) : 0;
    float open = E.open * (1 - blinkk);
    draw_eye(p, cx - 54 * sx, cy - 14 * sy, -1, sx, open);
    draw_eye(p, cx + 54 * sx, cy - 14 * sy, 1, sx, open);
    draw_mouth(p, cx, cy + 22 * sy);

    // sweat drop while the nozzle is hot
    const moonraker_data_t & d = moonraker.data;
    if (d.nozzle_actual >= 170 && mood != M_SLEEPY) {
        float q = fmodf(now_s * 0.5f, 1), dx = cx + 86 * sx, dy = cy - 40 + q * 26;
        lv_opa_t op = (lv_opa_t)(255 * (1 - q * 0.8f));
        lv_draw_rect_dsc_t rd; lv_draw_rect_dsc_init(&rd);
        rd.bg_color = p.fc; rd.bg_opa = op; rd.radius = LV_RADIUS_CIRCLE;
        lv_area_t a = {X(p, dx - 4), Y(p, dy - 3), X(p, dx + 4), Y(p, dy + 5)};
        lv_draw_rect(p.ctx, &rd, &a);
        line(p, dx, dy - 8, dx, dy - 2, 3, op);
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
