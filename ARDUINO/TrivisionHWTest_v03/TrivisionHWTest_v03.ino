/*
 * TrivisionHWTest_v03.ino
 * ============================================================
 * Trivision Kinetic Sculpture — Hardware Verification Test
 * Version: v03
 * Date: 2026-02-23
 * ============================================================
 *
 * Each prism rotates BACKWARD 120° (1067 steps), pauses 10 sec,
 * and repeats forever. 0.5 RPM with gentle ramp. 5-sec stagger.
 *
 * Direction: BACKWARD (negative steps — opposite of v01/v02)
 *
 * Board: Arduino Mega 2560 with HCDC screw terminal shield
 * Motors: NEMA 17 (0.9° step) via TMC2209 at 1/8 microstepping
 * Steps/rev: 3200
 * 120° = 3200 / 3 = 1066.67 → 1067 steps
 * Max speed: 0.5 RPM = 26.67 steps/sec
 * Hall sensors: NOT WIRED — pins 2-13 untouched
 * ============================================================
 */

#include <AccelStepper.h>

// ============================================================
// CONFIGURATION
// ============================================================
#define NUM_MOTORS      12
#define STEPS_PER_REV   3200L
#define STEPS_120DEG    1067L       // 3200 / 3, rounded
#define STAGGER_MS      5000UL
#define PAUSE_MS        10000UL     // 10 seconds between moves

// 0.5 RPM = 26.67 steps/sec
static const float MAX_SPEED_SPS = 26.67;

// Gentle ramp: ~1.8 seconds to reach full speed
static const float ACCEL_SPS2 = 15.0;

// ============================================================
// PIN ASSIGNMENTS
// ============================================================
static const uint8_t MOTOR_PINS[NUM_MOTORS][2] = {
  {35, 18},   // Motor  1
  {36, 19},   // Motor  2
  {37, 20},   // Motor  3
  {38, 21},   // Motor  4
  {39, 22},   // Motor  5
  {40, 23},   // Motor  6
  {41, 24},   // Motor  7
  {42, 25},   // Motor  8
  {43, 26},   // Motor  9
  {44, 27},   // Motor 10
  {45, 28},   // Motor 11
  {46, 29}    // Motor 12
};

// ============================================================
// STATE MACHINE
// ============================================================
enum MotorState : uint8_t {
  STATE_WAITING,    // Waiting for stagger
  STATE_RUNNING,    // Rotating backward 120°
  STATE_PAUSING     // Holding 10 seconds
};

// ============================================================
// GLOBALS
// ============================================================
AccelStepper* motors[NUM_MOTORS];
MotorState     states[NUM_MOTORS];
unsigned long  pauseStartMs[NUM_MOTORS];
unsigned long  testStartMs;
uint16_t       cycleCount[NUM_MOTORS];

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println(F(""));
  Serial.println(F("============================================================"));
  Serial.println(F("  TrivisionHWTest_v03"));
  Serial.println(F("  Backward 120deg — Pause 10s — Repeat Forever"));
  Serial.println(F("  12 Motors / AccelStepper / Open Loop / No Hall Sensors"));
  Serial.println(F("============================================================"));
  Serial.println(F(""));
  Serial.print(F("  Speed: 0.5 RPM ("));
  Serial.print(MAX_SPEED_SPS, 2);
  Serial.println(F(" steps/sec)"));
  Serial.print(F("  Step size: 120deg = "));
  Serial.print(STEPS_120DEG);
  Serial.println(F(" steps BACKWARD"));
  Serial.print(F("  Pause: "));
  Serial.print(PAUSE_MS / 1000);
  Serial.println(F(" sec between moves"));
  Serial.print(F("  Stagger: "));
  Serial.print(STAGGER_MS / 1000);
  Serial.println(F(" sec between motor starts"));
  Serial.println(F(""));

  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    motors[i] = new AccelStepper(AccelStepper::DRIVER,
                                  MOTOR_PINS[i][0],
                                  MOTOR_PINS[i][1]);
    motors[i]->setMaxSpeed(MAX_SPEED_SPS);
    motors[i]->setAcceleration(ACCEL_SPS2);
    motors[i]->setCurrentPosition(0);
    states[i] = STATE_WAITING;
    pauseStartMs[i] = 0;
    cycleCount[i] = 0;

    Serial.print(F("  Motor "));
    if (i + 1 < 10) Serial.print(F(" "));
    Serial.print(i + 1);
    Serial.print(F(":  STEP="));
    Serial.print(MOTOR_PINS[i][0]);
    Serial.print(F("  DIR="));
    Serial.println(MOTOR_PINS[i][1]);
  }

  Serial.println(F(""));
  Serial.println(F("  Starting sequence..."));
  Serial.println(F(""));

  testStartMs = millis();
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - testStartMs;

  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    switch (states[i]) {

      case STATE_WAITING:
        if (elapsed >= (unsigned long)i * STAGGER_MS) {
          // First move: backward 120°
          motors[i]->moveTo(motors[i]->currentPosition() - STEPS_120DEG);
          states[i] = STATE_RUNNING;
          cycleCount[i]++;

          Serial.print(F(">>> Motor "));
          Serial.print(i + 1);
          Serial.print(F(" — Cycle "));
          Serial.print(cycleCount[i]);
          Serial.println(F(" — BACKWARD 120deg"));
        }
        break;

      case STATE_RUNNING:
        if (motors[i]->distanceToGo() == 0) {
          pauseStartMs[i] = now;
          states[i] = STATE_PAUSING;
        }
        break;

      case STATE_PAUSING:
        if (now - pauseStartMs[i] >= PAUSE_MS) {
          // Next backward 120°
          motors[i]->moveTo(motors[i]->currentPosition() - STEPS_120DEG);
          states[i] = STATE_RUNNING;
          cycleCount[i]++;

          Serial.print(F(">>> Motor "));
          Serial.print(i + 1);
          Serial.print(F(" — Cycle "));
          Serial.print(cycleCount[i]);
          Serial.println(F(" — BACKWARD 120deg"));
        }
        break;
    }
  }

  // Drive all active motors
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    if (states[i] == STATE_RUNNING) {
      motors[i]->run();
    }
  }
}
