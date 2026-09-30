#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#define PIN_COIN 34#define PIN_BTN_U#define PIN_BTN_DOWN 33
#define PIN_BTN_OK 25
#define PIN_BUZZER 26
WebServer server(80);
volatile int coinCount = 0;
int credit = 0;
void IRAM_ATTR coinISR() { coinCount++; }
bool btnUp() { return digitalRead(PIN_BTN_UP) == LOW; }
bool btnDown() { return digitalRead(PIN_BTN_DOWN) == LOW; }
bool btnSelect() { return digitalRead(PIN_BTN_OK) == LOW; }
bool btnDownLong() { if (digitalRead(PIN_BTN_DOWN) == LOW) { delay(600); return digitalRead(PIN_BTN_DOWN) == LOW; } return false; }
void buzz(int ms) { digitalWrite(PIN_BUZZER, HIGH); delay(ms); digitalWrite(PIN_BUZZER, LOW); }
void setup() {
  Serial.begin(115200);
  pinMode(PIN_COIN, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_OK, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_COIN), coinISR, FALLING);
  WiFi.softAP("Tambay-Vendo", "12345678");
  server.on("/", []() { server.send(200, "text/plain", "OK"); });
  server.begin();
  buzz(200);
}
void loop() {
  server.handleClient();
  if (coinCount > 0) {
    noInterrupts();
   coinCount = 0;
   interrupts();
   credit++;
    buzz(100);
  }
}

