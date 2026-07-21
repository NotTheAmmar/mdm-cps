// Lab 15: Sound-Threshold based LED Control
// Captures audio from the on-board MP34DT05 PDM microphone.
// Green LED lights up when sound exceeds the threshold; Red LED stays on otherwise.

#include <PDM.h>

short sampleBuffer[256];
volatile int samplesRead;

// Tune this value for your environment (500 works well in a quiet room)
const int SOUND_THRESHOLD = 500;

void onPDMdata();

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Configure on-board RGB LED pins (active-LOW on Nano 33 BLE Sense)
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);

  // Start with Red LED on (quiet state)
  digitalWrite(LEDR, LOW);   // Red ON  (active-LOW)
  digitalWrite(LEDG, HIGH);  // Green OFF

  PDM.onReceive(onPDMdata);

  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to initialize microphone!");
    while (1);
  }

  Serial.println("Sound threshold LED control active.");
  Serial.print("Threshold: ");
  Serial.println(SOUND_THRESHOLD);
}

void loop() {
  if (samplesRead > 0) {
    // Compute peak amplitude of this audio block
    int maxAmplitude = 0;
    for (int i = 0; i < samplesRead; i++) {
      int val = abs(sampleBuffer[i]);
      if (val > maxAmplitude) {
        maxAmplitude = val;
      }
    }

    Serial.print("Sound Level: ");
    Serial.print(maxAmplitude);

    if (maxAmplitude > SOUND_THRESHOLD) {
      // Sound detected — turn Green ON, Red OFF
      digitalWrite(LEDG, LOW);   // Green ON
      digitalWrite(LEDR, HIGH);  // Red OFF
      Serial.println("  [LOUD — Green LED ON]");
    } else {
      // Quiet — turn Red ON, Green OFF
      digitalWrite(LEDR, LOW);   // Red ON
      digitalWrite(LEDG, HIGH);  // Green OFF
      Serial.println("  [Quiet — Red LED ON]");
    }

    samplesRead = 0;
  }
}

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}
