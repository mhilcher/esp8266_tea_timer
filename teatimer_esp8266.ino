/*
   Original Sketch by:

   Carlo Stramaglia

   https://www.youtube.com/c/CarloStramaglia
   This is the Arduino Tea Bag Timer.
   Once you setup the time you want your tea to stay in the water, the arm will automatically dip and release the tea bag.
   October 31st 2021
*/

/*
   Changes:

   - Included a different TM1637 Library: https://github.com/bremme/arduino-tm1637
   - Changed the code to work with an NodeMCU ESP8266
   - Included ArduinoOTA for updating the code via WiFi
   - Detach Servo after use for avoiding that the servo is still making noise after finishing
   - Switching display off when not in use
   - Included a web ui for setting the timer
   - Included smtp for sending a mail when tea is ready

   Todo:

    - include MQTT

   02/17/2022
   Martin Hilcher
*/

#include <Servo.h>
#include <SevenSegmentTM1637.h>
#include "OneButton.h"
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP_Mail_Client.h>

const char* ssid     = "your_wlan_network_ssid";
const char* password = "your_wlan_network_password";

// Define SMTP Host and Port, normally 465 for SSL
#define SMTP_HOST "smtp_host_of_your_provider"
#define SMTP_PORT 465


/* The sign in credentials */
#define AUTHOR_EMAIL "your_email_address"
#define AUTHOR_USERNAME "your_username(maybe indentical with email address)"
#define AUTHOR_PASSWORD "your_email_password"

SMTPSession smtp;
SMTP_Message message;
ESP_Mail_Session session;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

/* Recipient's email*/
#define RECIPIENT_EMAIL "email_address_recipient"

Servo myservo;

int minutes = 00;
int seconds = 0;
int i = 0;
String timeDisplay;
int minStart;
int hLen;
IPAddress myIPAddress;
String curIPAddrString;

// Variable to store the HTTP request
String header;

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
  myIPAddress = WiFi.localIP();
  curIPAddrString = String(myIPAddress[0]) + "." + String(myIPAddress[1]) + "." + String(myIPAddress[2]) + "." + String(myIPAddress[3]);
  Serial.println(curIPAddrString);
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

  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);

  /* Declare the session config data */
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_USERNAME;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  /* Set the message headers */
  message.sender.name = "Tea Timer";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "Tea is ready!";
  message.addRecipient("real_name_of_recipient", RECIPIENT_EMAIL);

  String textMsg = "Your tea is ready.";
  message.text.content = textMsg.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
  
  /* if your mail provider supports notifying you can uncomment this.
     if you get an error leave it commented */
  
  //message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

  /* Set the custom message header */
  //message.addHeader("Message-ID: <abcde.fghij@gmail.coum>");
}

void setDisplay();

void loop() {

  ArduinoOTA.handle();

  WiFiClient client = server.available();   // Listen for incoming clients

  button.tick();
  delay (10);

  if (client) {                             // If a new client connects,
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            if (header.indexOf("GET /start") >= 0 ) {

              minStart = header.indexOf("/min");
              hLen = header.length();

              minutes = header.substring(minStart + 5, hLen - 1).toInt();
            }
            

            if (minutes <= 0) {

              client.println("<!doctype html><html><head><meta charset=\"utf-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge,chrome=1\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Tea Timer 1.0</title>");
              client.println("<script>time='00:00';lastTime='00:00';timerRunning=0;alarmRunning=0;secs=0;let intVal;function showTime(){document.getElementById('timer_display').innerHTML=time}function parseTime(){const[minStr,secsStr]=time.split(':');const mins=parseInt(minStr,10);const secs=parseInt(secsStr,10);return[mins,secs]}function incMins(){let[mins,secs]=parseTime();mins+=1;time=padZeros(mins)+':'+padZeros(secs);lastTime=time;document.getElementById('startButton').href='http://'+'" + curIPAddrString + "'+'/start/min/'+String(mins);showTime()}function decMins(){let[mins,secs]=this.parseTime();mins=mins-1;if(mins<0){mins=0}else if(mins===0){document.getElementById('startButton').href='#'}else{document.getElementById('startButton').href='http://'+'" + curIPAddrString + "'+'/start/min/'+String(mins)}time=padZeros(mins)+':'+padZeros(secs);lastTime=time;showTime()}function reset(){time='00:00';lastTime='00:00';timer=0;if(timerRunning===1){timerRunning=0;clearInterval(intVal)}}function decTimer(){let[mins,secs]=parseTime();if(secs>0){secs-=1}else if(secs===0){if(mins>0){secs=59;mins-=1}else{stopTimer()}}const mtime=padZeros(mins)+':'+padZeros(secs);time=mtime;showTime()}function startTimer(){document.getElementById('startButton').disabled = true; document.getElementById('minus').disabled = true;document.getElementById('plus').disabled = true;document.getElementById('reset').disabled = true; timerRunning=1;intVal=setInterval(decTimer,1000)}function stopTimer(){clearInterval(intVal);timerRunning=0; document.getElementById('startButton').disabled = false;  document.getElementById('minus').disabled = false;document.getElementById('plus').disabled = false;document.getElementById('reset').disabled = false; }function startStopTimer(){if(alarmRunning){alarmRunning=0;time=lastTime}else{if(timerRunning===0){let[mins,secs]=parseTime();secs+=9;time=padZeros(mins)+':'+padZeros(secs);startTimer()}else{stopTimer()}}}function padZeros(number){let result=String(number);result=result.padStart(2,'0');return result}function sendHTTPReq(){var reqUrl=document.getElementById('startButton').href;var request=new XMLHttpRequest();if(reqUrl!='#'){request.onreadystatechange=function(){if(this.readyState==4){if(this.status==200){if(this.responseText!=null){document.getElementById('startButton').href = '#'; startStopTimer()}}}};request.open('GET',reqUrl,true);request.send(null)}}</script>");
              client.println("<style>html,body {font-family: cursive;height: 285px;margin: 0px;padding: 0px;overflow: hidden;text-align: -webkit-center;}.outer_frame {width: 200px;height: 280px;background-color: white;display: grid;grid-template-rows: auto auto;justify-items: center;border: solid;border-width: thin;}.display_frame {width: 160px;height: 50px;color: black;border: 2px solid;text-align: center;margin: auto;display: grid;grid-template-rows: auto;}.timer_display {font-size: 35px;background-color: white;}.key_panel {width: 160px;height: 160px;display: grid;grid-gap: 10px;grid-template-rows: auto auto auto;}.min_sec {display: grid;grid-template-columns: auto auto;grid-gap: 10px;}.lean_button {font-family: cursive;font-size: 20px;}.wide_button {font-family: cursive;    font-size: 20px;    text-decoration: none;    color: black;    border: solid;    border-color: black;}</style>");
              client.println("</head><body><div class=\"outer_frame\"><div class=\"display_frame\"><div id=\"timer_display\" class=\"timer_display\"></div></div><div class=\"key_panel\"> <div class=\"min_sec\"> <button class=\"lean_button\" id=\"plus\" onClick=\"incMins()\"> + </button> <button class=\"lean_button\" id=\"minus\" onClick=\"decMins()\"> - </button> </div><button class=\"wide_button\" id=\"startButton\" type=\"button\" onClick=\"sendHTTPReq()\">Start</button> <button class=\"wide_button\" id=\"reset\" onClick=\"reset()\"> Reset </button> </div></div></body> <script>");
            }
            
            if (minutes > 0) {

              client.println("OK");
              setDisplay();
            }
            else {
              client.println("document.getElementById('startButton').href = '#'; showTime();</script> </html>");
            }
            
            // The HTTP response ends with another blank line
            client.println();
            delay(100);
                        
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");

    if (minutes > 0) {

      doubleClick();
    }
  }
}

void singleClick()
{
  Serial.println("x1");
  minutes++;
  Serial.println(minutes);
  setDisplay();
} // singleClick Add minutes

void setDisplay() {

  tm1637.on();

  if (minutes < 10)
    tm1637.print("0" + String(minutes) + "00");
  else
    tm1637.print(String(minutes) + "00");
  delay(100);
}

void doubleClick()
{
  Serial.println("x2");
  if (minutes == 0) return;

  tm1637.blink(500, 4, 50, 0);

  myservo.attach(D7);

  for (i = 90; i >= 15; i--)
  {
    //Serial.println(i);
    myservo.write(i);
    delay (100);
  }

  minutes--;
  for (minutes ; minutes >= 0; minutes--) {
    for (int seconds = 59; seconds >= 0; seconds--) {
      if (minutes < 10 && minutes >= 0) {
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

  for (i = 16; i <= 90; i++)
  {
    //Serial.println(i);
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

  if (!smtp.connect(&session))
    return;

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());
  
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

void smtpCallback(SMTP_Status status) {
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()) {
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
  }
}
