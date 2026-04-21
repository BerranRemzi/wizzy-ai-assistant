#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <Audio.h>

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
#define ELEVENLABS_TEST_TEXT "Аз съм Маги. Ставайте мъничета!"
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
//UI
#include "ui.h"

char buf[128] = {};
int bufindex = 0;
int wifi_close_flag = 0;
char *info[128] = {};
int wifi_flag = 0;
int i = 0;
int touch_flag = 0;

static Preferences preferences;
static constexpr uint8_t MIC_ADC_PIN = 25;
static constexpr uint8_t RECORD_BUTTON_PIN = 32;
static constexpr uint8_t SPEAKER_PIN = 26;
static constexpr uint8_t AUDIO_LIB_VOLUME = 21; 
static constexpr uint16_t TTS_BRIDGE_PORT = 8081;
static lv_obj_t * test_status_label = NULL;
Audio audio(true, I2S_DAC_CHANNEL_LEFT_EN);
static WiFiServer tts_bridge_server(TTS_BRIDGE_PORT);

//2.4
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18
#define SD_CS 5


/*更改屏幕分辨率*/
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[ screenWidth * screenHeight / 8 ];

TFT_eSPI lcd = TFT_eSPI(); /* TFT实例 */




/* 显示器刷新 */
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
  uint32_t w = ( area->x2 - area->x1 + 1 );
  uint32_t h = ( area->y2 - area->y1 + 1 );

  lcd.startWrite();
  lcd.setAddrWindow( area->x1, area->y1, w, h );
  lcd.pushColors( ( uint16_t * )&color_p->full, w * h, true );
  lcd.endWrite();

  lv_disp_flush_ready( disp );
}

uint16_t touchX, touchY;
/*读取触摸板*/
void my_touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data )
{
  bool touched = lcd.getTouch( &touchX, &touchY, 600);
  if ( !touched )
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    data->state = LV_INDEV_STATE_PR;

    /*设置坐标*/
    data->point.x = touchX;
    data->point.y = touchY;

    Serial.print( "Data x " );
    Serial.println( touchX );

    Serial.print( "Data y " );
    Serial.println( touchY );
  }
}


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
uint16_t calData[5] = { 557, 3263, 369, 3493, 3  };




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
      lcd.setCursor(0, 2 * i);
      lcd.printf("FILE:%s", file.name());

      Serial.print("SIZE: ");
      Serial.println(file.size());
      lcd.setCursor(180, 2 * i);
      lcd.printf("SIZE:%d", file.size());
      i += 16;
    }

    file = root.openNextFile();
  }
}

//SD卡初始化
int SD_init()
{

  if (!SD.begin(SD_CS))
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
  if (test_status_label != NULL)
  {
    lv_label_set_text(test_status_label, text);
  }
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

  String status = read_http_line(upstream, 6000);
  bool ok = status.startsWith("HTTP/1.1 200") || status.startsWith("HTTP/1.0 200");

  bool chunked = false;
  bool has_content_type = false;
  String content_type = "audio/mpeg";
  while (true)
  {
    String h = read_http_line(upstream, 6000);
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

  downstream.print("HTTP/1.1 200 OK\r\n");
  if (has_content_type) downstream.print("Content-Type: " + content_type + "\r\n");
  else downstream.print("Content-Type: audio/mpeg\r\n");
  if (chunked) downstream.print("Transfer-Encoding: chunked\r\n");
  downstream.print("Connection: close\r\n\r\n");

  uint8_t relay_buf[1024];
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
      if ((millis() - idle_started) > 10000) break;
      delay(1);
    }
  }

  downstream.flush();
  upstream.stop();
  downstream.stop();
}

static void tts_bridge_task(void* parameter)
{
  for (;;)
  {
    WiFiClient client = tts_bridge_server.available();
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
  audio.stopSong();
  delay(50);

  Serial.println("TTS mode: bridge POST -> local GET");
  set_test_status("Connecting to TTS...");

  String local_url = "http://" + WiFi.localIP().toString() + ":" + String(TTS_BRIDGE_PORT) + "/tts";
  if (!audio.connecttohost(local_url.c_str()))
  {
    set_test_status("TTS bridge connect failed");
    return false;
  }

  set_test_status("Playing TTS stream");
  
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
  audio.stopSong();
  if (!audio.connecttohost(url))
  {
    Serial.printf("%s stream failed\n", name);
    set_test_status("Stream failed");
    return false;
  }

  Serial.printf("Playing %s stream\n", name);
  set_test_status("Playing stream");
  return true;
}

static void handle_serial_command(char cmd)
{
  switch (cmd)
  {
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
    case '\r':
    case '\n':
    case ' ':
      break;
    default:
      Serial.printf("Unknown serial command: %c\n", cmd);
      Serial.println("Use: 1=play radio, 2=play test, 3=elevenlabs test");
      break;
  }
}

static void on_test_button_event(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
  {
    return;
  }

  set_test_status("Requesting stream...");
  request_elevenlabs_stream_test();
}

static void on_ndr_button_event(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
  {
    return;
  }

  set_test_status("Requesting NDR stream...");
  request_http_stream_test(ICECAST_TEST_URL, "NDR");
}

static void on_nrj_button_event(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
  {
    return;
  }

  set_test_status("Requesting NRJ stream...");
  request_http_stream_test(NRJ_TEST_URL, "NRJ");
}

static void create_test_button()
{
  lv_obj_t *screen = lv_scr_act();
  lv_obj_t *test_button = lv_btn_create(screen);
  lv_obj_set_size(test_button, 92, 44);
  lv_obj_align(test_button, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_add_event_cb(test_button, on_test_button_event, LV_EVENT_ALL, NULL);

  lv_obj_t *test_label = lv_label_create(test_button);
  lv_label_set_text(test_label, "TTS");
  lv_obj_center(test_label);

  lv_obj_t *ndr_button = lv_btn_create(screen);
  lv_obj_set_size(ndr_button, 92, 44);
  lv_obj_align(ndr_button, LV_ALIGN_BOTTOM_RIGHT, -108, -10);
  lv_obj_add_event_cb(ndr_button, on_ndr_button_event, LV_EVENT_ALL, NULL);

  lv_obj_t *ndr_label = lv_label_create(ndr_button);
  lv_label_set_text(ndr_label, "NDR");
  lv_obj_center(ndr_label);

  lv_obj_t *nrj_button = lv_btn_create(screen);
  lv_obj_set_size(nrj_button, 92, 44);
  lv_obj_align(nrj_button, LV_ALIGN_BOTTOM_RIGHT, -206, -10);
  lv_obj_add_event_cb(nrj_button, on_nrj_button_event, LV_EVENT_ALL, NULL);

  lv_obj_t *nrj_label = lv_label_create(nrj_button);
  lv_label_set_text(nrj_label, "NRJ");
  lv_obj_center(nrj_label);

  test_status_label = lv_label_create(screen);
  lv_obj_set_width(test_status_label, 220);
  lv_label_set_long_mode(test_status_label, LV_LABEL_LONG_CLIP);
  lv_obj_align(test_status_label, LV_ALIGN_BOTTOM_LEFT, 10, -24);
  lv_label_set_text(test_status_label, "Audio test ready");
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

void touch_calibrate()//屏幕校准
{
  uint16_t calData[5];
  uint8_t calDataOK = 0;
  Serial.println("屏幕校准");

  //校准
  //  lcd.fillScreen(TFT_BLACK);
  //  lcd.setCursor(20, 0);
  //  Serial.println("setCursor");
  //  lcd.setTextFont(2);
  //  Serial.println("setTextFont");
  //  lcd.setTextSize(1);
  //  Serial.println("setTextSize");
  //  lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  //  lcd.println("按指示触摸角落");
  Serial.println("按指示触摸角落");
  lv_timer_handler();
  delay(100);
  //  lcd.setTextFont(1);
  //  lcd.println();
  Serial.println("setTextFont(1)");
  lcd.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
  Serial.println("calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15)");
  Serial.println(); Serial.println();
  Serial.println("//在setup()中使用此校准代码:");
  Serial.print("uint16_t calData[5] = ");
  Serial.print("{ ");



  for (uint8_t i = 0; i < 5; i++)
  {
    Serial.print(calData[i]);
    if (i < 4) Serial.print(", ");
  }

  Serial.println(" };");
  Serial.print("  tft.setTouch(calData);");
  Serial.println(); Serial.println();
  //  lcd.fillScreen(TFT_BLACK);
  //
  //  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  //  lcd.println("XZ OK!");
  //  lcd.println("Calibration code sent to Serial port.");

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

  audio.setVolume(AUDIO_LIB_VOLUME); // 0..21

  wifi_flag = connect_wifi_from_sources() ? 1 : 0;

  //lvgl初始化
  lv_init();

  //LCD初始化
  lcd.begin();          /*初始化*/
  lcd.fillScreen(TFT_BLACK);
  delay(300);
  //背光引脚
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  lcd.setRotation(1); /* 旋转 */
  //  lcd.fillScreen(TFT_RED);
  //  Serial.println( "111111111" );
  //  delay(500);
  //  lcd.fillScreen(TFT_GREEN);
  //  Serial.println( "222222222" );
  //  delay(500);
  //  lcd.fillScreen(TFT_BLUE);
  //  Serial.println( "33333333" );
  //  delay(500);
  //  lcd.fillScreen(TFT_BLACK);
  //  delay(500);

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

  //校准模式。一是四角定位、二是直接输入模拟数值直接定位
  //屏幕校准
  //  touch_calibrate();
  lcd.setTouch( calData );


  lv_disp_draw_buf_init( &draw_buf, buf1, NULL, screenWidth * screenHeight / 8 );

  /*初始化显示*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  /*将以下行更改为显示分辨率*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*初始化（虚拟）输入设备驱动程序*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register( &indev_drv );

  ui_init();//开机UI界面
  lv_timer_handler();
  create_test_button();

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
  request_elevenlabs_stream_test();
  Serial.println("Serial commands: 1=play radio, 2=play test, 3=elevenlabs test");
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
  audio.loop();
  lv_timer_handler();
  delay(1);
}