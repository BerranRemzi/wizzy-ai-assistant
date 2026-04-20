# WZ2432R024 Wizzy AI Assistant (ESP32 + LVGL)

Kid-friendly voice assistant project for ESP32 touchscreen hardware.
Current firmware focuses on display/UI and Wi-Fi bootstrap. Audio/STT/LLM/TTS pipeline is planned.

## Project Goals

- Build a child-friendly assistant with a playful voice style.
- Support Bulgarian speech pipeline (STT + TTS).
- Use ESP32 for UI, button, mic input, and audio output.
- Offload heavy AI processing to Raspberry Pi (n8n workflows), while keeping firmware responsive.

## Current Status

Implemented:

- LVGL + TFT_eSPI UI stack and touch handling.
- Boot UI flow and widget demo startup.
- Wi-Fi credential source chain:
  - Compile-time credentials from include/secrets.h (if present).
  - Runtime fallback from NVS keys: wifi_ssid, wifi_pass.
- GPIO setup aligned for voice hardware planning:
  - GPIO25 reserved for MAX4466 analog mic input.
  - GPIO32 configured as input pull-up for push-to-talk button.
- Project partition layout set to huge_app.csv to fit LVGL + Wi-Fi firmware.

Not implemented yet:

- Audio capture and voice activity flow.
- n8n webhook client requests.
- TTS audio playback (I2S amp path).
- Hourly assistant messages.

## Repository Layout

- platformio.ini: PlatformIO environment, dependencies, and build settings.
- src/main.cpp: Main firmware app, UI init, touch handling, Wi-Fi bootstrap.
- src/UI/: Generated LVGL UI sources and assets.
- include/: Headers and local credential template.
- include/secrets.h.example: Template for local Wi-Fi credentials.

## Hardware Assumptions

Target board and peripherals:

- ESP32 Denky32 board.
- 320x240 touch display (TFT_eSPI + LVGL).
- MAX4466 microphone module:
  - OUT -> GPIO25 (ADC input)
  - VCC -> 3.3V
  - GND -> GND
- Record button:
  - GPIO32 -> button -> GND (active-low)
  - Internal pull-up enabled in firmware.
- Planned speaker output:
  - I2S DAC/amp module (example: MAX98357A).

## Software Architecture (Current and Planned)

Current runtime:

- setup() initializes serial, pins, Wi-Fi bootstrap, LVGL display/touch drivers, UI screens.
- loop() runs lv_timer_handler() in a short cycle.

Planned architecture:

- Non-blocking voice pipeline with FreeRTOS tasks:
  - Input task: button handling and audio capture.
  - Network task: n8n webhook calls for STT + LLM + TTS orchestration.
  - Output task: stream/decode/play TTS audio.
- State machine for UI feedback:
  - IDLE -> LISTENING -> THINKING -> SPEAKING -> IDLE

## Wi-Fi Credential Setup

Preferred local development flow:

1. Copy include/secrets.h.example to include/secrets.h.
2. Set WIFI_SSID and WIFI_PASSWORD.
3. Build and upload.

Notes:

- include/secrets.h is git-ignored for security.
- If include/secrets.h is missing or empty, firmware attempts NVS fallback:
  - Namespace: wizzy
  - Keys: wifi_ssid, wifi_pass

## Build and Upload

Using PlatformIO:

- Build: platformio run
- Upload: platformio run --target upload

From VS Code tasks:

- Build task and Upload task are already configured and used in this workspace.

## Known Constraints

- Default ESP32 app partition was too small after adding Wi-Fi; huge_app.csv is required.
- Current code still has some generated UI warnings not blocking compilation.
- Voice and networking features must avoid blocking UI loop to keep touch/display responsive.

## Integration Plan (High Level)

1. Add push-to-talk logic on GPIO32 (hold to record, release to send).
2. Implement ADC audio capture path for MAX4466 on GPIO25.
3. Add HTTP client to post audio to Raspberry Pi n8n webhook.
4. Add response handling for text + TTS audio stream URL.
5. Implement I2S playback for TTS output.
6. Add hourly message scheduler (08:00-20:00) with suppression while recording/speaking.
7. Add resilient retries and friendly fallback messages.

## TODO

- [x] Add Wi-Fi credential fallback chain (secrets.h -> NVS).
- [x] Reserve GPIO25 for mic input and GPIO32 for record button.
- [x] Increase app partition size (huge_app.csv).
- [x] Add secrets template and ignore real secrets file.

Firmware core:

- [ ] Create voice_assistant state machine module.
- [ ] Add non-blocking button debouncing and edge detection on GPIO32.
- [ ] Add FreeRTOS queue contracts for voice pipeline events.
- [ ] Refactor long setup-time flow to avoid blocking behavior where possible.

Audio input:

- [ ] Implement ADC sampling from MAX4466 (GPIO25).
- [ ] Add input gain calibration and noise floor detection.
- [ ] Encode or package audio payload expected by backend webhook.

Networking and backend integration:

- [ ] Add HTTP client wrapper for webhook calls with timeouts/retries.
- [ ] Define request/response JSON contract with n8n workflow.
- [ ] Add connectivity status and recovery logic (Wi-Fi reconnect strategy).
- [ ] Implement pull API for pending playback jobs from n8n (ESP32 polling model).
- [ ] Define API endpoints:
  - [ ] `GET /api/v1/device/{deviceId}/next-audio` (returns next job or 204 when empty).
  - [ ] `POST /api/v1/device/{deviceId}/ack` (states: started, finished, failed).
  - [ ] `POST /api/v1/device/{deviceId}/heartbeat` (device online, firmware version, RSSI).
- [ ] Define playback job schema:
  - [ ] `jobId`, `priority`, `createdAt`, `expiresAt`, `interruptPolicy`, `repeat`.
  - [ ] `audioUrl` or `pcmBase64`, `sampleRate`, `durationMs`, `textPreview`.
  - [ ] `context` (`hourly_chime`, `assistant_reply`, `alert`) for UI and behavior rules.
- [ ] Add idempotency and duplicate protection:
  - [ ] Store `lastPlayedJobId` in RAM/NVS.
  - [ ] Ignore already acknowledged jobs after reconnect.
- [ ] Add backoff policy for polling:
  - [ ] Fast poll when idle queue may be non-empty (for example 1-2s).
  - [ ] Slow poll when empty or offline (for example 10-30s with jitter).
- [ ] Add secure API access:
  - [ ] Device token in `Authorization: Bearer <token>`.
  - [ ] Token rotation plan via n8n admin flow.
  - [ ] Optional request signature or nonce to reduce replay risk.
- [ ] Add failure handling policy:
  - [ ] On `audioUrl` download failure, send `ack=failed` with reason code.
  - [ ] Retry failed job up to N times before dropping.
  - [ ] Fallback local phrase if job fetch repeatedly fails.
- [ ] Add prefetch option for smoother playback:
  - [ ] Download next job while current job is playing when memory allows.
  - [ ] Enforce max cache size and eviction rules.

Audio output:

- [ ] Integrate I2S amp output for TTS playback.
- [ ] Add playback queue and interruption rules.
- [ ] Add volume normalization and clipping protection.

Scheduler and UX:

- [ ] Add NTP time sync and timezone handling.
- [ ] Implement hourly announcements (08:00-20:00).
- [ ] Add UI indicators: Wi-Fi, listening, processing, speaking, error.
- [ ] Add child-safe fallback phrases for backend/API failure.

Safety and quality:

- [ ] Add guardrails for kid-safe prompt and response filtering in n8n.
- [ ] Add transcript-only logging policy on Raspberry Pi with retention controls.
- [ ] Measure latency budget and optimize audio chunk sizes.
- [ ] Validate Bulgarian STT/TTS quality with real-world child voice tests.

## Recommended n8n Workflow (Reference)

Suggested flow:

- Webhook receives audio payload from ESP32.
- STT node (Bulgarian speech-to-text).
- Prompt composition node (kid-safe persona and context).
- LLM node generates short, friendly response.
- TTS node synthesizes response audio (Bulgarian voice).
- Response node returns metadata for ESP32 playback.

## Security Notes

- Never commit include/secrets.h.
- Prefer environment variables or secure credential store on Raspberry Pi for API keys.
- Keep cloud tokens and backend URLs out of firmware source when possible.

## License

Add your preferred license for distribution and contributions.
