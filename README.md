# Wizzie AI Assistant for ESP32

Wizzie is an ESP32-based AI assistant project. The current firmware is an early prototype: it plays random pre-recorded sounds from an SD card when the user interacts with the device. The next development step is to add microphone input, connect the device to a Cloud AI pipeline, and turn it into a fully functional voice assistant.

## Current Status

Implemented:

- ESP32 firmware built with PlatformIO and Arduino framework.
- LVGL/TFT display initialization.
- SD card audio playlist loading from JSON.
- Random sound selection from playlist sections.
- Audio playback through the ESP32 internal DAC path.
- Serial terminal debug commands.
- Wi-Fi bootstrap for future cloud features.
- Local ElevenLabs/TTS bridge experiments are present in the codebase, but the main current use case is SD-card playback.

Current user-facing behavior:

- The device starts, initializes Wi-Fi/display/audio, loads available SD-card sounds, and can play a random sound.
- Serial commands can be used for debugging playback and network audio paths.

Planned next steps:

- Add microphone recording.
- Send captured speech to a Cloud AI/STT pipeline.
- Generate an assistant reply using an LLM.
- Convert the reply to speech with TTS.
- Play the generated response on the ESP32.

## Hardware

Target hardware:

- ESP32 board.
- 320x240 display supported by TFT_eSPI/LVGL.
- SD card connected over SPI.
- Speaker/audio output using ESP32 internal DAC on GPIO26.
- Planned microphone input on GPIO25.
- Planned push-to-talk/debug button on GPIO32.

Current pin assignments are defined in `src/config.h`:

- `PIN_SPEAKER`: GPIO26
- `PIN_MIC_ADC`: GPIO25
- `PIN_RECORD_BUTTON`: GPIO32
- `PIN_SD_MOSI`: GPIO23
- `PIN_SD_MISO`: GPIO19
- `PIN_SD_SCK`: GPIO18
- `PIN_SD_CS`: GPIO5
- `PIN_BACKLIGHT`: GPIO27

## SD Card Layout

The firmware expects a playlist JSON file and audio files on the SD card.

Expected playlist path:

```text
/audio/list.json
```

Expected audio base directory:

```text
/audio/
```

Example structure:

```text
/audio/
  list.json
  obr_01.mp3
  wake_01.mp3
  fun_01.mp3
  system_01.mp3
```

The playlist is organized into sections such as:

- `obrashenija`
- `wake_up`
- `school_reminder`
- `fun`
- `threat`
- `adventure`
- `sleep`
- `evening`
- `system`

Example `list.json`:

```json
{
  "obrashenija": [
    { "file": "obr_01.mp3" }
  ],
  "wake_up": [
    { "file": "wake_01.mp3" }
  ],
  "school_reminder": [],
  "fun": [
    { "file": "fun_01.mp3" }
  ],
  "threat": [],
  "adventure": [],
  "sleep": [],
  "evening": [],
  "system": [
    { "file": "system_01.mp3" }
  ]
}
```

## Serial Debug Commands

Serial terminal commands are kept intentionally because they are useful during development and will be extended over time.

Current commands:

- `0`: Play random `obrashenija` sound, then a random mode sound.
- `1`: Play NRJ radio stream test.
- `2`: Play NDR/Icecast stream test.
- `3`: Play ElevenLabs/TTS stream test path.
- `s`: Stop audio and free decoder memory.

Serial speed:

```text
115200
```

## Software Architecture

The project has been split into small modules under `src/`.

```text
src/
  main.cpp                    Application entry point and UI setup
  config.h                    Pins, constants, URLs, credentials fallback
  core/
    audio_engine.*            Audio object, start/stop preparation, DMA cleanup
    callbacks.*               ESP32-audioI2S callback handlers
  commands/
    serial_commands.*         Serial debug command dispatcher
  network/
    wifi_manager.*            Wi-Fi connection logic
    tts_bridge.*              Local bridge for TTS streaming experiments
  playlist/
    playlist.*                Playlist loading and random selection
    playback.*                Playback requests and chained sound logic
  storage/
    sd_manager.*              SD card initialization and path helpers
  utils/
    http_helpers.*            HTTP line reading, URL encoding, JSON escaping
```

Important audio behavior:

- `audio_engine_stop()` stops playback, frees decoder memory, and clears DMA buffers.
- `audio_engine_prepare_start()` mutes, clears DMA, waits briefly, and prepares clean playback start.
- The local `ESP32-audioI2S` library has been modified to improve internal DAC startup/stop behavior and expose decoder memory cleanup.

## Wi-Fi Credentials

The firmware supports credentials from `include/secrets.h` if present.

Example:

```cpp
#pragma once
#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-password"
#define ELEVENLABS_API_KEY "optional-key"
```

If `include/secrets.h` is missing or empty, the firmware attempts to load Wi-Fi credentials from NVS:

- Namespace: `wizzy`
- Key: `wifi_ssid`
- Key: `wifi_pass`

Do not commit real secrets.

## Build

Build with PlatformIO:

```bash
pio run
```

Upload:

```bash
pio run --target upload
```

Monitor serial output:

```bash
pio device monitor -b 115200
```

## Roadmap

Near-term roadmap:

- Add microphone sampling from GPIO25.
- Add push-to-talk handling on GPIO32.
- Package recorded audio and send it to a cloud endpoint.
- Add cloud STT, LLM, and TTS integration.
- Stream or download TTS audio back to the device.
- Add assistant state handling: idle, listening, thinking, speaking, error.

Longer-term goals:

- Child-friendly assistant persona.
- Bulgarian language support.
- Safe response filtering in the cloud workflow.
- Offline fallback sounds from SD card.
- Better UI indicators for Wi-Fi, listening, speaking, and errors.
- Configurable playlists and assistant behavior.

## Notes

This repository is under active development. Current code is primarily focused on reliable SD-card audio playback and debugging infrastructure before adding the full microphone-to-cloud AI assistant loop.
