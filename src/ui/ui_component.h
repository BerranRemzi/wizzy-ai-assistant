#pragma once

#include <stdint.h>

void ui_component_init(uint16_t screen_width);
void ui_component_periodic(bool allow_updates, bool can_restore_clock_font);

// Kept for compatibility with playback memory-recovery hook.
void ui_release_heavy_assets_for_audio();
