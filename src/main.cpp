#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

#include "config.h"
#include "core/audio_engine.h"
#include "core/callbacks.h"
#include "network/wifi_manager.h"
#include "network/tts_bridge.h"
#include "storage/sd_manager.h"
#include "playlist/playlist.h"
#include "playlist/playback.h"
#include "commands/serial_commands.h"

// UI
#include "ui.h"

// Display
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[ screenWidth * screenHeight / 8 ];
static TFT_eSPI lcd = TFT_eSPI();
static lv_obj_t *test_status_label = NULL;
static uint16_t touch_cal_data[5] = { 557, 3263, 369, 3493, 3 };
static bool play_button_was_pressed = false;
static uint32_t play_button_changed_at_ms = 0;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.pushColors((uint16_t *)&color_p->full, w * h, true);
    lcd.endWrite();
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
#ifdef TOUCH_CS
    uint16_t touchX, touchY;
    bool touched = lcd.getTouch(&touchX, &touchY, 600);
    if (!touched) { data->state = LV_INDEV_STATE_REL; }
    else { data->state = LV_INDEV_STATE_PR; data->point.x = touchX; data->point.y = touchY; }
#else
    (void)indev_driver;
    data->state = LV_INDEV_STATE_REL;
#endif
}

static void set_status(const char *text)
{
    Serial.println(text);
    if (test_status_label != NULL) lv_label_set_text(test_status_label, text);
}

static void on_button_event(lv_event_t *e, const char *label, void (*action)())
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED) return;
    Serial.printf("UI: %s clicked\n", label);
    action();
}

static void on_sutrin(lv_event_t *e) { on_button_event(e, "Сутрин", [](){ playback_play_mode_pool(POOL_MORNING, POOL_MORNING_LEN); }); }
static void on_den(lv_event_t *e)    { on_button_event(e, "Ден",    [](){ playback_play_mode_pool(POOL_DAY, POOL_DAY_LEN); }); }
static void on_nosht(lv_event_t *e)  { on_button_event(e, "Нощ",    [](){ playback_play_mode_pool(POOL_NIGHT, POOL_NIGHT_LEN); }); }
static void on_iznenada(lv_event_t *e) { on_button_event(e, "Изненада", [](){ playback_play_obrashenija_plus_mode(); }); }
static void on_ndr(lv_event_t *e)    { on_button_event(e, "NDR",    [](){ playback_request_http_stream(ICECAST_TEST_URL, "NDR"); }); }
static void on_nrj(lv_event_t *e)    { on_button_event(e, "NRJ",    [](){ playback_request_http_stream(NRJ_TEST_URL, "NRJ"); }); }
static void on_sd(lv_event_t *e)     { on_button_event(e, "SD",     [](){ playback_request_sd_file_by_path("/audio/adv_02.mp3"); }); }
static void on_tts(lv_event_t *e)    { on_button_event(e, "TTS",    [](){ playback_request_tts_stream(); }); }

static void create_ui()
{
    lv_obj_t *screen = lv_scr_act();

    auto make_btn = [&](const char *text, lv_event_cb_t cb, int x, int y, int w = 90, int h = 42) {
        lv_obj_t *btn = lv_btn_create(screen);
        lv_obj_set_size(btn, w, h);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, x, y);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_ALL, NULL);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_center(lbl);
        return btn;
    };

    make_btn("Сутрин", on_sutrin, 10, -60);
    make_btn("Ден", on_den, 110, -60);
    make_btn("Нощ", on_nosht, 210, -60);
    make_btn("Изненада", on_iznenada, 90, -10, 140);

    test_status_label = lv_label_create(screen);
    lv_obj_set_width(test_status_label, 300);
    lv_label_set_long_mode(test_status_label, LV_LABEL_LONG_CLIP);
    lv_obj_align(test_status_label, LV_ALIGN_BOTTOM_LEFT, 10, -108);
    lv_label_set_text(test_status_label, "Готово");
}

static void handle_play_button()
{
    const bool pressed = digitalRead(PIN_PLAY_BUTTON) == LOW;
    const uint32_t now = millis();

    if (pressed != play_button_was_pressed && (now - play_button_changed_at_ms) >= 40)
    {
        play_button_changed_at_ms = now;
        play_button_was_pressed = pressed;

        if (pressed)
        {
            Serial.println("Button: play random obrashenija + mode");
            playback_play_obrashenija_plus_mode();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Serial2.begin(115200);

    if (PIN_MIC_ADC != PIN_SPEAKER) pinMode(PIN_MIC_ADC, INPUT);
    pinMode(PIN_PLAY_BUTTON, INPUT_PULLUP);
    play_button_was_pressed = digitalRead(PIN_PLAY_BUTTON) == LOW;

    audio.setVolume(AUDIO_LIB_VOLUME);
    wifi_manager_connect_from_sources();

    lv_init();
    lcd.begin();
    lcd.fillScreen(TFT_BLACK);
    delay(300);
    pinMode(PIN_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_BACKLIGHT, HIGH);
    lcd.setRotation(1);
#ifdef TOUCH_CS
    lcd.setTouch(touch_cal_data);
#endif

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, screenWidth * screenHeight / 8);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();
    lv_timer_handler();
    create_ui();

    tts_bridge_server_begin();
    // TTS bridge task is started in tts_bridge.cpp

    set_status("Audio test ready");
    playback_play_startup_system();
    Serial.println("Serial commands: 0=obrashenija+mode, 1=play radio, 2=play test, 3=elevenlabs test, s=stop audio");
    Serial.println("Setup done");
}

void loop()
{
    while (Serial.available() > 0)
    {
        serial_commands_handle((char)Serial.read());
    }

    audio.loop();
    tts_bridge_check_finished();
    handle_play_button();

    lv_timer_handler();
}
