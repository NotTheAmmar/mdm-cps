#include <Arduino.h>

UART Serial2(digitalPinToPinName(12), digitalPinToPinName(11), NC, NC);

void setup() {
  Serial.begin(9600);
  while(!Serial);

  Serial2.begin(9600);

  Serial.println("UART Loopback Started");
}

void loop() {
  Serial2.print("Hello from D12");
  
  if (Serial2.available()) {
    Serial.print("Received on D11: ");
    Serial.println(Serial2.readString());
  }

  delay(2000);
}
