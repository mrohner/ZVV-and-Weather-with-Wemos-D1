
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "ZVV.h"

ZVVClient::ZVVClient() {
}

void ZVVClient::doUpdate(String url) {
  JsonStreamingParser parser;
  parser.setListener(this);
  WiFiClient client;
  int downcount = 0;
  const int httpPort = 80;
  if (!client.connect("online.fahrplan.zvv.ch", httpPort)) {
    Serial.println("connection failed");
    Serial.println("wait 3 sec...");
    downcount++;
    if (downcount > 3) {
      downcount = 0;
      ESP.restart();
      }  
    delay(3000);
    return;
  }
  Serial.print("Requesting URL: ");
  Serial.println(url);
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: online.fahrplan.zvv.ch\r\n" +
               "Connection: close\r\n\r\n");
  int retryCounter = 0;
  while(!client.available()) {
    delay(1000);
    retryCounter++;
    if (retryCounter > 10) {
      ESP.restart();
    }
  }
  int pos = 0;
  boolean isBody = false;
  char c;
  int size = 0;
  client.setNoDelay(false);
  while(client.connected()) {
    while((size = client.available()) > 0) {
      c = client.read();
      if (c == '{' || c == '[') {
        isBody = true;
      }
      if (isBody) {
        parser.parse(c);
      }
    }
  }
}

void ZVVClient::whitespace(char c) {
  //Serial.println("whitespace");
}

void ZVVClient::startDocument() {
  //Serial.println("start document");
  count = -1;
}

void ZVVClient::key(String key) {
currentKey = String(key);
}

void ZVVClient::value(String value) {
  //Serial.print(currentParent[hierarchy_level]);
  //Serial.print("   ");
  //Serial.println(currentKey);
    if (currentParent[hierarchy_level] == "product") {  // Has a Parent key and 2 sub-keys
      if (currentKey == "type") {
        count++;
        //Serial.println(count);
        ZVVReadings[count].type = value.toInt();
        //Serial.println(value.toInt());
        }
      if (currentKey == "line") {
        ZVVReadings[count].line = value;
        //Serial.println(value);
        }
      if (currentKey == "direction") {
        ZVVReadings[count].direction_to = value;
        }
    }
    if (currentParent[hierarchy_level] == "mainLocation" && currentKey == "countdown") {
      ZVVReadings[count].a_countdown  = value.toInt();
      //Serial.println(value.toInt());
      }
      if (currentParent[hierarchy_level-1] == "mainLocation" && currentParent[hierarchy_level] == "realTime" && currentKey == "countdown") { 
      ZVVReadings[count].countdown = value.toInt();
        if (ZVVReadings[count].countdown == 0) ZVVReadings[count].countdown = ZVVReadings[count].a_countdown;
      //Serial.println(ZVVReadings[count].countdown);
    }
 }

void ZVVClient::endArray() {
}


void ZVVClient::startObject() {
  hierarchy_level++;
  currentParent[hierarchy_level] = currentKey;
}

void ZVVClient::endObject() {
  hierarchy_level--;
}

void ZVVClient::endDocument() {
sortZVVreadings();
  for (int i = 0; i < count+1;i++){
    Serial.print(F("ZVV "));
    Serial.print(ZVVReadings[i].type);
    Serial.print(F("   "));
    Serial.print(ZVVReadings[i].line);
    Serial.print(F("   "));
    Serial.print(ZVVReadings[i].a_countdown);
    Serial.print(F("   "));
    Serial.print(ZVVReadings[i].countdown);
    Serial.print(F("   "));
    Serial.println(ZVVReadings[i].direction_to);
    }
}

void ZVVClient::startArray() {
}


void ZVVClient::sortZVVreadings()
{
    bool swapped;
    ZVVReading temp;

    do
    {
        swapped = false;
        for (byte i = 0; i != count; ++i)
        {
            if (ZVVReadings[i].countdown > ZVVReadings[i+1].countdown)
            {
                temp = ZVVReadings[i];
                ZVVReadings[i] = ZVVReadings[i + 1];
                ZVVReadings[i + 1] = temp;
                swapped = true;
            }
        }
    } while (swapped);
}


int ZVVClient::getType(int i) {
  return ZVVReadings[i].type;
}
String ZVVClient::getLine(int i) {
  return ZVVReadings[i].line;
}
int ZVVClient::getCountdown(int i) {
  return ZVVReadings[i].countdown;
}
String ZVVClient::getDirection(int i) {
  return ZVVReadings[i].direction_to;
}

