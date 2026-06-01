/*
 * TrivisionHWTest_v02.ino
 * ============================================================
 * Trivision Kinetic Sculpture — Hardware Verification Test
 * Version: v02
 * Date: 2026-02-23
 * ============================================================
 *
 * Continuous forward rotation at 0.5 RPM with gentle ramp.
 * 5-second stagger between motor starts. Runs forever.
 *
 * Board: Arduino Mega 2560 with HCDC screw terminal shield
 * Motors: NEMA 17 (0.9° step) via TMC2209 at 1/8 microstepping
 * Steps/rev: 3200
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
#define STAGGER_MS      5000UL

// 0.5 RPM = 3200 steps / 120 sec = 26.67 steps/sec
static const float MAX_SPEED_SPS = 26.67;

// Gentle ramp: ~1.8 seconds to reach full speed
static const float ACCEL_SPS2 = 15.0;

// Effectively infinite target — runs for years
static const long INFINITE_TARGET = 2000000000L;

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
// GLOBALS
// ============================================================
AccelStepper* motors[NUM_MOTORS];
bool motorStarted[NUM_MOTORS];
unsigned long testStartMs;

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println(F(""));
  Serial.println(F("============================================================"));
  Serial.println(F("  TrivisionHWTest_v02"));
  Serial.println(F("  Continuous Forward Rotation — 0.5 RPM — Endless Loop"));
  Serial.println(F("  12 Motors / AccelStepper / Open Loop / No Hall Sensors"));
  Serial.println(F("============================================================"));
  Serial.println(F(""));
  Serial.print(F("  Speed: 0.5 RPM ("));
  Serial.print(MAX_SPEED_SPS, 2);
  Serial.println(F(" steps/sec) — 2 minutes per revolution"));
  Serial.print(F("  Accel: "));
  Serial.print(ACCEL_SPS2, 1);
  Serial.println(F(" steps/sec^2 (~1.8s ramp)"));
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
    motorStarted[i] = false;

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
  unsigned long elapsed = millis() - testStartMs;

  // Start motors according to stagger schedule
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    if (!motorStarted[i] && elapsed >= (unsigned long)i * STAGGER_MS) {
      motors[i]->moveTo(INFINITE_TARGET);
      motorStarted[i] = true;

      Serial.print(F(">>> Motor "));
      Serial.print(i + 1);
      Serial.println(F(" — STARTED"));
    }
  }

  // Drive all active motors
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    if (motorStarted[i]) {
      motors[i]->run();
    }
  }
}
