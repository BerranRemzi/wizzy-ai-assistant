#include "playback.h"
#include "playlist.h"
#include "storage/sd_manager.h"
#include "core/audio_engine.h"
#include "network/wifi_manager.h"
#include "config.h"
#include <Arduino.h>
#include <Audio.h>
#include <SD.h>
#include <FS.h>
#include <WiFi.h>

bool playback_sequence_active = false;
bool playback_waiting_for_followup = false;
static String s_followup_path;

static void set_status(const char *text) { Serial.println(text); }

// TTS tracking
static bool s_tts_stream_active = false;
void playback_set_tts_active(bool active) { s_tts_stream_active = active; }
bool playback_is_tts_active() { return s_tts_stream_active; }

void playback_stop_tts_tracking()
{
    if (s_tts_stream_active)
    {
        s_tts_stream_active = false;
        set_status("TTS finished");
    }
}

bool playback_request_sd_file(const char *fname)
{
    if (!sd_manager_ensure_ready()) return false;
    char full_path[32];
    sd_manager_build_path(fname, full_path, sizeof(full_path));
    if (!sd_manager_file_exists(full_path))
    {
        Serial.printf("Missing SD file: %s\n", full_path);
        set_status("Audio file missing");
        return false;
    }
    playlist_release_for_playback();
    audio_engine_prepare_start();
    if (!audio.connecttoFS(SD, full_path))
    {
        audio.setVolume(0);
        Serial.printf("Failed to play SD file: %s\n", full_path);
        set_status("SD file failed");
        return false;
    }
    audio.setVolume(AUDIO_LIB_VOLUME);
    Serial.printf("Playing SD file: %s\n", full_path);
    set_status("Playing SD audio");
    return true;
}

bool playback_request_sd_file_by_path(const char *path)
{
    if (!sd_manager_ensure_ready()) return false;
    playlist_release_for_playback();
    audio_engine_prepare_start();
    if (!audio.connecttoFS(SD, path))
    {
        audio.setVolume(0);
        Serial.printf("Failed to play SD file: %s\n", path);
        set_status("SD file failed");
        return false;
    }
    audio.setVolume(AUDIO_LIB_VOLUME);
    Serial.printf("Playing SD file: %s\n", path);
    set_status("Playing SD audio");
    return true;
}

bool playback_request_http_stream(const char *url, const char *name)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        if (!wifi_manager_connect_from_sources()) { set_status("WiFi not connected"); return false; }
    }
    audio_engine_prepare_start();
    playlist_unload_if_loaded();
    if (!audio.connecttohost(url))
    {
        audio.setVolume(0);
        Serial.printf("%s stream failed\n", name);
        set_status("Stream failed");
        return false;
    }
    audio.setVolume(AUDIO_LIB_VOLUME);
    Serial.printf("Playing %s stream\n", name);
    set_status("Playing stream");
    return true;
}

bool playback_request_tts_stream()
{
    if (strlen(ELEVENLABS_API_KEY) == 0) { set_status("Missing ELEVENLABS_API_KEY"); return false; }
    if (WiFi.status() != WL_CONNECTED)
    {
        if (!wifi_manager_connect_from_sources()) { set_status("WiFi not connected"); return false; }
    }
    audio_engine_prepare_start();
    playlist_unload_if_loaded();
    delay(50);
    Serial.println("TTS mode: bridge POST -> local GET");
    Serial.println("Connecting to TTS...");
    String local_url = "http://" + WiFi.localIP().toString() + ":" + String(TTS_BRIDGE_PORT) + "/tts";
    if (!audio.connecttohost(local_url.c_str()))
    {
        audio.setVolume(0);
        Serial.println("TTS bridge connect failed");
        return false;
    }
    audio.setVolume(AUDIO_LIB_VOLUME);
    playback_set_tts_active(true);
    Serial.println("Playing TTS stream");
    set_status("Playing TTS stream");
    return true;
}

bool playback_play_random_from_pool(const uint8_t *secs, uint8_t n)
{
    const char *fname = playlist_pick_random_from_sections(secs, n);
    if (fname == nullptr) { set_status("Pool is empty"); return false; }
    return playback_request_sd_file(fname);
}

bool playback_play_mode_pool(const uint8_t *pool_secs, uint8_t n)
{
    if (!playlist_ensure_loaded()) return false;
    const char *mode_fname = playlist_pick_random_from_sections(pool_secs, n);
    if (mode_fname == nullptr) { set_status("Mode pool is empty"); return false; }
    const uint8_t obr_sec[] = { SEC_OBRASHENIJA };
    const char *obr_fname = playlist_pick_random_from_sections(obr_sec, 1);
    if (obr_fname != nullptr)
    {
        char followup[32];
        sd_manager_build_path(mode_fname, followup, sizeof(followup));
        s_followup_path = followup;
        playback_waiting_for_followup = true;
        playback_sequence_active = true;
        if (playback_request_sd_file(obr_fname)) return true;
        s_followup_path = "";
        playback_waiting_for_followup = false;
        playback_sequence_active = false;
    }
    playback_sequence_active = false;
    playback_waiting_for_followup = false;
    s_followup_path = "";
    return playback_request_sd_file(mode_fname);
}

bool playback_play_obrashenija_plus_mode()
{
    if (!playlist_ensure_loaded()) return false;
    const uint8_t obr_sec[] = { SEC_OBRASHENIJA };
    const char *obr_fname = playlist_pick_random_from_sections(obr_sec, 1);
    if (obr_fname == nullptr) { set_status("obrashenija section empty"); return false; }
    uint8_t non_empty[7];
    uint8_t non_empty_count = 0;
    for (uint8_t i = 0; i < MODE_SECTIONS_LEN; i++)
    {
        if (g_section_count[MODE_SECTIONS[i]] > 0) non_empty[non_empty_count++] = MODE_SECTIONS[i];
    }
    if (non_empty_count == 0) { set_status("mode sections are empty"); return false; }
    uint8_t chosen_sec = non_empty[esp_random() % non_empty_count];
    const char *mode_fname = g_section_files[chosen_sec][esp_random() % g_section_count[chosen_sec]];
    char followup[32];
    sd_manager_build_path(mode_fname, followup, sizeof(followup));
    if (followup[0] == '\0') { set_status("mode path empty"); return false; }
    s_followup_path = followup;
    playback_waiting_for_followup = true;
    playback_sequence_active = true;
    if (playback_request_sd_file(obr_fname)) return true;
    s_followup_path = "";
    playback_waiting_for_followup = false;
    playback_sequence_active = false;
    return false;
}

bool playback_play_startup_system()
{
    if (!playlist_ensure_loaded()) return false;
    if (g_section_count[SEC_SYSTEM] == 0) { set_status("system section empty"); return false; }
    const uint8_t sys_sec[] = { SEC_SYSTEM };
    return playback_play_random_from_pool(sys_sec, 1);
}

bool playback_handle_followup_mp3()
{
    if (playback_sequence_active && playback_waiting_for_followup)
    {
        playback_waiting_for_followup = false;
        String path_to_play = s_followup_path;
        s_followup_path = "";
        playback_sequence_active = false;
        if (path_to_play.length() > 0) { playback_request_sd_file_by_path(path_to_play.c_str()); return true; }
    }
    playback_stop_tts_tracking();
    return false;
}

bool playback_handle_followup_stream()
{
    if (playback_sequence_active && playback_waiting_for_followup)
    {
        playback_waiting_for_followup = false;
        String path_to_play = s_followup_path;
        s_followup_path = "";
        playback_sequence_active = false;
        if (path_to_play.length() > 0) { playback_request_sd_file_by_path(path_to_play.c_str()); return true; }
    }
    playback_stop_tts_tracking();
    return false;
}
