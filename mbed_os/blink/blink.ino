#include <Arduino.h>
#include <mbed.h>

mbed::DigitalOut myLed(digitalPinToPinName(LED_BUILTIN));

rtos::Thread myThread;

void blink_thread() {
  while (true) {
    myLed = !myLed;
    rtos::ThisThread::sleep_for(500); 
  }
}

void setup() {
  Serial.begin(9600);
 
  myThread.start(blink_thread);
}

void loop() {
  Serial.println("Main loop is still running!");
  delay(2000);
}