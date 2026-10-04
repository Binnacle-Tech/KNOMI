#include "knomi_perf.h"
#include "knomi_health.h"
#include <esp_timer.h>
#include "lvgl_hal.h"
#include "pinout.h"

TFT_eSPI tft_gc9a01 = TFT_eSPI();
#ifdef CST816S_SUPPORT
extern TwoWire i2c0;
CST816S ts_cst816s = CST816S(CST816S_RST_PIN, CST816S_IRQ_PIN, &i2c0);
#endif

/* Display flushing
 *
 * Sending a frame to the screen over SPI takes ~12 ms and used to happen on the LVGL task, so drawing
 * and sending took turns on one core (/perf: core 1 ~90% busy, core 0 ~2%). Now LVGL draws into one of two
 * frame buffers while a small task on the other core sends the previous one. TFT_eSPI's own DMA isn't used:
 * in the version we build with it "draws once then freezes" on the ESP32-S3.
 */
typedef struct { lv_disp_drv_t *disp; lv_area_t area; lv_color_t *px; } flush_job_t;
static QueueHandle_t flush_q = NULL;
static TaskHandle_t lvgl_task = NULL;

static void push_area(const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    int64_t t0 = esp_timer_get_time();
    tft_gc9a01.startWrite();
    tft_gc9a01.setAddrWindow(area->x1, area->y1, w, h);
    tft_gc9a01.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft_gc9a01.endWrite();
    knomi_perf_flush((uint32_t)(esp_timer_get_time() - t0));
    knomi_perf_flush_area(area->x1, area->y1, area->x2, area->y2);
}

static void flush_task(void *) {
    flush_job_t j;
    for (;;) {
        if (xQueueReceive(flush_q, &j, portMAX_DELAY) != pdTRUE) continue;
        push_area(&j.area, j.px);
        lv_disp_flush_ready(j.disp);
        if (lvgl_task) xTaskNotifyGive(lvgl_task);   // LVGL may be waiting for this buffer
    }
}

void usr_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    if (flush_q) {
        flush_job_t j = {disp, *area, color_p};
        xQueueSend(flush_q, &j, portMAX_DELAY);
        return;
    }
    push_area(area, color_p);   // one buffer only (not enough memory): send it right here
    lv_disp_flush_ready(disp);
}

// LVGL waits here instead of spinning while the other buffer is still being sent
static void usr_disp_wait(lv_disp_drv_t *) {
    int64_t t0 = esp_timer_get_time();
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(4));
    knomi_perf_part(PP_WAIT, esp_timer_get_time() - t0);
}

#ifdef CST816S_SUPPORT
void touch_idle_time_clear(void);
bool knomi_power_filter_touch(bool pressed);
static void usr_touchpad_read_(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
void usr_touchpad_read(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {   // timed: shares I2C with the accelerometer
    int64_t t0 = esp_timer_get_time();
    usr_touchpad_read_(indev_drv, data);
    knomi_perf_part(PP_TOUCH, esp_timer_get_time() - t0);
}
static void usr_touchpad_read_(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
    static touch_event_t event;
    if(ts_cst816s.ready()) {
        ts_cst816s.getTouch(&event);
    }
    if (knomi_power_filter_touch(event.finger)) {
        // first touch on a sleeping screen only turns the backlight on
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    if(event.finger) {
        data->state = LV_INDEV_STATE_PR;
        /*Set the coordinates*/
        data->point.x = event.x;
        data->point.y = event.y;
        touch_idle_time_clear();
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
#endif

static int8_t aw9346_from_light = -1;

void tft_backlight_init(void) {
    pinMode(LCD_BL_PIN, OUTPUT);
    digitalWrite(LCD_BL_PIN, LOW);
    delay(3); // > 2.5ms for shutdown
    aw9346_from_light = 0;
}
void tft_set_backlight(int8_t aw9346_to_light) {
    if (aw9346_to_light > 16) aw9346_to_light = 16;
    if (aw9346_to_light < 0) aw9346_to_light = 0;
    if (aw9346_from_light == aw9346_to_light) return;

    if (aw9346_to_light == 0) {
        digitalWrite(LCD_BL_PIN, LOW);
        delay(3); // > 2.5ms for shutdown
        aw9346_from_light = 0;
        return;
    }
    if (aw9346_from_light <= 0) {
        digitalWrite(LCD_BL_PIN, HIGH);
        delayMicroseconds(25); // > 20us for poweron
        aw9346_from_light = 16;
    }

    if (aw9346_from_light < aw9346_to_light)
        aw9346_from_light += 16;

    int8_t num = aw9346_from_light - aw9346_to_light;

    for (int8_t i = 0; i < num; i++) {
        digitalWrite(LCD_BL_PIN, LOW);
        delayMicroseconds(1); // 0.5us < T_low < 500us
        digitalWrite(LCD_BL_PIN, HIGH);
        delayMicroseconds(1); // 0.5us < T_high
    }

    aw9346_from_light = aw9346_to_light;
}

void tft_fps_test(void) {
    uint8_t test_sec = 3;
    uint32_t ms = millis() + test_sec * 1000;
    uint32_t frames = 0;
    const uint32_t test_colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE};
    while(ms > millis()) {
        tft_gc9a01.fillScreen(test_colors[frames % (sizeof(test_colors) / sizeof(test_colors[0]))]);
        frames++;
    }
    Serial.println("\r\n******** lcd fps test *****\r\n");
    Serial.print("fps=");
    Serial.println(frames / test_sec, DEC);
    Serial.print("test_sec=");
    Serial.println(test_sec, DEC);
    Serial.println("\r\n***************************\r\n");
}

lv_indev_t * ts_cst816s_indev;
void lvgl_hal_init(void) {
#ifdef CST816S_SUPPORT
    // touch screen
    ts_cst816s.begin();
    ts_cst816s.setReportRate(2); // 20ms
    ts_cst816s.setReportMode(0x60); // touch + gesture generated interrupt
    ts_cst816s.setMotionMask(0); // disable motion
    ts_cst816s.setAutoRst(0); // disable auto reset
    ts_cst816s.setLongRst(0); // disable long press reset
    ts_cst816s.setDisAutoSleep(1); // disable auto sleep
#endif

    // display
    tft_gc9a01.begin();
    tft_gc9a01.invertDisplay(1);
    // tft_gc9a01.setRotation(2);

    tft_gc9a01.fillScreen(TFT_BLACK);
    tft_backlight_init();
    delay(50);
    tft_set_backlight(16);

    // tft_fps_test();

    // must static
    static lv_disp_draw_buf_t draw_buf;
    // two whole-screen buffers in PSRAM (plenty there): Coaster's face is drawn in one pass per frame
    static lv_color_t *color_buf = (lv_color_t *)LV_MEM_CUSTOM_ALLOC(TFT_WIDTH * TFT_HEIGHT * sizeof(lv_color_t));
    static lv_color_t *color_buf2 = (lv_color_t *)LV_MEM_CUSTOM_ALLOC(TFT_WIDTH * TFT_HEIGHT * sizeof(lv_color_t));
    lvgl_task = xTaskGetCurrentTaskHandle();
    if (color_buf2) {
        flush_q = xQueueCreate(1, sizeof(flush_job_t));
        if (!flush_q || xTaskCreatePinnedToCore(flush_task, "flush", 3072, NULL, 9, &knomi_tasks[KT_FLUSH], 0) != pdPASS) {
            flush_q = NULL;
            Serial.println("display: sending frames on the UI task (couldn't start the flush task)");
        }
    }
    lv_init();
    // animations (scrolling text, sliders, screen changes) step at most 30 times a second: they ran at the
    // 10 ms refresh rate, so one scrolling file name kept the print screen redrawing 41 times a second (/perf)
    lv_timer_set_period(lv_anim_get_timer(), 33);
    // one line short of the whole screen on purpose: with two full-size buffers LVGL waits for the previous
    // send before it starts drawing each area, so a frame with the face plus a clock or ring update drew
    // them one after another with a send in between (/perf OP35: 17 ms frames). A hair smaller and it
    // draws the next area while the last one is being sent. A full-height area is drawn in two parts;
    // Coaster's face draws the same both times (see face_measure).
    lv_disp_draw_buf_init(&draw_buf, color_buf, flush_q ? color_buf2 : NULL,
                          flush_q ? TFT_WIDTH * (TFT_HEIGHT - 1) : TFT_WIDTH * TFT_HEIGHT);

    /*Initialize the display*/
    // must static
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = usr_disp_flush;
    disp_drv.wait_cb = usr_disp_wait;
    disp_drv.monitor_cb = [](lv_disp_drv_t *, uint32_t ms, uint32_t px) { knomi_perf_frame(ms, px); };   // /perf
    disp_drv.draw_buf = &draw_buf;
    lv_disp_t * disp = lv_disp_drv_register(&disp_drv);
    knomi_perf_draw_hooks(disp->driver->draw_ctx);   // /perf parts_pct: drawing by kind
    // lv_disp_set_rotation(NULL, LV_DISP_ROT_180);

#ifdef CST816S_SUPPORT
    /* touch screen */
    // must static
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);      /*Basic initialization*/
    indev_drv.gesture_limit = 1;
    indev_drv.gesture_min_velocity = 1;
    indev_drv.type = LV_INDEV_TYPE_POINTER;                 /*See below.*/
    indev_drv.read_cb = usr_touchpad_read;              /*See below.*/
    /*Register the driver in LVGL and save the created input device object*/
    ts_cst816s_indev = lv_indev_drv_register(&indev_drv);
#endif

    /* set background color to black (default white) */
    lv_obj_set_style_bg_color(lv_scr_act(), LV_COLOR_MAKE(0, 0, 0), LV_STATE_DEFAULT);
}
