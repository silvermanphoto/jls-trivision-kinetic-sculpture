/*
 * TrivisionCascade_v06 — Peek-a-Boo CENTRIFUGE Edition
 * 
 * Sequence:
 *   1. Face showing: 1-5 sec random hold
 *   2. Fast hide: 180° CW in 0.75 sec (ramped)
 *   3. Hidden hold: 3 sec
 *   4. CENTRIFUGE REVEAL: 10 full CCW revolutions, each faster than the last
 *      (spinning up like a centrifuge), then 10 full CCW revolutions decelerating
 *      back to zero. Stops at original face-forward position.
 *   5. Repeat
 * 
 * Speed plan for centrifuge:
 *   Rev 1 (slowest):  ~2000 µs/step → 500 steps/sec  → 6.4 sec/rev
 *   Rev 10 (fastest):  ~150 µs/step → 6667 steps/sec → 0.48 sec/rev
 *   TMC2209 can handle pulses down to ~100µs but 150µs is safe with load.
 *   
 *   Total spin: 20 revolutions CCW = 20 × 360° = 7200°
 *   But we started by going CW 180°, so net position after centrifuge:
 *   CW 180° + CCW 20×360° = we need to end at start.
 *   Solution: centrifuge does 20 full revolutions (lands on same position)
 *   then we undo the 180° hide with a final slow CCW 180° — NO WAIT.
 *   
 *   Actually simpler: The centrifuge IS the reveal. 
 *   Hide = CW 180° (face now hidden).
 *   Centrifuge = CCW 10 rev accel + 10 rev decel = 20 full revolutions.
 *   20 full revolutions = back to hidden position (180° from face).
 *   So we need 20 revolutions MINUS 180° to land on face, 
 *   OR 20 revolutions PLUS 180° to land on face.
 *   Easiest: do 19.5 full revolutions total (19 full + 180°) to land on face.
 *   Split: 10 revs speeding up, then 9 revs + 180° slowing down.
 *   
 *   Let's just do exactly 10 + 10 = 20 full revs, which lands at hidden,
 *   then add a gentle 180° CCW to reveal face. Clean and predictable.
 */

#define STEP_PIN  35
#define DIR_PIN   18

#define STEPS_PER_REV  3200
#define STEPS_180      1600

#define CW   HIGH
#define CCW  LOW

// Hide move
#define SPEED_HIDE       469
#define RAMP_STEPS_HIDE  250
#define RAMP_START_SPEED 3500

// Hidden hold
#define HIDDEN_HOLD_MS  3000

// Centrifuge parameters
#define CENTRIFUGE_REVS     10      // Revs speeding up
#define CENTRIFUGE_SLOWEST  2000    // µs/step for rev 1 (slowest)
#define CENTRIFUGE_FASTEST  150     // µs/step for rev 10 (fastest — safe TMC2209 limit)

// Gentle final reveal after centrifuge lands
#define SPEED_REVEAL       1250
#define RAMP_STEPS_REVEAL  350

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  
  Serial.println(F("=== TrivisionCascade v06 — Peek-a-Boo CENTRIFUGE ==="));
  Serial.println();
  
  randomSeed(analogRead(A0));
  delay(2000);
}

void loop() {
  // --- FACE SHOWING ---
  long faceHold = random(1000, 5001);
  Serial.print(F("FACE showing for "));
  Serial.print(faceHold / 1000.0, 1);
  Serial.println(F(" sec"));
  delay(faceHold);
  
  // --- FAST HIDE (CW 180°) ---
  Serial.println(F("  HIDING! (180° CW fast)"));
  stepSmooth(CW, STEPS_180, SPEED_HIDE, RAMP_STEPS_HIDE);
  
  // --- HIDDEN HOLD ---
  Serial.println(F("  (hidden 3 sec)"));
  delay(HIDDEN_HOLD_MS);
  
  // --- CENTRIFUGE SPIN-UP: 10 revolutions CCW, each faster ---
  Serial.println(F("  CENTRIFUGE! Spinning up..."));
  for (int rev = 0; rev < CENTRIFUGE_REVS; rev++) {
    // Interpolate speed: rev 0 = slowest, rev 9 = fastest
    float t = (float)rev / (float)(CENTRIFUGE_REVS - 1);
    // Quadratic curve for dramatic acceleration feel
    t = t * t;
    unsigned int cruiseSpeed = CENTRIFUGE_SLOWEST - 
        (unsigned int)(t * (CENTRIFUGE_SLOWEST - CENTRIFUGE_FASTEST));
    
    // Short ramp per revolution — just enough to smooth transitions
    long rampSteps = 150;
    
    Serial.print(F("    Rev "));
    Serial.print(rev + 1);
    Serial.print(F("/10  speed: "));
    Serial.print(cruiseSpeed);
    Serial.println(F(" µs/step"));
    
    stepSmooth(CCW, STEPS_PER_REV, cruiseSpeed, rampSteps);
  }
  
  // --- CENTRIFUGE SPIN-DOWN: 10 revolutions CCW, each slower ---
  Serial.println(F("  Spinning down..."));
  for (int rev = 0; rev < CENTRIFUGE_REVS; rev++) {
    // Interpolate speed: rev 0 = fastest, rev 9 = slowest
    float t = (float)rev / (float)(CENTRIFUGE_REVS - 1);
    // Inverse quadratic for dramatic deceleration
    t = 1.0 - (1.0 - t) * (1.0 - t);
    unsigned int cruiseSpeed = CENTRIFUGE_FASTEST + 
        (unsigned int)(t * (CENTRIFUGE_SLOWEST - CENTRIFUGE_FASTEST));
    
    long rampSteps = 150;
    
    Serial.print(F("    Rev "));
    Serial.print(rev + 1);
    Serial.print(F("/10  speed: "));
    Serial.print(cruiseSpeed);
    Serial.println(F(" µs/step"));
    
    stepSmooth(CCW, STEPS_PER_REV, cruiseSpeed, rampSteps);
  }
  
  // --- GENTLE REVEAL (CCW 180° to face-forward) ---
  // 20 full revolutions landed us back at hidden position (180° from face)
  Serial.println(F("  Gentle reveal... (180° CCW slow)"));
  stepSmooth(CCW, STEPS_180, SPEED_REVEAL, RAMP_STEPS_REVEAL);
  
  Serial.println(F("  PEEK-A-BOO!"));
  Serial.println();
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
    
    // Clamp minimum to safe limit
    if (currentSpeed < 120) currentSpeed = 120;
    
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(2);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(currentSpeed);
  }
}
