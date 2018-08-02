/*Pin assignments:
 * Wemos
D0 to TX Nextion
D1 to Base of 2N2222 over a 1K resistor (LEDs)not used
D2 
D4 to RX Nextion
D5 to Pin 2 of SN74HCT125
D7 to Pin 5 of SN74HCT125
D8 to Pin 9 of SN74HCT125

V2.6
Replaced wunderground time with Timelib
- Time is requested from the Linux server through MQTT
Added Syslog facility to log errors etc to the Linux server
Connected to MQTT
- Switch displays off and on 
- sends departure data to MQTT

V2.5
Replaced TimeClient with wunderground time
*/

#include "WundergroundClient.h"
#include <Ticker.h>
#include <JsonListener.h> // I know two JSON libs are an overkill
#include <ArduinoJson.h>  // Just was to lazy to change the code to fit only one.
#include <LedControl.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "ZVV.h"
#include "Nextion.h"
#include <TimeLib.h>
#include <Syslog.h>
#include <PubSubClient.h>

SoftwareSerial SoftySerial(16,2); // RX, TX attached to D0, D4 on Wemos D1 mini
NexPage page0    = NexPage(0, 0, "page0");
NexPicture p1 = NexPicture(0, 1, "p1");
NexPicture p2 = NexPicture(0, 2, "p2");
NexPicture p3 = NexPicture(0, 3, "p3");
NexPicture p4 = NexPicture(0, 4, "p4");
NexPicture p5 = NexPicture(0, 5, "p5");
NexText t0 = NexText(0, 6, "t0");
NexText t1 = NexText(0, 7, "t1");
NexText t2 = NexText(0, 8, "t2");
NexText t3 = NexText(0, 9, "t3");
NexText t4 = NexText(0, 10, "t4");
NexText t5 = NexText(0, 11, "t5");
NexText t6 = NexText(0, 12, "t6");
NexText t7 = NexText(0, 13, "t7");
NexTouch *nex_listen_list[] = 
{
    &page0,
    NULL
};

const unsigned char CH[] = { //Font for LED matrices
B00000000, 
B00000000, 
B00000000, 
B00000000, // Space  (represented by "/")

B00000000, 
B11111110, 
B10000010, 
B11111110, // 0

B00000000, 
B10000100, 
B11111110, 
B10000000, // 1

B00000000, 
B11100010, 
B10010010, 
B10001100, // 2

B00000000, 
B10010010, 
B10010010, 
B11111110, // 3

B00000000, 
B00011110, 
B00010000, 
B11111110, // 4

B00000000, 
B10011110, 
B10010010, 
B01110010, // 5

B00000000, 
B11111100, 
B10010010, 
B11110010, // 6

B00000000, 
B00000010, 
B00000010, 
B11111110, // 7

B00000000, 
B01101100, 
B10010010, 
B01101100, // 8

B00000000, 
B10011110, 
B10010010, 
B01111110, // 9
};

// Display settings
int data = 13;    // DIN pin of MAX7219 module
int clk = 14;     // CLK pin of MAX7219 module
int load = 15;    // CS  pin of MAX7219 module
int maxInUse = 6; // how many MAX7219 are connected
LedControl lc=LedControl(data,clk,load,maxInUse); // define Library

// Wireless settings
const char* SSID = "****"; 
const char* PASSWORD = "*****";
int downcount = 0;

//MQTT Settings
const char* mqtt_server = "192.168.178.59";
const char* mqtt_username = "mqtt";
const char* mqtt_password = "****";
char* InTopic = "ZVV/in"; //subscribe to topic to be notified about
char* TimeTopic = "Time"; //subscribe to topic to be notified about
char* OutTopic = "domoticz/in"; 
char* OutTopic1 = "Request/Time"; 
const int SEVEN_IDX = 82;
const int NINE_IDX = 83;
const int SEVENTYFIVE_IDX = 84;
const int BUS_R_IDX = 85;
const int SBB_IDX = 86;
const int ZVV_IDX = 87;

// Syslog server connection info
#define SYSLOG_SERVER "192.168.178.59"
#define SYSLOG_PORT 514
// This device info
#define DEVICE_HOSTNAME "wemos33"
#define APP_NAME "ZVV"
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;
// Create a new syslog instance with LOG_KERN facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_KERN);


// ZVV settings
ZVVClient ZVVClient;
String url;
char* Dest1 = "/bin/stboard.exe/dny?input=Z%C3%BCrich,+Riedgraben&dirInput=Z%C3%BCrich,+Hallenbad+Oerlikon&maxJourneys=";
char* Dest1Num ="2"; // Number of connections (Journeys) we are requesting
char* Dest2 = "/bin/stboard.exe/dny?input=Z%C3%BCrich,+Waldgarten&dirInput=Z%C3%BCrich,+Milchbuck&maxJourneys=";
char* Dest2Num ="4";
char* Dest3 = "/bin/stboard.exe/dny?input=Z%C3%BCrich,+Waldgarten&dirInput=Z%C3%BCrich,+Friedackerstrasse&maxJourneys=";
char* Dest3Num ="2";
char* Dest4 = "/bin/stboard.exe/dny?input=Z%C3%BCrich,+Oerlikon+(SBB)&dirInput=Z%C3%BCrich+HB&maxJourneys=";
char* Dest4Num ="7";
//DestxNum cannot be greater than 10, otherwise modify the ZVV.h file
int Number_Journeys;
const char* STARTS = "&boardType=dep&start=1&tpl=stbResult2json&time=";
const int Walking_time_Riedgraben = 6;
const int Walking_time_Waldgarten = 7;
const int Riedgraben_Oerlikon = 8; 
bool IR_train = false; 
int cur_h, cur_m, connection;
int dep_h,dep_m;
int countdown_figure = 0;
int Bus75 = 0;
int BusRiedgraben = 0;
int Tram7 = 0;
int Tram9 = 0;
int TrainOe = 0;

// Timing
volatile int WDTCount = 0;
bool SyncNecessary = true;
time_t prevDisplay = 0; // when the digital clock was displayed
time_t t;
time_t lastMsg = 0;
const int INTERVAL = 600; // 10 mins = 600
bool readyForZVVUpdate = true;
bool readyForWeatherUpdate = true;
bool readyForForecastUpdate = true;
bool readyForSunsetUpdate = true;
Ticker ZVVticker;
Ticker Weatherticker;
Ticker Forecastticker;
Ticker SecondTick; // to stabilize execution
Ticker PublishTick;

// Weather
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "*****";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_STATION_ID = "IZURICH188";
const String WUNDERGROUND_COUNTRY = "CH";
const String WUNDERGROUND_CITY = "Zurich";
WundergroundClient wunderground(IS_METRIC);

// Light
bool lights_on = true;
bool old_lights_on = true;

WiFiClient espClient;
const int httpPort = 80;
PubSubClient client(espClient);
int counter = 0;


void setup() {
  Serial.begin(9600);
  delay(10);
  Serial.println("Entering Setup");
  for (int i = 0;i < maxInUse;i++){
    lc.shutdown(i,false);
    delay(100);
    // Set the brightness to a medium values 
    lc.setIntensity(i,1);
    delay(100);
    // and clear the display 
    lc.clearDisplay(i);
    delay(100);
  }
  lc.setIntensity(2,5); //Intensity for 7-segment display
  nexInit(); // Initialize Nextion display
  setup_wifi();

  client.setServer(mqtt_server, 8883);
  client.setCallback(callback);
  
  //delay(5000);
  ZVVticker.attach(60, setReadyForZVVUpdate);           // Update every 60 secs
  Weatherticker.attach(481, setReadyForWeatherUpdate);  // Update every 8 minutes
  Forecastticker.attach(1801, setReadyForForecastUpdate); // Update every 30 minutes
  SecondTick.attach(1,ISRwatchdog); //every second
  PublishTick.attach(600,Publish_Status); //every 10 minutes

//OTA
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Wemos33-ZVV");
  // No authentication by default
  ArduinoOTA.setPassword((const char *)"***");
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
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
    
  Serial.println(F("Entering loop")); 
}


void loop (){
  if (!client.connected()) reconnect();
  if (now() > (t + 82800)) setReadyForClockUpdate();
  nexLoop(nex_listen_list);
  client.loop();
  if (SyncNecessary == false) {
    displayTime();
    if (readyForZVVUpdate) updateZVV();
    wait(1);
    if (readyForSunsetUpdate) updateSunset();
    wait(1);
    if (readyForWeatherUpdate) updateWeather();
    wait(1);
    if (readyForForecastUpdate) updateForecast();
    }
 wait(1);
}


void setup_wifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA); 
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);

  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ZVVClient",mqtt_username,mqtt_password)) {
      Serial.println("connected");
      counter = 0;
      // Once connected, publish an announcement...
      // ... and resubscribe
      client.subscribe(InTopic);
      delay(10);
      client.subscribe(TimeTopic);
      client.publish(OutTopic1,"1");
      delay(10);
      Publish_Status();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in .3 seconds");
      ++counter;
      if (counter > 180) ESP.reset();
      // Wait 0.3 seconds before retrying
      ArduinoOTA.handle();
      wait(300);
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
    int idx = root["idx"]; // 34
    int nvalue = root["nvalue"]; // request: 0 = open, 1 = closed
    int day_from_system = root["day"]; // 9
    int month_from_system = root["month"]; // 11
    int year_from_system = root["year"]; // 2017
    int hour_from_system = root["hour"]; // 16
    int minutes_from_system = root["minutes"]; // 29
    int seconds_from_system = root["seconds"]; // 26

    String what = String(nvalue);
    if (day_from_system && SyncNecessary || timeStatus() == timeNotSet) {
      setTime(hour_from_system,minutes_from_system,seconds_from_system,day_from_system,month_from_system,year_from_system); // alternative to above, yr is 2 or 4 digit yr
      Serial.println(F("Setting Time"));
      SyncNecessary = false;
      t = now();
      do wait(1);          // wait for clock to be at 0 secs
      while (second() != 0);
      }
    if (idx == ZVV_IDX) {
      if (nvalue == 1) light_on();
      else light_off();
      if (lights_on != old_lights_on) {
        publish(ZVV_IDX,lights_on,0);
        old_lights_on = lights_on;
      }
    }
}  


void updateZVV() {
 int trams_found = 0;
 int last_tram = 0;
 int tint = 0;
 int i = -1;
 Serial.println(F("------------------------------------------------------------------------------"));
 // Departure Waldgarten Bus75
 depart(Walking_time_Waldgarten, Dest3, Dest3Num);
 Serial.println("Bus: 75");
 ZVVClient.doUpdate(url);
 countdown_figure = ZVVClient.getCountdown(0);
 Serial.print(F("countdown: "));
 Serial.println(countdown_figure);
 Serial.println();
 if (Bus75 != countdown_figure) {
  Bus75 = countdown_figure;
  publish(SEVENTYFIVE_IDX,0,Bus75);
 }
 displayChar(countdown_figure,3);

 // Departure Riedgraben Buses
 depart(Walking_time_Riedgraben, Dest1, Dest1Num);
 Serial.println(F("Bus Riedgraben:"));
 ZVVClient.doUpdate(url);
 countdown_figure = ZVVClient.getCountdown(0);
 Serial.print(F("countdown: "));
 Serial.println(countdown_figure);
 Serial.println();
 connection = countdown_figure;
 if (BusRiedgraben != countdown_figure) {
  BusRiedgraben = countdown_figure;
  publish(BUS_R_IDX,0,BusRiedgraben);
 }
 displayChar(countdown_figure,1);

// Departure Oerlikon SBB
 if (connection < Walking_time_Riedgraben) connection = Walking_time_Riedgraben;
 depart(connection+Riedgraben_Oerlikon, Dest4, Dest4Num);
 Serial.println(F("Trains Oerlikon:"));
 ZVVClient.doUpdate(url);
 do {
   i++;
   tint = ZVVClient.getType(i);
   if (tint == 5 || tint == 1) { //check if it is a S-Bahn (type 5) or InterRegio (type 1)
     Serial.print("type: "); 
     Serial.println(tint);
     Serial.print(F("countdown: "));
     Serial.println(ZVVClient.getCountdown(i));
     Serial.println();
     trams_found++; 
     if (tint == 1) IR_train = true;
     } 
   } while (trams_found < 1 && i < Number_Journeys -1);
 displayChar(ZVVClient.getCountdown(i),0);
 if (TrainOe != ZVVClient.getCountdown(i)) {
   TrainOe = ZVVClient.getCountdown(i);
   publish(SBB_IDX,0,TrainOe);
   }
 IR_train = false;
 i = -1;
 trams_found = 0;

 // Departure Waldgarten Trams
 depart(Walking_time_Waldgarten, Dest2, Dest2Num);
 Serial.println(F("Trams Waldgarten:"));
 ZVVClient.doUpdate(url);
 do {
   i++;
   tint = (ZVVClient.getLine(i)).toInt();
   if (last_tram != tint) {
     Serial.print("Trm: "); 
     Serial.println(tint);
     Serial.print(F("countdown: "));
     Serial.println(ZVVClient.getCountdown(i));
     if (tint == 7) { 
      displayChar(ZVVClient.getCountdown(i),5);
      if (Tram7 != ZVVClient.getCountdown(i)) {
        Tram7 = ZVVClient.getCountdown(i);
        publish(SEVEN_IDX,0,Tram7);
      }
     }
     else {
      displayChar(ZVVClient.getCountdown(i),4);
      if (Tram9 != ZVVClient.getCountdown(i)) {
        Tram9 = ZVVClient.getCountdown(i);
        publish(NINE_IDX,0,Tram9);
      }
     }
     last_tram = tint;
     trams_found++;
     }
   } while (trams_found < 2 && i < Number_Journeys -1);
   i = -1;
   last_tram = 0;
   trams_found = 0;
        
 readyForZVVUpdate = false;  
}


void depart(int walking_time, char* Dest, char* Num){
  String dep_time = "";
  Number_Journeys = atoi(Num);
  dep_h = hour();
  dep_m = minute()+walking_time;
  if(dep_m >= 60){
    dep_m =  dep_m % 60;
    dep_h++;
  }
  if (hour() == 24 && dep_h == 24) dep_h = 0;
  if (dep_h < 10) dep_time = "0";
  dep_time += String(dep_h) + ":";
  if (dep_m < 10) dep_time += "0";
  dep_time += String(dep_m);
  Serial.println(dep_time);
  url = Dest;
  url += Num;
  url += STARTS;
  url += dep_time;
}


void displayChar(int departure_time, int display_Matrix_Number){
  byte dot = B00000000;
  int char_val;
  String c = String(departure_time);
  lc.clearDisplay(display_Matrix_Number);
  int str_len = c.length() + 1; 
   if (str_len == 1 || str_len > 3) {
    c="//";
    str_len = 3;
   }
  // Prepare the character array (the buffer) 
  char char_array[str_len];
  // Copy it over 
  c.toCharArray(char_array, str_len);
  if (str_len == 3){ //swap digits
    char tt = char_array [0];
    char_array[0] = char_array [1];
    char_array[1]= tt; 
   }
  if(IR_train) dot = B00000001;
  for (int i=str_len-2;i>-1;i--){
  if (char_array[i] < 47) return;
    //Serial.print("C: ");
    char_val = char_array[i]-47;
    //Serial.println(char_val);
    for (int j=0;j<4;j++){
      lc.setRow(display_Matrix_Number,4-4*i+j,(CH[4*char_val+j]) | dot); // add a dot to indicate the type 1 train (IR)
      IR_train = false;
      dot = B00000000;
     }
   }
  yield();
}


void displayTime() {
  lc.setChar(2,0,minute()%10,false); //Display minutes
  lc.setChar(2,1,minute()/10,false);
  lc.setChar(2,2,hour()%10,true);
  if (hour() > 9) lc.setChar(2,3,hour()/10,false);
  else lc.setChar(2,3,' ',false);
}


void setReadyForClockUpdate() {
  SyncNecessary = true;  
}

void setReadyForZVVUpdate() {
  readyForZVVUpdate = true;  
}

void setReadyForWeatherUpdate() {
  readyForWeatherUpdate = true;  
}


void updateWeather() {
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_STATION_ID);
  if (hour() == 1 && minute() < 12) readyForSunsetUpdate = true;
  displayTemp();
  displayWeather();
  readyForWeatherUpdate = false;
}


void displayTemp() {
  String temp = wunderground.getCurrentTemp();
  Serial.println(temp);
  lc.setRow(2,4,B01100011); // degree sign on segment 4
  for (int i = 5;i < 8;i++){
    lc.setChar(2,i,' ',false); 
  }
  int str_len = temp.length();
  Serial.println(str_len);
  int segment = 4;
  for (int i = str_len-3;i>-1;i--){// - x020
    segment++;
    lc.setChar(2,segment,temp[i],false);
  }
}


void displayWeather() {
  t0.setText(wunderground.getWeatherText().c_str());
  t1.setText(wunderground.getWindSpeed().c_str());
  t2.setText(wunderground.getFeelsLike().c_str());
  t3.setText(wunderground.getHumidity().substring(0,13).c_str());
  String modified_IconText = "";
  Serial.print("sunrise: ");
  Serial.println(wunderground.getSunriseTime());
  Serial.print("sunset: ");
  Serial.println(wunderground.getSunsetTime());
  if (!isDay(hour(),wunderground.getSunriseTime().toInt(),wunderground.getSunsetTime().toInt())) {
    modified_IconText = "nt_" + wunderground.getTodayIconText();
  }
  else modified_IconText = wunderground.getTodayIconText();
  p1.setPic(wunderground.getMeteoconIcon(modified_IconText));
  yield();
}


void setReadyForForecastUpdate() {
  readyForForecastUpdate = true;  
}


void updateForecast() {
 wunderground.updateHourlyForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
 displayForecast();
  readyForForecastUpdate = false;
}


void displayForecast() {
  for (int i = 1;i < 9; i += 2) {
    String modified_IconText = "";
    String modified_hour="";
    if (!isDay(wunderground.getHourlyForecastHour(i).toInt(),wunderground.getSunriseTime().toInt(),wunderground.getSunsetTime().toInt())) {
      modified_IconText = "nt_" + wunderground.getHourlyForecastIcon(i);
    }
    else modified_IconText = wunderground.getHourlyForecastIcon(i);
    if (wunderground.getHourlyForecastHour(i).toInt() < 10) modified_hour += "0";
    modified_hour += wunderground.getHourlyForecastHour(i);
    modified_hour += ":00";
    Serial.print(i);
    Serial.print(modified_IconText);
    Serial.println(modified_hour);
    switch (i) {
    case 1: {
      p2.setPic(26 + wunderground.getMeteoconIcon(modified_IconText));
      t4.setText(modified_hour.c_str()); 
      }
      break;
    case 3: {
      p3.setPic(26 + wunderground.getMeteoconIcon(modified_IconText));
      t5.setText(modified_hour.c_str()); 
      }
      break;
    case 5: {
      p4.setPic(26 + wunderground.getMeteoconIcon(modified_IconText));
      t6.setText(modified_hour.c_str()); 
      }
      break;
    case 7: {
      p5.setPic(26 + wunderground.getMeteoconIcon(modified_IconText));
      t7.setText(modified_hour.c_str()); 
      }
      break;
    }
  }
}


void ISRwatchdog() {
  WDTCount++;
  if (WDTCount == 25) ESP.reset();
}


void light_off() {
  yield();
  for (int i = 0;i < maxInUse;i++){
    lc.shutdown(i,true);
    wait(100);
   }
  nexSerial.print("dim=0");
  nexSerial.write(0xff);
  nexSerial.write(0xff);
  nexSerial.write(0xff);
  lights_on = false;
  }


void light_on() {
  for (int i = 0;i < maxInUse;i++){
    lc.shutdown(i,false);
    wait(100);
  }
  nexSerial.print("dim=80");
  nexSerial.write(0xff);
  nexSerial.write(0xff);
  nexSerial.write(0xff);
  lights_on = true;
  }


void updateSunset() {
  wunderground.updateAstronomy(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  readyForSunsetUpdate = false;  
}

bool isDay (int current, int sun_rise, int sun_set) {
return (current < 23 && current > 4 && current < sun_set && current > sun_rise);
}


//{"idx":29,"nvalue":0,"svalue":"123"}
void publish(int idx, int nvalue, int svalue){ 
  char output[130];
  snprintf_P(output, sizeof(output), PSTR("{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"%d\"}"),idx,nvalue,svalue);
  client.publish(OutTopic,output);
  String log_output;
  log_output = output;
  syslog.logf(LOG_INFO, "%s", log_output.c_str());
}


void Publish_Status() {
  publish(ZVV_IDX,lights_on,0);
  old_lights_on = lights_on; 
}


void wait (int ms) {
  for(long i = 0;i <= ms * 30000; i++) asm ( "nop \n" ); //80kHz Wemos D1
  ArduinoOTA.handle();
  yield();
  WDTCount = 0;
}

