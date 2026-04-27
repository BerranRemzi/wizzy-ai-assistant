#include "wifi_manager.h"
#include "config.h"
#include <Preferences.h>

bool wifi_manager_connect_from_sources()
{
    String ssid = WIFI_SSID;
    String pass = WIFI_PASSWORD;

    if (ssid.isEmpty())
    {
        Preferences preferences;
        preferences.begin("wizzy", true);
        ssid = preferences.getString("wifi_ssid", "");
        pass = preferences.getString("wifi_pass", "");
        preferences.end();
    }

    if (ssid.isEmpty())
    {
        Serial.println("WiFi credentials missing. Provide include/secrets.h or store NVS keys: wifi_ssid, wifi_pass.");
        return false;
    }

    Serial.printf("Connecting WiFi SSID: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - started) < 10000UL)
    {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi connect timeout.");
        return false;
    }

    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
}
