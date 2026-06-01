/*
 * TrivisionCascade_v02 — Hello World with Smooth Ramping on ALL moves
 * 
 * Change from v01: Every move now uses trapezoidal acceleration/deceleration.
 * No more constant-speed stepping. All tests ramp up, cruise, ramp down.
 * 
 * Hardware: Arduino Mega 2560 + Adafruit TMC2209 breakout + NEMA 17 (0.9°)
 * Motor 1 wiring: STEP → pin 35, DIR → pin 18
 * TMC2209 defaults: 1/8 microstepping, StealthChop (silent)
 * 3200 microsteps = 1 full revolution
 * 1067 microsteps = 120° (one prism face change)
 */

#define STEP_PIN  35
#define DIR_PIN   18

#define STEPS_PER_REV   3200
#define STEPS_PER_FACE  1067

#define CW   HIGH
#define CCW  LOW

// Cruise speeds (µs between step pulses)
#define SPEED_FAST     250    // ~4000 steps/sec
#define SPEED_SLOW     1500   // ~667 steps/sec
#define SPEED_GALLERY  1172   // ~854 steps/sec (120° in 1.25 sec)

// All moves start/end at this gentle speed
#define RAMP_START_SPEED  3500  // Very slow initial pulse rate

// Accel ramp length scales with move distance
// Fast full-rev moves get longer ramps, short face moves get shorter
#define ACCEL_STEPS_LONG   400   // For full revolutions
#define ACCEL_STEPS_SHORT  200   // For 120° face changes

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  
  Serial.println(F("=== TrivisionCascade v02 — All Moves Ramped ==="));
  Serial.println(F("Motor 1: STEP=35, DIR=18"));
  Serial.println(F("Every move uses trapezoidal accel/decel"));
  Serial.println();
  
  delay(2000);
  
  // ---- TEST 1: Fast CW ----
  Serial.println(F("TEST 1: Fast CW — 1 full revolution (ramped)"));
  stepSmooth(CW, STEPS_PER_REV, SPEED_FAST, ACCEL_STEPS_LONG);
  Serial.println(F("  Done."));
  delay(2000);
  
  // ---- TEST 2: Fast CCW ----
  Serial.println(F("TEST 2: Fast CCW — 1 full revolution (ramped)"));
  stepSmooth(CCW, STEPS_PER_REV, SPEED_FAST, ACCEL_STEPS_LONG);
  Serial.println(F("  Done."));
  delay(2000);
  
  // ---- TEST 3: Slow CW ----
  Serial.println(F("TEST 3: Slow CW — 1 full revolution (ramped)"));
  stepSmooth(CW, STEPS_PER_REV, SPEED_SLOW, ACCEL_STEPS_LONG);
  Serial.println(F("  Done."));
  delay(2000);
  
  // ---- TEST 4: Slow CCW ----
  Serial.println(F("TEST 4: Slow CCW — 1 full revolution (ramped)"));
  stepSmooth(CCW, STEPS_PER_REV, SPEED_SLOW, ACCEL_STEPS_LONG);
  Serial.println(F("  Done."));
  delay(2000);
  
  // ---- TEST 5: Gallery-speed 120° ----
  Serial.println(F("TEST 5: Gallery-speed CW — 120° face change (ramped)"));
  stepSmooth(CW, STEPS_PER_FACE, SPEED_GALLERY, ACCEL_STEPS_SHORT);
  Serial.println(F("  Done."));
  delay(3000);
  
  // ---- TEST 6: Gallery cascade demo ----
  Serial.println(F("TEST 6: Gallery cascade — 3 faces CW, hold, 3 faces CCW"));
  for (int i = 0; i < 3; i++) {
    Serial.print(F("  Face CW "));
    Serial.println(i + 1);
    stepSmooth(CW, STEPS_PER_FACE, SPEED_GALLERY, ACCEL_STEPS_SHORT);
    delay(1500);
  }
  delay(3000);
  for (int i = 0; i < 3; i++) {
    Serial.print(F("  Face CCW "));
    Serial.println(i + 1);
    stepSmooth(CCW, STEPS_PER_FACE, SPEED_GALLERY, ACCEL_STEPS_SHORT);
    delay(1500);
  }
  
  Serial.println();
  Serial.println(F("=== ALL TESTS COMPLETE ==="));
}

void loop() {
  // One-shot test
}

// --- Trapezoidal acceleration stepping ---
// dir:        CW or CCW
// totalSteps: how many microsteps to move
// cruiseUs:   target cruise speed in µs between pulses
// rampSteps:  how many steps to spend accelerating (and decelerating)
void stepSmooth(uint8_t dir, long totalSteps, unsigned int cruiseUs, long rampSteps) {
  digitalWrite(DIR_PIN, dir);
  delayMicroseconds(5);  // DIR setup time
  
  // Clamp ramp if move is too short for full accel+decel
  if (totalSteps < rampSteps * 2) {
    rampSteps = totalSteps / 2;
  }
  long decelStart = totalSteps - rampSteps;
  
  for (long i = 0; i < totalSteps; i++) {
    unsigned int currentSpeed;
    
    if (i < rampSteps) {
      // Accelerating: ramp from RAMP_START_SPEED down to cruiseUs
      float t = (float)i / (float)rampSteps;
      // Use quadratic easing for smoother start: t² gives gentle onset
      t = t * t;
      currentSpeed = RAMP_START_SPEED - (unsigned int)(t * (RAMP_START_SPEED - cruiseUs));
    }
    else if (i >= decelStart) {
      // Decelerating: ramp from cruiseUs back up to RAMP_START_SPEED
      float t = (float)(i - decelStart) / (float)rampSteps;
      // Inverse quadratic: 1-(1-t)² gives gentle ending
      t = 1.0 - (1.0 - t) * (1.0 - t);
      currentSpeed = cruiseUs + (unsigned int)(t * (RAMP_START_SPEED - cruiseUs));
    }
    else {
      currentSpeed = cruiseUs;
    }
    
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(2);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(currentSpeed);
  }
}
