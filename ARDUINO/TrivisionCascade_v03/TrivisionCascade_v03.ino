/*
 * TrivisionCascade_v03 — Peek-a-Boo
 * 
 * Face forward (showing adorable 2-year-old Joel) for 1-5 sec random hold,
 * then FAST hide (180° in 0.75 sec), hold hidden 3 sec,
 * then SLOW reveal (180° back in 2 sec). Repeat forever.
 * 
 * All moves use quadratic-eased trapezoidal ramping.
 * 
 * 180° = 1600 microsteps (3200 / 2)
 * 
 * Hide speed:   1600 steps / 0.75 sec = 2133 steps/sec → ~469 µs cruise
 * Reveal speed:  1600 steps / 2.0 sec  = 800 steps/sec  → ~1250 µs cruise
 */

#define STEP_PIN  35
#define DIR_PIN   18

#define STEPS_180  1600   // Half revolution = 180°

#define CW   HIGH
#define CCW  LOW

// Cruise speeds
#define SPEED_HIDE    469    // Fast: 180° in 0.75 sec
#define SPEED_REVEAL  1250   // Slow: 180° in 2.0 sec

// Ramp parameters
#define RAMP_START_SPEED  3500
#define RAMP_STEPS_HIDE   250   // Shorter ramp for fast snap
#define RAMP_STEPS_REVEAL 350   // Longer ramp for gentle reveal

// Hidden hold time
#define HIDDEN_HOLD_MS  3000

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  
  Serial.println(F("=== TrivisionCascade v03 — Peek-a-Boo ==="));
  Serial.println(F("Face showing. Starting game..."));
  Serial.println();
  
  randomSeed(analogRead(A0));  // Seed from floating analog pin
  
  delay(2000);  // Initial settle
}

void loop() {
  // --- FACE SHOWING ---
  long faceHold = random(1000, 5001);  // 1–5 seconds
  Serial.print(F("PEEK-A-BOO! Face showing for "));
  Serial.print(faceHold / 1000.0, 1);
  Serial.println(F(" sec"));
  delay(faceHold);
  
  // --- FAST HIDE ---
  Serial.println(F("  ...HIDING! (180° fast)"));
  stepSmooth(CW, STEPS_180, SPEED_HIDE, RAMP_STEPS_HIDE);
  
  // --- HIDDEN HOLD ---
  Serial.println(F("  (hidden)"));
  delay(HIDDEN_HOLD_MS);
  
  // --- SLOW REVEAL ---
  Serial.println(F("  ...revealing (180° slow)"));
  stepSmooth(CCW, STEPS_180, SPEED_REVEAL, RAMP_STEPS_REVEAL);
}

// --- Quadratic-eased trapezoidal stepping ---
void stepSmooth(uint8_t dir, long totalSteps, unsigned int cruiseUs, long rampSteps) {
  digitalWrite(DIR_PIN, dir);
  delayMicroseconds(5);
  
  if (totalSteps < rampSteps * 2) {
    rampSteps = totalSteps / 2;
  }
  long decelStart = totalSteps - rampSteps;
  
  for (long i = 0; i < totalSteps; i++) {
    unsigned int currentSpeed;
    
    if (i < rampSteps) {
      float t = (float)i / (float)rampSteps;
      t = t * t;
      currentSpeed = RAMP_START_SPEED - (unsigned int)(t * (RAMP_START_SPEED - cruiseUs));
    }
    else if (i >= decelStart) {
      float t = (float)(i - decelStart) / (float)rampSteps;
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
