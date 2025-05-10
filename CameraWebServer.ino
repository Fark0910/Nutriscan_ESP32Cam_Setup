
// ========================================================================================================================================
// pins for leds because of most of the pins used by camera issue might persist and led wont glow check diff pins and pin configuration of esp32 cam
// ========================================================================================================================================
#include "pins.h"
#include "esp_camera.h"
#include <WiFi.h>
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"
// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid ="YOUR WIFISSID" ;                                                      
const char *password= "YOUR wIFIPASSWORD";                  

void startCameraServer();
void setupLedFlash(int pin);

// =========================================================================
// SDCARD SUPPORT CODE FOR ESP32 EXTRA STORAGE TO STORE IMAGES AND FILES
// =========================================================================
/*
#include "FS.h"
#include "SD_MMC.h"*/   
#include "SPI.h"

void setup() {
  Serial.begin(115200);
  pinMode(GREEN_LED_PIN ,OUTPUT);
  pinMode(RED_LED_PIN ,OUTPUT);

  Serial.setDebugOutput(true);
  Serial.println();

  Serial.begin(115200);

/*
// SDCARD SETUP

  if (!SD_MMC.begin()) {
    Serial.println("SD Card Mount Failed");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("SD Card mounted.");*/

// =========================================================================
// CAMERA PIN INITIALIZATION AND FURTHER SETUP
//CAMERA MODEL:RHYX M21-45
//CAMERA SENSOR:GC2145 //Important:Its not OV sensor 
// ==========================================================================

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
// =========================================================================
//Ensure the following are like this 
// =========================================================================
  config.frame_size = FRAMESIZE_240X240;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;

// =========================================================================
//Our camera RHYX M21-45 support this format only using PIXFORMAT_JPEG wont work 
// =========================================================================
  config.pixel_format = PIXFORMAT_RGB565;  // for streaming                   
  //config.pixel_format = PIXFORMAT_JPEG; // for face detection/recognition 
  config.jpeg_quality = 15; 


  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } 
// ===================================================================================================================
//This else statement will be executed for our camera and not if one these are checks to ensure the format it support 
// ==================================================================================================================
  else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }
// ==================================================================================================================================================
//This else statement will be executed for our camera and not if since it is has gc sensor all the tweaks related to brightness etc should be made here 
// ====================================================================================================================================================
  else {
    // GC sensor tuning (e.g. GC2145)
    s->set_brightness(s, 1);   // brighten the image
    s->set_contrast(s, 2);     // increase contrast for clarity
    s->set_sharpness(s, 2);    // improve sharpness
    s->set_saturation(s, 1);   // make colors pop a bit
    s->set_vflip(s, 1);        // flip if needed (try 0 if upside down)
    s->set_hmirror(s, 1);      // mirror horizontally if needed
    }
#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif
// ===================================================================================================================================================
//WIFI CONNECTION
// ====================================================================================================================================================

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("!!!WiFi connected!!!");

// ====================================================================================================================================================
//Start the camera server this functions is defined in app_httpd.cpp  
// ====================================================================================================================================================
  startCameraServer();
// ====================================================================================================================================================
//This is the local port on which UI will be visible 
// ====================================================================================================================================================
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  // ==================================================================================================================================================
//Add a delay
// ====================================================================================================================================================
  delay(10000);
}