#include <Arduino_LSM9DS1.h>
#include <Arduino_APDS9960.h> 

float ax, ay, az;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!IMU.begin()) {
    Serial.println("Failed to init IMU");
    while (1);
  }

  if (!APDS.begin()) {
    Serial.println("Failed to init Proximity Sensor");
    while (1);
  }
}

void loop() {
  if (IMU.accelerationAvailable() && APDS.proximityAvailable()) {
    IMU.readAcceleration(ax, ay, az);
    
    int proximity = APDS.readProximity(); 
    int fireLaser = (proximity < 200) ? 1 : 0; 

    // Calculate clean fundamental tilt vectors
    float pitchVal = atan2(ax, sqrt(ay * ay + az * az)); 
    float rollVal  = atan2(ay, az);

    // Send the raw orientation packets out immediately
    Serial.print("{\"pitch\":");
    Serial.print(pitchVal);
    Serial.print(",\"roll\":");
    Serial.print(rollVal);
    Serial.print(",\"fire\":");
    Serial.print(fireLaser);
    Serial.println("}");
  }
  
  delay(15); 
}