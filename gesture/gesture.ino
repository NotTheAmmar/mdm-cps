#include <Arduino_APDS9960.h>

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!APDS.begin()) {
    Serial.println("Failed to init APDS");
    while(1);
  }

  APDS.setGestureSensitivity(80);

  Serial.println("Detecting gestures ...");
}
void loop() {
  if (APDS.gestureAvailable()) {
    int gesture = APDS.readGesture();

    switch (gesture) {
      case GESTURE_UP:
        Serial.println("Detected UP gesture");
        break;
      case GESTURE_DOWN:
        Serial.println("Detected DOWN gesture");
        break;
      case GESTURE_LEFT:
        Serial.println("Detected LEFT gesture");
        break;
      case GESTURE_RIGHT:
        Serial.println("Detected RIGHT gesture");
        break;
      default:
        Serial.println("Unable to detect gesture");
    }
  }
}
