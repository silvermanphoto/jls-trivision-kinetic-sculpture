/*
 * TrivisionCascade_v09 — 4-Motor Cascade Wave Demo
 * 
 * Replicates the Blender cascade pattern on 4 physical motors:
 *   - Each prism rotates 120° (one face change)
 *   - Each prism starts 1.25 sec (30 frames @ 24fps) after the previous one
 *   - Rotation takes ~1.25 sec with smooth accel/decel
 *   - Hold at each face position before next wave
 * 
 * Pattern per cycle:
 *   Wave 1: All 4 prisms cascade 120° CW (face A → face B)
 *   Hold 3 sec
 *   Wave 2: All 4 prisms cascade 120° CW (face B → face C)
 *   Hold 3 sec
 *   Wave 3: All 4 prisms cascade 120° CW (face C → face A, full revolution)
 *   Hold 5 sec
 *   Repeat
 * 
 * Uses non-blocking millis()-based scheduling so motors can overlap.
 * Each motor runs its own acceleration profile independently.
 *
 * Motor 1: STEP=35, DIR=18
 * Motor 2: STEP=36, DIR=19
 * Motor 3: STEP=37, DIR=20
 * Motor 4: STEP=38, DIR=21
 */

#define NUM_MOTORS 4

const uint8_t stepPin[NUM_MOTORS] = {35, 36, 37, 38};
const uint8_t dirPin[NUM_MOTORS]  = {18, 19, 20, 21};

#define STEPS_PER_FACE  1067L   // 120°
#define STEPS_PER_REV   3200L

#define CW   HIGH
#define CCW  LOW

// Cascade timing: 1.25 sec offset between each prism (30 frames @ 24fps)
#define CASCADE_OFFSET_MS  1250

// Hold between waves
#define HOLD_BETWEEN_MS    3000
#define HOLD_CYCLE_END_MS  5000

// --- Per-motor state machine ---
struct MotorState {
  long   targetSteps;     // Total steps for current move
  long   currentStep;     // Steps completed so far
  long   rampSteps;       // Accel/decel length
  unsigned int cruiseUs;  // Cruise speed
  unsigned long lastStepUs; // Micros of last step pulse
  unsigned int currentDelayUs; // Current delay between steps
  bool   active;          // Is this motor currently moving?
  uint8_t dir;            // Direction for current move
};

MotorState motor[NUM_MOTORS];

// --- Speed parameters ---
#define RAMP_START_US    3500    // Gentle start/stop
#define GALLERY_CRUISE   1172    // 120° in ~1.25 sec
#define GALLERY_RAMP     200     // Ramp steps for face change

// --- Cascade sequencer ---
enum State {
  STATE_WAVE_START,
  STATE_WAVE_RUNNING,
  STATE_HOLD,
  STATE_CYCLE_HOLD
};

State currentState = STATE_WAVE_START;
int currentWave = 0;           // 0, 1, 2 (three 120° waves = full revolution)
int motorsLaunched = 0;        // How many motors have been triggered this wave
unsigned long stateTimer = 0;
unsigned long lastLaunchTime = 0;

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
  
  Serial.println(F("=== TrivisionCascade v09 — 4-Motor Cascade Wave ==="));
  Serial.println(F("Motors 1-4: cascade 120 per wave, 1.25 sec offset"));
  Serial.println(F("3 waves = full revolution. Loops forever."));
  Serial.println();
  
  delay(2000);
  stateTimer = millis();
}

void loop() {
  // Run all active motors (non-blocking step generation)
  runMotors();
  
  switch (currentState) {
    
    case STATE_WAVE_START:
      motorsLaunched = 0;
      lastLaunchTime = millis();
      // Launch first motor immediately
      startMove(0, CW, STEPS_PER_FACE, GALLERY_CRUISE, GALLERY_RAMP);
      motorsLaunched = 1;
      Serial.print(F("WAVE "));
      Serial.print(currentWave + 1);
      Serial.println(F("/3 — cascading..."));
      Serial.println(F("  Motor 1 GO"));
      currentState = STATE_WAVE_RUNNING;
      break;
      
    case STATE_WAVE_RUNNING:
      // Launch next motor after CASCADE_OFFSET_MS
      if (motorsLaunched < NUM_MOTORS) {
        if (millis() - lastLaunchTime >= CASCADE_OFFSET_MS) {
          startMove(motorsLaunched, CW, STEPS_PER_FACE, GALLERY_CRUISE, GALLERY_RAMP);
          Serial.print(F("  Motor "));
          Serial.println(motorsLaunched + 1);
          motorsLaunched++;
          lastLaunchTime = millis();
        }
      }
      
      // Check if all motors are done
      if (motorsLaunched >= NUM_MOTORS && allMotorsDone()) {
        currentWave++;
        if (currentWave >= 3) {
          // Full revolution complete
          Serial.println(F("Full revolution complete. Holding..."));
          currentState = STATE_CYCLE_HOLD;
          stateTimer = millis();
          currentWave = 0;
        } else {
          Serial.println(F("  Wave done. Brief hold..."));
          currentState = STATE_HOLD;
          stateTimer = millis();
        }
      }
      break;
      
    case STATE_HOLD:
      if (millis() - stateTimer >= HOLD_BETWEEN_MS) {
        currentState = STATE_WAVE_START;
      }
      break;
      
    case STATE_CYCLE_HOLD:
      if (millis() - stateTimer >= HOLD_CYCLE_END_MS) {
        Serial.println();
        Serial.println(F("=== NEW CYCLE ==="));
        currentState = STATE_WAVE_START;
      }
      break;
  }
}

// --- Start a move on a specific motor ---
void startMove(int m, uint8_t dir, long steps, unsigned int cruise, long ramp) {
  motor[m].targetSteps = steps;
  motor[m].currentStep = 0;
  motor[m].rampSteps = (steps < ramp * 2) ? steps / 2 : ramp;
  motor[m].cruiseUs = cruise;
  motor[m].dir = dir;
  motor[m].active = true;
  motor[m].lastStepUs = micros();
  motor[m].currentDelayUs = RAMP_START_US;
  
  digitalWrite(dirPin[m], dir);
}

// --- Non-blocking step generator for all motors ---
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
      
      // Check if done
      if (motor[m].currentStep >= motor[m].targetSteps) {
        motor[m].active = false;
        continue;
      }
      
      // Calculate next delay using quadratic easing
      long i = motor[m].currentStep;
      long total = motor[m].targetSteps;
      long ramp = motor[m].rampSteps;
      long decelStart = total - ramp;
      
      if (i < ramp) {
        // Accelerating
        float t = (float)i / (float)ramp;
        t = t * t;
        motor[m].currentDelayUs = RAMP_START_US - (unsigned int)(t * (RAMP_START_US - motor[m].cruiseUs));
      }
      else if (i >= decelStart) {
        // Decelerating
        float t = (float)(i - decelStart) / (float)ramp;
        t = 1.0 - (1.0 - t) * (1.0 - t);
        motor[m].currentDelayUs = motor[m].cruiseUs + (unsigned int)(t * (RAMP_START_US - motor[m].cruiseUs));
      }
      else {
        motor[m].currentDelayUs = motor[m].cruiseUs;
      }
    }
  }
}

// --- Check if all motors are idle ---
bool allMotorsDone() {
  for (int m = 0; m < NUM_MOTORS; m++) {
    if (motor[m].active) return false;
  }
  return true;
}
