/**
 * Player2_Nano33.ino  —  SERIAL VERSION
 * ─────────────────────────────────────────────────────────────────────────────
 * Identical to Player1_Nano33.ino except PLAYER_ID = "P2".
 * See Player1_Nano33.ino for full documentation.
 * ─────────────────────────────────────────────────────────────────────────────
 */

// ── Board Variant ─────────────────────────────────────────────────────────────
// #define USE_REV2_IMU

#ifdef USE_REV2_IMU
  #include <Arduino_BMI270_BMM150.h>
#else
  #include <Arduino_LSM9DS1.h>
#endif

#include <math.h>

// ── Peripheral Feature Flags ──────────────────────────────────────────────────
#define ENABLE_HAPTIC_MOTOR
#define ENABLE_BUZZER
#define ENABLE_CALIB_BUTTON

// ── Player Identity ───────────────────────────────────────────────────────────
#define PLAYER_ID "P2"    // ← only difference from Player1

// ── nRF52840 GPIO Bit Positions ───────────────────────────────────────────────
#define BTN_PIN_P0_BIT    11u
#define MOTOR_PIN_P0_BIT  12u
#define BUZZER_PIN_P1_BIT 15u

// ── GPIO Macros ───────────────────────────────────────────────────────────────
#define MOTOR_ON()    NRF_P0->OUTSET = (1u << MOTOR_PIN_P0_BIT)
#define MOTOR_OFF()   NRF_P0->OUTCLR = (1u << MOTOR_PIN_P0_BIT)
#define BUZZER_ON()   NRF_P1->OUTSET = (1u << BUZZER_PIN_P1_BIT)
#define BUZZER_OFF()  NRF_P1->OUTCLR = (1u << BUZZER_PIN_P1_BIT)
#define BTN_PRESSED() (!(NRF_P0->IN & (1u << BTN_PIN_P0_BIT)))

// ── Timing ────────────────────────────────────────────────────────────────────
#define HAPTIC_DURATION_MS   80u
#define BUZZER_DURATION_MS   40u
#define BTN_DEBOUNCE_MS      50u
#define IMU_LOOP_INTERVAL_MS 40u   // 25 Hz

// ── Calibration ───────────────────────────────────────────────────────────────
float pitchOffset = 0.0f;
float rollOffset  = 0.0f;

// ── Actuator Timers ───────────────────────────────────────────────────────────
unsigned long hapticStartMs = 0;
unsigned long buzzerStartMs = 0;
bool hapticActive = false;
bool buzzerActive = false;

// ── Button Debounce ───────────────────────────────────────────────────────────
bool          btnLastState  = false;
unsigned long btnLastEdgeMs = 0;

// ── IMU Loop Timer ────────────────────────────────────────────────────────────
unsigned long lastImuMs = 0;


// ═════════════════════════════════════════════════════════════════════════════
void configureGPIO() {
#ifdef ENABLE_CALIB_BUTTON
  NRF_P0->PIN_CNF[BTN_PIN_P0_BIT] =
      (GPIO_PIN_CNF_DIR_Input       << GPIO_PIN_CNF_DIR_Pos)   |
      (GPIO_PIN_CNF_INPUT_Connect   << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Pullup     << GPIO_PIN_CNF_PULL_Pos)  |
      (GPIO_PIN_CNF_DRIVE_S0S1      << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled  << GPIO_PIN_CNF_SENSE_Pos);
#endif
#ifdef ENABLE_HAPTIC_MOTOR
  NRF_P0->PIN_CNF[MOTOR_PIN_P0_BIT] =
      (GPIO_PIN_CNF_DIR_Output       << GPIO_PIN_CNF_DIR_Pos)   |
      (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled    << GPIO_PIN_CNF_PULL_Pos)  |
      (GPIO_PIN_CNF_DRIVE_S0S1       << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled   << GPIO_PIN_CNF_SENSE_Pos);
  MOTOR_OFF();
#endif
#ifdef ENABLE_BUZZER
  NRF_P1->PIN_CNF[BUZZER_PIN_P1_BIT] =
      (GPIO_PIN_CNF_DIR_Output       << GPIO_PIN_CNF_DIR_Pos)   |
      (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled    << GPIO_PIN_CNF_PULL_Pos)  |
      (GPIO_PIN_CNF_DRIVE_S0S1       << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled   << GPIO_PIN_CNF_SENSE_Pos);
  BUZZER_OFF();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
void triggerHapticFeedback() {
  unsigned long now = millis();
#ifdef ENABLE_HAPTIC_MOTOR
  hapticStartMs = now; hapticActive = true; MOTOR_ON();
#endif
#ifdef ENABLE_BUZZER
  buzzerStartMs = now; buzzerActive = true; BUZZER_ON();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
void updateActuators() {
  unsigned long now = millis();
#ifdef ENABLE_HAPTIC_MOTOR
  if (hapticActive && (now - hapticStartMs) >= HAPTIC_DURATION_MS) {
    MOTOR_OFF(); hapticActive = false;
  }
#endif
#ifdef ENABLE_BUZZER
  if (buzzerActive && (now - buzzerStartMs) >= BUZZER_DURATION_MS) {
    BUZZER_OFF(); buzzerActive = false;
  }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
void handleCalibrationButton(float rawPitch, float rawRoll) {
#ifdef ENABLE_CALIB_BUTTON
  bool pressed = BTN_PRESSED();
  unsigned long now = millis();
  if (pressed && !btnLastState && (now - btnLastEdgeMs) > BTN_DEBOUNCE_MS) {
    pitchOffset   = rawPitch;
    rollOffset    = rawRoll;
    btnLastEdgeMs = now;
    Serial.println("STATUS:CALIBRATED");
  }
  btnLastState = pressed;
#endif
}

// ═════════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 2000);

  configureGPIO();

  if (!IMU.begin()) {
    Serial.println("ERROR:IMU_INIT_FAILED");
    while (1);
  }

  Serial.println("DEVICE:" PLAYER_ID);
  Serial.println("STATUS:READY");
}

// ═════════════════════════════════════════════════════════════════════════════
void loop() {
  // Handle inbound serial commands
  while (Serial.available()) {
    char cmd = (char)Serial.read();
    if (cmd == 'H') {
      triggerHapticFeedback();
    } else if (cmd == '?') {
      Serial.println("DEVICE:" PLAYER_ID);
    }
  }

  updateActuators();

  unsigned long now = millis();
  if ((now - lastImuMs) >= IMU_LOOP_INTERVAL_MS) {
    lastImuMs = now;

    if (IMU.accelerationAvailable()) {
      float ax, ay, az;
      IMU.readAcceleration(ax, ay, az);

      float rawPitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * (180.0f / PI);
      float rawRoll  = atan2f( ay, az)                        * (180.0f / PI);

      handleCalibrationButton(rawPitch, rawRoll);

      float pitch = rawPitch - pitchOffset;
      float roll  = rawRoll  - rollOffset;

      Serial.print("D:");
      Serial.print(pitch, 2);
      Serial.print(",");
      Serial.println(roll, 2);
    }
  }
}
