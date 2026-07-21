/**
 * Player1_Nano33.ino  —  SERIAL VERSION
 * ─────────────────────────────────────────────────────────────────────────────
 * Hardware: Arduino Nano 33 BLE Sense (Rev 1 / Rev 2 / Lite)
 *
 * Board Variants:
 *   Rev 1 / Lite : LSM9DS1  →  uses Arduino_LSM9DS1.h        (default)
 *   Rev 2        : BMI270   →  uncomment #define USE_REV2_IMU
 *
 * Serial Protocol  (115200 baud, LF-terminated):
 *   OUT  "DEVICE:P1\n"         sent at startup — bridge uses this to identify
 *   OUT  "D:pitch,roll\n"      IMU data at 25 Hz  (e.g.  "D:12.34,-5.67")
 *   OUT  "STATUS:CALIBRATED\n" after a successful zero-point calibration
 *   IN   'H'                   trigger haptic + buzzer hit feedback
 *
 * Pin Map (bare-metal nRF52840):
 *   D2  → P0.11  : Calibration button  (INPUT, pull-up, active-LOW)
 *   D3  → P0.12  : Vibration motor     (OUTPUT, 80 ms pulse)
 *   D4  → P1.15  : Active piezo buzzer (OUTPUT, 40 ms pulse)  ← Port 1!
 * ─────────────────────────────────────────────────────────────────────────────
 */

// ── Board Variant ─────────────────────────────────────────────────────────────
// Uncomment ONLY for Rev 2 boards (BMI270 chip)
// #define USE_REV2_IMU

#ifdef USE_REV2_IMU
  #include <Arduino_BMI270_BMM150.h>
#else
  #include <Arduino_LSM9DS1.h>
#endif

#include <math.h>

// ── Peripheral Feature Flags ──────────────────────────────────────────────────
// Comment out any line to disable that peripheral (safe to run bare-board)
#define ENABLE_HAPTIC_MOTOR   // D3 (P0.12) — vibration motor
#define ENABLE_BUZZER         // D4 (P1.15) — active piezo buzzer
#define ENABLE_CALIB_BUTTON   // D2 (P0.11) — zero-point calibration button

// ── Player Identity ───────────────────────────────────────────────────────────
#define PLAYER_ID "P1"

// ── nRF52840 GPIO Bit Positions ───────────────────────────────────────────────
#define BTN_PIN_P0_BIT    11u   // P0.11  D2
#define MOTOR_PIN_P0_BIT  12u   // P0.12  D3
#define BUZZER_PIN_P1_BIT 15u   // P1.15  D4  (Port 1!)

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
#define IMU_LOOP_INTERVAL_MS 40u   // 25 Hz — safe packet rate for serial

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
  // D2 (P0.11) — input with internal pull-up; stable HIGH when nothing connected
  NRF_P0->PIN_CNF[BTN_PIN_P0_BIT] =
      (GPIO_PIN_CNF_DIR_Input       << GPIO_PIN_CNF_DIR_Pos)   |
      (GPIO_PIN_CNF_INPUT_Connect   << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Pullup     << GPIO_PIN_CNF_PULL_Pos)  |
      (GPIO_PIN_CNF_DRIVE_S0S1      << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled  << GPIO_PIN_CNF_SENSE_Pos);
#endif

#ifdef ENABLE_HAPTIC_MOTOR
  // D3 (P0.12) — push-pull output, start LOW
  NRF_P0->PIN_CNF[MOTOR_PIN_P0_BIT] =
      (GPIO_PIN_CNF_DIR_Output       << GPIO_PIN_CNF_DIR_Pos)   |
      (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled    << GPIO_PIN_CNF_PULL_Pos)  |
      (GPIO_PIN_CNF_DRIVE_S0S1       << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled   << GPIO_PIN_CNF_SENSE_Pos);
  MOTOR_OFF();
#endif

#ifdef ENABLE_BUZZER
  // D4 (P1.15) — push-pull output on PORT 1, start LOW
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

  // Wait up to 2 s for USB serial host to open the port
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 2000);

  configureGPIO();

  if (!IMU.begin()) {
    Serial.println("ERROR:IMU_INIT_FAILED");
    while (1);
  }

  // Announce identity — bridge uses this to map port → player
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
      // Bridge is requesting identity (sent when board was already running
      // and missed the startup DEVICE: message)
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

      // "D:pitch,roll\n"
      Serial.print("D:");
      Serial.print(pitch, 2);
      Serial.print(",");
      Serial.println(roll, 2);
    }
  }
}
