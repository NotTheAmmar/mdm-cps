#include <Arduino_APDS9960.h>

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!APDS.begin()) {
    Serial.println("Failed to init APDS");
    while(1);
  }

  APDS.setGestureSensitivity(80);

  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);

  Serial.println("Detecting gestures ...");
}
void loop() {
  if (APDS.gestureAvailable()) {
    int gesture = APDS.readGesture();

    switch (gesture) {
      case GESTURE_UP:
        digitalWrite(LEDR, !HIGH);
        digitalWrite(LEDB, !LOW);
        digitalWrite(LEDG, !LOW);
        break;
      case GESTURE_DOWN:
        digitalWrite(LEDR, !LOW);
        digitalWrite(LEDB, !LOW);
        digitalWrite(LEDG, !LOW);
        break;
      case GESTURE_LEFT:
        digitalWrite(LEDR, !LOW);
        digitalWrite(LEDG, !HIGH);
        digitalWrite(LEDB, !LOW);
        break;
      case GESTURE_RIGHT:
        digitalWrite(LEDR, !LOW);
        digitalWrite(LEDG, !LOW);
        digitalWrite(LEDB, !HIGH);
        break;
      default:
        Serial.println("Unable to detect gesture");
    }
  }
}
