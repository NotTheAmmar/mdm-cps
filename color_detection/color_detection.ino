#include <Arduino_APDS9960.h>

int r, g, b;

void setup() {
  Serial.begin(9600);
  while(!Serial);

  if (!APDS.begin()) {
    Serial.println("Failed to init APDS");
    while(1);
  }

  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
}

void loop() {
  while (APDS.colorAvailable()) {
    APDS.readColor(r, g, b);

    Serial.print("R: ");
    Serial.print(r);
    Serial.print(" G: ");
    Serial.print(g);
    Serial.print(" B: ");
    Serial.println(b);

    if (r > 200 && g > 255 && b > 255) {
      digitalWrite(LEDR, !HIGH);
      digitalWrite(LEDG, !HIGH);
      digitalWrite(LEDB, !HIGH);
    } else if (r > 150 && r > g && r > b) {
      digitalWrite(LEDR, !HIGH);
      digitalWrite(LEDG, !LOW);
      digitalWrite(LEDB, !LOW);
    } else if (g > 150 && g > r && g > b) {
      digitalWrite(LEDR, !LOW);
      digitalWrite(LEDG, !HIGH);
      digitalWrite(LEDB, !LOW);
    } else if (b > 150 && b > r && b > g) {
      digitalWrite(LEDR, !LOW);
      digitalWrite(LEDG, !LOW);
      digitalWrite(LEDB, !HIGH);
    } else {
      digitalWrite(LEDR, !LOW);
      digitalWrite(LEDG, !LOW);
      digitalWrite(LEDB, !LOW);
    }

    delay(10);
  }
}
