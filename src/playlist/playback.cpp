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
static const uint8_t FOLLOWUP_QUEUE_CAPACITY = 8;
static String s_followup_queue[FOLLOWUP_QUEUE_CAPACITY];
static uint8_t s_followup_count = 0;

static void set_status(const char *text) { Serial.println(text); }

static void clear_followup_queue()
{
    for (uint8_t i = 0; i < FOLLOWUP_QUEUE_CAPACITY; i++) s_followup_queue[i] = "";
    s_followup_count = 0;
}

static bool enqueue_followup_path(const char *path)
{
    if (path == nullptr || path[0] == '\0') return false;
    if (s_followup_count >= FOLLOWUP_QUEUE_CAPACITY) return false;
    s_followup_queue[s_followup_count++] = path;
    return true;
}

static bool dequeue_followup_path(String *out_path)
{
    if (out_path == nullptr || s_followup_count == 0) return false;
    *out_path = s_followup_queue[0];
    for (uint8_t i = 1; i < s_followup_count; i++) s_followup_queue[i - 1] = s_followup_queue[i];
    s_followup_queue[s_followup_count - 1] = "";
    s_followup_count--;
    return true;
}

static bool start_primary_with_followups(const char *primary_fname,
                                         const char followups[][PLAYLIST_MAX_FNAME],
                                         uint8_t followup_count)
{
    if (primary_fname == nullptr || primary_fname[0] == '\0') return false;

    clear_followup_queue();
    for (uint8_t i = 0; i < followup_count; i++)
    {
        char full_path[32];
        sd_manager_build_path(followups[i], full_path, sizeof(full_path));
        if (!enqueue_followup_path(full_path))
        {
            clear_followup_queue();
            return false;
        }
    }

    playback_waiting_for_followup = (s_followup_count > 0);
    playback_sequence_active = playback_waiting_for_followup;
    if (playback_request_sd_file(primary_fname)) return true;

    clear_followup_queue();
    playback_waiting_for_followup = false;
    playback_sequence_active = false;
    return false;
}

static void stop_wifi_if_not_connected()
{
    if (WiFi.status() == WL_CONNECTED) return;
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
}

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
    stop_wifi_if_not_connected();
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
        Serial.printf("First SD play attempt failed, trying memory recovery: %s\n", full_path);
        audio_engine_prepare_start();

        if (!audio.connecttoFS(SD, full_path))
        {
            audio.setVolume(0);
            Serial.printf("Failed to play SD file: %s\n", full_path);
            set_status("SD file failed");
            return false;
        }
    }
    audio.setVolume(AUDIO_LIB_VOLUME);
    Serial.printf("Playing SD file: %s\n", full_path);
    set_status("Playing SD audio");
    return true;
}

bool playback_request_sd_file_by_path(const char *path)
{
    stop_wifi_if_not_connected();
    if (!sd_manager_ensure_ready()) return false;
    playlist_release_for_playback();
    audio_engine_prepare_start();
    if (!audio.connecttoFS(SD, path))
    {
        Serial.printf("First SD play attempt failed, trying memory recovery: %s\n", path);
        audio_engine_prepare_start();

        if (!audio.connecttoFS(SD, path))
        {
            audio.setVolume(0);
            Serial.printf("Failed to play SD file: %s\n", path);
            set_status("SD file failed");
            return false;
        }
    }
    audio.setVolume(AUDIO_LIB_VOLUME);
    Serial.printf("Playing SD file: %s\n", path);
    set_status("Playing SD audio");
    return true;
}

bool playback_request_http_stream(const char *url, const char *name)
{
    if (!wifi_manager_is_connected())
    {
        wifi_manager_connect_from_sources();
        set_status("WiFi connecting... retry");
        return false;
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
    if (!wifi_manager_is_connected())
    {
        wifi_manager_connect_from_sources();
        set_status("WiFi connecting... retry");
        return false;
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
        char followups[1][PLAYLIST_MAX_FNAME];
        strncpy(followups[0], mode_fname, PLAYLIST_MAX_FNAME - 1);
        followups[0][PLAYLIST_MAX_FNAME - 1] = '\0';
        if (start_primary_with_followups(obr_fname, followups, 1)) return true;
    }
    playback_sequence_active = false;
    playback_waiting_for_followup = false;
    clear_followup_queue();
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
    char followups[1][PLAYLIST_MAX_FNAME];
    strncpy(followups[0], mode_fname, PLAYLIST_MAX_FNAME - 1);
    followups[0][PLAYLIST_MAX_FNAME - 1] = '\0';
    return start_primary_with_followups(obr_fname, followups, 1);
}

bool playback_play_button_sequence()
{
    if (!playlist_ensure_loaded()) return false;

    char sequence_files[PLAYLIST_MAX_BUTTON_SEQUENCE_STEPS][PLAYLIST_MAX_FNAME];
    uint8_t sequence_count = 0;
    if (!playlist_pick_button_sequence_files(sequence_files, PLAYLIST_MAX_BUTTON_SEQUENCE_STEPS, &sequence_count))
    {
        set_status("button sequence empty");
        return false;
    }

    if (sequence_count == 1) return playback_request_sd_file(sequence_files[0]);
    return start_primary_with_followups(sequence_files[0], &sequence_files[1], sequence_count - 1);
}

bool playback_play_clock_hour(uint8_t hour)
{
    if (!playlist_ensure_loaded()) return false;

    const char *time_fname = playlist_pick_random_clock_time(hour);
    const char *compose_fname = playlist_pick_random_clock_compose(hour);

    if (time_fname == nullptr && compose_fname == nullptr)
    {
        set_status("clock hour has no entries");
        return false;
    }
    if (time_fname != nullptr && compose_fname != nullptr)
    {
        char followups[1][PLAYLIST_MAX_FNAME];
        strncpy(followups[0], compose_fname, PLAYLIST_MAX_FNAME - 1);
        followups[0][PLAYLIST_MAX_FNAME - 1] = '\0';
        return start_primary_with_followups(time_fname, followups, 1);
    }
    if (time_fname != nullptr) return playback_request_sd_file(time_fname);
    return playback_request_sd_file(compose_fname);
}

bool playback_play_startup_system()
{
    if (!playlist_ensure_loaded()) return false;
    if (g_section_count[SEC_SYSTEM] == 0) { set_status("system section empty"); return false; }
    const uint8_t sys_sec[] = { SEC_SYSTEM };
    return playback_play_random_from_pool(sys_sec, 1);
}

// Chained version: skips stop/clearDMA/settle since the previous track already
// finished (eof callback). This removes the audible gap between two SD files.
bool playback_request_sd_file_by_path_chained(const char *path)
{
    stop_wifi_if_not_connected();
    if (!sd_manager_ensure_ready()) return false;
    playlist_release_for_playback();
    audio_engine_prepare_chain(); // volume=0, no stop/settle
    if (!audio.connecttoFS(SD, path))
    {
        audio.setVolume(0);
        Serial.printf("Failed to play chained SD file: %s\n", path);
        set_status("SD file failed");
        return false;
    }
    audio.setVolume(AUDIO_LIB_VOLUME);
    Serial.printf("Playing chained SD file: %s\n", path);
    set_status("Playing SD audio");
    return true;
}

bool playback_handle_followup_mp3()
{
    if (playback_sequence_active && playback_waiting_for_followup)
    {
        String path_to_play;
        if (dequeue_followup_path(&path_to_play) && path_to_play.length() > 0)
        {
            playback_waiting_for_followup = (s_followup_count > 0);
            playback_sequence_active = playback_waiting_for_followup;
            playback_request_sd_file_by_path_chained(path_to_play.c_str());
            return true;
        }
        playback_waiting_for_followup = false;
        playback_sequence_active = false;
        clear_followup_queue();
    }
    playback_stop_tts_tracking();
    return false;
}

bool playback_handle_followup_stream()
{
    if (playback_sequence_active && playback_waiting_for_followup)
    {
        String path_to_play;
        if (dequeue_followup_path(&path_to_play) && path_to_play.length() > 0)
        {
            playback_waiting_for_followup = (s_followup_count > 0);
            playback_sequence_active = playback_waiting_for_followup;
            playback_request_sd_file_by_path_chained(path_to_play.c_str());
            return true;
        }
        playback_waiting_for_followup = false;
        playback_sequence_active = false;
        clear_followup_queue();
    }
    playback_stop_tts_tracking();
    return false;
}
