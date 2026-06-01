/*
 * TrivisionCascade_v14 — 4-Motor 180° Cascade + Reverse
 * 
 * FIXED: Integer-only easing math to eliminate float-induced stutter.
 * Pre-computes delay values into a lookup table at launch time.
 * 
 *   Motor 1: STEP=35, DIR=18
 *   Motor 2: STEP=37, DIR=19
 *   Motor 3: STEP=39, DIR=20
 *   Motor 4: STEP=41, DIR=21
 * 
 * Pattern:
 *   Cascade CW 180° (1→2→3→4, 1.25s stagger) → hold 3s →
 *   Reverse CCW 180° (4→3→2→1, 1.25s stagger) → hold 3s → repeat
 */

#define NUM_MOTORS 4

const uint8_t stepPin[NUM_MOTORS] = {35, 37, 39, 41};
const uint8_t dirPin[NUM_MOTORS]  = {18, 19, 20, 21};

#define STEPS_180  1600L
#define CW   HIGH
#define CCW  LOW

#define CASCADE_OFFSET_MS  1250
#define HOLD_MS            3000

#define RAMP_STEPS     250L
#define RAMP_START_US  3500U
#define CRUISE_US      1100U
#define SPEED_RANGE    (RAMP_START_US - CRUISE_US)  // 2400

// Pre-computed ramp lookup table (shared by all motors, same profile)
// Only need ramp portion — cruise is constant
unsigned int rampUpTable[RAMP_STEPS];
unsigned int rampDownTable[RAMP_STEPS];

void buildRampTables() {
  for (long i = 0; i < RAMP_STEPS; i++) {
    // Quadratic ease-in: t^2 mapped to integer
    // delay = RAMP_START - (i*i * SPEED_RANGE) / (RAMP_STEPS * RAMP_STEPS)
    long t2 = i * i;
    long denom = RAMP_STEPS * RAMP_STEPS;
    rampUpTable[i] = RAMP_START_US - (unsigned int)((t2 * (long)SPEED_RANGE) / denom);
    
    // Quadratic ease-out for decel
    long j = RAMP_STEPS - 1 - i;  // Reverse index
    long j2 = j * j;
    rampDownTable[i] = RAMP_START_US - (unsigned int)((j2 * (long)SPEED_RANGE) / denom);
  }
}

// --- Per-motor state (lean — no float math at runtime) ---
struct MotorState {
  long   targetSteps;
  long   currentStep;
  unsigned long lastStepUs;
  unsigned int currentDelayUs;
  bool   active;
};

MotorState motor[NUM_MOTORS];

// --- Sequencer ---
enum State {
  STATE_CASCADE,
  STATE_HOLD_AFTER_FWD,
  STATE_REVERSE,
  STATE_HOLD_AFTER_REV
};

State currentState = STATE_CASCADE;
int motorsLaunched = 0;
unsigned long lastLaunchTime = 0;
unsigned long holdTimer = 0;

const int fwdOrder[NUM_MOTORS] = {0, 1, 2, 3};
const int revOrder[NUM_MOTORS] = {3, 2, 1, 0};

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  
  for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(stepPin[i], OUTPUT);
    pinMode(dirPin[i], OUTPUT);
    digitalWrite(stepPin[i], LOW);
    digitalWrite(dirPin[i], LOW);
    motor[i].active = false;
  }
  
  buildRampTables();
  
  Serial.println(F("=== TrivisionCascade v14 — Integer Ramp, No Flutter ==="));
  Serial.println(F("STEP={35,37,39,41} DIR={18,19,20,21}"));
  Serial.println(F("180 per move, pre-computed ramp tables"));
  Serial.println();
  
  delay(2000);
  
  motorsLaunched = 0;
  lastLaunchTime = millis();
  launchMotor(fwdOrder[0], CW);
  motorsLaunched = 1;
  Serial.println(F(">>> CASCADE CW 180 >>>"));
  Serial.println(F("  Motor 1 GO"));
}

void loop() {
  runMotors();
  
  switch (currentState) {
    
    case STATE_CASCADE:
      if (motorsLaunched < NUM_MOTORS) {
        if (millis() - lastLaunchTime >= CASCADE_OFFSET_MS) {
          int m = fwdOrder[motorsLaunched];
          launchMotor(m, CW);
          Serial.print(F("  Motor "));
          Serial.println(m + 1);
          motorsLaunched++;
          lastLaunchTime = millis();
        }
      }
      if (motorsLaunched >= NUM_MOTORS && allDone()) {
        Serial.println(F("  Hold..."));
        holdTimer = millis();
        currentState = STATE_HOLD_AFTER_FWD;
      }
      break;
      
    case STATE_HOLD_AFTER_FWD:
      if (millis() - holdTimer >= HOLD_MS) {
        motorsLaunched = 0;
        lastLaunchTime = millis();
        launchMotor(revOrder[0], CCW);
        motorsLaunched = 1;
        Serial.println(F("<<< REVERSE CASCADE CCW 180 <<<"));
        Serial.print(F("  Motor "));
        Serial.println(revOrder[0] + 1);
        currentState = STATE_REVERSE;
      }
      break;
      
    case STATE_REVERSE:
      if (motorsLaunched < NUM_MOTORS) {
        if (millis() - lastLaunchTime >= CASCADE_OFFSET_MS) {
          int m = revOrder[motorsLaunched];
          launchMotor(m, CCW);
          Serial.print(F("  Motor "));
          Serial.println(m + 1);
          motorsLaunched++;
          lastLaunchTime = millis();
        }
      }
      if (motorsLaunched >= NUM_MOTORS && allDone()) {
        Serial.println(F("  Hold..."));
        holdTimer = millis();
        currentState = STATE_HOLD_AFTER_REV;
      }
      break;
      
    case STATE_HOLD_AFTER_REV:
      if (millis() - holdTimer >= HOLD_MS) {
        motorsLaunched = 0;
        lastLaunchTime = millis();
        launchMotor(fwdOrder[0], CW);
        motorsLaunched = 1;
        Serial.println();
        Serial.println(F(">>> CASCADE CW 180 >>>"));
        Serial.println(F("  Motor 1 GO"));
        currentState = STATE_CASCADE;
      }
      break;
  }
}

void launchMotor(int m, uint8_t dir) {
  motor[m].targetSteps = STEPS_180;
  motor[m].currentStep = 0;
  motor[m].active = true;
  motor[m].lastStepUs = micros();
  motor[m].currentDelayUs = RAMP_START_US;
  
  digitalWrite(dirPin[m], dir);
  delayMicroseconds(5);
}

// --- Zero float math in the step loop ---
void runMotors() {
  unsigned long nowUs = micros();
  
  for (int m = 0; m < NUM_MOTORS; m++) {
    if (!motor[m].active) continue;
    
    if (nowUs - motor[m].lastStepUs >= motor[m].currentDelayUs) {
      // Pulse
      digitalWrite(stepPin[m], HIGH);
      delayMicroseconds(2);
      digitalWrite(stepPin[m], LOW);
      
      motor[m].lastStepUs = nowUs;
      motor[m].currentStep++;
      
      if (motor[m].currentStep >= motor[m].targetSteps) {
        motor[m].active = false;
        continue;
      }
      
      // Look up next delay — no math, just table index
      long i = motor[m].currentStep;
      long decelStart = STEPS_180 - RAMP_STEPS;
      
      if (i < RAMP_STEPS) {
        motor[m].currentDelayUs = rampUpTable[i];
      }
      else if (i >= decelStart) {
        motor[m].currentDelayUs = rampDownTable[i - decelStart];
      }
      else {
        motor[m].currentDelayUs = CRUISE_US;
      }
    }
  }
}

bool allDone() {
  for (int m = 0; m < NUM_MOTORS; m++) {
    if (motor[m].active) return false;
  }
  return true;
}
