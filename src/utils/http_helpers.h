#pragma once
#include <WString.h>
#include <WiFiClient.h>

String http_helpers_json_escape(const char *text);
String http_helpers_url_encode(const char *text);
String http_helpers_read_http_line(WiFiClient &client, uint32_t timeout_ms);
bool http_helpers_read_exact_with_timeout(WiFiClient &client, uint8_t *dst, size_t len, uint32_t timeout_ms);
