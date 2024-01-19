#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <TaskScheduler.h>

#include <Wire.h>
#include <SPI.h>

#include <ScioSense_ENS160.h>

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#define SCREEN_ADDRESS 0x3C

#include <ArduinoOTA.h>

bool display_on=true;
bool immediate_update=false;
void update_display();
 
const char* ssid = "TRNNET-2G";
const char* password = "ripcord1";
const char *hostname="ESP_ens160";

ScioSense_ENS160      mySensor(ENS160_I2CADDR_1);
uint16_t co2;
uint16_t aqi;
uint16_t tvoc;

uint16_t co2_min=9999;
uint16_t co2_max=0;
uint16_t co2_overall_max=0;

uint32_t co2_sum=0;
uint8_t  co2_count=0;

Scheduler runner;
Task *update;
void updater();
 
ESP8266WebServer server(80);
 
// Serving Hello world
void getHelloWord() {
    server.send(200, "text/json", "{\"name\": \"Hello world\"}");
}

Adafruit_SSD1306 display(128, 32, &Wire, -1 );

IRAM_ATTR void display_mode()
{
   if (display_on)
      display_on=false;
   else
      display_on=true;

   immediate_update=true;
  
}

void getco2(){

   String message;

   message="";

   message+=String(co2);
   message+="\t";
   message+=String(aqi);
   message+="\t";
   message+=String(tvoc);
   message+="\t";
   message+=String(co2_overall_max);
   message+="\t";
   message+=String(co2_min);
   message+="\t";
   message+=String(co2_max);
   message+="\t";
   message+=String(co2_sum/co2_count);
   message+="\t";
   message+=String(co2_count);
   message+="\t";
   message+=String(millis()/1000/60);
   message+="\n";

    server.send(200, "text/plain", message);

    co2_min=9999;
    co2_max=0;
    co2_sum=co2;
    co2_count=1;

}

void getSettings() {
    String response = "{";
 
    response+= "\"ip\": \""+WiFi.localIP().toString()+"\"";
    response+= ",\"gw\": \""+WiFi.gatewayIP().toString()+"\"";
    response+= ",\"nm\": \""+WiFi.subnetMask().toString()+"\"";
 
    if (server.arg("signalStrength")== "true"){
        response+= ",\"signalStrengh\": \""+String(WiFi.RSSI())+"\"";
    }
 
    if (server.arg("chipInfo")== "true"){
        response+= ",\"chipId\": \""+String(ESP.getChipId())+"\"";
        response+= ",\"flashChipId\": \""+String(ESP.getFlashChipId())+"\"";
        response+= ",\"flashChipSize\": \""+String(ESP.getFlashChipSize())+"\"";
        response+= ",\"flashChipRealSize\": \""+String(ESP.getFlashChipRealSize())+"\"";
    }
    if (server.arg("freeHeap")== "true"){
        response+= ",\"freeHeap\": \""+String(ESP.getFreeHeap())+"\"";
    }
    response+="}";
 
    server.send(200, "text/json", response);
}
 
// Define routing
void restServerRouting() {
    server.on("/", HTTP_GET, []() {
        server.send(200, F("text/html"),
            F("Welcome to the REST Web Server"));
    });
    server.on(F("/helloWorld"), HTTP_GET, getHelloWord);
    server.on(F("/settings"), HTTP_GET, getSettings);
    server.on(F("/co2"),HTTP_GET,getco2);
}
 

// Manage not found URL
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void update_display()
{
  display.clearDisplay();

  if (display_on) {
      display.setRotation(2);
      display.setTextSize(2,2);             
      display.setTextColor(SSD1306_WHITE);        
      display.setCursor(0,0); 
      display.dim(true);

      display.printf("CO2: %u\n",co2) ;
      display.setTextSize(1,2);
      display.printf("AQI: %u  TVOC:%u",aqi,tvoc);
}    
  display.display();
}

void updater()
{

 if (mySensor.available()) {
  mySensor.measure(true);
  mySensor.measureRaw(true);
  

  Serial.println("Updater");
  co2=mySensor.geteCO2();
  aqi=mySensor.getAQI();
  tvoc=mySensor.getTVOC();
 }

  if (co2<co2_min)
     co2_min=co2;
  if (co2>co2_max)
     co2_max=co2;

  if (co2>co2_overall_max)
     co2_overall_max=co2;

  if(co2_count<10000){
        co2_sum+=co2;
        co2_count++;
  } else {
      co2_sum=co2;
      co2_count=1;
  }

  update_display();

}
 
void setup(void) {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);

  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  Serial.println("");
 
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  pinMode(D5, INPUT);
  attachInterrupt(digitalPinToInterrupt(D5), display_mode, RISING);

  Wire.begin();

    Serial.print("ENS160...");
  mySensor.begin();
  Serial.println(mySensor.available() ? "done." : "failed!");
  if (mySensor.available()) {
    // Print ENS160 versions
    Serial.print("\tRev: "); Serial.print(mySensor.getMajorRev());
    Serial.print("."); Serial.print(mySensor.getMinorRev());
    Serial.print("."); Serial.println(mySensor.getBuild());
  
    Serial.print("\tStandard mode ");
    Serial.println(mySensor.setMode(ENS160_OPMODE_STD) ? "done." : "failed!");
  }
 
  
 
  // Set server routing
  restServerRouting();
  // Set not found response
  server.onNotFound(handleNotFound);
  // Start server
  server.begin();
  Serial.println("HTTP server started");

  runner.init();

  update= new Task(10*1000, TASK_FOREVER, &updater);
  runner.addTask(*update);
  update->enable();

   display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  ArduinoOTA.begin();
}
 
void loop(void) {
  server.handleClient();
  runner.execute();
  ArduinoOTA.handle();

  if (immediate_update) {
    update_display();
    immediate_update=false;
  }
}