const int analogPin = A6; 

void setup() {
  Serial.begin(9600);
  while(!Serial);

  analogReadResolution(12);
  pinMode(analogPin, INPUT);
}

void loop() {
  int pinValue = analogRead(analogPin);  
  Serial.print("Analog Value: ");
  Serial.println(pinValue);

  int ledBrightness = map(pinValue, 0, 4095, 0, 255);
  Serial.print("LED Brightness: ");
  Serial.println(ledBrightness);

  analogWrite(LED_BUILTIN, ledBrightness);

  delay(100);
}
