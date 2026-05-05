#pragma once

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

#ifndef WEB_DAV_ENABLED
#define WEB_DAV_ENABLED 1
#endif

#ifndef WEB_DAV_PORT
#define WEB_DAV_PORT 80
#endif

#ifndef WEB_DAV_BASE_PATH
#define WEB_DAV_BASE_PATH "/dav"
#endif

#ifndef WEB_DAV_USERNAME
#define WEB_DAV_USERNAME ""
#endif

#ifndef WEB_DAV_PASSWORD
#define WEB_DAV_PASSWORD ""
#endif

#ifndef OTA_ENABLED
#define OTA_ENABLED 1
#endif

#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "wizzy-assistant"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

// Hardware pins
#define PIN_MIC_ADC          25
#define PIN_PLAY_BUTTON      21
#define PIN_SPEAKER          26
// SD on VSPI (keep SD on separate SPI bus from TFT)
#define PIN_SD_MOSI          23
#define PIN_SD_MISO          19
#define PIN_SD_SCK           18
#define PIN_SD_CS            5
#define PIN_BACKLIGHT        27

// Audio
#define AUDIO_LIB_VOLUME     21

// TTS bridge
#define TTS_BRIDGE_PORT      8081
#define TTS_DRAIN_MIN_MS     1000
#define TTS_DRAIN_FORCE_MS   12000
#define TTS_DRAIN_BUFFER_BYTES 256

// Audio DMA settle delay before playback starts
#define AUDIO_DMA_SETTLE_MS  5

// UI memory guard: when audio is running and free heap is below this threshold,
// screen updates are temporarily paused to prioritize audio stability.
#define UI_UPDATE_MIN_FREE_HEAP_BYTES 35000

// Playlist
#define PLAYLIST_JSON_PATH   "/audio/list.json"
#define PLAYLIST_AUDIO_BASE  "/audio/"
#define PLAYLIST_NUM_SECTIONS 9
#define PLAYLIST_MAX_ENTRIES  20
#define PLAYLIST_MAX_FNAME    16
