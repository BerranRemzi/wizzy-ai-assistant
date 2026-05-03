#include "callbacks.h"
#include <Arduino.h>
#include <Audio.h>

extern Audio audio;

// Forward declared in playlist/playback.h
extern bool playback_handle_followup_mp3();
extern bool playback_handle_followup_stream();

void audio_info(const char *info)
{
    // Keep this quiet in normal runs; frequent logs can affect streaming smoothness.
    (void)info;
}

void audio_showstreamtitle(const char *info)
{
    Serial.print("audio_title: ");
    Serial.println(info);
}

void audio_eof_stream(const char *info)
{
    Serial.print("audio_eof_stream: ");
    Serial.println(info ? info : "");
    if (!playback_handle_followup_stream())
    {
        // TTS finished path handled in playback
    }
}

void audio_eof_mp3(const char *info)
{
    Serial.print("audio_eof_mp3: ");
    Serial.println(info ? info : "");
    if (!playback_handle_followup_mp3())
    {
        // TTS finished path handled in playback
    }
}
