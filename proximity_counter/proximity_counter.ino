// Lab 17: Bare-Metal Timer as Counter with Proximity Sensor
// Uses the APDS9960 proximity sensor as a pulse source.
// Counts rising-edge crossings of the proximity threshold (100).
// Blinks the built-in orange LED on each valid count.
// When the counter reaches 15, the Green LED turns on permanently.

#include <Arduino_APDS9960.h>

const int PROXIMITY_THRESHOLD = 100; // 0-255 scale; hand nearby > 100
const int TARGET_COUNT        = 15;

int  counter      = 0;
bool prevAbove    = false; // tracks previous state for edge detection
bool goalReached  = false;

void blinkBuiltIn() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(80);
  digitalWrite(LED_BUILTIN, LOW);
  delay(80);
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960!");
    while (1);
  }

  // Configure LEDs
  pinMode(LED_BUILTIN, OUTPUT); // Orange built-in LED
  pinMode(LEDR,        OUTPUT); // Red   (active-LOW)
  pinMode(LEDG,        OUTPUT); // Green (active-LOW)
  pinMode(LEDB,        OUTPUT); // Blue  (active-LOW)

  // Initial state: Red ON, others OFF
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(LEDR,        LOW);   // Red ON
  digitalWrite(LEDG,        HIGH);  // Green OFF
  digitalWrite(LEDB,        HIGH);  // Blue OFF

  Serial.println("Proximity Counter Ready.");
  Serial.print("Wave hand ");
  Serial.print(TARGET_COUNT);
  Serial.println(" times to light the Green LED!");
}

void loop() {
  if (goalReached) {
    // Nothing more to do — Green LED stays on
    return;
  }

  if (APDS.proximityAvailable()) {
    int proximity = APDS.readProximity(); // 0 = far, 255 = very close

    bool currentlyAbove = (proximity > PROXIMITY_THRESHOLD);

    // Rising edge: hand just entered the threshold zone
    if (currentlyAbove && !prevAbove) {
      counter++;

      Serial.print("Count: ");
      Serial.print(counter);
      Serial.print(" / ");
      Serial.println(TARGET_COUNT);

      blinkBuiltIn(); // Acknowledge the pulse

      if (counter >= TARGET_COUNT) {
        // Goal reached!
        goalReached = true;

        digitalWrite(LEDR,  HIGH); // Red OFF
        digitalWrite(LEDG,  LOW);  // Green ON
        digitalWrite(LEDB,  HIGH); // Blue OFF

        Serial.println("=== TARGET REACHED! Green LED ON ===");
      }
    }

    prevAbove = currentlyAbove;
  }

  delay(20); // ~50 Hz poll rate — fast enough to catch hand movements
}
