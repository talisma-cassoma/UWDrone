/*
INTELLCAP_BOT
IBIN TOFAIL UNIVERSITY - KENITRA
*/
#include <IOXhop_FirebaseESP32.h>  //funcionando com a versao 5.1.1 do <arduinoJson.h>   

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base64.h"


#define WIFI_SSID "*************"                    
#define WIFI_PASSWORD "*************"          
#define FIREBASE_HOST "*************"      
#define FIREBASE_AUTH "*************"   

#define IN1_PIN 12 //dc motors pin controls
#define IN2_PIN 13
//#define IN3_PIN 14 //electric cylinder pin controls
//#define IN4_PIN 15

#define SERVO_1 14

Servo servoN1;
Servo servoN2;
Servo servo1;

int angleDegree = 0; 
 
#include "esp_camera.h"

// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled

//CAMERA_MODEL_AI_THINKER
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


String Photo2Base64(){
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();  
    if(!fb) {
      Serial.println("Camera capture failed");
      return "";
    }
  
    String imageFile = "data:image/jpeg;base64,";
    char *input = (char *)fb->buf;
    char output[base64_enc_len(3)];
    for (int i=0;i<fb->len;i++) {
      base64_encode(output, (input++), 3);
      if (i%3==0) imageFile += urlencode(String(output));
    }

    esp_camera_fb_return(fb);
    
    return imageFile;
}

//https://github.com/zenmanenergy/ESP8266-Arduino-Examples/
String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
}

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  
  servo1.setPeriodHertz(50);    // standard 50 hz servo
  
  servoN1.attach(2, 1000, 2000);
  servoN2.attach(13, 1000, 2000);
  
  servo1.attach(SERVO_1, 1000, 2000);
  
  servo1.write(angleDegree);

  Serial.begin(115200);
  delay(10);

pinMode(IN1_PIN, OUTPUT);
pinMode(IN2_PIN, OUTPUT);
//pinMode(IN3_PIN, OUTPUT);
//pinMode(IN4_PIN, OUTPUT);

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
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 8;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;/*(Quarter Quarter VGA) A screen resolution of 160x120 pixels, 
    which is one fourth the total number of pixels in the QVGA standard (320x240)*/
    config.jpeg_quality = 8;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  //drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QQVGA);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA

  //WIFI
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Conectando ao wifi");
  
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  
  Serial.println();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.stream("/controls", [](FirebaseStream stream) {
    String eventType = stream.getEvent();
    eventType.toLowerCase();
    
    Serial.print("event: ");
    Serial.println(eventType);
    
    
    if(eventType == "patch"){
      String path = stream.getPath();
      Serial.print("Path: ");
      Serial.println(path);
      
      JsonObject&  readCommand = stream.getData();
    
        if(readCommand.containsKey("angle")){//controls rudder angle 
           angleDegree = readCommand["angle"].as<int>();
           servo1.write(angleDegree);
           Serial.print("angulo: ");
           Serial.println(angleDegree);
          }
        else if(readCommand.containsKey("rotation")){//rotatate clockwise/unclockwise
         
          String rotation = readCommand["rotation"].as<String>();
           Serial.print("rotation: ");
           Serial.println(rotation);
          
          if(rotation =="clockwise"){
              digitalWrite(IN1_PIN, LOW);
              digitalWrite(IN2_PIN, HIGH);
          }
        else if(rotation =="unclockwise"){
              digitalWrite(IN1_PIN, HIGH);
              digitalWrite(IN2_PIN, LOW);
          }
        else if(rotation =="off"){
              digitalWrite(IN1_PIN, LOW);
              digitalWrite(IN2_PIN, LOW);
          }
        }
      else if(readCommand.containsKey("dive")){//dive button on/off 
         
          String dive = readCommand["dive"].as<String>();
           Serial.print("dive Bot: ");
           Serial.println(dive);
           /*if(readCommand =="ON"){
              digitalWrite(IN3_PIN, LOW);
              digitalWrite(IN4_PIN, HIGH);
              }
          else if(readCommand =="OFF"){
              digitalWrite(IN3_PIN, lOW);
              digitalWrite(IN4_PIN, LOW);
           }*/ 
         }
      }
  });
}
 
void loop()
{
   Firebase.setString("/imageCaptured/base64Image", Photo2Base64());
}