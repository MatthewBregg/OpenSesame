#include <SPIFFS.h>

#include <ArduinoJson.h>

#include <ESPAsyncWebSrv.h>

#include <Update.h>


#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include "./wifi.h"
#include "./html.h"
// In the above wifi.h file, place 
//  const char* ssid = "SSID";
//  const char* password = "PASS";
// Also for the OTA update!
//  const char* ota_user = "USER";
//  const char* ota_pass = "PASS";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)


//flag to use from web update to reboot the ESP
bool shouldReboot = false;


const int led = 2;
const int relay = 14;
const int enable_update_pin = 4;
const int RELAY_ON = HIGH;
const int RELAY_OFF = LOW;


int opened_at = 0;


void onRequest(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(404);
}

String getStatusAsJson() {
  const int capacity= JSON_OBJECT_SIZE(5);
  StaticJsonDocument<capacity> doc;
  doc["uptime"] = millis()/1000;
  doc["opened_at"] = opened_at/1000;
  doc["ota"] = update_pin_read_no_debounce();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["max_heap"] = ESP.getMaxAllocHeap();
  String output;
  serializeJson(doc, output);
  return output;
}

bool allow_update = false;

bool update_pin_read_no_debounce() {
  return digitalRead(enable_update_pin) == LOW;
}

bool update_pin_set() {
   if (update_pin_read_no_debounce()) {
      delay(50);
      if (update_pin_read_no_debounce()) {
        return true;
      }
   }
   return false;
}

unsigned long allowed_update_at = 0;
void setup(){
  Serial.begin(115200);
  pinMode(led,OUTPUT);
  pinMode(relay, OUTPUT);
  pinMode(enable_update_pin, INPUT_PULLUP);
  digitalWrite(led, LOW);
  digitalWrite(relay, RELAY_OFF);
  const char* hostname = "open_sesame";
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Set the hostname: https://github.com/espressif/arduino-esp32/issues/3438
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE); // required to set hostname properly
  WiFi.setHostname(hostname);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    // Upon entering Loop(), the ESP32 will be rebooted.
    return;
  }
  // Disable wifi power saving mode!
  WiFi.setSleep(false);
   
  // We must initialize spiffs in order to read the favicon/any other files.
  // IF WE DON'T INIT SPIFFS any send(SPIFFS,...) will fail mysteriously!
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  
 

  // attach AsyncEventSource
  server.addHandler(&events);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", GetHtml());
  });
  server.on("/status_json", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getStatusAsJson());
  });
  // respond to GET requests on URL $1....
  server.on("/open_door_ajax", HTTP_GET, [](AsyncWebServerRequest *request){
    // Return OK.
    opened_at = millis();
    request->send(200, "text/plain", getStatusAsJson());

    // Now do the important bit.
    digitalWrite(led, HIGH);
    digitalWrite(relay,RELAY_ON);
    delay(500); 
    digitalWrite(relay, RELAY_OFF);
    digitalWrite(led, LOW);
   
   

  });

  // Place a 16x16 png in the data folder named favicon.png in order for a favicon to be loader.
  // Then follow the directions on https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/ to flash the flash memory.
  // Favicon!
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.png", "image/png");
  });

   // HTTP basic authentication
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(ota_user, ota_pass)) {
        return request->requestAuthentication();
    }
    // Update security: Only allow OTA updates if the physical pin is set.
    if (update_pin_set()) {
      request->send(200, "text/plain", "Login Success!");
      allow_update = true;
      allowed_update_at = millis();
    } else {
      request->send(200, "text/plain", "Login succeeded, but update pin not set.");
      allow_update = false;
    }
  });

  // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    if (allow_update) {
      request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
    } else {
      request->send(403, "text/plain", "forbidden");
    }
  });
 
  // From the WebServers examples list: https://github.com/me-no-dev/ESPAsyncWebServer. 
  // Can export compiled binary for this from Sketch menu (Ctrl-Alt-S).. 
  // Note: It's the bin file, not the elf file.
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
      shouldReboot = !Update.hasError();
      String response_text = shouldReboot?"OK":"FAIL";
      if (!allow_update) {
        response_text = "Forbidden!";
      }
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", response_text);
      response->addHeader("Connection", "close");
      request->send(response);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    // If updates are not allowed, return immedietely.
    if (!allow_update) {
      return;
    }
    if(!index){
      Serial.printf("Update Start: %s\n", filename.c_str());
      Serial.printf("Update Start: %s\n", filename.c_str());
      if(!Update.begin()) {
        Update.printError(Serial);
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        Update.printError(Serial);
      }
    }
    if(final){
      if(Update.end(true)){
        Serial.printf("Update Success: %uB\n", index+len);
      } else {
        Update.printError(Serial);
      }
    }
  });

  
  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.
  server.onNotFound(onRequest);
  server.begin();
}

void loop(){
  // Disable update after a minute.
  if (millis() - allowed_update_at > (60* 1000)) {
    allow_update = false;
  }

  if(shouldReboot 
  // This WiFi lib. will not auto-reconnect if the wifi blips. (Or fails to connect in the first place) 
  // Since this is such a simple device, just reboot when this happens. 
  // See https://forum.arduino.cc/t/auto-reconnect-to-wifi/1082190/2.
  // There are better ways to do this, mentioned ^ and 
  // https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/station-class.html#setautoreconnect
  // But those seem to have more intricacies, and if this works...
  // Note: Reconnect quick.  Realized my AP kick command has a two minute ban hidden in it. 
      || (WiFi.status() != WL_CONNECTED)) {
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }
 
 
}
