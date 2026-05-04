#pragma once

#include <stdint.h>

class TFT_eSPI;

void ui_component_init(TFT_eSPI *display, uint16_t screen_width, uint16_t screen_height);
void ui_component_task(bool allow_updates);
