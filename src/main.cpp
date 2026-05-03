#define TOUCH_CS 33
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <libs/tiny_ttf/lv_tiny_ttf.h>

#include "config.h"
#include "core/audio_engine.h"
#include "network/wifi_manager.h"
#include "network/tts_bridge.h"
#include "playlist/playback.h"
#include "commands/serial_commands.h"
#include "fonts/ubuntu_font.h"

// Forward declarations
static void create_ui();
static void update_clock_and_ip();
static void handle_play_button();
static bool load_clock_ttf_font();

// Display
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
static lv_color_t buf1[ screenWidth * screenHeight / 8 ];
static TFT_eSPI lcd = TFT_eSPI();
static lv_obj_t *test_status_label = NULL;
static uint16_t touch_cal_data[5] = { 557, 3263, 369, 3493, 3 };
static lv_obj_t *ui_clock_label = NULL;
static lv_obj_t *ui_ip_label = NULL;
static bool play_button_was_pressed = false;
static uint32_t play_button_changed_at_ms = 0;
static const int32_t CLOCK_TTF_SIZE = 100;
static lv_font_t *clock_ttf_font = NULL;

static uint32_t lv_tick_get_cb(void)
{
    return millis();
}

static bool load_clock_ttf_font()
{
    clock_ttf_font = lv_tiny_ttf_create_data(ubuntu_font, (size_t)ubuntu_font_size, CLOCK_TTF_SIZE);
    if (clock_ttf_font == NULL)
    {
        Serial.println("Clock TTF: lv_tiny_ttf_create_data failed");
        return false;
    }

    Serial.printf("Clock TTF: embedded ubuntu_font loaded (%u bytes) at size %d\n", (unsigned)ubuntu_font_size, (int)CLOCK_TTF_SIZE);
    return true;
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.pushColors((uint16_t *)px_map, w * h, true);
    lcd.endWrite();
    lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
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

static void create_ui()
{
    lv_obj_t *screen = lv_scr_act();

    /* Ensure the screen is clean and show only clock + IP */
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Large centered clock that fills the screen width */
    ui_clock_label = lv_label_create(screen);
    lv_label_set_text(ui_clock_label, "00:00");
    lv_obj_set_width(ui_clock_label, screenWidth);
    lv_obj_set_style_text_font(ui_clock_label, clock_ttf_font ? clock_ttf_font : &lv_font_montserrat_48, 0);
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
    lv_tick_set_cb(lv_tick_get_cb);

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

    lv_display_t *disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    load_clock_ttf_font();
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
