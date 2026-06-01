/*
 * TrivisionCascade_v04 — Peek-a-Boo: Dizzy Reveal Edition
 * 
 * 1. Face showing: 1–5 sec random hold
 * 2. FAST hide: CW 180° in 0.75 sec
 * 3. Hidden hold: 3 sec
 * 4. DIZZY REVEAL: 10 full CCW revolutions, each noticeably faster
 *    than the last, ending with face forward (10 × 360° = 3600°,
 *    plus the 180° owed = 3780° total CCW, but we want to land
 *    face-forward, so: 9.5 full revolutions CCW = 9.5 × 360° = 3420°
 *    which is 180° + 9 × 360°, landing back at start)
 * 
 * Speed ramp across 10 spins:
 *   Spin 1:  ~1500 µs cruise (slow, dramatic start)
 *   Spin 10: ~150 µs cruise (near TMC2209 practical limit)
 *   Each spin ramps up and down individually.
 * 
 * Motor safe limit: TMC2209 handles step pulses down to ~100µs.
 * We cap at 150µs for reliability with 24V and 1/8 microstepping.
 * At 150µs: ~6667 steps/sec = ~2.08 rev/sec. Fast but safe.
 */

#define STEP_PIN  35
#define DIR_PIN   18

#define STEPS_PER_REV  3200
#define STEPS_180      1600

#define CW   HIGH
#define CCW  LOW

// Hide speed (same as v03)
#define SPEED_HIDE       469
#define RAMP_STEPS_HIDE  250

// Dizzy reveal parameters
#define DIZZY_SPINS       10
#define DIZZY_SLOW_US     1500   // First spin cruise speed
#define DIZZY_FAST_US     150    // Last spin cruise speed (safe floor)
#define RAMP_START_SPEED  3500   // Universal ramp start

// Hidden hold
#define HIDDEN_HOLD_MS  3000

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  
  Serial.println(F("=== TrivisionCascade v04 — Peek-a-Boo: Dizzy Reveal ==="));
  Serial.println();
  
  randomSeed(analogRead(A0));
  delay(2000);
}

void loop() {
  // --- FACE SHOWING ---
  long faceHold = random(1000, 5001);
  Serial.print(F("PEEK-A-BOO! Face showing for "));
  Serial.print(faceHold / 1000.0, 1);
  Serial.println(F(" sec"));
  delay(faceHold);
  
  // --- FAST HIDE: CW 180° ---
  Serial.println(F("  ...HIDING! (CW 180°)"));
  stepSmooth(CW, STEPS_180, SPEED_HIDE, RAMP_STEPS_HIDE);
  
  // --- HIDDEN HOLD ---
  Serial.println(F("  (hidden for 3 sec)"));
  delay(HIDDEN_HOLD_MS);
  
  // --- DIZZY REVEAL: CCW ---
  // We need to end up 180° CCW from hidden position = back to face.
  // Spin 1: 180° (gets us to face-forward)
  // Spins 2–10: each a full 360° (keeps landing face-forward)
  // Total: 180° + 9×360° = 3420° CCW
  Serial.println(F("  DIZZY REVEAL! 10 spins CCW, accelerating..."));
  
  for (int spin = 0; spin < DIZZY_SPINS; spin++) {
    // Calculate cruise speed for this spin: logarithmic distribution
    // so early spins have noticeable speed jumps too
    float t = (float)spin / (float)(DIZZY_SPINS - 1);  // 0.0 to 1.0
    // Use cubic curve so speed increase feels dramatic
    t = t * t * t;
    unsigned int cruiseUs = DIZZY_SLOW_US - (unsigned int)(t * (DIZZY_SLOW_US - DIZZY_FAST_US));
    
    // First spin is only 180°, rest are full 360°
    long steps = (spin == 0) ? STEPS_180 : STEPS_PER_REV;
    
    // Scale ramp steps: shorter ramps as speed increases (punchier feel)
    long rampSteps = 300 - (spin * 20);  // 300 down to 120
    if (rampSteps < 120) rampSteps = 120;
    
    // Also adjust ramp start speed — faster spins need less gentle entry
    // after the first couple
    unsigned int rampStart = RAMP_START_SPEED;
    if (spin > 2) {
      // Later spins start their ramp closer to cruise speed
      rampStart = RAMP_START_SPEED - (spin * 200);
      if (rampStart < cruiseUs + 500) rampStart = cruiseUs + 500;
    }
    
    Serial.print(F("    Spin "));
    Serial.print(spin + 1);
    Serial.print(F(": cruise="));
    Serial.print(cruiseUs);
    Serial.print(F("µs ("));
    Serial.print(spin == 0 ? 180 : 360);
    Serial.println(F("°)"));
    
    stepSmoothVariable(CCW, steps, cruiseUs, rampSteps, rampStart);
  }
  
  Serial.println(F("  Face revealed! Dizzy yet?"));
  Serial.println();
}

// --- Standard quadratic-eased stepping ---
void stepSmooth(uint8_t dir, long totalSteps, unsigned int cruiseUs, long rampSteps) {
  stepSmoothVariable(dir, totalSteps, cruiseUs, rampSteps, RAMP_START_SPEED);
}

// --- Quadratic-eased stepping with variable ramp start ---
void stepSmoothVariable(uint8_t dir, long totalSteps, unsigned int cruiseUs, 
                         long rampSteps, unsigned int rampStartUs) {
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
      currentSpeed = rampStartUs - (unsigned int)(t * (rampStartUs - cruiseUs));
    }
    else if (i >= decelStart) {
      float t = (float)(i - decelStart) / (float)rampSteps;
      t = 1.0 - (1.0 - t) * (1.0 - t);
      currentSpeed = cruiseUs + (unsigned int)(t * (rampStartUs - cruiseUs));
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
