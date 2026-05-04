#define TOUCH_CS 33
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "core/audio_engine.h"
#include "network/wifi_manager.h"
#include "network/tts_bridge.h"
#include "network/ota_manager.h"
#include "network/webdav_manager.h"
#include "playlist/playback.h"
#include "commands/serial_commands.h"
#include "ui/ui_component.h"

// Forward declarations
static void handle_play_button();

// Display
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
static TFT_eSPI lcd = TFT_eSPI();
static uint16_t touch_cal_data[5] = { 557, 3263, 369, 3493, 3 };
static bool play_button_was_pressed = false;
static uint32_t play_button_changed_at_ms = 0;
static bool ntp_configured = false;

static bool ui_updates_allowed()
{
    if (!audio.isRunning()) return true;
    return ESP.getFreeHeap() >= UI_UPDATE_MIN_FREE_HEAP_BYTES;
}

static void set_status(const char *text)
{
    Serial.println(text);
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
    wifi_manager_init();
    ota_manager_init();
    webdav_manager_init();
    wifi_manager_connect_from_sources();

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

    Serial.println("Starting lcd.init()...");
    lcd.init();
    Serial.println("lcd.init() returned");
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
    lcd.invertDisplay(false);
    lcd.fillScreen(TFT_BLACK);
#if defined(TFT_eSPI_h) || defined(TFT_eSPI)
    // Try to print some driver/readout info where available
    Serial.printf("Display size: %dx%d\n", lcd.width(), lcd.height());
#endif
#ifdef TOUCH_CS
    lcd.setTouch(touch_cal_data);
#endif

    // Start audio early so decoder buffers reserve memory before heavy UI/font allocations.
    set_status("Audio test ready");
    playback_play_startup_system();

    Serial.printf("Heap before UI init: free=%u min=%u largest=%u\n",
                  ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    ui_component_init(&lcd, (uint16_t)lcd.width(), (uint16_t)lcd.height());
    ui_component_task(true);

    Serial.printf("Heap after UI init: free=%u min=%u largest=%u\n",
                  ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    tts_bridge_server_begin();
    // TTS bridge task is started in tts_bridge.cpp

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
    wifi_manager_task();
    ota_manager_task();
    webdav_manager_task();

    if (!ntp_configured && wifi_manager_is_connected())
    {
        configTzTime("EET-2EEST-3,M3.5.0/3,M10.5.0/4", "pool.ntp.org", "time.google.com");
        ntp_configured = true;
        Serial.println("NTP configured after WiFi connect");
    }

    tts_bridge_check_finished();

    static uint32_t last_inputs_and_status_ms = 0;
    static bool ui_paused_for_memory = false;
    const uint32_t now = millis();

    const bool allow_ui_updates = ui_updates_allowed();
    if (allow_ui_updates != !ui_paused_for_memory)
    {
        ui_paused_for_memory = !allow_ui_updates;
        if (ui_paused_for_memory)
        {
            Serial.printf("UI paused for low heap (%u < %u) while audio running\n",
                          (unsigned)ESP.getFreeHeap(),
                          (unsigned)UI_UPDATE_MIN_FREE_HEAP_BYTES);
        }
        else
        {
            Serial.printf("UI resumed (heap=%u)\n", (unsigned)ESP.getFreeHeap());
        }
    }

    if ((now - last_inputs_and_status_ms) >= 20)
    {
        last_inputs_and_status_ms = now;
        handle_play_button();
        ui_component_task(allow_ui_updates);
    }

    //yield();
}
