#include <HardwareBLESerial.h>

HardwareBLESerial &bleSerial = HardwareBLESerial::getInstance();

// Pin Definitions (Swapped LEDG/LEDB for Rev2/Lite hardware layout)
const int pinOrange = LED_BUILTIN;
const int pinRed    = LEDR;
const int pinGreen  = LEDB; 
const int pinBlue   = LEDG; 

// Helper function to turn off all LEDs
void turnAllOff() {
  digitalWrite(pinOrange, LOW);   // Orange LED: Active HIGH (LOW = OFF)
  digitalWrite(pinRed, HIGH);     // RGB Pins: Active LOW (HIGH = OFF)
  digitalWrite(pinGreen, HIGH);
  digitalWrite(pinBlue, HIGH);
}

void setup() {
  Serial.begin(115200);

  // Configure Pin Modes
  pinMode(pinOrange, OUTPUT);
  pinMode(pinRed, OUTPUT);
  pinMode(pinGreen, OUTPUT);
  pinMode(pinBlue, OUTPUT);

  // Ensure everything starts turned off
  turnAllOff();

  // Initialize BLE Serial
  while (!bleSerial.beginAndSetupBLE("Nano33_RGB_Node")) {
    Serial.println("Waiting for BLE initialization...");
    delay(500);
  }

  Serial.println("Multi-Color BLE Controller Active! Connect via nRF Connect.");
}

void loop() {
  // Keep the BLE connection stack active
  bleSerial.poll();

  if (bleSerial.available()) {
    char cmd = bleSerial.read();

    switch (cmd) {
      case 'o': // Orange (Built-in)
        turnAllOff();
        digitalWrite(pinOrange, HIGH);
        bleSerial.println("Active LED: Orange");
        break;

      case 'r': // Red
        turnAllOff();
        digitalWrite(pinRed, LOW);
        bleSerial.println("Active LED: Red");
        break;

      case 'g': // Green
        turnAllOff();
        digitalWrite(pinGreen, LOW);
        bleSerial.println("Active LED: Green");
        break;

      case 'b': // Blue
        turnAllOff();
        digitalWrite(pinBlue, LOW);
        bleSerial.println("Active LED: Blue");
        break;

      case 'y': // Yellow (Red + Green)
        turnAllOff();
        digitalWrite(pinRed, LOW);
        digitalWrite(pinGreen, LOW);
        bleSerial.println("Active LED: Yellow");
        break;

      case 'p': // Purple (Red + Blue)
        turnAllOff();
        digitalWrite(pinRed, LOW);
        digitalWrite(pinBlue, LOW);
        bleSerial.println("Active LED: Purple");
        break;

      case 'c': // Cyan (Green + Blue)
        turnAllOff();
        digitalWrite(pinGreen, LOW);
        digitalWrite(pinBlue, LOW);
        bleSerial.println("Active LED: Cyan");
        break;

      case 'w': // White (Red + Green + Blue)
        turnAllOff();
        digitalWrite(pinRed, LOW);
        digitalWrite(pinGreen, LOW);
        digitalWrite(pinBlue, LOW);
        bleSerial.println("Active LED: White");
        break;

      case 'x': // Turn All OFF
      case '0':
        turnAllOff();
        bleSerial.println("Active LED: ALL OFF");
        break;

      default:
        if (cmd != '\n' && cmd != '\r') {
          bleSerial.print("Unknown Command: ");
          bleSerial.println(cmd);
        }
        break;
    }
  }
}