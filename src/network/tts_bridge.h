#pragma once
#include <stdint.h>

void tts_bridge_server_begin();
void tts_bridge_check_finished();
void tts_bridge_mark_finished();
bool tts_bridge_has_finished();
uint32_t tts_bridge_finished_at_ms();
