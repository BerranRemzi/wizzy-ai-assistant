#include "ui_component.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include <time.h>

#include <libs/tiny_ttf/lv_tiny_ttf.h>

#include "config.h"
#include "fonts/ubuntu_font.h"

namespace {

static const int32_t CLOCK_TTF_SIZE = 100;
static const int32_t CLOCK_DOT_SIZE = 20;

static uint16_t g_screen_width = 320;

static lv_obj_t *g_clock_label = NULL;
static lv_obj_t *g_ip_label = NULL;
static lv_obj_t *g_clock_dot_top = NULL;
static lv_obj_t *g_clock_dot_bottom = NULL;
static lv_font_t *g_clock_ttf_font = NULL;

static bool load_clock_ttf_font()
{
    if (g_clock_ttf_font != NULL) return true;

    g_clock_ttf_font = lv_tiny_ttf_create_data(ubuntu_font, (size_t)ubuntu_font_size, CLOCK_TTF_SIZE);
    if (g_clock_ttf_font == NULL)
    {
        Serial.println("Clock TTF: lv_tiny_ttf_create_data failed");
        return false;
    }

    Serial.printf("Clock TTF: embedded ubuntu_font loaded (%u bytes) at size %d\n", (unsigned)ubuntu_font_size, (int)CLOCK_TTF_SIZE);
    return true;
}

static void create_ui()
{
    lv_obj_t *screen = lv_scr_act();

    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_clock_label = lv_label_create(screen);
    lv_label_set_text(g_clock_label, "0000");
    lv_obj_set_width(g_clock_label, g_screen_width);

    if (g_clock_ttf_font != NULL)
    {
        lv_obj_set_style_text_font(g_clock_label, g_clock_ttf_font, 0);
    }
    else
    {
        lv_obj_add_flag(g_clock_label, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_style_text_color(g_clock_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_align(g_clock_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(g_clock_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(g_clock_label, LV_ALIGN_CENTER, 0, -8);

    g_clock_dot_top = lv_obj_create(screen);
    lv_obj_remove_style_all(g_clock_dot_top);
    lv_obj_set_size(g_clock_dot_top, CLOCK_DOT_SIZE, CLOCK_DOT_SIZE);
    lv_obj_set_style_radius(g_clock_dot_top, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(g_clock_dot_top, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_clock_dot_top, lv_color_hex(0x000000), 0);
    lv_obj_align(g_clock_dot_top, LV_ALIGN_CENTER, 0, -26);
    if (g_clock_ttf_font == NULL) lv_obj_add_flag(g_clock_dot_top, LV_OBJ_FLAG_HIDDEN);

    g_clock_dot_bottom = lv_obj_create(screen);
    lv_obj_remove_style_all(g_clock_dot_bottom);
    lv_obj_set_size(g_clock_dot_bottom, CLOCK_DOT_SIZE, CLOCK_DOT_SIZE);
    lv_obj_set_style_radius(g_clock_dot_bottom, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(g_clock_dot_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_clock_dot_bottom, lv_color_hex(0x000000), 0);
    lv_obj_align(g_clock_dot_bottom, LV_ALIGN_CENTER, 0, 18);
    if (g_clock_ttf_font == NULL) lv_obj_add_flag(g_clock_dot_bottom, LV_OBJ_FLAG_HIDDEN);

    g_ip_label = lv_label_create(screen);
    lv_label_set_text(g_ip_label, "IP: --.--.--.--");
    lv_obj_set_style_text_font(g_ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_ip_label, lv_color_hex(0x000000), 0);
    lv_obj_set_width(g_ip_label, g_screen_width);
    lv_obj_set_style_text_align(g_ip_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(g_ip_label, LV_LABEL_LONG_CLIP);
    lv_obj_align(g_ip_label, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void update_clock_and_ip()
{
    static char ip_buf[32];
    static char time_buf[6] = "-- --";
    static uint32_t last_ip_update = 0;
    static int last_min = -1;
    static bool dots_on = true;
    static uint32_t last_dots_toggle_ms = 0;

    if (g_clock_ttf_font != NULL && g_clock_label != NULL)
    {
        time_t nowt = time(nullptr);
        if (nowt > 100000)
        {
            struct tm timeinfo;
            localtime_r(&nowt, &timeinfo);
            if (timeinfo.tm_min != last_min)
            {
                last_min = timeinfo.tm_min;
                snprintf(time_buf, sizeof(time_buf), "%02d %02d", timeinfo.tm_hour, timeinfo.tm_min);
                lv_label_set_text(g_clock_label, time_buf);
            }
        }
        else
        {
            strcpy(time_buf, "-- --");
            last_min = -1;
            lv_label_set_text(g_clock_label, time_buf);
        }
    }

    const uint32_t now = millis();

    if ((now - last_dots_toggle_ms) >= 500)
    {
        last_dots_toggle_ms = now;
        dots_on = !dots_on;

        if (g_clock_ttf_font != NULL && g_clock_dot_top != NULL && g_clock_dot_bottom != NULL)
        {
            lv_opa_t opa = dots_on ? LV_OPA_COVER : LV_OPA_TRANSP;
            lv_obj_set_style_bg_opa(g_clock_dot_top, opa, 0);
            lv_obj_set_style_bg_opa(g_clock_dot_bottom, opa, 0);
        }
    }

    if (now - last_ip_update > 10000 || strlen(ip_buf) == 0)
    {
        last_ip_update = now;
        if (WiFi.status() == WL_CONNECTED)
        {
            String ip = WiFi.localIP().toString();
            snprintf(ip_buf, sizeof(ip_buf), "IP: %s", ip.c_str());
        }
        else
        {
            strcpy(ip_buf, "IP: offline");
        }

        if (g_ip_label != NULL) lv_label_set_text(g_ip_label, ip_buf);
    }
}

} // namespace

void ui_component_init(uint16_t screen_width)
{
    g_screen_width = screen_width;
    load_clock_ttf_font();
    create_ui();
    update_clock_and_ip();
}

void ui_component_periodic(bool allow_updates, bool can_restore_clock_font)
{
    static uint32_t last_clock_ttf_restore_ms = 0;

    const uint32_t now = millis();

    if (allow_updates)
    {
        update_clock_and_ip();
    }

    if ((now - last_clock_ttf_restore_ms) >= 5000)
    {
        last_clock_ttf_restore_ms = now;
        if (g_clock_ttf_font == NULL && can_restore_clock_font && g_clock_label != NULL)
        {
            if (load_clock_ttf_font())
            {
                lv_obj_set_style_text_font(g_clock_label, g_clock_ttf_font, 0);
                lv_obj_clear_flag(g_clock_label, LV_OBJ_FLAG_HIDDEN);
                if (g_clock_dot_top != NULL) lv_obj_clear_flag(g_clock_dot_top, LV_OBJ_FLAG_HIDDEN);
                if (g_clock_dot_bottom != NULL) lv_obj_clear_flag(g_clock_dot_bottom, LV_OBJ_FLAG_HIDDEN);
                update_clock_and_ip();
                lv_obj_invalidate(g_clock_label);
                Serial.println("Clock TTF: restored");
            }
        }
    }
}

void ui_release_heavy_assets_for_audio()
{
    if (g_clock_ttf_font == NULL) return;

    if (g_clock_label != NULL)
    {
        // Break the reference before destroying the dynamic font.
        lv_obj_set_style_text_font(g_clock_label, &lv_font_montserrat_14, 0);
        lv_obj_add_flag(g_clock_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (g_clock_dot_top != NULL) lv_obj_add_flag(g_clock_dot_top, LV_OBJ_FLAG_HIDDEN);
    if (g_clock_dot_bottom != NULL) lv_obj_add_flag(g_clock_dot_bottom, LV_OBJ_FLAG_HIDDEN);

    lv_tiny_ttf_destroy(g_clock_ttf_font);
    g_clock_ttf_font = NULL;
    Serial.println("Clock TTF: emergency release for audio memory");
}
