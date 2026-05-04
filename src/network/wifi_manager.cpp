#include "wifi_manager.h"
#include "config.h"
#include <Preferences.h>

namespace {

enum class WifiState : uint8_t {
    Idle = 0,
    Connecting,
    Connected,
    Failed,
};

static WifiState s_state = WifiState::Idle;
static String s_ssid;
static String s_pass;
static uint32_t s_connect_started_ms = 0;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000UL;

static bool load_credentials_from_sources()
{
    s_ssid = WIFI_SSID;
    s_pass = WIFI_PASSWORD;

    if (s_ssid.isEmpty())
    {
        Preferences preferences;
        preferences.begin("wizzy", true);
        s_ssid = preferences.getString("wifi_ssid", "");
        s_pass = preferences.getString("wifi_pass", "");
        preferences.end();
    }

    if (s_ssid.isEmpty())
    {
        Serial.println("WiFi credentials missing. Provide include/secrets.h or store NVS keys: wifi_ssid, wifi_pass.");
        return false;
    }

    return true;
}

static bool start_connect_if_possible()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        s_state = WifiState::Connected;
        return true;
    }

    if (s_state == WifiState::Connecting)
    {
        return true;
    }

    if (!load_credentials_from_sources())
    {
        s_state = WifiState::Failed;
        return false;
    }

    Serial.printf("Connecting WiFi SSID: %s\n", s_ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(s_ssid.c_str(), s_pass.c_str());

    s_connect_started_ms = millis();
    s_state = WifiState::Connecting;
    return true;
}

} // namespace

void wifi_manager_init()
{
    s_state = WifiState::Idle;
    s_ssid = "";
    s_pass = "";
    s_connect_started_ms = 0;
}

void wifi_manager_task()
{
    if (s_state != WifiState::Connecting)
    {
        if (WiFi.status() == WL_CONNECTED) s_state = WifiState::Connected;
        return;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        s_state = WifiState::Connected;
        Serial.print("WiFi connected. IP: ");
        Serial.println(WiFi.localIP());
        return;
    }

    if ((millis() - s_connect_started_ms) >= WIFI_CONNECT_TIMEOUT_MS)
    {
        Serial.println("WiFi connect timeout.");
        // Prevent background reconnect churn from affecting local audio playback.
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
        Serial.println("WiFi disabled after timeout.");
        s_state = WifiState::Failed;
    }
}

bool wifi_manager_connect_from_sources()
{
    start_connect_if_possible();
    return wifi_manager_is_connected();
}

bool wifi_manager_is_connected()
{
    return WiFi.status() == WL_CONNECTED;
}

bool wifi_manager_is_connecting()
{
    return s_state == WifiState::Connecting;
}
