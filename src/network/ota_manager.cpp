#include "ota_manager.h"

#include <Arduino.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "wifi_manager.h"

namespace {

static bool s_ota_started = false;

} // namespace

void ota_manager_init()
{
    s_ota_started = false;
}

void ota_manager_task()
{
#if OTA_ENABLED
    if (!s_ota_started)
    {
        if (!wifi_manager_is_connected()) return;

        ArduinoOTA.setHostname(OTA_HOSTNAME);
        if (strlen(OTA_PASSWORD) > 0)
        {
            ArduinoOTA.setPassword(OTA_PASSWORD);
        }

        ArduinoOTA.onStart([]() {
            Serial.println("OTA start");
        });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            static uint32_t last_print_ms = 0;
            const uint32_t now = millis();
            if (now - last_print_ms < 300) return;
            last_print_ms = now;
            Serial.printf("OTA progress: %u%%\n", (unsigned)((progress * 100U) / total));
        });

        ArduinoOTA.onEnd([]() {
            Serial.println("OTA done");
        });

        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("OTA error: %u\n", (unsigned)error);
        });

        ArduinoOTA.begin();
        s_ota_started = true;
        Serial.printf("OTA ready. Hostname: %s\n", OTA_HOSTNAME);
    }

    ArduinoOTA.handle();
#endif
}

bool ota_manager_is_running()
{
    return s_ota_started;
}
