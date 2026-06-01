/*
 * TrivisionHWTest_v01.ino
 * ============================================================
 * Trivision Kinetic Sculpture — Hardware Verification Test
 * Version: v01
 * Date: 2026-02-23
 * ============================================================
 *
 * Tests all 12 stepper motors sequentially with 5-second stagger.
 * Each motor sequence:
 *   1. Forward 2 rotations (6400 steps) at 1 RPM with gentle ramp
 *   2. Pause 15 seconds
 *   3. Backward 1 rotation (3200 steps) at 1 RPM with gentle ramp
 *
 * Board: Arduino Mega 2560 with HCDC screw terminal shield
 * Motors: NEMA 17 (0.9° step) via TMC2209 at 1/8 microstepping
 * Steps/rev: 3200
 * Max speed: 1 RPM = 53.33 steps/sec
 * Hall sensors: NOT WIRED — pins 2-13 untouched
 *
 * AccelStepper handles acceleration profiling for smooth ramp.
 * No Hall sensor logic. Pure open-loop verification.
 * ============================================================
 */

#include <AccelStepper.h>

// ============================================================
// CONFIGURATION
// ============================================================
#define NUM_MOTORS      12
#define STEPS_PER_REV   3200L       // 0.9° step × 1/8 microstepping
#define FWD_ROTATIONS   2           // Forward test: 2 full rotations
#define BWD_ROTATIONS   1           // Backward test: 1 full rotation
#define FWD_STEPS       (STEPS_PER_REV * FWD_ROTATIONS)  // 6400
#define BWD_STEPS       (STEPS_PER_REV * BWD_ROTATIONS)  // 3200
#define STAGGER_MS      5000UL      // 5 seconds between motor starts
#define PAUSE_MS        15000UL     // 15 seconds between fwd and bwd

// 1 RPM = 3200 steps / 60 sec = 53.33 steps/sec
static const float MAX_SPEED_SPS = 53.33;

// Gentle acceleration: ~1.8 seconds to reach full speed
// 53.33 / 30 = 1.78s ramp time
static const float ACCEL_SPS2 = 30.0;

// ============================================================
// PIN ASSIGNMENTS — confirmed wired to HCDC shield
// {STEP pin, DIR pin} — Hall sensors NOT connected
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
// MOTOR STATE MACHINE
// ============================================================
enum MotorState : uint8_t {
  STATE_WAITING,      // Waiting for stagger delay
  STATE_RUNNING_FWD,  // Moving forward
  STATE_PAUSING,      // Stationary pause between fwd/bwd
  STATE_RUNNING_BWD,  // Moving backward
  STATE_DONE          // Test complete for this motor
};

// ============================================================
// GLOBALS
// ============================================================
AccelStepper* motors[NUM_MOTORS];
MotorState     states[NUM_MOTORS];
unsigned long  pauseStartMs[NUM_MOTORS];
unsigned long  testStartMs;
uint8_t        motorsCompleted = 0;

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  // Banner
  Serial.println(F(""));
  Serial.println(F("============================================================"));
  Serial.println(F("  TrivisionHWTest_v01"));
  Serial.println(F("  Trivision Kinetic Sculpture — Hardware Verification"));
  Serial.println(F("  12 Motors / AccelStepper / Open Loop / No Hall Sensors"));
  Serial.println(F("============================================================"));
  Serial.println(F(""));
  Serial.println(F("Configuration:"));
  Serial.print(F("  Microstepping:  1/8 ("));
  Serial.print(STEPS_PER_REV);
  Serial.println(F(" steps/rev)"));
  Serial.print(F("  Max speed:      1 RPM ("));
  Serial.print(MAX_SPEED_SPS, 1);
  Serial.println(F(" steps/sec)"));
  Serial.print(F("  Acceleration:   "));
  Serial.print(ACCEL_SPS2, 1);
  Serial.println(F(" steps/sec^2 (~1.8s ramp)"));
  Serial.print(F("  Forward:        "));
  Serial.print(FWD_STEPS);
  Serial.print(F(" steps ("));
  Serial.print(FWD_ROTATIONS);
  Serial.println(F(" rotations)"));
  Serial.print(F("  Backward:       "));
  Serial.print(BWD_STEPS);
  Serial.print(F(" steps ("));
  Serial.print(BWD_ROTATIONS);
  Serial.println(F(" rotation)"));
  Serial.print(F("  Stagger:        "));
  Serial.print(STAGGER_MS / 1000);
  Serial.println(F(" sec between motor starts"));
  Serial.print(F("  Pause:          "));
  Serial.print(PAUSE_MS / 1000);
  Serial.println(F(" sec between fwd/bwd"));
  Serial.println(F(""));

  // Initialize motors
  Serial.println(F("Pin assignments:"));
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    motors[i] = new AccelStepper(AccelStepper::DRIVER,
                                  MOTOR_PINS[i][0],
                                  MOTOR_PINS[i][1]);
    motors[i]->setMaxSpeed(MAX_SPEED_SPS);
    motors[i]->setAcceleration(ACCEL_SPS2);
    motors[i]->setCurrentPosition(0);

    states[i] = STATE_WAITING;
    pauseStartMs[i] = 0;

    Serial.print(F("  Motor "));
    if (i + 1 < 10) Serial.print(F(" "));
    Serial.print(i + 1);
    Serial.print(F(":  STEP="));
    Serial.print(MOTOR_PINS[i][0]);
    Serial.print(F("  DIR="));
    Serial.println(MOTOR_PINS[i][1]);
  }

  Serial.println(F(""));
  Serial.println(F("============================================================"));
  Serial.println(F("  STARTING TEST SEQUENCE"));
  Serial.println(F("============================================================"));
  Serial.println(F(""));

  testStartMs = millis();
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - testStartMs;

  // --- State machine for each motor ---
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    switch (states[i]) {

      case STATE_WAITING:
        if (elapsed >= (unsigned long)i * STAGGER_MS) {
          Serial.print(F(">>> Motor "));
          Serial.print(i + 1);
          Serial.print(F(" — STEP pin "));
          Serial.print(MOTOR_PINS[i][0]);
          Serial.print(F(", DIR pin "));
          Serial.print(MOTOR_PINS[i][1]);
          Serial.println(F(""));
          Serial.print(F("    FORWARD "));
          Serial.print(FWD_ROTATIONS);
          Serial.print(F(" rotations ("));
          Serial.print(FWD_STEPS);
          Serial.println(F(" steps)..."));

          motors[i]->setCurrentPosition(0);
          motors[i]->moveTo(FWD_STEPS);
          states[i] = STATE_RUNNING_FWD;
        }
        break;

      case STATE_RUNNING_FWD:
        if (motors[i]->distanceToGo() == 0) {
          Serial.print(F("    Motor "));
          Serial.print(i + 1);
          Serial.print(F(" — Forward COMPLETE (pos="));
          Serial.print(motors[i]->currentPosition());
          Serial.print(F("). Pausing "));
          Serial.print(PAUSE_MS / 1000);
          Serial.println(F(" sec..."));

          pauseStartMs[i] = now;
          states[i] = STATE_PAUSING;
        }
        break;

      case STATE_PAUSING:
        if (now - pauseStartMs[i] >= PAUSE_MS) {
          Serial.print(F("    Motor "));
          Serial.print(i + 1);
          Serial.print(F(" — BACKWARD "));
          Serial.print(BWD_ROTATIONS);
          Serial.print(F(" rotation ("));
          Serial.print(BWD_STEPS);
          Serial.println(F(" steps)..."));

          motors[i]->moveTo(motors[i]->currentPosition() - BWD_STEPS);
          states[i] = STATE_RUNNING_BWD;
        }
        break;

      case STATE_RUNNING_BWD:
        if (motors[i]->distanceToGo() == 0) {
          Serial.print(F("    Motor "));
          Serial.print(i + 1);
          Serial.print(F(" — Backward COMPLETE (pos="));
          Serial.print(motors[i]->currentPosition());
          Serial.println(F(") — DONE"));
          Serial.println(F(""));

          states[i] = STATE_DONE;
          motorsCompleted++;

          if (motorsCompleted == NUM_MOTORS) {
            printSummary();
          }
        }
        break;

      case STATE_DONE:
        break;
    }
  }

  // --- Drive all active motors ---
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    if (states[i] == STATE_RUNNING_FWD || states[i] == STATE_RUNNING_BWD) {
      motors[i]->run();
    }
  }
}

// ============================================================
// SUMMARY — printed once after all 12 motors complete
// ============================================================
void printSummary() {
  unsigned long totalSec = (millis() - testStartMs) / 1000;
  long expectedPos = FWD_STEPS - BWD_STEPS;  // 6400 - 3200 = 3200

  Serial.println(F("============================================================"));
  Serial.println(F("  TEST COMPLETE — ALL 12 MOTORS FINISHED"));
  Serial.println(F("============================================================"));
  Serial.println(F(""));
  Serial.println(F("  Motor   Final Pos   Expected   Status"));
  Serial.println(F("  -----   ---------   --------   ------"));

  uint8_t okCount = 0;
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    long pos = motors[i]->currentPosition();
    bool match = (pos == expectedPos);
    if (match) okCount++;

    Serial.print(F("    "));
    if (i + 1 < 10) Serial.print(F(" "));
    Serial.print(i + 1);
    Serial.print(F("       "));
    if (pos >= 0 && pos < 10000) Serial.print(F(" "));
    if (pos >= 0 && pos < 1000)  Serial.print(F(" "));
    if (pos >= 0 && pos < 100)   Serial.print(F(" "));
    Serial.print(pos);
    Serial.print(F("       "));
    Serial.print(expectedPos);
    Serial.print(F("    "));
    Serial.println(match ? F("OK") : F("MISMATCH"));
  }

  Serial.println(F(""));
  Serial.print(F("  Result: "));
  Serial.print(okCount);
  Serial.print(F("/"));
  Serial.print(NUM_MOTORS);
  Serial.println(F(" motors at expected position"));
  Serial.print(F("  Total test time: "));
  Serial.print(totalSec);
  Serial.println(F(" seconds"));
  Serial.println(F(""));
  Serial.println(F("  VISUAL CHECKS:"));
  Serial.println(F("  - Did each motor spin smoothly (no vibration/stall)?"));
  Serial.println(F("  - Did forward direction match expectation?"));
  Serial.println(F("  - Did backward reverse correctly?"));
  Serial.println(F("  - Did the correct physical prism move for each motor #?"));
  Serial.println(F("  - Any unexpected noise, heat, or skipped steps?"));
  Serial.println(F(""));
  Serial.println(F("  Test halted. Reset board to re-run."));
  Serial.println(F("============================================================"));

  // Halt — do not loop
  while (true) { ; }
}
