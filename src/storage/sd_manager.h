#pragma once
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"

bool sd_manager_init();
bool sd_manager_is_ready();
bool sd_manager_ensure_ready();
bool sd_manager_file_exists(const char *path);
const char* sd_manager_build_path(const char *fname, char *out, size_t out_size);
