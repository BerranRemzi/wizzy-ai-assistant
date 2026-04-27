#pragma once
#include <stdint.h>

// Sequencing state
extern bool playback_sequence_active;
extern bool playback_waiting_for_followup;

// High-level playback functions
bool playback_request_sd_file(const char *fname);
bool playback_request_sd_file_by_path(const char *path);
bool playback_request_http_stream(const char *url, const char *name);
bool playback_request_tts_stream();
bool playback_handle_followup_mp3();
bool playback_handle_followup_stream();
void playback_stop_tts_tracking();

// Pools
bool playback_play_random_from_pool(const uint8_t *secs, uint8_t n);
bool playback_play_mode_pool(const uint8_t *pool_secs, uint8_t n);
bool playback_play_obrashenija_plus_mode();
bool playback_play_startup_system();
