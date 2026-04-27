#include "tts_bridge.h"
#include "config.h"
#include "core/audio_engine.h"
#include "utils/http_helpers.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// From playback module
extern void playback_set_tts_active(bool active);
extern bool playback_is_tts_active();

static WiFiServer s_tts_bridge_server(TTS_BRIDGE_PORT);
static volatile bool s_tts_bridge_finished = false;
static volatile uint32_t s_tts_bridge_finished_at_ms = 0;

static String read_http_line(WiFiClient &client, uint32_t timeout_ms)
{
    return http_helpers_read_http_line(client, timeout_ms);
}

static bool read_exact(WiFiClient &client, uint8_t *dst, size_t len, uint32_t timeout_ms)
{
    return http_helpers_read_exact_with_timeout(client, dst, len, timeout_ms);
}

static void handle_client(WiFiClient &downstream)
{
    String request_line = read_http_line(downstream, 4000);
    if (!request_line.startsWith("GET "))
    {
        downstream.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
        downstream.stop();
        return;
    }

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

    String voice_id = http_helpers_url_encode(ELEVENLABS_VOICE_ID);
    String payload = "{\"text\":\"" + http_helpers_json_escape(ELEVENLABS_TEST_TEXT) + "\",\"model_id\":\"" + http_helpers_json_escape(ELEVENLABS_MODEL_ID) + "\"}";
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
            if (chunk_line.length() == 0) break;

            int semicolon = chunk_line.indexOf(';');
            if (semicolon >= 0) chunk_line = chunk_line.substring(0, semicolon);
            chunk_line.trim();
            size_t chunk_size = strtoul(chunk_line.c_str(), nullptr, 16);
            if (chunk_size == 0)
            {
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
                if (!read_exact(upstream, relay_buf, part, 30000)) { remaining = 0; break; }
                downstream.write(relay_buf, part);
                remaining -= part;
            }

            uint8_t crlf[2];
            if (!read_exact(upstream, crlf, 2, 30000)) break;
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
                if (n > 0) { downstream.write(relay_buf, (size_t)n); idle_started = millis(); }
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

    s_tts_bridge_finished = true;
    s_tts_bridge_finished_at_ms = millis();
}

static void bridge_task(void* parameter)
{
    for (;;)
    {
        WiFiClient client = s_tts_bridge_server.accept();
        if (client) handle_client(client);
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
}

void tts_bridge_server_begin()
{
    s_tts_bridge_server.begin();
    xTaskCreate(bridge_task, "TTS_Bridge", 8192, NULL, 1, NULL);
}

void tts_bridge_check_finished()
{
    if (playback_is_tts_active() && s_tts_bridge_finished)
    {
        uint32_t now = millis();
        uint32_t elapsed = now - s_tts_bridge_finished_at_ms;
        uint32_t buffered = audio.inBufferFilled();

        if ((elapsed >= TTS_DRAIN_MIN_MS && buffered <= TTS_DRAIN_BUFFER_BYTES) ||
            (elapsed >= TTS_DRAIN_FORCE_MS))
        {
            audio_engine_stop_soft();
            playback_set_tts_active(false);
            s_tts_bridge_finished = false;
            s_tts_bridge_finished_at_ms = 0;
        }
    }
}

void tts_bridge_mark_finished()
{
    playback_set_tts_active(false);
    s_tts_bridge_finished = false;
    s_tts_bridge_finished_at_ms = 0;
}

bool tts_bridge_has_finished() { return s_tts_bridge_finished; }
uint32_t tts_bridge_finished_at_ms() { return s_tts_bridge_finished_at_ms; }
