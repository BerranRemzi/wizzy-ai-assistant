#include "sd_manager.h"

static bool g_sd_ready = false;
static SPIClass g_sd_spi(VSPI);

static void list_dir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            if (levels) list_dir(fs, file.name(), levels - 1);
        }
        else
        {
            Serial.print("FILE: ");
            Serial.print(file.name());
            Serial.print(" SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

bool sd_manager_init()
{
    g_sd_spi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS, g_sd_spi))
    {
        Serial.println("Card Mount Failed");
        return false;
    }
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE)
    {
        Serial.println("No TF card attached");
        return false;
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("TF Card Size: %lluMB\n", cardSize);
    list_dir(SD, "/", 2);
    g_sd_ready = true;
    return true;
}

bool sd_manager_is_ready() { return g_sd_ready; }

bool sd_manager_ensure_ready()
{
    if (!g_sd_ready) return sd_manager_init();
    return true;
}

bool sd_manager_file_exists(const char *path) { return SD.exists(path); }

const char* sd_manager_build_path(const char *fname, char *out, size_t out_size)
{
    if (fname[0] == '/')
        strncpy(out, fname, out_size - 1);
    else
        snprintf(out, out_size, "%s%s", PLAYLIST_AUDIO_BASE, fname);
    out[out_size - 1] = '\0';
    return out;
}
