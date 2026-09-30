#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
WebServer s(80);
int c=0;
void IRAM_ATTR q(){c++;}
void setup(){
Serial.begin(115200);
pinMode(34,INPUT_PULLUP);
pinMode(26,OUTPUT);
attachInterrupt(34,q,FALLING);
WiFi.softAP("Tambay","12345678");
s.begin();
}
void loop(){
s.handleClient();
}
