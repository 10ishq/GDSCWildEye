#include "Arduino.h" // General functionality
#include "esp_camera.h" // Camera
#include <SD.h> // SD Card 
#include "FS.h" // File System
#include "soc/soc.h" // System settings (e.g. brownout)
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
#include <EEPROM.h> // EEPROM flash memory
#include <WiFi.h> // WiFi
#include "time.h" // Time functions
// "ESP Mail Client" by Mobizt, tested with v1.6.4
#include "ESP_Mail_Client.h" // e-Mail

// DEFINES
#define USE_INCREMENTAL_FILE_NUMBERING //Uses EEPROM to store latest file stored
#define TRIGGER_MODE // Photo capture triggered by GPIO pin rising/falling

// CONSTANTS
// GPIO Pin 33 is small red LED near to RESET button on the back of the board
const byte ledPin = GPIO_NUM_33;
// GPIO Pin 4 is bright white front-facing LED 
const byte flashPin = GPIO_NUM_4;
// When using TRIGGER_MODE, this pin will be used to initiate photo capture
const byte triggerPin = GPIO_NUM_13;
// Flash strength (0=Off, 255=Max Brightness)
// Setting a low flash value can provide a useful visual indicator of when a photo is being taken
const byte flashPower = 1;
const int startupDelayMillis = 2000; // time to wait after initialising  camera before taking photo

// GLOBALS
// Keep track of number of pictures taken for incremental file naming
int pictureNumber = 0;
// Full path of filename of the last photo saved
String path;

void sleep() {
  // IMPORTANT - we define pin mode for the trigger pin at the end of setup, because most pins on the ESP32-CAM
  // have dual functions, and may have previously been used by the camera or SD card access. So we overwrite them here
  pinMode(triggerPin, INPUT_PULLDOWN);
  // Ensure the flash stays off while we sleep
  rtc_gpio_hold_en(GPIO_NUM_4);
  // Turn off the LED
  digitalWrite(ledPin, HIGH);
  delay(1000);
  #ifdef TRIGGER_MODE
    // Use this to wakeup when trigger pin goes HIGH
    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 1);
    // Use this to wakeup when trigger pin goes LOW
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 0);
  #endif
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void setup() {
  // Light up the discrete red LED on the back of the board to show the device is active
  pinMode(ledPin, OUTPUT);
  // It's an active low pin, so we write a LOW value to turn it on
  digitalWrite(ledPin, HIGH);

  // CAUTION - We'll disable the brownout detection
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // Start serial connection for debugging purposes
  Serial.begin(115200);
  
  // Pin definition for CAMERA_MODEL_AI_THINKER
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // If the board has additional "pseudo RAM", we can create larger images
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // UXGA=1600x1200. Alternative values: FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Disable any hold on pin 4 that was placed before ESP32 went to sleep
  rtc_gpio_hold_dis(GPIO_NUM_4);
  // Use PWM channel 7 to control the white on-board LED (flash) connected to GPIO 4
  ledcSetup(7, 5000, 8);
  ledcAttachPin(4, 7);
  // Turn the LED on at specified power
  ledcWrite(7, flashPower);

  // Initialise the camera
  // Short pause helps to ensure the I2C interface has initialised properly before attempting to detect the camera
  delay(250);
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    sleep();
  }

  // Image settings
  sensor_t * s = esp_camera_sensor_get();
  // Gain
  s->set_gain_ctrl(s, 1);      // Auto-Gain Control 0 = disable , 1 = enable
  s->set_agc_gain(s, 0);       // Manual Gain 0 to 30
  s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
  // Exposure
  s->set_exposure_ctrl(s, 1);  // Auto-Exposure Control 0 = disable , 1 = enable
  s->set_aec_value(s, 1200);    // Manual Exposure 0 to 1200
  // Exposure Correction
  s->set_aec2(s, 1);           // Automatic Exposure Correction 0 = disable , 1 = enable
  s->set_ae_level(s, 0);       // Manual Exposure Correction -2 to 2
  // White Balance
  s->set_awb_gain(s, 1);       // Auto White Balance 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);        // White Balance Mode 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_whitebal(s, 1);       // White Balance 0 = disable , 1 = enable
  s->set_bpc(s, 0);            // Black Pixel Correction 0 = disable , 1 = enable
  s->set_wpc(s, 1);            // White Pixel Correction 0 = disable , 1 = enable
  s->set_brightness(s, 0);     // Brightness -2 to 2
  s->set_contrast(s, 0);       // Contrast -2 to 2
  s->set_saturation(s, 0);     // Saturation -2 to 2
  s->set_special_effect(s, 0); // (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  // Additional settings
  s->set_lenc(s, 1);           // Lens correction 0 = disable , 1 = enable
  s->set_hmirror(s, 0);        // Horizontal flip image 0 = disable , 1 = enable
  s->set_vflip(s, 0);          // Vertical flip image 0 = disable , 1 = enable
  s->set_colorbar(s, 0);       // Colour Testbar 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
  s->set_dcw(s, 1);            // 0 = disable , 1 = enable
  
  // We want to take the picture as soon as possible after the sensor has been triggered, so we'll do that first, before
  // setting up the SD card, Wifi etc.
  // Initialise a framebuffer 
  camera_fb_t *fb = NULL;
  // But... we still need to give the camera a few seconds to adjust the auto-exposure before taking the picture
  // Otherwise you get a green-tinged image as per https://github.com/espressif/esp32-camera/issues/55
  // Two seconds should be enough
  delay(startupDelayMillis);
  // Take picture
  fb = esp_camera_fb_get();
  // Check it was captured ok  
  if(!fb) {
    Serial.println("Camera capture failed");
    sleep();
  }

  // Turn flash off after taking picture
  ledcWrite(7, 0);

  // Build up the string of the filename we'll use to save the file
  path = "/WE001";

  // Following section creates filename based on increment value saved in EEPROM
  #ifdef USE_INCREMENTAL_FILE_NUMBERING
    // We only need 2 bytes of EEPROM to hold a single int value, but according to
    // https://arduino-esp8266.readthedocs.io/en/latest/libraries.html#eeprom
    // Minimum reserved size is 4 bytes, so we'll use that
    EEPROM.begin(4);
    // Read the value from the EEPROM cache
    EEPROM.get(0, pictureNumber);
    pictureNumber += 1;
    // Path where new picture will be saved in SD Card
    path += String(pictureNumber) + "_";
    // Update the EEPROM cache
    EEPROM.put(0, pictureNumber);
    // And then actually write the modified cache values back to EEPROM
    EEPROM.commit();
  #endif

  // Add the file extension
  path += ".jpg";
  
  // Next, we need to start the SD card
  Serial.println("Starting SD Card");
  if(!MailClient.sdBegin(14, 2, 15, 13)) {
    Serial.println("SD Card Mount Failed");
    sleep();
  }

  // Access the file system on the SD card
  fs::FS &fs = SD;
  // Attempt to save the image to the specified path
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.printf("Failed to save to path: %s\n", path.c_str());
    sleep();
  }
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.printf("Saved file to path: %s\n", path.c_str());
  }
  file.close();

  // Now that we've written the file to SD card, we can release the framebuffer memory of the camera
  esp_camera_fb_return(fb);
  // And breathe for a moment...
  delay(1000);
   // And go to bed until the next time we are triggered to take a photo
  sleep();
}

void loop() {
}
