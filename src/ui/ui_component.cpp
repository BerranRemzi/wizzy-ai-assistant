#include "ui_component.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>

namespace {

static const uint16_t COLOR_BG = TFT_BLACK;
static const uint16_t COLOR_FG = TFT_WHITE;
static const uint16_t COLOR_DIM = 0x39C4;
static const uint16_t COLOR_BAR = TFT_WHITE;
static const uint8_t CLOCK_MAIN_FONT = 6; // Loaded in User_Setup.h
static const uint8_t CLOCK_MAIN_SIZE = 2;

static const int32_t BAR_X = 20;
static const int32_t BAR_Y = 10;
static const int32_t BAR_H = 10;
static const int32_t DOT_RADIUS = 7;
static const int32_t DOT_TOP_Y = 98;
static const int32_t DOT_BOTTOM_Y = 142;

static TFT_eSPI *g_display = NULL;
static uint16_t g_screen_width = 320;
static uint16_t g_screen_height = 240;

static char g_last_ip_text[32] = "";
static bool g_surface_initialized = false;
static int g_last_hh = -1;
static int g_last_mm = -1;
static int g_last_ss = -1;
static bool g_last_dots_on = false;

static void draw_top_second_bar(int ss)
{
    if (g_display == NULL) return;

    const int32_t bar_w = (int32_t)g_screen_width - (BAR_X * 2);
    g_display->fillRect(BAR_X, BAR_Y, bar_w, BAR_H, COLOR_DIM);
    if (ss < 0) ss = 0;
    if (ss > 59) ss = 59;
    const int32_t fill_w = (bar_w * (ss + 1)) / 60;
    g_display->fillRect(BAR_X, BAR_Y, fill_w, BAR_H, COLOR_BAR);
}

static void draw_dots(bool on)
{
    if (g_display == NULL) return;
    const int32_t cx = (int32_t)g_screen_width / 2;
    const uint16_t color = on ? COLOR_FG : COLOR_BG;
    g_display->fillCircle(cx, DOT_TOP_Y, DOT_RADIUS, color);
    g_display->fillCircle(cx, DOT_BOTTOM_Y, DOT_RADIUS, color);
}

static void draw_clock_numbers(int hh, int mm)
{
    if (g_display == NULL) return;

    const int32_t ypos = 80;

    // Clear numbers area only. Dots are rendered separately for blink control.
    g_display->fillRect(0, ypos - 8, g_screen_width, 106, COLOR_BG);

    g_display->setTextFont(1);
    g_display->setTextSize(CLOCK_MAIN_SIZE);
    g_display->setTextColor(COLOR_FG, COLOR_BG);

    char buf[6];
    snprintf(buf, sizeof(buf), "%02d %02d", hh, mm);
    g_display->drawCentreString(buf, g_screen_width / 2, ypos, CLOCK_MAIN_FONT);
}

static void draw_ip_text(const String &ip_text)
{
    if (g_display == NULL) return;

    g_display->setTextFont(1);
    g_display->setTextSize(1);
    g_display->setTextColor(COLOR_FG, COLOR_BG);

    // Clear a fixed bottom band before writing IP text.
    const int32_t clear_h = 22;
    const int32_t clear_y = (int32_t)g_screen_height - clear_h;
    g_display->fillRect(0, clear_y, g_screen_width, clear_h, COLOR_BG);

    g_display->drawCentreString(ip_text, g_screen_width / 2, g_screen_height - 22, 2);
}

static void draw_static_surface()
{
    if (g_display == NULL) return;

    g_display->fillScreen(COLOR_BG);
    draw_top_second_bar(0);
    draw_clock_numbers(0, 0);
    draw_dots(true);
    draw_ip_text(String("IP: offline"));
    g_surface_initialized = true;
}

static void update_clock_and_ip()
{
    if (g_display == NULL) return;

    static uint32_t last_ip_update = 0;
    static uint32_t last_clock_update = 0;
    static uint32_t last_dot_toggle_ms = 0;
    static bool clock_drawn_once = false;

    const uint32_t now = millis();

    if (!clock_drawn_once || (now - last_clock_update) >= 1000)
    {
        last_clock_update = now;
        clock_drawn_once = true;

        time_t nowt = time(nullptr);
        if (nowt > 100000)
        {
            struct tm timeinfo;
            localtime_r(&nowt, &timeinfo);
            if (timeinfo.tm_hour != g_last_hh || timeinfo.tm_min != g_last_mm)
            {
                draw_clock_numbers(timeinfo.tm_hour, timeinfo.tm_min);
                g_last_hh = timeinfo.tm_hour;
                g_last_mm = timeinfo.tm_min;
            }
            if (timeinfo.tm_sec != g_last_ss)
            {
                draw_top_second_bar(timeinfo.tm_sec);
                g_last_ss = timeinfo.tm_sec;
            }
        }
        else
        {
            const int fallback_ss = (int)((now / 1000) % 60);
            if (g_last_hh != 0 || g_last_mm != 0)
            {
                draw_clock_numbers(0, 0);
                g_last_hh = 0;
                g_last_mm = 0;
            }
            if (fallback_ss != g_last_ss)
            {
                draw_top_second_bar(fallback_ss);
                g_last_ss = fallback_ss;
            }
        }
    }

    if ((now - last_dot_toggle_ms) >= 500)
    {
        last_dot_toggle_ms = now;
        g_last_dots_on = !g_last_dots_on;
        draw_dots(g_last_dots_on);
    }

    if (now - last_ip_update > 10000 || g_last_ip_text[0] == '\0')
    {
        last_ip_update = now;
        String ip_text;
        if (WiFi.status() == WL_CONNECTED)
        {
            ip_text = String("IP: ") + WiFi.localIP().toString();
        }
        else
        {
            ip_text = "IP: offline";
        }

        char ip_buf[32];
        snprintf(ip_buf, sizeof(ip_buf), "%s", ip_text.c_str());

        if (strcmp(ip_buf, g_last_ip_text) != 0)
        {
            draw_ip_text(ip_text);
            strncpy(g_last_ip_text, ip_buf, sizeof(g_last_ip_text));
            g_last_ip_text[sizeof(g_last_ip_text) - 1] = '\0';
        }
    }
}

} // namespace

void ui_component_init(TFT_eSPI *display, uint16_t screen_width, uint16_t screen_height)
{
    g_display = display;
    g_screen_width = screen_width;
    g_screen_height = screen_height;
    g_last_ip_text[0] = '\0';

    draw_static_surface();
    update_clock_and_ip();
}

void ui_component_periodic(bool allow_updates, bool can_restore_clock_font)
{
    (void)can_restore_clock_font;
    if (g_display == NULL) return;

    if (!g_surface_initialized)
    {
        draw_static_surface();
    }

    if (allow_updates)
    {
        update_clock_and_ip();
    }
}

void ui_release_heavy_assets_for_audio()
{
    // No-op for TFT direct rendering: no dynamic LVGL font assets to release.
}
