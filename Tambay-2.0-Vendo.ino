BUZZER, HIGH);
  delay(ms);
  digitalWrite(PIN_BUZZER, LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_COIN, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_OK, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_COIN), coinISR, FALLING);
  WiFi.softAP("Tambay-Vendo", "12345678");
  server.on("/", []() {
    server.send(200, "text/plain", "OK");
  });
  server.begin();
  buzz(200);
}

void loop() {
  server.handleClient();
  if (coinCount > 0) {
    noInterrupts();
    coinCount = 0;
