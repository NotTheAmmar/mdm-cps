#include <Arduino_LSM9DS1.h> // Change to <Arduino_BMI270_BMM150.h> if using Rev2 board
#include <HardwareBLESerial.h>

HardwareBLESerial &bleSerial = HardwareBLESerial::getInstance();

void setup() {
  Serial.begin(115200);

  // Initialize BLE Serial with a unique name
  while (!bleSerial.beginAndSetupBLE("Nano33_IMU_Node")) {
    Serial.println("Waiting for BLE initialization...");
    delay(500);
  }

  // Initialize IMU sensor
  while (!IMU.begin()) {
    Serial.println("Waiting for IMU initialization...");
    delay(500);
  }

  Serial.println("BLE IMU Serial Ready! Connect using nRF Connect.");
}

void loop() {
  // Always poll BLE to maintain connection stack
  bleSerial.poll();

  float x, y, z;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);

    // Stream to USB Serial
    Serial.print(x); Serial.print('\t');
    Serial.print(y); Serial.print('\t');
    Serial.println(z);

    // Stream over BLE
    bleSerial.print(x); bleSerial.print('\t');
    bleSerial.print(y); bleSerial.print('\t');
    bleSerial.println(z);
  }

  delay(10); // Throttle sampling rate (~100Hz) to prevent radio buffer flooding
}