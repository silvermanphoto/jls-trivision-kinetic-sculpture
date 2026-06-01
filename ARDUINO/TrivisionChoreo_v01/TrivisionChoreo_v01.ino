/*
 * TrivisionChoreo_v01.ino
 * 
 * 12-Motor Odd/Even Cascade
 * 
 * ANTI-STUTTER: Timer1 interrupt drives stepping at fixed 50µs tick.
 * All step timing is quantized to 50µs increments — perfectly uniform.
 * No polling jitter, no digitalWrite overhead in critical path.
 *
 * All 12 motors must step backward 120° (1067 steps at 1/8 microstepping).
 * Speed: 0.5 RPM (120° takes ~40 seconds overall, but we have a 2s ramp up/down).
 * Wait 10 seconds.
 * Repeat.
 *
 * TO PREVENT PRISM COLLISIONS:
 * The 12 motors are triggered in an Odd/Even sequence.
 * 1. Prisms 1, 3, 5, 7, 9, 11 cascade.
 * 2. When they finish, Prisms 2, 4, 6, 8, 10, 12 cascade.
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#define NUM_MOTORS 12

// Per the Trivision Manual:
// Motor 1: 35(PC2), Motor 2: 36(PC1), Motor 3: 37(PC0)
// Motor 4: 38(PD7)
// Motor 5: 39(PG2), Motor 6: 40(PG1), Motor 7: 41(PG0)
// Motor 8: 42(PL7), Motor 9: 43(PL6), Motor 10: 44(PL5), Motor 11: 45(PL4), Motor 12: 46(PL3)
volatile uint8_t* stepPort[NUM_MOTORS] = {
  &PORTC, &PORTC, &PORTC, 
  &PORTD, 
  &PORTG, &PORTG, &PORTG, 
  &PORTL, &PORTL, &PORTL, &PORTL, &PORTL
};

const uint8_t stepBit[NUM_MOTORS] = {
  2, 1, 0,
  7,
  2, 1, 0,
  7, 6, 5, 4, 3
};

const uint8_t dirPin[NUM_MOTORS] = {18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29};

// 120 degrees at 3200 steps/rev (1/8 microstepping)
#define STEPS_120  1067L
#define CCW  LOW

// Timing Configuration
#define ODD_EVEN_CASCADE_OFFSET_MS 500  // Delay between starting subsequent odd (or even) motors
#define HOLD_MS                    10000 // 10 second hold between full sequence loops

// Timing in TICKS (1 tick = 50µs)
// 0.5 RPM = 3 degrees per sec.
// 120 degrees = 40 sec.
// 1067 steps / 40 sec = 26.67 steps/sec
// Cruise interval = 1,000,000 / 26.67 = 37,500µs between steps.
// Ticks at cruise = 37,500µs / 50µs = 750 ticks.
#define RAMP_STEPS      200L
#define RAMP_START_TK   2000U    // Start very slow: 100,000µs between steps (0.1 RPM)
#define CRUISE_TK       750U     // 37,500µs / 50µs = 750 ticks (0.5 RPM)
#define SPEED_RANGE_TK  (RAMP_START_TK - CRUISE_TK)  // 1250

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
  volatile unsigned int ticksToNext;  
  volatile unsigned int tickCounter;  
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
      
      // Delay for TMC2209 STEP high time
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
      long decelStart = STEPS_120 - RAMP_STEPS;
      
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

// --- Sequencer ---
enum State {
  STATE_ODD_CASCADE,     // Firing 1, 3, 5, 7, 9, 11
  STATE_WAIT_ODD_DONE,
  STATE_EVEN_CASCADE,    // Firing 2, 4, 6, 8, 10, 12
  STATE_WAIT_EVEN_DONE,
  STATE_HOLD
};

State currentState = STATE_ODD_CASCADE;
int launchIndex = 0;
unsigned long lastLaunchTime = 0;
unsigned long holdTimer = 0;

// Indices (0-11 for arrays)
const int oddMotors[6] =  {0, 2, 4, 6, 8, 10}; // physical 1, 3, 5, 7, 9, 11
const int evenMotors[6] = {1, 3, 5, 7, 9, 11}; // physical 2, 4, 6, 8, 10, 12

void launchMotor(int m, uint8_t dir) {
  digitalWrite(dirPin[m], dir);
  delayMicroseconds(5);
  
  cli();
  motor[m].targetSteps = STEPS_120;
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
  DDRC |= (1 << 2) | (1 << 1) | (1 << 0);       // PC2(35), PC1(36), PC0(37)
  DDRD |= (1 << 7);                             // PD7(38)
  DDRG |= (1 << 2) | (1 << 1) | (1 << 0);       // PG2(39), PG1(40), PG0(41)
  DDRL |= (1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3); // PL7(42)...PL3(46)
  
  PORTC &= ~((1 << 2) | (1 << 1) | (1 << 0));
  PORTD &= ~(1 << 7);
  PORTG &= ~((1 << 2) | (1 << 1) | (1 << 0));
  PORTL &= ~((1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3));
  
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
  OCR1A = 99;                           // 50µs period
  TIMSK1 = (1 << OCIE1A);               // Enable compare match interrupt
  sei();
  
  Serial.println(F("=== TrivisionChoreo v01 — 12-Motor Odd/Even Cascade ==="));
  Serial.println(F("Speed: 0.5 RPM CCW     Distance: 120 deg (1067 steps)"));
  Serial.println(F("Collision-safe mapping (odd then even motors)."));
  Serial.println();
  
  delay(3000); // Wait for Power
  
  launchIndex = 0;
  lastLaunchTime = millis();
  launchMotor(oddMotors[0], CCW);
  launchIndex = 1;
  Serial.println(F(">>> FIRING ODD PRISMS >>>"));
  Serial.print(F("  Motor ")); Serial.println(oddMotors[0] + 1);
}

void loop() {
  switch (currentState) {
    
    case STATE_ODD_CASCADE:
      if (launchIndex < 6) {
        if (millis() - lastLaunchTime >= ODD_EVEN_CASCADE_OFFSET_MS) {
          int m = oddMotors[launchIndex];
          launchMotor(m, CCW);
          Serial.print(F("  Motor "));
          Serial.println(m + 1);
          launchIndex++;
          lastLaunchTime = millis();
        }
      } else {
        currentState = STATE_WAIT_ODD_DONE;
      }
      break;
      
    case STATE_WAIT_ODD_DONE:
      if (allDone()) {
        Serial.println(F("  Odd prisms finished. Launching even prisms..."));
        launchIndex = 0;
        lastLaunchTime = millis();
        launchMotor(evenMotors[0], CCW);
        launchIndex = 1;
        Serial.println(F(">>> FIRING EVEN PRISMS >>>"));
        Serial.print(F("  Motor ")); Serial.println(evenMotors[0] + 1);
        currentState = STATE_EVEN_CASCADE;
      }
      break;
      
    case STATE_EVEN_CASCADE:
      if (launchIndex < 6) {
        if (millis() - lastLaunchTime >= ODD_EVEN_CASCADE_OFFSET_MS) {
          int m = evenMotors[launchIndex];
          launchMotor(m, CCW);
          Serial.print(F("  Motor "));
          Serial.println(m + 1);
          launchIndex++;
          lastLaunchTime = millis();
        }
      } else {
        currentState = STATE_WAIT_EVEN_DONE;
      }
      break;
      
    case STATE_WAIT_EVEN_DONE:
      if (allDone()) {
        Serial.println(F("  Sequence Complete. Holding for 10 seconds."));
        holdTimer = millis();
        currentState = STATE_HOLD;
      }
      break;
      
    case STATE_HOLD:
      if (millis() - holdTimer >= HOLD_MS) {
        Serial.println();
        Serial.println(F("=== REPEATING SEQUENCE ==="));
        launchIndex = 0;
        lastLaunchTime = millis();
        launchMotor(oddMotors[0], CCW);
        launchIndex = 1;
        Serial.println(F(">>> FIRING ODD PRISMS >>>"));
        Serial.print(F("  Motor ")); Serial.println(oddMotors[0] + 1);
        currentState = STATE_ODD_CASCADE;
      }
      break;
  }
}
