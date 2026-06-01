/*
 * TrivisionCascade_v07 — Peek-a-Boo Centrifuge Edition
 * 
 * Sequence (loops forever):
 *   1. Face showing: random 1–5 sec
 *   2. SNAP hide: 180° CW in 0.75 sec (ramped)
 *   3. Hidden hold: 3 sec
 *   4. CENTRIFUGE: 10 CCW revolutions speeding up, then 10 CCW slowing to zero
 *      - Continuous motion, never stops mid-spin
 *      - Cosine curve for buttery smooth speed envelope
 *      - Each revolution noticeably faster/slower than the last
 *      - Peak speed: 150 µs/step (~6667 steps/sec) — safe max for NEMA 17 at 24V
 *   5. Now 180° offset from home (20 full revs = net 0°, but we hid 180° CW first)
 *      So we do a gentle 180° CCW to return face-forward
 *   6. Repeat
 *
 * Position math:
 *   Start at 0°. Hide +180° CW. Centrifuge 20 full revs CCW (net 0° rotation).
 *   Still at +180° from home. Final reveal -180° CCW → back to 0°. Clean.
 */

#define STEP_PIN  35
#define DIR_PIN   18

#define STEPS_PER_REV   3200L
#define STEPS_180       1600L

#define CW   HIGH
#define CCW  LOW

// --- Hide move ---
#define SPEED_HIDE       469     // 180° in 0.75 sec
#define RAMP_HIDE        250
#define RAMP_START       3500    // Gentle start/stop speed for all ramped moves

// --- Centrifuge ---
#define SPINUP_REVS      10
#define SLOWDOWN_REVS    10
#define CENTRIFUGE_SLOW  2500    // µs — starting/ending gentle speed
#define CENTRIFUGE_FAST  150     // µs — peak screaming speed (safe floor for NEMA 17)

// --- Final gentle reveal back to face ---
#define SPEED_REVEAL     1250    // 180° in 2 sec
#define RAMP_REVEAL      350

// --- Hold times ---
#define HIDDEN_HOLD_MS   3000

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  
  Serial.println(F("=== TrivisionCascade v07 — Peek-a-Boo Centrifuge ==="));
  Serial.println();
  
  randomSeed(analogRead(A0));
  delay(2000);
}

void loop() {
  // --- 1. FACE SHOWING ---
  long faceHold = random(1000, 5001);
  Serial.print(F("FACE for "));
  Serial.print(faceHold / 1000.0, 1);
  Serial.println(F(" sec"));
  delay(faceHold);
  
  // --- 2. SNAP HIDE (CW 180°) ---
  Serial.println(F("HIDE! (180 CW fast)"));
  stepRamped(CW, STEPS_180, SPEED_HIDE, RAMP_HIDE);
  
  // --- 3. HIDDEN HOLD ---
  Serial.println(F("(hidden 3 sec)"));
  delay(HIDDEN_HOLD_MS);
  
  // --- 4. CENTRIFUGE (CCW, 20 full revolutions continuous) ---
  Serial.println(F("CENTRIFUGE! 10 revs speeding up, 10 slowing down"));
  centrifuge(CCW, SPINUP_REVS, SLOWDOWN_REVS);
  Serial.println(F("  Centrifuge complete."));
  
  // --- 5. GENTLE REVEAL (CCW 180° back to face) ---
  delay(500);
  Serial.println(F("Gentle reveal (180 CCW slow)"));
  stepRamped(CCW, STEPS_180, SPEED_REVEAL, RAMP_REVEAL);
}

/*
 * centrifuge() — Continuous multi-revolution spin with smooth speed envelope.
 * 
 * Uses a cosine curve across all (spinupRevs + slowdownRevs) revolutions
 * so speed rises and falls with zero jerk at transitions.
 * 
 * The speed at any point is determined by where we are in the total step count:
 *   - At step 0: CENTRIFUGE_SLOW
 *   - At the midpoint: CENTRIFUGE_FAST  
 *   - At the end: CENTRIFUGE_SLOW
 *   
 * Cosine maps this as: speed = slow + (slow - fast) * 0.5 * (cos(π * progress) - 1)
 * which gives smooth acceleration, smooth peak, smooth deceleration — no jerk anywhere.
 */
void centrifuge(uint8_t dir, int spinupRevs, int slowdownRevs) {
  digitalWrite(DIR_PIN, dir);
  delayMicroseconds(5);
  
  long totalRevs = (long)spinupRevs + (long)slowdownRevs;
  long totalSteps = totalRevs * STEPS_PER_REV;
  
  // Midpoint is where we hit peak speed
  long midpoint = (long)spinupRevs * STEPS_PER_REV;
  
  float slowF = (float)CENTRIFUGE_SLOW;
  float fastF = (float)CENTRIFUGE_FAST;
  
  long revCounter = 0;
  long stepsInRev = 0;
  
  for (long i = 0; i < totalSteps; i++) {
    // Cosine envelope: 0→midpoint uses first half of cosine, midpoint→end uses second half
    float progress;
    if (i <= midpoint) {
      // Speeding up: map 0..midpoint to 0..PI
      progress = (float)i / (float)midpoint;
    } else {
      // Slowing down: map midpoint..totalSteps to 0..PI
      progress = (float)(totalSteps - i) / (float)(totalSteps - midpoint);
    }
    
    // Cosine interpolation: 1.0 at ends (slow), 0.0 at peak (fast)
    // (1 + cos(PI * progress)) / 2 gives us 1→0 as progress goes 0→1... wrong direction
    // We want: progress=0 → slow, progress=1 → fast
    // Use: (1 - cos(PI * progress)) / 2 → 0 at progress=0, 1 at progress=1
    float blend = (1.0 - cos(progress * PI)) / 2.0;
    
    unsigned int currentSpeed = (unsigned int)(slowF - blend * (slowF - fastF));
    
    // Clamp safety
    if (currentSpeed < CENTRIFUGE_FAST) currentSpeed = CENTRIFUGE_FAST;
    if (currentSpeed > CENTRIFUGE_SLOW) currentSpeed = CENTRIFUGE_SLOW;
    
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(2);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(currentSpeed);
    
    // Revolution counter for serial reporting
    stepsInRev++;
    if (stepsInRev >= STEPS_PER_REV) {
      stepsInRev = 0;
      revCounter++;
      Serial.print(F("  Rev "));
      Serial.print(revCounter);
      Serial.print(F("/"));
      Serial.print(totalRevs);
      Serial.print(F("  speed: "));
      Serial.print(currentSpeed);
      Serial.println(F(" us"));
    }
  }
}

/*
 * stepRamped() — Single move with quadratic-eased trapezoidal profile.
 */
void stepRamped(uint8_t dir, long totalSteps, unsigned int cruiseUs, long rampSteps) {
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
      currentSpeed = RAMP_START - (unsigned int)(t * (RAMP_START - cruiseUs));
    }
    else if (i >= decelStart) {
      float t = (float)(i - decelStart) / (float)rampSteps;
      t = 1.0 - (1.0 - t) * (1.0 - t);
      currentSpeed = cruiseUs + (unsigned int)(t * (RAMP_START - cruiseUs));
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
