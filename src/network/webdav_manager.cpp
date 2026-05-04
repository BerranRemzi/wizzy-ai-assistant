#include "webdav_manager.h"

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTP_Method.h>

#include "config.h"
#include "storage/sd_manager.h"
#include "wifi_manager.h"

namespace {

static WebServer s_server(WEB_DAV_PORT);
static bool s_started = false;

static bool is_auth_ok()
{
    if (strlen(WEB_DAV_USERNAME) == 0) return true;
    if (s_server.authenticate(WEB_DAV_USERNAME, WEB_DAV_PASSWORD)) return true;
    s_server.requestAuthentication();
    return false;
}

static bool is_dav_uri(const String &uri)
{
    if (uri == "*") return true;
    if (uri == "/") return true;
    if (String(WEB_DAV_BASE_PATH) == "/") return true;
    if (uri == WEB_DAV_BASE_PATH) return true;
    String base_slash = String(WEB_DAV_BASE_PATH) + "/";
    return uri.startsWith(base_slash);
}

static String normalize_dav_path(const String &uri)
{
    if (uri == "*" || uri == "/") return "/";

    String path = uri;
    const String base = WEB_DAV_BASE_PATH;
    if (base.length() > 0 && base != "/" && path.startsWith(base))
    {
        path.remove(0, base.length());
    }
    if (path.length() == 0) path = "/";
    path = WebServer::urlDecode(path);
    if (!path.startsWith("/")) path = "/" + path;
    while (path.indexOf("//") >= 0) path.replace("//", "/");
    return path;
}

static String escape_xml(const String &input)
{
    String out = input;
    out.replace("&", "&amp;");
    out.replace("<", "&lt;");
    out.replace(">", "&gt;");
    out.replace("\"", "&quot;");
    out.replace("'", "&apos;");
    return out;
}

static void ensure_parent_dirs(const String &path)
{
    int idx = 1;
    while ((idx = path.indexOf('/', idx)) > 0)
    {
        const String dir = path.substring(0, idx);
        if (!SD.exists(dir)) SD.mkdir(dir);
        idx++;
    }
}

static bool delete_recursive(const String &path)
{
    if (path == "/") return false;
    File node = SD.open(path, FILE_READ);
    if (!node) return false;

    if (!node.isDirectory())
    {
        node.close();
        return SD.remove(path);
    }

    File child = node.openNextFile();
    while (child)
    {
        const String child_path = child.name();
        const bool child_is_dir = child.isDirectory();
        child.close();

        if (child_is_dir)
        {
            if (!delete_recursive(child_path))
            {
                node.close();
                return false;
            }
        }
        else
        {
            if (!SD.remove(child_path))
            {
                node.close();
                return false;
            }
        }

        child = node.openNextFile();
    }

    node.close();
    return SD.rmdir(path);
}

static void send_dav_common_headers()
{
    s_server.sendHeader("DAV", "1");
    s_server.sendHeader("Allow", "OPTIONS, PROPFIND, GET, HEAD, PUT, DELETE, MKCOL");
}

static void handle_options()
{
    send_dav_common_headers();
    s_server.send(200, "text/plain", "");
}

static void append_prop_entry(String &xml, const String &dav_href, bool is_dir, size_t file_size)
{
    xml += "<d:response>";
    xml += "<d:href>" + escape_xml(dav_href) + "</d:href>";
    xml += "<d:propstat><d:prop>";
    xml += "<d:resourcetype>";
    if (is_dir) xml += "<d:collection/>";
    xml += "</d:resourcetype>";
    if (!is_dir)
    {
        xml += "<d:getcontentlength>" + String((unsigned long)file_size) + "</d:getcontentlength>";
    }
    xml += "</d:prop>";
    xml += "<d:status>HTTP/1.1 200 OK</d:status></d:propstat>";
    xml += "</d:response>";
}

static String fs_path_to_dav_href(const String &fs_path, bool is_dir)
{
    String href = fs_path;
    if (!href.startsWith("/")) href = "/" + href;
    while (href.indexOf("//") >= 0) href.replace("//", "/");
    if (is_dir && !href.endsWith("/")) href += "/";
    return href;
}

static void handle_propfind(const String &uri, const String &path)
{
    File node = SD.open(path, FILE_READ);
    if (!node)
    {
        s_server.send(404, "text/plain", "Not found");
        return;
    }

    const String depth = s_server.hasHeader("Depth") ? s_server.header("Depth") : "infinity";
    const bool include_children = depth != "0";
    const bool is_dir = node.isDirectory();

    String xml;
    xml.reserve(2048);
    xml += "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    xml += "<d:multistatus xmlns:d=\"DAV:\">";

    const String self_href = fs_path_to_dav_href(path, is_dir);
    append_prop_entry(xml, self_href, is_dir, node.size());

    if (is_dir && include_children)
    {
        File child = node.openNextFile();
        while (child)
        {
            const String href = fs_path_to_dav_href(String(child.name()), child.isDirectory());
            append_prop_entry(xml, href, child.isDirectory(), child.size());
            child = node.openNextFile();
        }
    }

    node.close();

    xml += "</d:multistatus>";
    send_dav_common_headers();
    s_server.send(207, "text/xml", xml);
}

static String guess_content_type(const String &path)
{
    if (path.endsWith(".mp3")) return "audio/mpeg";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".txt")) return "text/plain";
    return "application/octet-stream";
}

static void handle_get(const String &path)
{
    File f = SD.open(path, FILE_READ);
    if (!f)
    {
        s_server.send(404, "text/plain", "Not found");
        return;
    }

    if (f.isDirectory())
    {
        String listing;
        listing.reserve(512);
        listing += "Directory listing:\n";
        File child = f.openNextFile();
        while (child)
        {
            listing += child.name();
            if (child.isDirectory()) listing += "/";
            listing += "\n";
            child = f.openNextFile();
        }
        f.close();
        s_server.send(200, "text/plain", listing);
        return;
    }

    const String type = guess_content_type(path);
    s_server.streamFile(f, type);
    f.close();
}

static void handle_put(const String &path)
{
    if (path == "/")
    {
        s_server.send(403, "text/plain", "Invalid target");
        return;
    }

    const int content_len = s_server.clientContentLength();
    if (content_len < 0)
    {
        s_server.send(411, "text/plain", "Content-Length required");
        return;
    }

    if (SD.exists(path))
    {
        File existing = SD.open(path, FILE_READ);
        const bool is_dir = existing && existing.isDirectory();
        if (existing) existing.close();
        if (is_dir)
        {
            s_server.send(409, "text/plain", "Target is directory");
            return;
        }
        SD.remove(path);
    }

    ensure_parent_dirs(path);
    File out = SD.open(path, FILE_WRITE);
    if (!out)
    {
        s_server.send(500, "text/plain", "Cannot create file");
        return;
    }

    WiFiClient client = s_server.client();
    uint8_t buf[1024];
    int total = 0;
    uint32_t last_rx = millis();
    while (total < content_len)
    {
        const int avail = client.available();
        if (avail <= 0)
        {
            if (millis() - last_rx > 10000)
            {
                out.close();
                SD.remove(path);
                s_server.send(408, "text/plain", "Upload timeout");
                return;
            }
            delay(1);
            continue;
        }

        const size_t to_read = (size_t)min((int)sizeof(buf), min(avail, content_len - total));
        const int n = client.read(buf, to_read);
        if (n <= 0) continue;
        if ((int)out.write(buf, n) != n)
        {
            out.close();
            SD.remove(path);
            s_server.send(507, "text/plain", "Write failed");
            return;
        }
        total += n;
        last_rx = millis();
    }

    out.close();
    s_server.send(201, "text/plain", "Created");
}

static void handle_mkcol(const String &path)
{
    if (path == "/")
    {
        s_server.send(405, "text/plain", "Method not allowed");
        return;
    }

    if (SD.exists(path))
    {
        s_server.send(405, "text/plain", "Already exists");
        return;
    }

    ensure_parent_dirs(path);
    if (!SD.mkdir(path))
    {
        s_server.send(409, "text/plain", "Cannot create directory");
        return;
    }
    s_server.send(201, "text/plain", "Created");
}

static void handle_delete(const String &path)
{
    if (!SD.exists(path))
    {
        s_server.send(404, "text/plain", "Not found");
        return;
    }

    if (!delete_recursive(path))
    {
        s_server.send(500, "text/plain", "Delete failed");
        return;
    }
    s_server.send(204, "text/plain", "");
}

static void handle_dav_request()
{
    if (!is_auth_ok()) return;

    if (!sd_manager_ensure_ready())
    {
        s_server.send(503, "text/plain", "SD unavailable");
        return;
    }

    const String uri = s_server.uri();
    if (!is_dav_uri(uri))
    {
        // Some clients probe unexpected paths before deciding DAV root.
        // Return DAV capabilities on OPTIONS instead of hard failing.
        if (s_server.method() == HTTP_OPTIONS)
        {
            handle_options();
            return;
        }
        s_server.send(404, "text/plain", "Not found");
        return;
    }

    const String path = normalize_dav_path(uri);
    if (path.indexOf("..") >= 0)
    {
        s_server.send(400, "text/plain", "Invalid path");
        return;
    }

    switch (s_server.method())
    {
        case HTTP_OPTIONS:
            handle_options();
            return;
        case HTTP_PROPFIND:
            handle_propfind(uri, path);
            return;
        case HTTP_GET:
        case HTTP_HEAD:
            handle_get(path);
            return;
        case HTTP_PUT:
            handle_put(path);
            return;
        case HTTP_MKCOL:
            handle_mkcol(path);
            return;
        case HTTP_DELETE:
            handle_delete(path);
            return;
        default:
            send_dav_common_headers();
            s_server.send(405, "text/plain", "Method not allowed");
            return;
    }
}

} // namespace

void webdav_manager_init()
{
    s_started = false;
}

void webdav_manager_task()
{
#if WEB_DAV_ENABLED
    if (!s_started)
    {
        if (!wifi_manager_is_connected()) return;
        if (!sd_manager_ensure_ready()) return;

        const char *headers[] = {"Depth"};
        s_server.collectHeaders(headers, 1);
        s_server.on("/", HTTP_ANY, handle_dav_request);
        s_server.on(WEB_DAV_BASE_PATH, HTTP_ANY, handle_dav_request);
        s_server.onNotFound(handle_dav_request);
        s_server.begin();

        s_started = true;
        Serial.printf("WebDAV ready: http://%s:%u%s\n",
                      WiFi.localIP().toString().c_str(),
                      (unsigned)WEB_DAV_PORT,
                      WEB_DAV_BASE_PATH);
        if (String(WEB_DAV_BASE_PATH) != "/")
        {
            Serial.printf("WebDAV root alias: http://%s:%u/\n",
                          WiFi.localIP().toString().c_str(),
                          (unsigned)WEB_DAV_PORT);
        }
    }

    s_server.handleClient();
#endif
}

bool webdav_manager_is_running()
{
    return s_started;
}
