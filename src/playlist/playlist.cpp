#include "playlist.h"
#include "storage/sd_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <FS.h>
#include <esp_heap_caps.h>

char g_section_files[PLAYLIST_NUM_SECTIONS][PLAYLIST_MAX_ENTRIES][PLAYLIST_MAX_FNAME];
uint8_t g_section_count[PLAYLIST_NUM_SECTIONS];
uint8_t g_button_sequence_step_count;
uint8_t g_button_sequence_section_count[PLAYLIST_MAX_BUTTON_SEQUENCE_STEPS];
uint8_t g_button_sequence_sections[PLAYLIST_MAX_BUTTON_SEQUENCE_STEPS][PLAYLIST_NUM_SECTIONS];
char g_clock_time_files[24][PLAYLIST_MAX_ENTRIES][PLAYLIST_MAX_FNAME];
uint8_t g_clock_time_count[24];
uint8_t g_clock_compose_count[24];
uint8_t g_clock_compose_sections[24][PLAYLIST_NUM_SECTIONS];

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
static const uint8_t BUTTON_RECENT_HISTORY_SIZE = 10;

static bool g_playlist_loaded = false;
static uint32_t g_last_missing_log_ms = 0;
static uint32_t g_last_parse_log_ms = 0;
static const uint32_t PLAYLIST_ERROR_LOG_INTERVAL_MS = 5000;
static char g_recent_button_files[BUTTON_RECENT_HISTORY_SIZE][PLAYLIST_MAX_FNAME];
static uint8_t g_recent_button_file_count = 0;
static uint8_t g_recent_button_file_next = 0;

static void log_playlist_error_throttled(uint32_t *last_ms, const char *fmt, const char *arg)
{
    const uint32_t now = millis();
    if (*last_ms == 0 || (uint32_t)(now - *last_ms) >= PLAYLIST_ERROR_LOG_INTERVAL_MS)
    {
        Serial.printf(fmt, arg);
        *last_ms = now;
    }
}

static int8_t section_from_name(const char *name)
{
    if (name == nullptr) return -1;
    if (strcmp(name, "obrashenija") == 0) return SEC_OBRASHENIJA;
    if (strcmp(name, "wake_up") == 0) return SEC_WAKE_UP;
    if (strcmp(name, "school_reminder") == 0) return SEC_SCHOOL;
    if (strcmp(name, "fun") == 0) return SEC_FUN;
    if (strcmp(name, "threat") == 0) return SEC_THREAT;
    if (strcmp(name, "adventure") == 0) return SEC_ADVENTURE;
    if (strcmp(name, "sleep") == 0) return SEC_SLEEP;
    if (strcmp(name, "evening") == 0) return SEC_EVENING;
    if (strcmp(name, "system") == 0) return SEC_SYSTEM;
    return -1;
}

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

static void append_clock_time_entries(JsonArrayConst arr, uint8_t hour)
{
    for (JsonObjectConst item : arr)
    {
        const char *file = item["file"] | "";
        if (file == nullptr || file[0] == '\0') continue;
        if (g_clock_time_count[hour] >= PLAYLIST_MAX_ENTRIES) continue;
        strncpy(g_clock_time_files[hour][g_clock_time_count[hour]], file, PLAYLIST_MAX_FNAME - 1);
        g_clock_time_files[hour][g_clock_time_count[hour]][PLAYLIST_MAX_FNAME - 1] = '\0';
        g_clock_time_count[hour]++;
    }
}

static void append_compose_sections(JsonArrayConst arr, uint8_t *out_sections, uint8_t *out_count, uint8_t max_count)
{
    for (JsonVariantConst item : arr)
    {
        const char *name = item.as<const char*>();
        int8_t sec = section_from_name(name);
        if (sec < 0) continue;

        bool exists = false;
        for (uint8_t i = 0; i < *out_count; i++)
        {
            if (out_sections[i] == (uint8_t)sec) { exists = true; break; }
        }
        if (exists) continue;
        if (*out_count >= max_count) continue;
        out_sections[*out_count] = (uint8_t)sec;
        (*out_count)++;
    }
}

static bool is_recent_button_file(const char *fname)
{
    if (fname == nullptr || fname[0] == '\0') return false;
    for (uint8_t i = 0; i < g_recent_button_file_count; i++)
    {
        if (strncmp(g_recent_button_files[i], fname, PLAYLIST_MAX_FNAME) == 0) return true;
    }
    return false;
}

static void remember_recent_button_file(const char *fname)
{
    if (fname == nullptr || fname[0] == '\0') return;

    strncpy(g_recent_button_files[g_recent_button_file_next], fname, PLAYLIST_MAX_FNAME - 1);
    g_recent_button_files[g_recent_button_file_next][PLAYLIST_MAX_FNAME - 1] = '\0';
    g_recent_button_file_next = (uint8_t)((g_recent_button_file_next + 1) % BUTTON_RECENT_HISTORY_SIZE);
    if (g_recent_button_file_count < BUTTON_RECENT_HISTORY_SIZE) g_recent_button_file_count++;
}

static const char* pick_random_from_sections_avoiding_recent(const uint8_t *secs, uint8_t n)
{
    uint16_t total = 0;
    uint16_t fresh_total = 0;

    for (uint8_t i = 0; i < n; i++)
    {
        const uint8_t sec = secs[i];
        total += g_section_count[sec];
        for (uint8_t j = 0; j < g_section_count[sec]; j++)
        {
            if (!is_recent_button_file(g_section_files[sec][j])) fresh_total++;
        }
    }

    if (total == 0) return nullptr;

    uint16_t idx = (uint16_t)(esp_random() % (fresh_total > 0 ? fresh_total : total));
    for (uint8_t i = 0; i < n; i++)
    {
        const uint8_t sec = secs[i];
        for (uint8_t j = 0; j < g_section_count[sec]; j++)
        {
            const bool is_recent = is_recent_button_file(g_section_files[sec][j]);
            if (fresh_total > 0 && is_recent) continue;
            if (idx == 0) return g_section_files[sec][j];
            idx--;
        }
    }

    return nullptr;
}

static bool load_from_sd()
{
    if (!sd_manager_ensure_ready()) return false;

    if (!SD.exists(PLAYLIST_JSON_PATH))
    {
        log_playlist_error_throttled(&g_last_missing_log_ms,
                                     "Playlist json is missing: %s\n",
                                     PLAYLIST_JSON_PATH);
        return false;
    }

    File list_file = SD.open(PLAYLIST_JSON_PATH, FILE_READ);
    if (!list_file)
    {
        log_playlist_error_throttled(&g_last_missing_log_ms,
                                     "Failed to open playlist json: %s\n",
                                     PLAYLIST_JSON_PATH);
        return false;
    }

    if (list_file.size() == 0)
    {
        list_file.close();
        SD.remove(PLAYLIST_JSON_PATH);
        log_playlist_error_throttled(&g_last_missing_log_ms,
                                     "Playlist json was empty and removed: %s\n",
                                     PLAYLIST_JSON_PATH);
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, list_file);
    list_file.close();
    if (err)
    {
        log_playlist_error_throttled(&g_last_parse_log_ms,
                                     "Playlist json parse failed: %s\n",
                                     err.c_str());
        return false;
    }

    g_last_missing_log_ms = 0;
    g_last_parse_log_ms = 0;

    playlist_clear_data();

    JsonObjectConst categories = doc["categories"].as<JsonObjectConst>();
    if (categories.isNull()) categories = doc.as<JsonObjectConst>();

    append_section_entries(categories["obrashenija"].as<JsonArrayConst>(), SEC_OBRASHENIJA);
    append_section_entries(categories["wake_up"].as<JsonArrayConst>(), SEC_WAKE_UP);
    append_section_entries(categories["school_reminder"].as<JsonArrayConst>(), SEC_SCHOOL);
    append_section_entries(categories["fun"].as<JsonArrayConst>(), SEC_FUN);
    append_section_entries(categories["threat"].as<JsonArrayConst>(), SEC_THREAT);
    append_section_entries(categories["adventure"].as<JsonArrayConst>(), SEC_ADVENTURE);
    append_section_entries(categories["sleep"].as<JsonArrayConst>(), SEC_SLEEP);
    append_section_entries(categories["evening"].as<JsonArrayConst>(), SEC_EVENING);
    append_section_entries(categories["system"].as<JsonArrayConst>(), SEC_SYSTEM);

    JsonArrayConst button_sequence = doc["button"]["sequence"].as<JsonArrayConst>();
    for (JsonObjectConst step : button_sequence)
    {
        if (g_button_sequence_step_count >= PLAYLIST_MAX_BUTTON_SEQUENCE_STEPS) break;
        uint8_t idx = g_button_sequence_step_count;
        append_compose_sections(step["compose"].as<JsonArrayConst>(),
                                g_button_sequence_sections[idx],
                                &g_button_sequence_section_count[idx],
                                PLAYLIST_NUM_SECTIONS);
        if (g_button_sequence_section_count[idx] > 0) g_button_sequence_step_count++;
    }

    JsonObjectConst clock = doc["clock"].as<JsonObjectConst>();
    for (JsonPairConst hour_pair : clock)
    {
        const char *hour_key = hour_pair.key().c_str();
        int hour = atoi(hour_key);
        if (hour < 0 || hour > 23) continue;

        JsonObjectConst hour_obj = hour_pair.value().as<JsonObjectConst>();
        append_clock_time_entries(hour_obj["time"].as<JsonArrayConst>(), (uint8_t)hour);
        append_compose_sections(hour_obj["compose"].as<JsonArrayConst>(),
                                g_clock_compose_sections[hour],
                                &g_clock_compose_count[hour],
                                PLAYLIST_NUM_SECTIONS);
    }

    g_playlist_loaded = true;
    Serial.printf(
        "Playlist loaded: obr=%u, wake=%u, school=%u, fun=%u, threat=%u, adv=%u, sleep=%u, evening=%u, system=%u, button_steps=%u\n",
        (unsigned)g_section_count[SEC_OBRASHENIJA], (unsigned)g_section_count[SEC_WAKE_UP],
        (unsigned)g_section_count[SEC_SCHOOL], (unsigned)g_section_count[SEC_FUN],
        (unsigned)g_section_count[SEC_THREAT], (unsigned)g_section_count[SEC_ADVENTURE],
        (unsigned)g_section_count[SEC_SLEEP], (unsigned)g_section_count[SEC_EVENING],
        (unsigned)g_section_count[SEC_SYSTEM], (unsigned)g_button_sequence_step_count);
    return true;
}

bool playlist_ensure_loaded()
{
    if (g_playlist_loaded) return true;
    return load_from_sd();
}

void playlist_clear_data()
{
    memset(g_section_count, 0, sizeof(g_section_count));
    memset(g_button_sequence_section_count, 0, sizeof(g_button_sequence_section_count));
    memset(g_button_sequence_sections, 0, sizeof(g_button_sequence_sections));
    g_button_sequence_step_count = 0;
    memset(g_clock_time_count, 0, sizeof(g_clock_time_count));
    memset(g_clock_compose_count, 0, sizeof(g_clock_compose_count));
    memset(g_clock_compose_sections, 0, sizeof(g_clock_compose_sections));
    memset(g_recent_button_files, 0, sizeof(g_recent_button_files));
    g_recent_button_file_count = 0;
    g_recent_button_file_next = 0;
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

bool playlist_pick_button_sequence_files(char out_files[][PLAYLIST_MAX_FNAME], uint8_t max_files, uint8_t *out_count)
{
    if (out_count == nullptr) return false;
    *out_count = 0;
    if (max_files == 0) return false;

    uint8_t steps = g_button_sequence_step_count;
    if (steps == 0)
    {
        // Backward compatible fallback: obrashenija + random mode section.
        const uint8_t fallback1[] = { SEC_OBRASHENIJA };
        const uint8_t fallback2[] = { SEC_WAKE_UP, SEC_SCHOOL, SEC_FUN, SEC_THREAT, SEC_ADVENTURE, SEC_SLEEP, SEC_EVENING };
        const char *f1 = pick_random_from_sections_avoiding_recent(fallback1, sizeof(fallback1));
        const char *f2 = pick_random_from_sections_avoiding_recent(fallback2, sizeof(fallback2));
        if (f1 == nullptr || f2 == nullptr) return false;
        strncpy(out_files[0], f1, PLAYLIST_MAX_FNAME - 1);
        out_files[0][PLAYLIST_MAX_FNAME - 1] = '\0';
        if (max_files > 1)
        {
            strncpy(out_files[1], f2, PLAYLIST_MAX_FNAME - 1);
            out_files[1][PLAYLIST_MAX_FNAME - 1] = '\0';
            *out_count = 2;
        }
        else
        {
            *out_count = 1;
        }

        for (uint8_t i = 0; i < *out_count; i++) remember_recent_button_file(out_files[i]);
        return true;
    }

    for (uint8_t i = 0; i < steps && *out_count < max_files; i++)
    {
        uint8_t sec_count = g_button_sequence_section_count[i];
        if (sec_count == 0) continue;
        const char *picked = pick_random_from_sections_avoiding_recent(g_button_sequence_sections[i], sec_count);
        if (picked == nullptr) continue;
        strncpy(out_files[*out_count], picked, PLAYLIST_MAX_FNAME - 1);
        out_files[*out_count][PLAYLIST_MAX_FNAME - 1] = '\0';
        (*out_count)++;
    }

    for (uint8_t i = 0; i < *out_count; i++) remember_recent_button_file(out_files[i]);
    return *out_count > 0;
}

const char* playlist_pick_random_clock_time(uint8_t hour)
{
    if (hour > 23) return nullptr;
    if (g_clock_time_count[hour] == 0) return nullptr;
    return g_clock_time_files[hour][esp_random() % g_clock_time_count[hour]];
}

const char* playlist_pick_random_clock_compose(uint8_t hour)
{
    if (hour > 23) return nullptr;
    uint8_t cnt = g_clock_compose_count[hour];
    if (cnt == 0) return nullptr;
    return playlist_pick_random_from_sections(g_clock_compose_sections[hour], cnt);
}
