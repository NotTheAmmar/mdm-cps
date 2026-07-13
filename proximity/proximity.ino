#include <Arduino_APDS9960.h>

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!APDS.begin()) {
    Serial.println("Failed to init APDS");
    while(1);
  }

  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
}

void loop() {
  if (APDS.proximityAvailable()) {
    int proximity = APDS.readProximity();
    int ledBrightness = map(proximity, 0, 255, 255, 0);

    analogWrite(LEDR, ledBrightness);
    analogWrite(LEDG, ledBrightness);
    analogWrite(LEDB, ledBrightness);    
  }
}
