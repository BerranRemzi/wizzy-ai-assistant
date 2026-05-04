#pragma once
#include <WiFi.h>

void wifi_manager_init();
void wifi_manager_task();
bool wifi_manager_connect_from_sources();
bool wifi_manager_is_connected();
bool wifi_manager_is_connecting();
