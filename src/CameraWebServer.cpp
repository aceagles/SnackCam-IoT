/*********
  Rui Santos
  Complete instructions at: https://RandomNerdTutorials.com/esp32-cam-save-picture-firebase-storage/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  Based on the example provided by the ESP Firebase Client Library
*********/

#include "Arduino.h"
#include "WiFi.h"
#include "esp_camera.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <LittleFS.h>
#include <FS.h>
#include "time.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>



struct tm timeinfo;
const char* ntpServer = "pool.ntp.org";
const char* TZ_INFO    = "GMT+0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00";  // enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
long unsigned lastNTPtime;
time_t now;


//Replace with your network credentials
const char* ssid = "23-Oak-Crescent";
const char* password = "23MountainHouses";

// Insert Firebase project API Key
#define API_KEY "AIzaSyD0vzUmoym2OXYL2gKFWe7LBOk-pesbq6Q"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "a.c.eagles70@gmail.com"
#define USER_PASSWORD "172927"

// Insert Firebase storage bucket ID e.g bucket-name.appspot.com
#define STORAGE_BUCKET_ID "snackcam-4d1d0.appspot.com"
// For example:
//#define STORAGE_BUCKET_ID "esp-iot-app.appspot.com"

// Photo File Name to save in LittleFS
#define FILE_PHOTO_PATH "/photo.jpg"
String BUCKET_PHOTO =  "/data/photo_";

String localTime() {
 struct tm timeinfo;
 char ttime[40];
 if(!getLocalTime(&timeinfo)) return"Failed to obtain time";
 strftime(ttime,40,  "%Y%m%d-%H%M%S", &timeinfo);
 return ttime;
}

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define N 5
boolean takeNewPhoto = false;

//Define Firebase Data objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;

void fcsUploadCallback(FCS_UploadStatusInfo info);

int taskCompleted = 1;

camera_fb_t* fb_array[N];
int fb_index = 0, loopCount = 0;

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS( void ) {
  // Dispose first pictures because of bad quality
  
  // Skip first 3 frames (increase/decrease number as needed).
  
  esp_camera_fb_return(fb_array[fb_index]);
  
  
  // Take a new photo
  fb_array[fb_index] = NULL;  
  
  fb_array[fb_index] = esp_camera_fb_get();  
  
  if(!fb_array[fb_index]) {
    Serial.println("Camera capture failed");
  } 

  // Change the index
  fb_index++;
  if(fb_index >= N ) fb_index = 0;
  
}

void initWiFi(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
}

void initLittleFS(){
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("LittleFS mounted successfully");
  }
}

void initCamera(){
 // OV2640 camera module
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = N;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  } 
}

bool getNTPtime(int sec) {

   uint32_t start = millis();      // timeout timer

   do {
     time(&now);
     localtime_r(&now, &timeinfo);
     Serial.print(".");
     delay(100);
   } while (((millis() - start) <= (1000 * sec)) && (timeinfo.tm_year < (2016 - 1900)));

   if (timeinfo.tm_year <= (2016 - 1900)) return false;  // the NTP call was not successful
  
     Serial.print("now ");
     Serial.println(now);
   

   // Display time
   
    char time_output[30];
    strftime(time_output, 30, "%a  %d-%m-%y %T", localtime(&now));
    Serial.println(time_output);
    Serial.println();
   
 return true;
}



void setup() {

  
  for (int i = 0; i < N; i++) {
      fb_array[i] = (camera_fb_t*)malloc(sizeof(camera_fb_t));
      if (fb_array[i] == NULL) {
          // Handle allocation failure
      }
      // Initialize fb_array[i] as needed
  }


  // Serial port for debugging purposes
  Serial.begin(115200);
  initWiFi();
  initLittleFS();
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  initCamera();

  //Firebase
  // Assign the api key
  configF.api_key = API_KEY;
  //Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  //Assign the callback function for the long running token generation task
  configF.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("\nGetting real time (NTP)");
   configTime(0, 0, ntpServer);
   setenv("TZ", TZ_INFO, 1);
   if (getNTPtime(10)) {  // wait up to 10 sec to sync
   } else {
      Serial.println("Time not set");
   }
  lastNTPtime = time(&now);

  Serial.print("Local time: ");
  Serial.println(localTime());
  pinMode(4, OUTPUT);

  pinMode(14, INPUT_PULLDOWN);
}

void loop() {
  
  if (loopCount >= 10) {
    capturePhotoSaveLittleFS();
    loopCount = 0;
  }
  delay(1);
  if (Firebase.ready() && taskCompleted == 0){
    taskCompleted = 1;
    for(int i = 0; i < N; i++){
    if(fb_array[i] != NULL){
    Serial.print("Uploading picture... ");

    //MIME type should be valid to avoid the download problem.
    //The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
    for (int i = 0; i < 4; i++){
      Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
      if(Firebase.Storage.upload(&fbdo, 
        STORAGE_BUCKET_ID /* Firebase Storage bucket id */, 
        fb_array[i]->buf /* byte array from ram or flash */, 
        fb_array[i]->len /*  size of data in bytes */, 
        BUCKET_PHOTO + localTime() + ".jpg" /* path of remote file stored in the bucket */, 
        "image/jpeg" /* mime type */, 
        fcsUploadCallback /* callback function */)) break;
      esp_camera_fb_return(fb_array[i]);
    }
    } else {
      Serial.println("NULL buffer");
    }
    }
  }

  //Capture two more images after the trigger
  if (digitalRead(14)) taskCompleted = -2;
  if (taskCompleted < 0) taskCompleted++;
  delay(100);
  loopCount++;
}

// The Firebase Storage upload callback function
void fcsUploadCallback(FCS_UploadStatusInfo info){
    if (info.status == firebase_fcs_upload_status_init){
        Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
    }
    else if (info.status == firebase_fcs_upload_status_upload)
    {
        Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    }
    else if (info.status == firebase_fcs_upload_status_complete)
    {
        Serial.println("Upload completed\n");
        FileMetaInfo meta = fbdo.metaData();
        Serial.printf("Name: %s\n", meta.name.c_str());
        Serial.printf("Bucket: %s\n", meta.bucket.c_str());
        Serial.printf("contentType: %s\n", meta.contentType.c_str());
        Serial.printf("Size: %d\n", meta.size);
        Serial.printf("Generation: %lu\n", meta.generation);
        Serial.printf("Metageneration: %lu\n", meta.metageneration);
        Serial.printf("ETag: %s\n", meta.etag.c_str());
        Serial.printf("CRC32: %s\n", meta.crc32.c_str());
        Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
        Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
    }
    else if (info.status == firebase_fcs_upload_status_error){
        Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    }
}