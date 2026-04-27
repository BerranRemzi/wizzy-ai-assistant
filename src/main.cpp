#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <Audio.h>
#include <ArduinoJson.h>
#include <vector>
#include <esp_heap_caps.h>

#if __has_include("secrets.h")
#  include "secrets.h"
#else
#  warning "Create include/secrets.h or credentials will be loaded from NVS only"
#  define WIFI_SSID     ""
#  define WIFI_PASSWORD ""
#  define ELEVENLABS_API_KEY ""
#endif

#ifndef ELEVENLABS_API_KEY
#define ELEVENLABS_API_KEY ""
#endif

#ifndef ELEVENLABS_VOICE_ID
#define ELEVENLABS_VOICE_ID "JBFqnCBsd6RMkjVDRZzb"
#endif

#ifndef ELEVENLABS_MODEL_ID
#define ELEVENLABS_MODEL_ID "eleven_multilingual_v2"
#endif

#ifndef ELEVENLABS_TEST_TEXT
#define ELEVENLABS_TEST_TEXT "Здравей, Бернар. Аз съм Маги. А тези мъничета защо спят?"
#endif

#ifndef ELEVENLABS_OUTPUT_FORMAT
#define ELEVENLABS_OUTPUT_FORMAT "mp3_22050_32"
#endif

#ifndef ICECAST_TEST_URL
#define ICECAST_TEST_URL "http://icecast.ndr.de/ndr/ndr1wellenord/kiel/mp3/128/stream.mp3"
#endif

#ifndef NRJ_TEST_URL
#define NRJ_TEST_URL "http://play.global.audio/nrj64"
#endif

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

char buf[128] = {};
int bufindex = 0;
int wifi_close_flag = 0;
char *info[128] = {};
int wifi_flag = 0;
int i = 0;

static Preferences preferences;
static constexpr uint8_t MIC_ADC_PIN = 25;
static constexpr uint8_t RECORD_BUTTON_PIN = 32;
static constexpr uint8_t SPEAKER_PIN = 26;
static constexpr uint8_t AUDIO_LIB_VOLUME = 21; 
static constexpr uint16_t TTS_BRIDGE_PORT = 8081;
static constexpr uint32_t TTS_DRAIN_MIN_MS = 1000;
static constexpr uint32_t TTS_DRAIN_FORCE_MS = 12000;
static constexpr uint32_t TTS_DRAIN_BUFFER_BYTES = 256;
static constexpr uint32_t AUDIO_DMA_SETTLE_MS = 5;
static constexpr uint32_t STARTUP_RAMP_TEST_PLAY_MS = 1800;
static constexpr uint16_t MANUAL_RAMP_SLOW_STEPS = 512;
static constexpr uint16_t MANUAL_RAMP_SLOW_HOLD_SAMPLES = 256;
static constexpr const char *PLAYLIST_JSON_PATH = "/audio/list.json";
static constexpr const char *PLAYLIST_AUDIO_BASE = "/audio/";
static bool sd_ready = false;
static SPIClass sd_spi(HSPI);
Audio audio(true, I2S_DAC_CHANNEL_LEFT_EN);
static WiFiServer tts_bridge_server(TTS_BRIDGE_PORT);
static volatile bool tts_stream_active = false;
static volatile bool tts_bridge_finished = false;
static volatile uint32_t tts_bridge_finished_at_ms = 0;
static bool startup_ramp_test_active = false;
static uint32_t startup_ramp_test_started_at_ms = 0;

struct PlaylistEntry
{
  String text;
  String file;
};

static std::vector<PlaylistEntry> section_obrashenija;
static std::vector<PlaylistEntry> section_wake_up;
static std::vector<PlaylistEntry> section_school_reminder;
static std::vector<PlaylistEntry> section_fun;
static std::vector<PlaylistEntry> section_threat;
static std::vector<PlaylistEntry> section_adventure;
static std::vector<PlaylistEntry> section_sleep;
static std::vector<PlaylistEntry> section_evening;
static std::vector<PlaylistEntry> section_system;

static std::vector<PlaylistEntry> pool_morning;
static std::vector<PlaylistEntry> pool_day;
static std::vector<PlaylistEntry> pool_night;
static std::vector<PlaylistEntry> pool_surprise;

static bool playlist_loaded = false;
static bool first_mode_click_needs_obrashenija = true;
static bool category_sequence_active = false;
static bool category_waiting_for_followup = false;
static String category_followup_path;

//2.4
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18
#define SD_CS 5


unsigned char buffer[256]; // buffer array for data recieve over serial port
int serial_buffer_count = 0;   // counter for buffer array
void clearBufferArray()              // function to clear buffer array
{
  for (int i = 0; i < serial_buffer_count; i++)
  {
    buffer[i] = 0;
  }
}

char CloseData;
int NO_Test_Flag = 0;
int Test_Flag = 0;
int Close_Flag = 0;




//遍历SD卡目录
void listDir(fs::FS & fs, const char *dirname, uint8_t levels)
{
  //  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    //Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  i = 0;
  while (file)
  {
    if (file.isDirectory())
    {
      //      Serial.print("  DIR : ");
      //      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else
    {
      Serial.print("FILE: ");
      Serial.print(file.name());

      Serial.print("SIZE: ");
      Serial.println(file.size());
      i += 16;
    }

    file = root.openNextFile();
  }
}

//SD卡初始化
int SD_init()
{
  // Keep SD on a dedicated SPI bus so TFT/touch bus pin mapping remains untouched.
  sd_spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sd_spi))
  {
    Serial.println("Card Mount Failed");
    return 1;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No TF card attached");
    return 1;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("TF Card Size: %lluMB\n", cardSize);
  listDir(SD, "/", 2);

  //  listDir(SD, "/", 0);
  //  createDir(SD, "/mydir");
  //  listDir(SD, "/", 0);
  //  removeDir(SD, "/mydir");
  //  listDir(SD, "/", 2);
  //  writeFile(SD, "/hello.txt", "Hello ");
  //  appendFile(SD, "/hello.txt", "World!\n");
  //  readFile(SD, "/hello.txt");
  //  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  //  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
  //  Serial.println("SD init over.");

  return 0;
}

static bool connect_wifi_from_sources()
{
  String ssid = WIFI_SSID;
  String pass = WIFI_PASSWORD;

  if (ssid.isEmpty())
  {
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
  // Power-save can cause audio stream underruns/pops on ESP32.
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

static void set_test_status(const char *text)
{
  Serial.println(text);
}

static bool ensure_sd_ready();
static void release_playlist_cache_for_playback();
static void stop_audio_soft_if_active();

static void stop_audio_soft()
{
  audio.stopSong();
  audio.clearDmaBuffer();
}

static void prepare_audio_start()
{
  stop_audio_soft_if_active();
  audio.setVolume(0);
  audio.clearDmaBuffer();
  delay(AUDIO_DMA_SETTLE_MS);
}

static void stop_audio_soft_if_active()
{
  if (!audio.isRunning() && audio.inBufferFilled() == 0)
  {
    return;
  }

  stop_audio_soft();
}

static uint32_t random_u32()
{
  return esp_random();
}

static bool ensure_sd_ready()
{
  if (!sd_ready)
  {
    if (SD_init() != 0)
    {
      set_test_status("SD init failed");
      return false;
    }
    sd_ready = true;
  }

  return true;
}

static void clear_playlist_data()
{
  section_obrashenija.clear();
  section_obrashenija.shrink_to_fit();
  section_wake_up.clear();
  section_wake_up.shrink_to_fit();
  section_school_reminder.clear();
  section_school_reminder.shrink_to_fit();
  section_fun.clear();
  section_fun.shrink_to_fit();
  section_threat.clear();
  section_threat.shrink_to_fit();
  section_adventure.clear();
  section_adventure.shrink_to_fit();
  section_sleep.clear();
  section_sleep.shrink_to_fit();
  section_evening.clear();
  section_evening.shrink_to_fit();
  section_system.clear();
  section_system.shrink_to_fit();
  pool_morning.clear();
  pool_morning.shrink_to_fit();
  pool_day.clear();
  pool_day.shrink_to_fit();
  pool_night.clear();
  pool_night.shrink_to_fit();
  pool_surprise.clear();
  pool_surprise.shrink_to_fit();
}

static void unload_playlist_if_loaded()
{
  if (!playlist_loaded)
  {
    return;
  }

  clear_playlist_data();
  playlist_loaded = false;
  category_sequence_active = false;
  category_waiting_for_followup = false;
  category_followup_path = "";

  Serial.printf("Playlist cache released. Free heap: %lu, largest block: %lu\n",
                (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

static void release_playlist_cache_for_playback()
{
  if (!playlist_loaded)
  {
    return;
  }

  // Keep category follow-up state intact; only free memory-heavy vectors.
  clear_playlist_data();
  playlist_loaded = false;

  Serial.printf("Playlist vectors released for playback. Free heap: %lu, largest block: %lu\n",
                (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

static void append_section_entries(JsonArrayConst arr, std::vector<PlaylistEntry> &out)
{
  for (JsonObjectConst item : arr)
  {
    const char *text = item["text"] | "";
    const char *file = item["file"] | "";
    if (file == nullptr || file[0] == '\0')
    {
      continue;
    }

    PlaylistEntry entry;
    entry.text = text;
    entry.file = file;
    out.push_back(entry);
  }
}

static bool load_playlist_from_sd()
{
  if (!ensure_sd_ready())
  {
    return false;
  }

  File list_file = SD.open(PLAYLIST_JSON_PATH, FILE_READ);
  if (!list_file)
  {
    Serial.printf("Failed to open playlist json: %s\n", PLAYLIST_JSON_PATH);
    set_test_status("list.json open failed");
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, list_file);
  list_file.close();
  if (err)
  {
    Serial.printf("Playlist json parse failed: %s\n", err.c_str());
    set_test_status("list.json parse failed");
    return false;
  }

  clear_playlist_data();

  append_section_entries(doc["obrashenija"].as<JsonArrayConst>(), section_obrashenija);
  append_section_entries(doc["wake_up"].as<JsonArrayConst>(), section_wake_up);
  append_section_entries(doc["school_reminder"].as<JsonArrayConst>(), section_school_reminder);
  append_section_entries(doc["fun"].as<JsonArrayConst>(), section_fun);
  append_section_entries(doc["threat"].as<JsonArrayConst>(), section_threat);
  append_section_entries(doc["adventure"].as<JsonArrayConst>(), section_adventure);
  append_section_entries(doc["sleep"].as<JsonArrayConst>(), section_sleep);
  append_section_entries(doc["evening"].as<JsonArrayConst>(), section_evening);
  append_section_entries(doc["system"].as<JsonArrayConst>(), section_system);

  pool_morning.insert(pool_morning.end(), section_wake_up.begin(), section_wake_up.end());
  pool_morning.insert(pool_morning.end(), section_school_reminder.begin(), section_school_reminder.end());

  pool_day.insert(pool_day.end(), section_fun.begin(), section_fun.end());
  pool_day.insert(pool_day.end(), section_threat.begin(), section_threat.end());
  pool_day.insert(pool_day.end(), section_adventure.begin(), section_adventure.end());

  pool_night.insert(pool_night.end(), section_sleep.begin(), section_sleep.end());
  pool_night.insert(pool_night.end(), section_evening.begin(), section_evening.end());

  pool_surprise.insert(pool_surprise.end(), section_obrashenija.begin(), section_obrashenija.end());
  pool_surprise.insert(pool_surprise.end(), section_wake_up.begin(), section_wake_up.end());
  pool_surprise.insert(pool_surprise.end(), section_school_reminder.begin(), section_school_reminder.end());
  pool_surprise.insert(pool_surprise.end(), section_fun.begin(), section_fun.end());
  pool_surprise.insert(pool_surprise.end(), section_threat.begin(), section_threat.end());
  pool_surprise.insert(pool_surprise.end(), section_adventure.begin(), section_adventure.end());
  pool_surprise.insert(pool_surprise.end(), section_sleep.begin(), section_sleep.end());
  pool_surprise.insert(pool_surprise.end(), section_evening.begin(), section_evening.end());
  pool_surprise.insert(pool_surprise.end(), section_system.begin(), section_system.end());

  playlist_loaded = true;
  Serial.printf(
    "Playlist loaded: obr=%u, wake=%u, school=%u, fun=%u, threat=%u, adv=%u, sleep=%u, evening=%u, system=%u\n",
    (unsigned)section_obrashenija.size(),
    (unsigned)section_wake_up.size(),
    (unsigned)section_school_reminder.size(),
    (unsigned)section_fun.size(),
    (unsigned)section_threat.size(),
    (unsigned)section_adventure.size(),
    (unsigned)section_sleep.size(),
    (unsigned)section_evening.size(),
    (unsigned)section_system.size());
  return true;
}

static bool ensure_playlist_loaded()
{
  if (playlist_loaded)
  {
    return true;
  }

  return load_playlist_from_sd();
}

static String build_audio_path(const String &file_name)
{
  if (file_name.length() == 0)
  {
    return String();
  }

  if (file_name.startsWith("/"))
  {
    return file_name;
  }

  return String(PLAYLIST_AUDIO_BASE) + file_name;
}

static const PlaylistEntry* pick_random_entry(const std::vector<PlaylistEntry> &entries)
{
  if (entries.empty())
  {
    return nullptr;
  }

  uint32_t index = random_u32() % entries.size();
  return &entries[index];
}

static bool request_sd_file_from_playlist_entry(const PlaylistEntry &entry)
{
  if (!ensure_sd_ready())
  {
    return false;
  }

  String full_path = build_audio_path(entry.file);
  if (full_path.length() == 0)
  {
    set_test_status("Empty audio path");
    return false;
  }

  if (!SD.exists(full_path.c_str()))
  {
    Serial.printf("Missing SD file: %s\n", full_path.c_str());
    set_test_status("Audio file missing");
    return false;
  }

  release_playlist_cache_for_playback();
  prepare_audio_start();
  if (!audio.connecttoFS(SD, full_path.c_str()))
  {
    audio.setVolume(0);
    Serial.printf("Failed to play SD file: %s\n", full_path.c_str());
    set_test_status("SD file failed");
    return false;
  }
  audio.setVolume(AUDIO_LIB_VOLUME);

  Serial.printf("Playing SD file: %s\n", full_path.c_str());
  if (entry.text.length() > 0)
  {
    set_test_status(entry.text.c_str());
  }
  else
  {
    set_test_status("Playing SD audio");
  }

  return true;
}

static bool play_random_from_pool(const std::vector<PlaylistEntry> &pool)
{
  const PlaylistEntry *entry = pick_random_entry(pool);
  if (entry == nullptr)
  {
    set_test_status("Pool is empty");
    return false;
  }

  return request_sd_file_from_playlist_entry(*entry);
}

static bool play_mode_pool(const std::vector<PlaylistEntry> &mode_pool)
{
  if (!ensure_playlist_loaded())
  {
    return false;
  }

  const PlaylistEntry *mode_entry = pick_random_entry(mode_pool);
  if (mode_entry == nullptr)
  {
    set_test_status("Mode pool is empty");
    return false;
  }

  if (first_mode_click_needs_obrashenija)
  {
    const PlaylistEntry *obr_entry = pick_random_entry(section_obrashenija);
    if (obr_entry != nullptr)
    {
      String followup = build_audio_path(mode_entry->file);
      if (followup.length() > 0)
      {
        category_followup_path = followup;
        category_waiting_for_followup = true;
        category_sequence_active = true;
      }

      first_mode_click_needs_obrashenija = false;
      if (request_sd_file_from_playlist_entry(*obr_entry))
      {
        return true;
      }

      category_followup_path = "";
      category_waiting_for_followup = false;
      category_sequence_active = false;
    }
  }

  first_mode_click_needs_obrashenija = false;
  category_sequence_active = false;
  category_waiting_for_followup = false;
  category_followup_path = "";
  return request_sd_file_from_playlist_entry(*mode_entry);
}

static bool play_random_obrashenija_plus_random_mode()
{
  if (!ensure_playlist_loaded())
  {
    return false;
  }

  const PlaylistEntry *obr_entry = pick_random_entry(section_obrashenija);
  if (obr_entry == nullptr)
  {
    set_test_status("obrashenija section empty");
    return false;
  }

  std::vector<const std::vector<PlaylistEntry>*> non_empty_mode_sections;
  if (!section_wake_up.empty()) non_empty_mode_sections.push_back(&section_wake_up);
  if (!section_school_reminder.empty()) non_empty_mode_sections.push_back(&section_school_reminder);
  if (!section_fun.empty()) non_empty_mode_sections.push_back(&section_fun);
  if (!section_threat.empty()) non_empty_mode_sections.push_back(&section_threat);
  if (!section_adventure.empty()) non_empty_mode_sections.push_back(&section_adventure);
  if (!section_sleep.empty()) non_empty_mode_sections.push_back(&section_sleep);
  if (!section_evening.empty()) non_empty_mode_sections.push_back(&section_evening);

  if (non_empty_mode_sections.empty())
  {
    set_test_status("mode sections are empty");
    return false;
  }

  uint32_t section_index = random_u32() % non_empty_mode_sections.size();
  const std::vector<PlaylistEntry> *chosen_section = non_empty_mode_sections[section_index];
  const PlaylistEntry *mode_entry = pick_random_entry(*chosen_section);
  if (mode_entry == nullptr)
  {
    set_test_status("mode entry missing");
    return false;
  }

  String followup = build_audio_path(mode_entry->file);
  if (followup.length() == 0)
  {
    set_test_status("mode path empty");
    return false;
  }

  category_followup_path = followup;
  category_waiting_for_followup = true;
  category_sequence_active = true;

  if (request_sd_file_from_playlist_entry(*obr_entry))
  {
    return true;
  }

  category_followup_path = "";
  category_waiting_for_followup = false;
  category_sequence_active = false;
  return false;
}

static bool play_startup_system_random()
{
  if (!ensure_playlist_loaded())
  {
    return false;
  }

  if (section_system.empty())
  {
    set_test_status("system section empty");
    return false;
  }

  return play_random_from_pool(section_system);
}

static String json_escape(const char *text)
{
  String out;
  while (*text)
  {
    char c = *text++;
    switch (c)
    {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

static String url_encode(const char *text)
{
  static const char hex[] = "0123456789ABCDEF";
  String out;
  while (*text)
  {
    uint8_t c = (uint8_t)(*text++);
    bool safe = (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~';
    if (safe)
    {
      out += (char)c;
    }
    else
    {
      out += '%';
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

static String read_http_line(WiFiClient &client, uint32_t timeout_ms)
{
  String line;
  uint32_t started = millis();
  while ((millis() - started) < timeout_ms)
  {
    while (client.available())
    {
      char ch = (char)client.read();
      if (ch == '\n') return line;
      if (ch != '\r') line += ch;
    }
    delay(1);
  }
  return String();
}

static bool read_exact_with_timeout(WiFiClient &client, uint8_t *dst, size_t len, uint32_t timeout_ms)
{
  size_t offset = 0;
  uint32_t last_data = millis();
  while (offset < len)
  {
    int avail = client.available();
    if (avail > 0)
    {
      size_t want = min((size_t)avail, len - offset);
      int n = client.read(dst + offset, want);
      if (n > 0)
      {
        offset += (size_t)n;
        last_data = millis();
      }
    }
    else
    {
      if ((millis() - last_data) > timeout_ms) return false;
      delay(1);
    }
  }
  return true;
}

static void handle_tts_bridge_client(WiFiClient &downstream)
{
  String request_line = read_http_line(downstream, 4000);
  if (!request_line.startsWith("GET "))
  {
    downstream.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
    downstream.stop();
    return;
  }

  // Consume request headers.
  while (true)
  {
    String h = read_http_line(downstream, 2000);
    if (h.length() == 0) break;
  }

  if (strlen(ELEVENLABS_API_KEY) == 0)
  {
    downstream.print("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\nMissing API key\n");
    downstream.stop();
    return;
  }

  WiFiClientSecure upstream;
  upstream.setInsecure();
  if (!upstream.connect("api.elevenlabs.io", 443))
  {
    downstream.print("HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\nUpstream connect failed\n");
    downstream.stop();
    return;
  }

  String voice_id = url_encode(ELEVENLABS_VOICE_ID);
  String payload = "{\"text\":\"" + json_escape(ELEVENLABS_TEST_TEXT) + "\",\"model_id\":\"" + json_escape(ELEVENLABS_MODEL_ID) + "\"}";
  String path = "/v1/text-to-speech/" + voice_id + "/stream?output_format=" + ELEVENLABS_OUTPUT_FORMAT;

  upstream.print("POST " + path + " HTTP/1.1\r\n");
  upstream.print("Host: api.elevenlabs.io\r\n");
  upstream.print("xi-api-key: " + String(ELEVENLABS_API_KEY) + "\r\n");
  upstream.print("Content-Type: application/json\r\n");
  upstream.print("Accept: audio/mpeg\r\n");
  upstream.print("Accept-Encoding: identity\r\n");
  upstream.print("Connection: close\r\n");
  upstream.print("Content-Length: " + String(payload.length()) + "\r\n\r\n");
  upstream.print(payload);

  String status = read_http_line(upstream, 15000);
  bool ok = status.startsWith("HTTP/1.1 200") || status.startsWith("HTTP/1.0 200");

  bool chunked = false;
  bool has_content_type = false;
  String content_type = "audio/mpeg";
  while (true)
  {
    String h = read_http_line(upstream, 15000);
    if (h.length() == 0) break;
    String hl = h;
    hl.toLowerCase();
    if (hl.startsWith("transfer-encoding:") && hl.indexOf("chunked") >= 0) chunked = true;
    if (hl.startsWith("content-type:"))
    {
      has_content_type = true;
      content_type = h.substring(strlen("Content-Type:"));
      content_type.trim();
    }
  }

  if (!ok)
  {
    downstream.print("HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\nElevenLabs error: ");
    downstream.print(status);
    downstream.print("\n");
    upstream.stop();
    downstream.stop();
    return;
  }

  // Always stream raw MP3 bytes downstream. If upstream is chunked,
  // we decode chunks here so the audio client receives plain audio payload.
  downstream.print("HTTP/1.0 200 OK\r\n");
  if (has_content_type) downstream.print("Content-Type: " + content_type + "\r\n");
  else downstream.print("Content-Type: audio/mpeg\r\n");
  downstream.print("Connection: close\r\n\r\n");

  uint8_t relay_buf[1024];
  if (chunked)
  {
    for (;;)
    {
      String chunk_line = read_http_line(upstream, 30000);
      if (chunk_line.length() == 0)
      {
        break;
      }

      int semicolon = chunk_line.indexOf(';');
      if (semicolon >= 0) chunk_line = chunk_line.substring(0, semicolon);
      chunk_line.trim();
      size_t chunk_size = strtoul(chunk_line.c_str(), nullptr, 16);
      if (chunk_size == 0)
      {
        // Consume optional trailer headers.
        while (true)
        {
          String trailer = read_http_line(upstream, 30000);
          if (trailer.length() == 0) break;
        }
        break;
      }

      size_t remaining = chunk_size;
      while (remaining > 0)
      {
        size_t part = min(remaining, sizeof(relay_buf));
        if (!read_exact_with_timeout(upstream, relay_buf, part, 30000))
        {
          remaining = 0;
          break;
        }
        downstream.write(relay_buf, part);
        remaining -= part;
      }

      uint8_t crlf[2];
      if (!read_exact_with_timeout(upstream, crlf, 2, 30000))
      {
        break;
      }
    }
  }
  else
  {
    uint32_t idle_started = millis();
    while (upstream.connected() || upstream.available())
    {
      int avail = upstream.available();
      if (avail > 0)
      {
        int n = upstream.read(relay_buf, (size_t)min(avail, (int)sizeof(relay_buf)));
        if (n > 0)
        {
          downstream.write(relay_buf, (size_t)n);
          idle_started = millis();
        }
      }
      else
      {
        if ((millis() - idle_started) > 30000) break;
        delay(1);
      }
    }
  }

  downstream.clear();
  upstream.stop();
  downstream.stop();

  tts_bridge_finished = true;
  tts_bridge_finished_at_ms = millis();
}

static void tts_bridge_task(void* parameter)
{
  for (;;)
  {
    WiFiClient client = tts_bridge_server.accept();
    if (client)
    {
      handle_tts_bridge_client(client);
    }
    vTaskDelay(2 / portTICK_PERIOD_MS);
  }
}

static bool request_elevenlabs_stream_test()
{
  if (strlen(ELEVENLABS_API_KEY) == 0)
  {
    set_test_status("Missing ELEVENLABS_API_KEY");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    wifi_flag = connect_wifi_from_sources() ? 1 : 0;
    if (wifi_flag == 0)
    {
      set_test_status("WiFi not connected");
      return false;
    }
  }

  // Free current stream/network resources before opening TTS stream.
  prepare_audio_start();
  unload_playlist_if_loaded();
  tts_stream_active = false;
  tts_bridge_finished = false;
  tts_bridge_finished_at_ms = 0;
  delay(50);

  Serial.println("TTS mode: bridge POST -> local GET");
  set_test_status("Connecting to TTS...");

  String local_url = "http://" + WiFi.localIP().toString() + ":" + String(TTS_BRIDGE_PORT) + "/tts";
  if (!audio.connecttohost(local_url.c_str()))
  {
    audio.setVolume(0);
    set_test_status("TTS bridge connect failed");
    return false;
  }
  audio.setVolume(AUDIO_LIB_VOLUME);

  set_test_status("Playing TTS stream");
  tts_stream_active = true;
  
  return true;
}

static bool request_http_stream_test(const char *url, const char *name)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    wifi_flag = connect_wifi_from_sources() ? 1 : 0;
    if (wifi_flag == 0)
    {
      set_test_status("WiFi not connected");
      return false;
    }
  }

  // Always stop current stream before switching to another station.
  prepare_audio_start();
  unload_playlist_if_loaded();
  if (!audio.connecttohost(url))
  {
    audio.setVolume(0);
    Serial.printf("%s stream failed\n", name);
    set_test_status("Stream failed");
    return false;
  }
  audio.setVolume(AUDIO_LIB_VOLUME);

  Serial.printf("Playing %s stream\n", name);
  set_test_status("Playing stream");
  return true;
}

static bool request_sd_file_test(const char *path)
{
  if (!ensure_sd_ready())
  {
    return false;
  }

  release_playlist_cache_for_playback();
  prepare_audio_start();
  if (!audio.connecttoFS(SD, path))
  {
    audio.setVolume(0);
    Serial.printf("Failed to play SD file: %s\n", path);
    set_test_status("SD file failed");
    return false;
  }
  audio.setVolume(AUDIO_LIB_VOLUME);

  Serial.printf("Playing SD file: %s\n", path);
  set_test_status("Playing SD audio");
  return true;
}

static void run_startup_ramp_test()
{
  set_test_status("Startup ramp test: ramp-up");
  if (request_sd_file_test("/audio/adv_02.mp3"))
  {
    startup_ramp_test_active = true;
    startup_ramp_test_started_at_ms = millis();
  }
  else
  {
    startup_ramp_test_active = false;
  }
}

static void handle_serial_command(char cmd)
{
  switch (cmd)
  {
    case '0':
      Serial.println("Serial cmd 0: random obrashenija + random mode");
      set_test_status("Random obrashenija + mode...");
      play_random_obrashenija_plus_random_mode();
      break;
    case '1':
      Serial.println("Serial cmd 1: play radio");
      set_test_status("Requesting radio stream...");
      request_http_stream_test(NRJ_TEST_URL, "NRJ");
      break;
    case '2':
      Serial.println("Serial cmd 2: play test");
      set_test_status("Requesting test stream...");
      request_http_stream_test(ICECAST_TEST_URL, "NDR");
      break;
    case '3':
      Serial.println("Serial cmd 3: elevenlabs test");
      set_test_status("Requesting TTS stream...");
      request_elevenlabs_stream_test();
      break;
    case 't':
      Serial.println("Serial cmd t: ramp-up/down test with SD file");
      startup_ramp_test_active = false;
      run_startup_ramp_test();
      break;
    case 'T':
      Serial.println("Serial cmd T: slow DAC ramp-up/down only");
      stop_audio_soft();
      startup_ramp_test_active = false;
      set_test_status("Slow DAC ramp cycle");
      //audio.runInternalDACBiasCycle(MANUAL_RAMP_SLOW_STEPS, MANUAL_RAMP_SLOW_HOLD_SAMPLES);
      set_test_status("Slow DAC ramp complete");
      break;
    case '\r':
    case '\n':
    case ' ':
      break;
    default:
      Serial.printf("Unknown serial command: %c\n", cmd);
      Serial.println("Use: 0=obrashenija+mode, 1=play radio, 2=play test, 3=elevenlabs test, t=SD ramp test, T=slow DAC ramp");
      break;
  }
}

void audio_info(const char *info)
{
  Serial.print("audio_info: ");
  Serial.println(info);
}

void audio_showstreamtitle(const char *info)
{
  Serial.print("audio_title: ");
  Serial.println(info);
}

void audio_eof_stream(const char *info)
{
  Serial.print("audio_eof_stream: ");
  Serial.println(info ? info : "");
  if (category_sequence_active && category_waiting_for_followup)
  {
    category_waiting_for_followup = false;
    String path_to_play = category_followup_path;
    category_followup_path = "";
    category_sequence_active = false;
    if (path_to_play.length() > 0)
    {
      request_sd_file_test(path_to_play.c_str());
      return;
    }
  }

  if (tts_stream_active)
  {
    tts_stream_active = false;
    tts_bridge_finished = false;
    tts_bridge_finished_at_ms = 0;
    set_test_status("TTS finished");
  }
}

void audio_eof_mp3(const char *info)
{
  Serial.print("audio_eof_mp3: ");
  Serial.println(info ? info : "");
  if (category_sequence_active && category_waiting_for_followup)
  {
    category_waiting_for_followup = false;
    String path_to_play = category_followup_path;
    category_followup_path = "";
    category_sequence_active = false;
    if (path_to_play.length() > 0)
    {
      request_sd_file_test(path_to_play.c_str());
      return;
    }
  }

  if (tts_stream_active)
  {
    tts_stream_active = false;
    tts_bridge_finished = false;
    tts_bridge_finished_at_ms = 0;
    set_test_status("TTS finished");
  }
}

void setup()
{
  Serial.begin( 115200 ); /*初始化串口*/
  Serial2.begin( 115200 ); /*初始化串口2*/

  // If mic and speaker share a pin, speaker mode takes precedence for this firmware path.
  if (MIC_ADC_PIN != SPEAKER_PIN)
  {
    pinMode(MIC_ADC_PIN, INPUT);
  }
  // Push-to-talk button: active-low on GPIO32.
  pinMode(RECORD_BUTTON_PIN, INPUT_PULLUP);

  //audio.setInternalDACBiasRamp(128, 96);
  audio.setVolume(AUDIO_LIB_VOLUME); // 0..21

  wifi_flag = connect_wifi_from_sources() ? 1 : 0;

  //SD卡
  //  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  //  delay(100);
  //  if (SD_init() == 1)
  //  {
  //    Serial.println("SD卡初始化失败！");
  //  }
  //  else
  //    Serial.println("SD卡初始化成功");
  //  delay(2000);

  tts_bridge_server.begin();
  xTaskCreate(
    tts_bridge_task,
    "TTS_Bridge",
    8192,
    NULL,
    1,
    NULL
  );

  set_test_status("Audio test ready");
  play_startup_system_random();
  Serial.println("Serial commands: 0=obrashenija+mode, 1=play radio, 2=play test, 3=elevenlabs test, t=SD ramp test, T=slow DAC ramp");
  Serial.println( "Setup done" );
}

void loop()
{
  while (Serial.available() > 0)
  {
    char cmd = (char)Serial.read();
    handle_serial_command(cmd);
  }

  audio.loop();

  if (tts_stream_active && tts_bridge_finished)
  {
    uint32_t now = millis();
    uint32_t elapsed = now - tts_bridge_finished_at_ms;
    uint32_t buffered = audio.inBufferFilled();

    // Prefer natural decoder EOF. Fallback stop only after data has had time
    // to drain from the internal buffer, or after a long hard timeout.
    if ((elapsed >= TTS_DRAIN_MIN_MS && buffered <= TTS_DRAIN_BUFFER_BYTES) ||
        (elapsed >= TTS_DRAIN_FORCE_MS))
    {
      stop_audio_soft();
      tts_stream_active = false;
      tts_bridge_finished = false;
      tts_bridge_finished_at_ms = 0;
      set_test_status("TTS finished");
    }
  }

  if (startup_ramp_test_active)
  {
    uint32_t now = millis();
    if ((now - startup_ramp_test_started_at_ms) >= STARTUP_RAMP_TEST_PLAY_MS)
    {
      set_test_status("Startup ramp test: ramp-down");
      stop_audio_soft();
      startup_ramp_test_active = false;
      set_test_status("Startup ramp test complete");
    }
  }
  //delay(1);
}
