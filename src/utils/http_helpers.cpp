#include "http_helpers.h"

String http_helpers_json_escape(const char *text)
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

String http_helpers_url_encode(const char *text)
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
        if (safe) out += (char)c;
        else
        {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

String http_helpers_read_http_line(WiFiClient &client, uint32_t timeout_ms)
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

bool http_helpers_read_exact_with_timeout(WiFiClient &client, uint8_t *dst, size_t len, uint32_t timeout_ms)
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
            if (n > 0) { offset += (size_t)n; last_data = millis(); }
        }
        else
        {
            if ((millis() - last_data) > timeout_ms) return false;
            delay(1);
        }
    }
    return true;
}
