/*
 * TrivisionCascade_v01 — Hello World Stepper Test
 * 
 * Hardware: Arduino Mega 2560 + Adafruit TMC2209 breakout + NEMA 17 (0.9°)
 * Motor 1 wiring: STEP → pin 35, DIR → pin 18
 * TMC2209 defaults: 1/8 microstepping, StealthChop (silent)
 * 3200 microsteps = 1 full revolution
 * 1067 microsteps = 120° (one prism face change)
 * 
 * Test sequence:
 *   1. Fast CW full revolution
 *   2. Fast CCW full revolution  
 *   3. Slow CW full revolution
 *   4. Slow CCW full revolution
 *   5. Gallery-speed 120° rotation (Blender cascade timing: ~1.25 sec per face)
 *   6. Gallery-speed smooth back-and-forth demo
 */

#define STEP_PIN  35
#define DIR_PIN   18

#define STEPS_PER_REV   3200    // 400 full steps × 8 microsteps
#define STEPS_PER_FACE  1067    // 120° = 3200 / 3

// Direction constants
#define CW   HIGH
#define CCW  LOW

// Speed settings (microseconds between step pulses)
// Lower = faster. Minimum reliable is ~100µs for TMC2209.
#define SPEED_FAST     250    // ~4000 steps/sec → full rev in 0.8 sec
#define SPEED_SLOW     1500   // ~667 steps/sec → full rev in 4.8 sec

// Gallery speed: 120° in ~1.25 sec = 1067 steps / 1.25 sec = 854 steps/sec
#define SPEED_GALLERY  1172   // 1,000,000 µs / 854 steps ≈ 1172 µs

// Acceleration for smooth gallery moves
// We ramp from a slow start speed down to gallery speed over a number of steps
#define ACCEL_START_SPEED  3000   // Start slow (µs between pulses)
#define ACCEL_STEPS        200    // Number of steps to ramp up/down

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  
  Serial.println(F("=== TrivisionCascade v01 — Stepper Hello World ==="));
  Serial.println(F("Motor 1: STEP=35, DIR=18"));
  Serial.println(F("3200 microsteps/rev, 1067 microsteps/face"));
  Serial.println(F("TMC2209 StealthChop, 1/8 microstepping"));
  Serial.println();
  
  delay(2000);  // Let everything settle
  
  // ---- TEST 1: Fast CW ----
  Serial.println(F("TEST 1: Fast CW — 1 full revolution"));
  stepMotor(CW, STEPS_PER_REV, SPEED_FAST);
  Serial.println(F("  Done."));
  delay(2000);
  
  // ---- TEST 2: Fast CCW ----
  Serial.println(F("TEST 2: Fast CCW — 1 full revolution"));
  stepMotor(CCW, STEPS_PER_REV, SPEED_FAST);
  Serial.println(F("  Done."));
  delay(2000);
  
  // ---- TEST 3: Slow CW ----
  Serial.println(F("TEST 3: Slow CW — 1 full revolution"));
  stepMotor(CW, STEPS_PER_REV, SPEED_SLOW);
  Serial.println(F("  Done."));
  delay(2000);
  
  // ---- TEST 4: Slow CCW ----
  Serial.println(F("TEST 4: Slow CCW — 1 full revolution"));
  stepMotor(CCW, STEPS_PER_REV, SPEED_SLOW);
  Serial.println(F("  Done."));
  delay(2000);
  
  // ---- TEST 5: Gallery-speed 120° with acceleration ----
  Serial.println(F("TEST 5: Gallery-speed CW — 120° (one face change) with smooth accel/decel"));
  stepMotorSmooth(CW, STEPS_PER_FACE);
  Serial.println(F("  Done."));
  delay(3000);
  
  // ---- TEST 6: Gallery-speed back-and-forth (3 faces forward, 3 faces back) ----
  Serial.println(F("TEST 6: Gallery cascade demo — 3 face changes CW, then 3 CCW"));
  for (int i = 0; i < 3; i++) {
    Serial.print(F("  Face CW "));
    Serial.println(i + 1);
    stepMotorSmooth(CW, STEPS_PER_FACE);
    delay(1500);  // Hold at each face — simulates the Blender hold frames
  }
  delay(3000);  // Longer hold at full rotation
  for (int i = 0; i < 3; i++) {
    Serial.print(F("  Face CCW "));
    Serial.println(i + 1);
    stepMotorSmooth(CCW, STEPS_PER_FACE);
    delay(1500);
  }
  
  Serial.println();
  Serial.println(F("=== ALL TESTS COMPLETE ==="));
  Serial.println(F("Motor should be back at starting position."));
}

void loop() {
  // Nothing — one-shot test
}

// --- Simple constant-speed stepping ---
void stepMotor(uint8_t dir, long steps, unsigned int speedUs) {
  digitalWrite(DIR_PIN, dir);
  delayMicroseconds(5);  // DIR setup time for TMC2209
  
  for (long i = 0; i < steps; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(2);  // Minimum pulse width for TMC2209
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(speedUs);
  }
}

// --- Smooth gallery-speed stepping with trapezoidal acceleration ---
void stepMotorSmooth(uint8_t dir, long totalSteps) {
  digitalWrite(DIR_PIN, dir);
  delayMicroseconds(5);
  
  long accelSteps = ACCEL_STEPS;
  long decelStart = totalSteps - accelSteps;
  
  // If move is too short for full accel+decel, split evenly
  if (totalSteps < accelSteps * 2) {
    accelSteps = totalSteps / 2;
    decelStart = totalSteps - accelSteps;
  }
  
  for (long i = 0; i < totalSteps; i++) {
    unsigned int currentSpeed;
    
    if (i < accelSteps) {
      // Accelerating: linearly ramp from ACCEL_START_SPEED to SPEED_GALLERY
      float progress = (float)i / (float)accelSteps;
      currentSpeed = ACCEL_START_SPEED - (unsigned int)(progress * (ACCEL_START_SPEED - SPEED_GALLERY));
    } 
    else if (i >= decelStart) {
      // Decelerating: linearly ramp from SPEED_GALLERY to ACCEL_START_SPEED
      float progress = (float)(i - decelStart) / (float)accelSteps;
      currentSpeed = SPEED_GALLERY + (unsigned int)(progress * (ACCEL_START_SPEED - SPEED_GALLERY));
    } 
    else {
      // Cruising at gallery speed
      currentSpeed = SPEED_GALLERY;
    }
    
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(2);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(currentSpeed);
  }
}
