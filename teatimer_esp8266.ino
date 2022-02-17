/*
 * Original Sketch by:
 * 
 * Carlo Stramaglia
 * 
 * https://www.youtube.com/c/CarloStramaglia
 * This is the Arduino Tea Bag Timer. 
 * Once you setup the time you want your tea to stay in the water, the arm will automatically dip and release the tea bag.
 * October 31st 2021
 */

/*
 * Changes: 
 * 
 * - Included a different TM1637 Library: https://github.com/bremme/arduino-tm1637
 * - Changed the code to work with an NodeMCU ESP8266
 * - Included ArduinoOTA for updating the code via WiFi
 * - Detach Servo after use for avoiding that the servo is still making noise after finishing
 * - Switching display off when not in use
 * 
 * Todo:
 * 
 * - Include a web ui for setting the timer
 * - Include a messaging api for sending messages when tea is ready
 * 
 * 02/17/2022
 * Martin Hilcher
*/
 
#include <Servo.h>
#include <SevenSegmentTM1637.h>
#include "OneButton.h"
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

const char* ssid     = "your_wlan_network_ssid";
const char* password = "your_wlan_network_password";

Servo myservo;

int minutes=00;
int seconds=0;
int i=0;
String timeDisplay;

// Display Pins configurations
// Pin D3 - > DIO
// Pin D4 - > CLK
SevenSegmentTM1637 tm1637(D4, D3);

// Push Button PIN definition
#define PIN_INPUT D2

OneButton button(PIN_INPUT, true);

// Set web server port number to 80
WiFiServer server(80);

void setup() {
  Serial.begin(9600);
  Serial.println("Arduino Tea Bag Timer sketch. Carlo Stramaglia https://www.youtube.com/c/CarloStramaglia");
  myservo.attach(D7);
  myservo.write(90);
  myservo.detach();

  tm1637.init();
  tm1637.setBacklight(50);
  button.attachClick(singleClick);
  button.attachDoubleClick(doubleClick);
  button.attachLongPressStart(longPressStart);
  tm1637.clear();
  tm1637.setColonOn(false);
  tm1637.print("OK");
  delay(1000);  
  tm1637.setColonOn(true);
  tm1637.off();
  
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    });
  ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
  ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}

void loop() {

  ArduinoOTA.handle();

  button.tick();
  delay (10);
}

void singleClick()
{
  Serial.println("x1");
  minutes++;
  Serial.println(minutes);
  
  tm1637.on();
  
  if (minutes < 10)
      tm1637.print("0" + String(minutes) + "00");
  else
      tm1637.print(String(minutes) + "00");
  delay(100);
} // singleClick Add minutes


void doubleClick()
{
  Serial.println("x2");
  if (minutes == 0) return;
    
  tm1637.blink(500, 4, 50, 0);

  myservo.attach(D7);
    
  for (i=90;i>=15;i--)
  {
    Serial.println(i);
    myservo.write(i);
    delay (100);
  }
  
  minutes--;
  for (minutes ; minutes >= 0; minutes--) {
        for (int seconds = 59; seconds >= 0; seconds--) {
            if (minutes < 10 && minutes >=0) {
              if (seconds < 10 && seconds >= 0) {
                timeDisplay = "0" + String(minutes) + "0" + String(seconds);
              }
              else timeDisplay = "0" + String(minutes) + String(seconds);
            }
            else {
              if (seconds < 10 && seconds >= 0) {
                timeDisplay = String(minutes) + "0" + String(seconds);
              }
              else timeDisplay = String(minutes) + String(seconds);              
            }
            tm1637.print(timeDisplay);
            
               //tm1637.switchColon(); 
            delay(1000);
        }
    }

    tm1637.blink(500, 4, 50, 0);
    
    for (i=16;i<=90;i++)
    {
      Serial.println(i);
      myservo.write(i);
      delay (100);
    }

   myservo.detach();

  tm1637.setColonOn(true);
  tm1637.print("0000");
  minutes = 0;
  tm1637.setColonOn(true);
 
  Serial.println("Timer Stop");
  delay(1000);
  tm1637.off();
} // doubleClick Timer Start

void longPressStart()
{
  Serial.println("long Press");
  minutes = 0;
  tm1637.setColonOn(true);
  tm1637.print("0000");
  tm1637.setColonOn(true);
  delay(1000);
  tm1637.off();  
} // longPressStart Timer Reset
