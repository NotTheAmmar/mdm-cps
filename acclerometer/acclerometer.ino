#include <Arduino_LSM9DS1.h>

float x, y, z;

void setup() {
  Serial.begin(9600);
  while(!Serial);

  if (!IMU.begin()) {
    Serial.println("Failed to init IMU");
    while(1);
  }

  pinMode(LEDR, OUTPUT);
  pinMode(LEDB, OUTPUT);
}

void loop() {
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);

    if (y > x && y > z) {
      digitalWrite(LEDR, !HIGH);
      digitalWrite(LEDB, !LOW);
    } else if (y < x && y < z) {
      digitalWrite(LEDR, !LOW);
      digitalWrite(LEDB, !HIGH);
    } else if (z > x && z > y) {
      digitalWrite(LEDR, !LOW);
      digitalWrite(LEDB, !LOW);
    }
  }
}
