void setup() {
  Serial.begin(9600);
  while(!Serial);

  Serial.println("UART Started");
}

void loop() {
  if (Serial.available()) {
    Serial.print("Received: ");
    Serial.print(Serial.readString());
    Serial.print("\n");
  }

  delay(100);
}
