/*
 * TrivisionCascade_v15 — 4-Motor 180° Cascade + Reverse
 * 
 * ANTI-STUTTER: Timer1 interrupt drives stepping at fixed 50µs tick.
 * All step timing is quantized to 50µs increments — perfectly uniform.
 * No polling jitter, no digitalWrite overhead in critical path.
 * 
 *   Motor 1: STEP=35 (PC2), DIR=18
 *   Motor 2: STEP=37 (PC0), DIR=19
 *   Motor 3: STEP=39 (PG2), DIR=20
 *   Motor 4: STEP=41 (PG0), DIR=21
 * 
 * Pattern:
 *   Cascade CW 180° (1→2→3→4, 1.25s stagger) → hold 3s →
 *   Reverse CCW 180° (4→3→2→1, 1.25s stagger) → hold 3s → repeat
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#define NUM_MOTORS 4

// Direct port registers for STEP pins (much faster than digitalWrite)
// Pin 35 = PC2, Pin 37 = PC0, Pin 39 = PG2, Pin 41 = PG0
volatile uint8_t* stepPort[NUM_MOTORS] = {&PORTC, &PORTC, &PORTG, &PORTG};
const uint8_t stepBit[NUM_MOTORS]      = {2,      0,      2,      0};

const uint8_t dirPin[NUM_MOTORS] = {18, 19, 20, 21};

#define STEPS_180  1600L
#define CW   HIGH
#define CCW  LOW

#define CASCADE_OFFSET_MS  1250
#define HOLD_MS            3000

// Timing in TICKS (1 tick = 50µs)
#define RAMP_STEPS      250L
#define RAMP_START_TK   70U     // 3500µs / 50 = 70 ticks
#define CRUISE_TK       22U     // 1100µs / 50 = 22 ticks
#define SPEED_RANGE_TK  (RAMP_START_TK - CRUISE_TK)  // 48

// Pre-computed ramp tables in ticks
unsigned int rampUpTable[RAMP_STEPS];
unsigned int rampDownTable[RAMP_STEPS];

void buildRampTables() {
  for (long i = 0; i < RAMP_STEPS; i++) {
    long t2 = i * i;
    long denom = RAMP_STEPS * RAMP_STEPS;
    rampUpTable[i] = RAMP_START_TK - (unsigned int)((t2 * (long)SPEED_RANGE_TK) / denom);
    
    long j = RAMP_STEPS - 1 - i;
    long j2 = j * j;
    rampDownTable[i] = RAMP_START_TK - (unsigned int)((j2 * (long)SPEED_RANGE_TK) / denom);
  }
}

// --- Per-motor state (accessed from ISR — must be volatile) ---
struct MotorState {
  volatile long   targetSteps;
  volatile long   currentStep;
  volatile unsigned int ticksToNext;  // Ticks until next step
  volatile unsigned int tickCounter;  // Current countdown
  volatile bool   active;
};

volatile MotorState motor[NUM_MOTORS];

// --- Timer1 ISR: fires every 50µs --- 
ISR(TIMER1_COMPA_vect) {
  for (uint8_t m = 0; m < NUM_MOTORS; m++) {
    if (!motor[m].active) continue;
    
    motor[m].tickCounter--;
    
    if (motor[m].tickCounter == 0) {
      // STEP pulse — direct port, ~125ns
      *stepPort[m] |= (1 << stepBit[m]);   // HIGH
      // ~62.5ns per NOP at 16MHz, need ~2µs = 32 NOPs... 
      // but at this speed a few cycles is fine for TMC2209
      __asm__ __volatile__("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
                           "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
      *stepPort[m] &= ~(1 << stepBit[m]);  // LOW
      
      motor[m].currentStep++;
      
      if (motor[m].currentStep >= motor[m].targetSteps) {
        motor[m].active = false;
        continue;
      }
      
      // Look up next interval
      long i = motor[m].currentStep;
      long decelStart = STEPS_180 - RAMP_STEPS;
      
      if (i < RAMP_STEPS) {
        motor[m].ticksToNext = rampUpTable[i];
      } else if (i >= decelStart) {
        motor[m].ticksToNext = rampDownTable[i - decelStart];
      } else {
        motor[m].ticksToNext = CRUISE_TK;
      }
      
      motor[m].tickCounter = motor[m].ticksToNext;
    }
  }
}

// --- Sequencer (runs in main loop, non-time-critical) ---
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

void launchMotor(int m, uint8_t dir) {
  digitalWrite(dirPin[m], dir);
  delayMicroseconds(5);
  
  // Set up motor state (disable interrupts briefly for atomic write)
  cli();
  motor[m].targetSteps = STEPS_180;
  motor[m].currentStep = 0;
  motor[m].ticksToNext = RAMP_START_TK;
  motor[m].tickCounter = RAMP_START_TK;
  motor[m].active = true;
  sei();
}

bool allDone() {
  for (int m = 0; m < NUM_MOTORS; m++) {
    if (motor[m].active) return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  
  // Configure STEP pins via DDR (output mode)
  DDRC |= (1 << 2) | (1 << 0);  // Pins 35 (PC2), 37 (PC0)
  DDRG |= (1 << 2) | (1 << 0);  // Pins 39 (PG2), 41 (PG0)
  PORTC &= ~((1 << 2) | (1 << 0));  // Start LOW
  PORTG &= ~((1 << 2) | (1 << 0));
  
  for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(dirPin[i], OUTPUT);
    digitalWrite(dirPin[i], LOW);
    motor[i].active = false;
  }
  
  buildRampTables();
  
  // Configure Timer1 for 50µs interrupt (20kHz)
  // 16MHz / 8 prescaler = 2MHz, count to 100 = 50µs
  cli();
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11);  // CTC mode, prescaler /8
  OCR1A = 99;                              // 50µs period
  TIMSK1 = (1 << OCIE1A);                 // Enable compare match interrupt
  sei();
  
  Serial.println(F("=== TrivisionCascade v15 — Timer ISR, Zero Jitter ==="));
  Serial.println(F("STEP={35,37,39,41} DIR={18,19,20,21}"));
  Serial.println(F("50us tick, direct port pulses, integer ramp tables"));
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
