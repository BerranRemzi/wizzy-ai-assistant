#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <demos/lv_demos.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Audio.h>

#if __has_include("secrets.h")
#  include "secrets.h"
#else
#  warning "Create include/secrets.h or credentials will be loaded from NVS only"
#  define WIFI_SSID     ""
#  define WIFI_PASSWORD ""
#endif

#ifndef ELEVENLABS_TEST_STREAM_URL
#define ELEVENLABS_TEST_STREAM_URL ""
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
#include <Ticker.h>          //Call the ticker. H Library
Ticker ticker1;
static int first_flag = 0;
extern int zero_clean;
extern int goto_widget_flag;
extern int bar_flag;
extern lv_obj_t * ui_MENU;
extern lv_obj_t * ui_TOUCH;
extern lv_obj_t * ui_JIAOZHUN;
extern lv_obj_t * ui_Label2;
static lv_obj_t * ui_Label;//TOUCH界面label
static lv_obj_t * ui_Label3;//TOUCH界面label3
static lv_obj_t * ui_Labe2;//Menu界面进度条label
static lv_obj_t * bar;//Menu界面进度条
static int val = 100;

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
static constexpr uint8_t AUDIO_LIB_VOLUME = 6;
static lv_obj_t * test_status_label = NULL;
Audio audio(true, I2S_DAC_CHANNEL_LEFT_EN);

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
void callback1()  //Callback function
{
  if (bar_flag == 6)
  {
    if (val > 1)
    {
      val--;
      lv_bar_set_value(bar, val, LV_ANIM_OFF);
      lv_label_set_text_fmt(ui_Labe2, "%d %%", val);
    }
    else
    {
      lv_obj_clear_flag(ui_touch, LV_OBJ_FLAG_CLICKABLE);
      lv_label_set_text(ui_Labe2, "Loading");
      delay(150);
      val = 100;
      bar_flag = 0; //停止进度条标志
      goto_widget_flag = 1; //进入widget标志

    }
  }
}





//触摸Label控件
void label_xy()
{
  ui_Label = lv_label_create(ui_TOUCH);
  lv_obj_enable_style_refresh(true);
  lv_obj_set_width(ui_Label, LV_SIZE_CONTENT);   /// 1
  lv_obj_set_height(ui_Label, LV_SIZE_CONTENT);    /// 1
  lv_obj_set_x(ui_Label, -30);
  lv_obj_set_y(ui_Label, -35);
  lv_obj_set_align(ui_Label, LV_ALIGN_CENTER);
  lv_obj_set_style_text_color(ui_Label, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Label3 = lv_label_create(ui_TOUCH);
  lv_obj_enable_style_refresh(true);
  lv_obj_set_width(ui_Label3, LV_SIZE_CONTENT);   /// 1
  lv_obj_set_height(ui_Label3, LV_SIZE_CONTENT);    /// 1
  lv_obj_set_x(ui_Label3, 58);
  lv_obj_set_y(ui_Label3, -35);
  lv_obj_set_align(ui_Label3, LV_ALIGN_CENTER);
  lv_obj_set_style_text_color(ui_Label3, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_Label3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label3, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
}


//进度条控件
void lv_example_bar(void)
{
  //////////////////////////////
  bar = lv_bar_create(ui_MENU);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);
  lv_obj_set_width(bar, 150);
  lv_obj_set_height(bar, 15);
  lv_obj_set_x(bar, 0);
  lv_obj_set_y(bar, 90);
  lv_obj_set_align(bar, LV_ALIGN_CENTER);
  lv_obj_set_style_bg_img_src(bar, &ui_img_bar_320_01_png, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_bg_img_src(bar, &ui_img_bar_320_02_png, LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(bar, lv_color_hex(0x2D8812), LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_opa(bar, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  ui_Labe2 = lv_label_create(bar);//创建标签
  lv_obj_set_style_text_color(ui_Labe2, lv_color_hex(0x09BEFB), LV_STATE_DEFAULT);
  lv_label_set_text(ui_Labe2, "0%");
  lv_obj_center(ui_Labe2);
}




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

static bool request_elevenlabs_stream_test()
{
  if (strlen(ELEVENLABS_TEST_STREAM_URL) == 0)
  {
    set_test_status("Set ELEVENLABS_TEST_STREAM_URL");
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

  if (!audio.connecttohost(ELEVENLABS_TEST_STREAM_URL))
  {
    set_test_status("ElevenLabs URL failed");
    return false;
  }

  set_test_status("Playing ElevenLabs URL");
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
  lv_label_set_text(test_label, "Test");
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
  Serial.begin( 9600 ); /*初始化串口*/
  Serial2.begin( 9600 ); /*初始化串口2*/

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
  while (1)
  {
    audio.loop();

    if (goto_widget_flag == 1)//进入widget
    {
      if (ticker1.active() == true)
      {
        ticker1.detach();
      }
      goto_widget_flag = 0;
      delay(300);
      break;
    }

    if (goto_widget_flag == 3)//进入触摸界面，先把进度条线程关闭
    {
      bar_flag = 0; //停止进度条标志
      if (ticker1.active() == true)
      {
        ticker1.detach();
      }
      if (first_flag == 0 || first_flag == 1)
      {
        label_xy();
        first_flag = 2;
      }
      if (zero_clean == 1)
      {
        touchX = 0;
        touchY = 0;
        zero_clean = 0;
      }
      lv_label_set_text(ui_Label, "Touch Adjust:");
      lv_label_set_text_fmt(ui_Label3, "%d  %d", touchX, touchY); //显示触摸信息
    }

    if (goto_widget_flag == 4)//触摸界面返回到Menu界面,使进度条加满
    {
      val = 100;
      delay(100);
      ticker1.attach_ms(35, callback1);//每35ms调用callback1
      goto_widget_flag = 0;
    }

    if (goto_widget_flag == 5) //触发校准信号
    {
      lv_scr_load_anim(ui_touch_calibrate, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
      lv_timer_handler();
      lv_timer_handler();
      delay(100);
      touch_calibrate();//触摸校准
      lcd.setTouch( calData );
      lv_scr_load_anim(ui_TOUCH, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
      lv_timer_handler();
      goto_widget_flag = 3; //进入触摸界面标志
      touchX = 0;
      touchY = 0;
    }

    if (bar_flag == 6)//刚开机进入Menu界面时运行进度条一次，之后就不再运行
    {
      if (first_flag == 0)
      {
        lv_example_bar();
        ticker1.attach_ms(35, callback1);//每35ms调用callback1
        first_flag = 1;
      }
    }

    lv_timer_handler();
  }


  lcd.fillScreen(TFT_BLACK);
  lv_demo_widgets();//主UI界面
  create_test_button();
  Serial.println( "Setup done" );
}

void loop()
{
  audio.loop();
  audio.loop();
  lv_timer_handler();
  delay(1);
}