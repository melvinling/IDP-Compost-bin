// First ESP32 test: display a message in Serial Monitor every second.
// Serial Monitor baud rate: 115200.

void setup() {
  Serial.begin(115200);
}

void loop() {
  Serial.println("Compost bin ready");
  delay(1000);
}
