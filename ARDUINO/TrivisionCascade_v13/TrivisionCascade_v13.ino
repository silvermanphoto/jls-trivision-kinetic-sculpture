/*
 * TrivisionCascade_v13 — 4-Motor 180° Cascade + Reverse Demo
 * 
 * Two-sided picture demo: 180° per move instead of 120°.
 * 
 *   Motor 1: STEP=35, DIR=18
 *   Motor 2: STEP=37, DIR=19
 *   Motor 3: STEP=39, DIR=20
 *   Motor 4: STEP=41, DIR=21
 * 
 * Pattern:
 *   1. Cascade CW: Motors 1→2→3→4, each 180° CW, 1.25 sec stagger
 *   2. Hold 3 sec
 *   3. Reverse cascade CCW: Motors 4→3→2→1, each 180° CCW, 1.25 sec stagger
 *   4. Hold 3 sec
 *   5. Repeat
 */

#define NUM_MOTORS 4

const uint8_t stepPin[NUM_MOTORS] = {35, 37, 39, 41};
const uint8_t dirPin[NUM_MOTORS]  = {18, 19, 20, 21};

#define STEPS_180  1600L   // 180° at 1/8 microstepping (3200 * 0.5)
#define CW   HIGH
#define CCW  LOW

#define CASCADE_OFFSET_MS  1250
#define HOLD_MS            3000

#define RAMP_START_US  3500
#define CRUISE_US      1100      // Slightly faster cruise for longer move
#define RAMP_STEPS     250       // Longer ramp for smoother 180°

struct MotorState {
  long   targetSteps;
  long   currentStep;
  long   rampSteps;
  unsigned int cruiseUs;
  unsigned long lastStepUs;
  unsigned int currentDelayUs;
  bool   active;
};

MotorState motor[NUM_MOTORS];

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
  
  Serial.println(F("=== TrivisionCascade v13 — 180 Cascade + Reverse ==="));
  Serial.println(F("STEP={35,37,39,41} DIR={18,19,20,21}"));
  Serial.println(F("180 per move, two-sided picture demo"));
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
  motor[m].rampSteps = RAMP_STEPS;
  motor[m].cruiseUs = CRUISE_US;
  motor[m].active = true;
  motor[m].lastStepUs = micros();
  motor[m].currentDelayUs = RAMP_START_US;
  
  digitalWrite(dirPin[m], dir);
  delayMicroseconds(5);
}

void runMotors() {
  unsigned long nowUs = micros();
  
  for (int m = 0; m < NUM_MOTORS; m++) {
    if (!motor[m].active) continue;
    
    if (nowUs - motor[m].lastStepUs >= motor[m].currentDelayUs) {
      digitalWrite(stepPin[m], HIGH);
      delayMicroseconds(2);
      digitalWrite(stepPin[m], LOW);
      
      motor[m].lastStepUs = nowUs;
      motor[m].currentStep++;
      
      if (motor[m].currentStep >= motor[m].targetSteps) {
        motor[m].active = false;
        continue;
      }
      
      long i = motor[m].currentStep;
      long total = motor[m].targetSteps;
      long ramp = motor[m].rampSteps;
      long decelStart = total - ramp;
      
      if (i < ramp) {
        float t = (float)i / (float)ramp;
        t = t * t;
        motor[m].currentDelayUs = RAMP_START_US - (unsigned int)(t * (RAMP_START_US - motor[m].cruiseUs));
      }
      else if (i >= decelStart) {
        float t = (float)(i - decelStart) / (float)ramp;
        t = 1.0f - (1.0f - t) * (1.0f - t);
        motor[m].currentDelayUs = motor[m].cruiseUs + (unsigned int)(t * (RAMP_START_US - motor[m].cruiseUs));
      }
      else {
        motor[m].currentDelayUs = motor[m].cruiseUs;
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
