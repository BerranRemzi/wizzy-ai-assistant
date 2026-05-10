# Wizzie AI Assistant for ESP32

Wizzie is an ESP32-based assistant prototype built with PlatformIO and Arduino. The current firmware is centered on reliable audio playback from SD card, simple UI feedback, Wi-Fi services, and experiments around streamed TTS. It is not yet a full voice assistant, but the codebase already includes the building blocks for playlist-driven playback, OTA, WebDAV file access, and ElevenLabs speech generation workflows.

## Current Firmware Behavior

Implemented today:

- ESP32 firmware using PlatformIO and the Arduino framework.
- TFT display initialization with a lightweight UI update loop.
- SD card playlist loading from `/audio/list.json`.
- Startup playback from the `system` category.
- Configurable play-button sequence playback from SD card.
- Hourly clock chimes based on NTP time when Wi-Fi is connected.
- HTTP audio stream tests and ElevenLabs TTS stream tests over Wi-Fi.
- OTA update support.
- WebDAV access to the SD card over Wi-Fi.
- Serial debug commands for playback and diagnostics.

Current user-facing behavior:

- On boot, the device initializes serial, audio, Wi-Fi services, display, SD-backed playlist handling, and starts a startup sound from the `system` category when available.
- Pressing the play button triggers a configured multi-step audio sequence from the playlist.
- When Wi-Fi is connected and time is synced, the device can automatically play hourly clock announcements.
- Serial commands can trigger SD playback, radio streams, clock tests, and ElevenLabs TTS streaming.

Planned next steps:

- Add microphone capture on the ESP32.
- Add push-to-talk or trigger handling for assistant interactions.
- Send captured audio to a cloud STT pipeline.
- Generate replies with an LLM.
- Convert replies to speech and play them back on the device.

## Hardware

Target hardware:

- ESP32 board.
- 320x240 display driven by TFT_eSPI.
- SD card connected over SPI.
- Speaker output through the ESP32 internal DAC.
- Planned microphone input on an ADC pin.
- A physical play button for local playback actions.

Current pin assignments from `src/config.h`:

- `PIN_MIC_ADC`: GPIO25
- `PIN_PLAY_BUTTON`: GPIO21
- `PIN_SPEAKER`: GPIO26
- `PIN_SD_MOSI`: GPIO23
- `PIN_SD_MISO`: GPIO19
- `PIN_SD_SCK`: GPIO18
- `PIN_SD_CS`: GPIO5
- `PIN_BACKLIGHT`: GPIO27

## SD Card Layout

The firmware expects playlist metadata and audio files on the SD card.

Required paths:

```text
/audio/list.json
/audio/*.mp3
```

Example structure:

```text
/audio/
  list.json
  obr_01.mp3
  fun_01.mp3
  sys_01.mp3
  clk_08_01.mp3
```

### Playlist Format

The firmware accepts playlist categories either directly at the top level or inside a top-level `categories` object. The current project data uses the `categories` layout because it also leaves room for `clock` and `button` configuration.

Recognized category names:

- `obrashenija`
- `wake_up`
- `school_reminder`
- `fun`
- `threat`
- `adventure`
- `sleep`
- `evening`
- `system`

Each playable entry must have a `file` field. A `text` field is optional for firmware playback, but recommended if you also use the ElevenLabs batch generator described later.

Example `list.json`:

```json
{
  "categories": {
    "obrashenija": [
      { "text": "Brave hero", "file": "obr_01.mp3" }
    ],
    "wake_up": [
      { "text": "Time to wake up", "file": "wu_01.mp3" }
    ],
    "school_reminder": [],
    "fun": [
      { "text": "Mission fun is starting", "file": "fun_01.mp3" }
    ],
    "threat": [],
    "adventure": [],
    "sleep": [],
    "evening": [],
    "system": [
      { "text": "System ready", "file": "sys_01.mp3" }
    ]
  },
  "clock": {
    "08": {
      "time": [
        { "text": "It is eight o'clock", "file": "clk_08_01.mp3" }
      ],
      "compose": ["school_reminder", "wake_up"]
    }
  },
  "button": {
    "sequence": [
      { "compose": ["obrashenija"] },
      { "compose": ["fun", "threat", "adventure"] }
    ]
  }
}
```

Notes:

- `button.sequence` defines the order of category groups played when the play button is pressed.
- `clock.<hour>.time` contains direct hour announcement files.
- `clock.<hour>.compose` contains category names from which a follow-up sound can be selected.
- Unknown extra sections are ignored by the firmware parser.

## ElevenLabs Helper

The repository includes a helper script at `tools/elevenlabs-helper/elevenlabs.py` for generating MP3 files with ElevenLabs. It supports two modes:

- Single-text mode: synthesize one line of text into one MP3 file.
- Batch mode: read a JSON file, look for objects that contain both `text` and `file`, and generate any missing MP3 files beside that JSON file.

### Requirements

- Python 3.
- The `requests` package.
- An ElevenLabs API key provided either through `--api-key` or the `ELEVENLABS_API_KEY` environment variable.

Install the dependency:

```bash
pip install requests
```

### PowerShell Setup

From the helper directory:

```powershell
cd .\tools\elevenlabs-helper
$env:ELEVENLABS_API_KEY="your_key"
```

### Single-Text Example

Generate one MP3 from a text string:

```powershell
python .\elevenlabs.py "System ready" -o .\audio\sys_01.mp3
```

Or pass the key explicitly:

```powershell
python .\elevenlabs.py --api-key your_key "System ready" -o .\audio\sys_01.mp3
```

### Batch JSON Example

Generate all missing files referenced by a JSON file:

```powershell
python .\elevenlabs.py .\audio\list.json
```

Batch mode behavior:

- The script recursively scans the JSON for objects containing both `text` and `file`.
- Existing MP3 files are skipped.
- New MP3 files are written next to the JSON file.
- File names must be plain filenames ending in `.mp3`, not nested paths.

This makes it practical to keep one source-of-truth playlist JSON for both the firmware and audio generation.

## Wi-Fi, TTS, OTA, and WebDAV

Wi-Fi credentials can come from `include/secrets.h` or from NVS if that file is absent.

Example `include/secrets.h`:

```cpp
#pragma once
#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-password"
#define ELEVENLABS_API_KEY "optional-key"
```

If credentials are not compiled in, the firmware falls back to NVS keys:

- Namespace: `wizzy`
- Key: `wifi_ssid`
- Key: `wifi_pass`

Other network features currently in the firmware:

- Local TTS bridge on port `8081` used by the serial ElevenLabs test path.
- Arduino OTA enabled by default with hostname `wizzy-assistant`.
- WebDAV enabled by default on port `80` with base path `/dav`.

Do not commit real secrets.

## Serial Debug Commands

Serial speed:

```text
115200
```

Current commands:

- `0`: Play the configured play-button sequence from the playlist.
- `1`: Play the NRJ radio stream test.
- `2`: Play the NDR/Icecast stream test.
- `3`: Play the ElevenLabs TTS stream test through the local bridge.
- `h`: Test clock playback for the current hour, or the next hour that has available clock entries.
- `s`: Stop audio playback.
- `?`: Print command help.

## Build and Upload

Primary PlatformIO environment:

```text
ai-assistant
```

Build:

```bash
pio run -e ai-assistant
```

Upload over serial:

```bash
pio run -e ai-assistant --target upload
```

Monitor serial output:

```bash
pio device monitor -b 115200
```

Upload over OTA after the device is on Wi-Fi:

```bash
pio run -e ai-assistant-ota --target upload
```

## Software Architecture

The project is organized into focused modules under `src/`.

```text
src/
  main.cpp                    Application entry point, loop scheduling, button handling
  config.h                    Pins, constants, URLs, feature flags, credentials fallback
  commands/
    serial_commands.*         Serial command dispatcher and help output
  core/
    audio_engine.*            Playback preparation, stop logic, DMA cleanup
    callbacks.*               Audio library callback hooks
  network/
    wifi_manager.*            Wi-Fi connection logic
    ota_manager.*             Arduino OTA startup and handling
    tts_bridge.*              Local bridge that relays ElevenLabs audio to the player
    webdav_manager.*          SD card browsing and file transfer over WebDAV
  playlist/
    playlist.*                Playlist parsing, random selection, button and clock metadata
    playback.*                SD, HTTP, chained playback, and TTS stream requests
  storage/
    sd_manager.*              SD card initialization and path helpers
  ui/
    ui_component.*            Display updates and simple runtime UI
  utils/
    http_helpers.*            HTTP line reading, exact reads, escaping helpers
```

Important audio behavior:

- Audio is started early in `setup()` so decoder buffers reserve memory before UI allocations.
- UI refreshes are temporarily paused while audio is running if free heap drops below the configured threshold.
- Chained SD playback is used for back-to-back sounds to reduce audible gaps.
- The project uses a custom `ESP32-audioI2S` fork for internal DAC behavior and decoder cleanup.

## Roadmap

Near term:

- Add microphone capture and recording flow.
- Add assistant state handling such as idle, listening, thinking, speaking, and error.
- Connect recorded audio to STT, LLM, and TTS cloud services.
- Keep offline SD-card playback as a fallback path.

Longer term:

- Improve child-friendly assistant behavior.
- Expand Bulgarian voice content and assistant prompts.
- Add better on-screen indicators for Wi-Fi, playback, and errors.
- Make playlists and behavior more configurable.

## Notes

This repository is still in active development. The current codebase is strongest in SD-card playback, playlist-driven behavior, and tooling around speech/audio content generation rather than end-to-end assistant conversation yet.
