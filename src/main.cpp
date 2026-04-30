#define TOUCH_CS 33
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
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

// Forward declarations
static void create_ui();
static void update_clock_and_ip();
static void handle_play_button();

// Display
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[ screenWidth * screenHeight / 8 ];
static TFT_eSPI lcd = TFT_eSPI();
static lv_obj_t *test_status_label = NULL;
static uint16_t touch_cal_data[5] = { 557, 3263, 369, 3493, 3 };
static lv_obj_t *ui_clock_label = NULL;
static lv_obj_t *ui_ip_label = NULL;
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

    /* Ensure the screen is clean and show only clock + IP */
    lv_obj_clean(screen);

    /* Large centered clock that fills the screen width */
    ui_clock_label = lv_label_create(screen);
    lv_label_set_text(ui_clock_label, "00:00");
    lv_obj_set_width(ui_clock_label, screenWidth);
    lv_obj_set_style_text_font(ui_clock_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_clock_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_align(ui_clock_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ui_clock_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui_clock_label, LV_ALIGN_CENTER, 0, -8);

    /* Small IP address at bottom center */
    ui_ip_label = lv_label_create(screen);
    lv_label_set_text(ui_ip_label, "IP: --.--.--.--");
    lv_obj_set_style_text_font(ui_ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_ip_label, lv_color_hex(0x000000), 0);
    lv_obj_set_width(ui_ip_label, screenWidth);
    lv_obj_set_style_text_align(ui_ip_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ui_ip_label, LV_LABEL_LONG_CLIP);
    lv_obj_align(ui_ip_label, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void update_clock_and_ip()
{
    static char ip_buf[32];
    static char time_buf[6];
    static uint32_t last_ip_update = 0;
    static int last_min = -1;

    // Use NTP/local time when available (Sofia timezone configured in setup)
    time_t nowt = time(nullptr);
    if (nowt > 100000)
    {
        struct tm timeinfo;
        localtime_r(&nowt, &timeinfo);
        if (timeinfo.tm_min != last_min)
        {
            last_min = timeinfo.tm_min;
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            lv_label_set_text(ui_clock_label, time_buf);
        }
    }
    else
    {
        lv_label_set_text(ui_clock_label, "--:--");
    }

    uint32_t now = millis();

    // Update IP every 10 seconds or if empty
    if (now - last_ip_update > 10000 || strlen(ip_buf) == 0)
    {
        last_ip_update = now;
        if (WiFi.status() == WL_CONNECTED)
        {
            String ip = WiFi.localIP().toString();
            snprintf(ip_buf, sizeof(ip_buf), "IP: %s", ip.c_str());
        }
        else
        {
            strcpy(ip_buf, "IP: offline");
        }
        lv_label_set_text(ui_ip_label, ip_buf);
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
    bool wifi_ok = wifi_manager_connect_from_sources();
    if (wifi_ok)
    {
        configTzTime("EET-2EEST-3,M3.5.0/3,M10.5.0/4", "pool.ntp.org", "time.google.com");
        Serial.println("Waiting for NTP time sync...");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000))
        {
            Serial.printf("NTP time synced: %04d-%02d-%02d %02d:%02d:%02d\n", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
        else
        {
            Serial.println("Failed to sync time");
        }
    }
    else
    {
        Serial.println("Skipping NTP (WiFi not connected)");
    }

    lv_init();

    // Diagnostic info: TFT pin macros (from include/User_Setup.h)
#ifdef TFT_CS
    int dbg_TFT_CS = TFT_CS;
#else
    int dbg_TFT_CS = -1;
#endif
#ifdef TFT_DC
    int dbg_TFT_DC = TFT_DC;
#else
    int dbg_TFT_DC = -1;
#endif
#ifdef TFT_MOSI
    int dbg_TFT_MOSI = TFT_MOSI;
#else
    int dbg_TFT_MOSI = -1;
#endif
#ifdef TFT_SCLK
    int dbg_TFT_SCLK = TFT_SCLK;
#else
    int dbg_TFT_SCLK = -1;
#endif
#ifdef TFT_MISO
    int dbg_TFT_MISO = TFT_MISO;
#else
    int dbg_TFT_MISO = -1;
#endif
#ifdef TFT_BL
    int dbg_TFT_BL = TFT_BL;
#else
    int dbg_TFT_BL = -1;
#endif
#ifdef TFT_RST
    int dbg_TFT_RST = TFT_RST;
#else
    int dbg_TFT_RST = -1;
#endif
    Serial.printf("TFT pins: CS=%d DC=%d MOSI=%d SCLK=%d MISO=%d BL=%d RST=%d\n", dbg_TFT_CS, dbg_TFT_DC, dbg_TFT_MOSI, dbg_TFT_SCLK, dbg_TFT_MISO, dbg_TFT_BL, dbg_TFT_RST);

    Serial.println("Starting lcd.begin()...");
    lcd.begin();
    Serial.println("lcd.begin() returned");
    lcd.fillScreen(TFT_WHITE);
    delay(300);
    pinMode(PIN_BACKLIGHT, OUTPUT);
    // Backlight diagnostics: try toggle to verify BL pin and polarity
    Serial.printf("Toggling backlight pin %d (LOW->HIGH)\n", PIN_BACKLIGHT);
    digitalWrite(PIN_BACKLIGHT, LOW);
    delay(200);
    digitalWrite(PIN_BACKLIGHT, HIGH);
    delay(50);
    Serial.println("Backlight should be ON now");
    lcd.setRotation(1);
#if defined(TFT_eSPI_h) || defined(TFT_eSPI)
    // Try to print some driver/readout info where available
    Serial.printf("Display size: %dx%d\n", lcd.width(), lcd.height());
#endif
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
    update_clock_and_ip();

    lv_timer_handler();
}
