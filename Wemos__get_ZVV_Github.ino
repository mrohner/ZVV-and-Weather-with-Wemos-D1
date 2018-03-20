/*Pin assignments:
 * Wemos
D0 to TX Nextion
D1 to Base of 2N2222 over a 1K resistor (a switch for LEDs to highlight tram and bus numbers)
D2 to OUT PIR sensor (control if lights on when movement detected)
D4 to RX Nextion
D5 to Pin 2 of SN74HCT125
D7 to Pin 5 of SN74HCT125
D8 to Pin 9 of SN74HCT125
*/

#include "WundergroundClient.h"
#include <Ticker.h>
#include <JsonListener.h>
#include "TimeClient.h"
#include <LedControl.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "ZVV.h"
#include "Nextion.h"
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

const unsigned char CH[] = { //characters to be displayed on Matrix
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
const char* SSID = "xxxxxxx"; 
const char* PASSWORD = "xxxxxxx";
int downcount = 0;

// ZVV settings
ZVVClient ZVVClient;
String url;
const char* STARTS = "&boardType=dep&start=1&tpl=stbResult2json&time=";
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
const int Walking_time_Riedgraben = 6;
const int Walking_time_Waldgarten = 7;
const int Riedgraben_Oerlikon = 8; //commute time
bool IR_train = false; // Inter Regio
int cur_h, cur_m, connection;
int dep_h,dep_m;
int countdown_figure = 0;

// Timing
TimeClient TimeClient(1);
bool readyForTimeUpdate = true;
bool readyForZVVUpdate = true;
bool readyForWeatherUpdate = true;
bool readyForForecastUpdate = true;
bool readyForSunsetUpdate = true;
Ticker Timeticker;
Ticker ZVVticker;
Ticker Weatherticker;
Ticker Forecastticker;
Ticker SecondTick; // to stabilize execution
volatile int WDTCount = 0;

// Weather
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "xxxxxxx";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_STATION_ID = "IZRICH31"; // Change to a station close to you
const String WUNDERGROUND_COUNTRY = "CH";
const String WUNDERGROUND_CITY = "Zurich";
WundergroundClient wunderground(IS_METRIC);

// Light
const int LIGHTPIN = 5;
bool lights_on = false;

// PIR sensor
const int PIRPin = 4;
volatile bool move_on = false;


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
  Serial.println();
  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(SSID);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println(F("WiFi connected"));  
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
  delay(5000);
  Timeticker.attach(599, setReadyForTimeUpdate);        // Update every 10 minutes
  ZVVticker.attach(30, setReadyForZVVUpdate);           // Update every 30 secs
  Weatherticker.attach(481, setReadyForWeatherUpdate);  // Update every 8 minutes
  Forecastticker.attach(1801, setReadyForForecastUpdate); // Update every 30 minutes
  SecondTick.attach(1,ISRwatchdog);

// Lights
pinMode (LIGHTPIN,OUTPUT);
digitalWrite(LIGHTPIN, LOW); 

// PIR sensor
pinMode (PIRPin,INPUT); 

//OTA
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Wemos33-ZVV");
  // No authentication by default
  ArduinoOTA.setPassword((const char *)"xxx");
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


WiFiClient client;
const int httpPort = 80;


void loop (){
  nexLoop(nex_listen_list);
  if (readyForTimeUpdate) updateTime();
  ArduinoOTA.handle();
  lightcontrol();
  WDTCount = 0;
  if (readyForSunsetUpdate) updateSunset();
  ArduinoOTA.handle();
  if (readyForZVVUpdate) updateZVV();
  WDTCount = 0;
  ArduinoOTA.handle();
  if (readyForWeatherUpdate) updateWeather();
  WDTCount = 0;
  lightcontrol();
  ArduinoOTA.handle();
  if (readyForForecastUpdate) updateForecast();
  lightcontrol();
  WDTCount = 0;
}


void updateZVV() {
 int trams_found = 0;
 int last_tram = 0;
 int tint = 0;
 int i = -1;
 Serial.println(F("------------------------------------------------------------------------------"));
 // Departure Waldgarten Bus
 depart(Walking_time_Waldgarten, Dest3, Dest3Num);
 Serial.println("Bus: 75");
 ZVVClient.doUpdate(url);
 countdown_figure = ZVVClient.getCountdown(0);
 Serial.print(F("countdown: "));
 Serial.println(countdown_figure);
 Serial.println();
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
     if (tint == 7) displayChar(ZVVClient.getCountdown(i),5);
     else displayChar(ZVVClient.getCountdown(i),4);
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
  displayTime();
  lightcontrol();
  ArduinoOTA.handle();
  String dep_time = "";
  Number_Journeys = atoi(Num);
  dep_h=(TimeClient.getHours().toInt());
  dep_m=(TimeClient.getMinutes().toInt())+walking_time;
  if(dep_m >= 60){
    dep_m =  dep_m % 60;
    dep_h++;
  }
  if ((TimeClient.getHours().toInt()) == 24 && dep_h == 24) dep_h = 0;
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


void setReadyForTimeUpdate() {
  readyForTimeUpdate = true;
}


void updateTime() {
  TimeClient.updateTime();
  if (TimeClient.getHours().toInt() == 1 && TimeClient.getMinutes().toInt() < 12) readyForSunsetUpdate = true;
  readyForTimeUpdate = false;
}


void displayTime() {
  lc.setChar(2,0,TimeClient.getMinutes()[1],false); //Display minutes
  lc.setChar(2,1,TimeClient.getMinutes()[0],false);
  lc.setChar(2,2,TimeClient.getHours()[1],true);
  if ((TimeClient.getHours().toInt()) > 9) lc.setChar(2,3,TimeClient.getHours()[0],false);
  else lc.setChar(2,3,' ',false);
}


void setReadyForZVVUpdate() {
  readyForZVVUpdate = true;  
}


void setReadyForWeatherUpdate() {
  readyForWeatherUpdate = true;  
}


void updateWeather() {
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_STATION_ID);
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
  t3.setText(wunderground.getHumidity().c_str());
  String modified_IconText = "";
  if (!isDay(TimeClient.getHours().toInt(),wunderground.getSunriseTime().toInt(),wunderground.getSunsetTime().toInt())) {
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
  move_on = digitalRead(PIRPin);
}


void lightcontrol() {
  yield();
  /*
  Serial.println (move_on);
  if (TimeClient.getHours().toInt() < 6 && TimeClient.getHours().toInt() > 0 && lights_on || !move_on){
    digitalWrite(LIGHTPIN, LOW);
    for (int i = 0;i < maxInUse;i++){
      lc.shutdown(i,true);
      delay(100);
    }
    String dim = "dim=0";  
    nexSerial.print("dim=0");
    nexSerial.write(0xff);
    nexSerial.write(0xff);
    nexSerial.write(0xff);
    lights_on = false;
  }
  else if (TimeClient.getHours().toInt() > 5 && lights_on == false && move_on) {
    digitalWrite(LIGHTPIN, HIGH);
    for (int i = 0;i < maxInUse;i++){
      lc.shutdown(i,false);
      delay(100);
    }
    nexSerial.print("dim=80");
    nexSerial.write(0xff);
    nexSerial.write(0xff);
    nexSerial.write(0xff);
    lights_on = true;
  }
  */
}

void updateSunset() {
  wunderground.updateAstronomy(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  readyForSunsetUpdate = false;  
}

bool isDay (int current, int sun_rise, int sun_set) {
return (current < 23 && current > 4 && current < sun_set && current > sun_rise);
}

