#include "serial_commands.h"
#include "playlist/playback.h"
#include "playlist/playlist.h"
#include "network/tts_bridge.h"
#include "core/audio_engine.h"
#include "config.h"
#include <Arduino.h>

static void set_status(const char *text) { Serial.println(text); }

void serial_commands_handle(char cmd)
{
    switch (cmd)
    {
        case '0':
            Serial.println("Serial cmd 0: random obrashenija + random mode");
            set_status("Random obrashenija + mode...");
            playback_play_obrashenija_plus_mode();
            break;
        case '1':
            Serial.println("Serial cmd 1: play radio");
            set_status("Requesting radio stream...");
            playback_request_http_stream(NRJ_TEST_URL, "NRJ");
            break;
        case '2':
            Serial.println("Serial cmd 2: play test");
            set_status("Requesting test stream...");
            playback_request_http_stream(ICECAST_TEST_URL, "NDR");
            break;
        case '3':
            Serial.println("Serial cmd 3: elevenlabs test");
            set_status("Requesting TTS stream...");
            playback_request_tts_stream();
            break;
        case 's':
            Serial.println("Serial cmd s: stop audio");
            audio_engine_stop_soft();
            set_status("Audio stopped");
            break;
        case '\r':
        case '\n':
        case ' ':
            break;
        default:
            Serial.printf("Unknown serial command: %c\n", cmd);
            Serial.println("Use: 0=obrashenija+mode, 1=play radio, 2=play test, 3=elevenlabs test, s=stop audio");
            break;
    }
}
