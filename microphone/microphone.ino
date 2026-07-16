#include <PDM.h>

short sampleBuffer[256];
volatile int samplesRead;

const int SOUND_THRESHOLD = 500; 

void onPDMdata();

void setup() {
  Serial.begin(115200);
  while (!Serial);

  PDM.onReceive(onPDMdata);

  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to init microphone");
    while (1);
  }
}

void loop() {
  if (samplesRead > 0) {
    int maxAmplitude = 0;

    for (int i = 0; i < samplesRead; i++) {
      int absoluteValue = abs(sampleBuffer[i]);
      if (absoluteValue > maxAmplitude) {
        maxAmplitude = absoluteValue;
      }
    }

    Serial.print("Volume_Level:");
    Serial.print(maxAmplitude);
    
    if (maxAmplitude > SOUND_THRESHOLD) {
      Serial.print("   [SOUND DETECTED!]");
    }
    
    Serial.println();

    samplesRead = 0;
  }

  delay(500);
}

void onPDMdata() {
  int bytesAvailable = PDM.available();
  
  PDM.read(sampleBuffer, bytesAvailable);
  
  samplesRead = bytesAvailable / 2;
}