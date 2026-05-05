#include "serial_commands.h"
#include "playlist/playback.h"
#include "playlist/playlist.h"
#include "network/tts_bridge.h"
#include "core/audio_engine.h"
#include "config.h"
#include <Arduino.h>
#include <time.h>

static void set_status(const char *text) { Serial.println(text); }

void serial_commands_print_help()
{
    Serial.println("Serial commands:");
    Serial.println("  0  = play configured button sequence");
    Serial.println("  1  = play radio stream");
    Serial.println("  2  = play test stream");
    Serial.println("  3  = play elevenlabs TTS test");
    Serial.println("  h  = test clock hour reading (current hour)");
    Serial.println("  s  = stop audio");
    Serial.println("  ?  = print this help");
}

static void handle_clock_hour_test()
{
    time_t now = time(nullptr);
    if (now <= 0)
    {
        Serial.println("Clock test failed: time not synced yet");
        Serial.println("Connect WiFi and wait for NTP sync, then retry with 'h'");
        return;
    }

    struct tm local_tm;
    localtime_r(&now, &local_tm);

    if (!playlist_ensure_loaded())
    {
        Serial.println("Clock hour test failed: playlist not available");
        return;
    }

    int selected_hour = -1;
    for (int offset = 0; offset < 24; offset++)
    {
        const uint8_t hour = (uint8_t)((local_tm.tm_hour + offset) % 24);
        if (playlist_pick_random_clock_time(hour) != nullptr ||
            playlist_pick_random_clock_compose(hour) != nullptr)
        {
            selected_hour = hour;
            break;
        }
    }

    if (selected_hour < 0)
    {
        Serial.println("Clock hour test failed: no clock entries available");
        return;
    }

    if (selected_hour == local_tm.tm_hour)
    {
        Serial.printf("Serial cmd h: clock hour test for %02d:00\n", selected_hour);
    }
    else
    {
        Serial.printf("Serial cmd h: clock hour test for %02d:00, using next available %02d:00\n",
                      local_tm.tm_hour,
                      selected_hour);
    }

    set_status("Clock hour test...");

    if (!playback_play_clock_hour((uint8_t)selected_hour))
    {
        Serial.println("Clock hour test failed");
    }
}

static bool handle_playback_command(char cmd)
{
    switch (cmd)
    {
        case '0':
            Serial.println("Serial cmd 0: play configured button sequence");
            set_status("Button sequence...");
            playback_play_button_sequence();
            return true;
        case '1':
            Serial.println("Serial cmd 1: play radio");
            set_status("Requesting radio stream...");
            playback_request_http_stream(NRJ_TEST_URL, "NRJ");
            return true;
        case '2':
            Serial.println("Serial cmd 2: play test");
            set_status("Requesting test stream...");
            playback_request_http_stream(ICECAST_TEST_URL, "NDR");
            return true;
        case '3':
            Serial.println("Serial cmd 3: elevenlabs test");
            set_status("Requesting TTS stream...");
            playback_request_tts_stream();
            return true;
        case 'h':
            handle_clock_hour_test();
            return true;
        default:
            return false;
    }
}

static bool handle_control_command(char cmd)
{
    switch (cmd)
    {
        case 's':
            Serial.println("Serial cmd s: stop audio");
            audio_engine_stop();
            set_status("Audio stopped");
            return true;
        case '?':
            serial_commands_print_help();
            return true;
        default:
            return false;
    }
}

void serial_commands_handle(char cmd)
{
    switch (cmd)
    {
        case '\r':
        case '\n':
        case ' ':
            return;
        default:
            break;
    }

    if (handle_playback_command(cmd)) return;
    if (handle_control_command(cmd)) return;

    Serial.printf("Unknown serial command: %c\n", cmd);
    serial_commands_print_help();
}
