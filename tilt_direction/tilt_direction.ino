// Lab 7: IMU-based Tilt Direction Labels
// Reads accelerometer X and Y axes from the LSM9DS1 IMU.
// Classifies board tilt and prints the direction label on Serial Console.

#include <Arduino_LSM9DS1.h>

const float THRESHOLD = 0.5; // g — dead zone to avoid noise near flat position

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  Serial.println("Tilt the board to see direction labels...");
}

void loop() {
  float x, y, z;

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);

    if (x > THRESHOLD) {
      Serial.println("Right");
    } else if (x < -THRESHOLD) {
      Serial.println("Left");
    } else if (y > THRESHOLD) {
      Serial.println("Front");
    } else if (y < -THRESHOLD) {
      Serial.println("Back");
    }
    // No label printed when board is flat (within dead zone)

    delay(300); // Avoid flooding Serial Console
  }
}
