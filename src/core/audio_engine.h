#pragma once
#include <Audio.h>

extern Audio audio;

void audio_engine_stop();
void audio_engine_prepare_start();
void audio_engine_stop_if_active();
uint8_t audio_engine_get_volume();
