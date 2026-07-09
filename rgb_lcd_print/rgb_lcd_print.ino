#include <Wire.h>
#include "rgb_lcd.h"

rgb_lcd lcd;

void setup() {
  lcd.begin(16, 2);
  lcd.setRGB(0, 0, 255);

  lcd.setCursor(0, 0);
  lcd.print("Hello Ammar");

}

void loop() {

}
