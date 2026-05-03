#include "playlist.h"
#include "storage/sd_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <FS.h>
#include <esp_heap_caps.h>

char g_section_files[PLAYLIST_NUM_SECTIONS][PLAYLIST_MAX_ENTRIES][PLAYLIST_MAX_FNAME];
uint8_t g_section_count[PLAYLIST_NUM_SECTIONS];

const uint8_t POOL_MORNING[]  = { SEC_WAKE_UP, SEC_SCHOOL };
const uint8_t POOL_DAY[]      = { SEC_FUN, SEC_THREAT, SEC_ADVENTURE };
const uint8_t POOL_NIGHT[]    = { SEC_SLEEP, SEC_EVENING };
const uint8_t POOL_SURPRISE[] = { SEC_OBRASHENIJA, SEC_WAKE_UP, SEC_SCHOOL,
                                   SEC_FUN, SEC_THREAT, SEC_ADVENTURE,
                                   SEC_SLEEP, SEC_EVENING, SEC_SYSTEM };
const uint8_t MODE_SECTIONS[] = { SEC_WAKE_UP, SEC_SCHOOL, SEC_FUN,
                                   SEC_THREAT, SEC_ADVENTURE, SEC_SLEEP, SEC_EVENING };

const size_t POOL_MORNING_LEN  = sizeof(POOL_MORNING) / sizeof(POOL_MORNING[0]);
const size_t POOL_DAY_LEN      = sizeof(POOL_DAY) / sizeof(POOL_DAY[0]);
const size_t POOL_NIGHT_LEN    = sizeof(POOL_NIGHT) / sizeof(POOL_NIGHT[0]);
const size_t POOL_SURPRISE_LEN = sizeof(POOL_SURPRISE) / sizeof(POOL_SURPRISE[0]);
const size_t MODE_SECTIONS_LEN = sizeof(MODE_SECTIONS) / sizeof(MODE_SECTIONS[0]);

static bool g_playlist_loaded = false;

extern void ui_release_heavy_assets_for_audio();

static void append_section_entries(JsonArrayConst arr, uint8_t sec)
{
    for (JsonObjectConst item : arr)
    {
        const char *file = item["file"] | "";
        if (file == nullptr || file[0] == '\0') continue;
        if (g_section_count[sec] >= PLAYLIST_MAX_ENTRIES) continue;
        strncpy(g_section_files[sec][g_section_count[sec]], file, PLAYLIST_MAX_FNAME - 1);
        g_section_files[sec][g_section_count[sec]][PLAYLIST_MAX_FNAME - 1] = '\0';
        g_section_count[sec]++;
    }
}

static bool load_from_sd()
{
    if (!sd_manager_ensure_ready()) return false;

    File list_file = SD.open(PLAYLIST_JSON_PATH, FILE_READ);
    if (!list_file)
    {
        Serial.printf("Failed to open playlist json: %s\n", PLAYLIST_JSON_PATH);
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, list_file);
    list_file.close();
    if (err)
    {
        Serial.printf("Playlist json parse failed: %s\n", err.c_str());
        return false;
    }

    playlist_clear_data();

    append_section_entries(doc["obrashenija"].as<JsonArrayConst>(), SEC_OBRASHENIJA);
    append_section_entries(doc["wake_up"].as<JsonArrayConst>(), SEC_WAKE_UP);
    append_section_entries(doc["school_reminder"].as<JsonArrayConst>(), SEC_SCHOOL);
    append_section_entries(doc["fun"].as<JsonArrayConst>(), SEC_FUN);
    append_section_entries(doc["threat"].as<JsonArrayConst>(), SEC_THREAT);
    append_section_entries(doc["adventure"].as<JsonArrayConst>(), SEC_ADVENTURE);
    append_section_entries(doc["sleep"].as<JsonArrayConst>(), SEC_SLEEP);
    append_section_entries(doc["evening"].as<JsonArrayConst>(), SEC_EVENING);
    append_section_entries(doc["system"].as<JsonArrayConst>(), SEC_SYSTEM);

    g_playlist_loaded = true;
    Serial.printf(
        "Playlist loaded: obr=%u, wake=%u, school=%u, fun=%u, threat=%u, adv=%u, sleep=%u, evening=%u, system=%u\n",
        (unsigned)g_section_count[SEC_OBRASHENIJA], (unsigned)g_section_count[SEC_WAKE_UP],
        (unsigned)g_section_count[SEC_SCHOOL], (unsigned)g_section_count[SEC_FUN],
        (unsigned)g_section_count[SEC_THREAT], (unsigned)g_section_count[SEC_ADVENTURE],
        (unsigned)g_section_count[SEC_SLEEP], (unsigned)g_section_count[SEC_EVENING],
        (unsigned)g_section_count[SEC_SYSTEM]);
    return true;
}

bool playlist_ensure_loaded()
{
    if (g_playlist_loaded) return true;

    if (load_from_sd()) return true;

    // If JSON parsing fails due low heap, release heavy UI assets and retry once.
    ui_release_heavy_assets_for_audio();
    return load_from_sd();
}

void playlist_clear_data()
{
    memset(g_section_count, 0, sizeof(g_section_count));
}

void playlist_unload_if_loaded()
{
    if (!g_playlist_loaded) return;
    playlist_clear_data();
    g_playlist_loaded = false;
    Serial.printf("Playlist cache released. Free heap: %lu, largest block: %lu\n",
                  (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void playlist_release_for_playback()
{
    // Keep parsed playlist cached. Clearing it here forces reparsing during playback,
    // which can fail with NoMemory under runtime heap pressure.
}

const char* playlist_pick_random_from_sections(const uint8_t *secs, uint8_t n)
{
    uint16_t total = 0;
    for (uint8_t i = 0; i < n; i++) total += g_section_count[secs[i]];
    if (total == 0) return nullptr;
    uint16_t idx = (uint16_t)(esp_random() % total);
    for (uint8_t i = 0; i < n; i++)
    {
        uint8_t cnt = g_section_count[secs[i]];
        if (idx < cnt) return g_section_files[secs[i]][idx];
        idx -= cnt;
    }
    return nullptr;
}
