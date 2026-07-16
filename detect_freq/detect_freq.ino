#include <PDM.h>
#include <math.h>

const int SAMPLE_RATE = 16000;
short sampleBuffer[256];
volatile int samplesRead;

const int NUM_FREQS = 3;
float targetFreqs[NUM_FREQS] = {1000.0, 4000.0, 8000.0};

const float DETECTION_THRESHOLD = 1500.0; 

float getFrequencyMagnitude(short* numSamples, int bufferSize, float targetFreq, float sampleRate);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  PDM.onReceive(onPDMdata);

  if (!PDM.begin(1, SAMPLE_RATE)) {
    Serial.println("Failed to init microphone");
    while (1);
  }
  
  Serial.println("Listening for 100 Hz, 500 Hz, and 1000 Hz...");
  for (int i = 0; i < NUM_FREQS; i++) {
    Serial.print(targetFreqs[i]);
    Serial.print(" Hz ");
  }
  Serial.println("");
}

void loop() {
  if (samplesRead > 0) {
    int currentBlockSize = samplesRead;

    for (int i = 0; i < NUM_FREQS; i++) {
      float freq = targetFreqs[i];
      
      float magnitude = getFrequencyMagnitude(sampleBuffer, currentBlockSize, freq, SAMPLE_RATE);

      if (magnitude > DETECTION_THRESHOLD) {
        Serial.print("-> DETECTED FREQ: ");
        if (freq >= 1000.0) {
          Serial.print(freq / 1000.0, 1);
          Serial.println(" kHz!");
        } else {
          Serial.print((int)freq);
          Serial.println(" Hz!");
        }
        
        delay(250); 
        break; 
      }
    }

    samplesRead = 0;
  }
}

float getFrequencyMagnitude(short* samples, int N, float targetFreq, float sampleRate) {
  int k = (int)(0.5 + ((float)N * targetFreq / sampleRate));
  float omega = (2.0 * M_PI * k) / (float)N;
  float cosine = cos(omega);
  float coeff = 2.0 * cosine;

  float q0 = 0;
  float q1 = 0;
  float q2 = 0;
  for (int i = 0; i < N; i++) {
    q0 = (float)samples[i] + (coeff * q1) - q2;
    q2 = q1;
    q1 = q0;
  }

  float magnitudeSquared = (q1 * q1) + (q2 * q2) - (coeff * q1 * q2);
  return sqrt(magnitudeSquared);
}

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}