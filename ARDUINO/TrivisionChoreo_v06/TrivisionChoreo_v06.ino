/*
 * TrivisionChoreo_v06.ino
 *
 * 12-Motor Odd/Even Interleaved Cascade — 20% FASTER
 *
 * ANTI-STUTTER: Timer1 interrupt drives stepping at fixed 50µs tick.
 * All step timing is quantized to 50µs increments — perfectly uniform.
 * No polling jitter, no digitalWrite overhead in critical path.
 *
 * SPEED CHANGE from v05:
 *   v05 = 0.50 RPM  →  v06 = 0.60 RPM  (+20%)
 *   Cascade stagger: 2000ms → 1667ms
 *   Hold pause:     10000ms → 8333ms
 *
 * Sequence:
 * 1. ODD MOTORS (1, 3, 5, 7, 9, 11) cascade with 1.67-second stagger.
 *    Because they are spaced 8.9" apart, they safely clear each other.
 * 2. WAIT for all odd motors to finish.
 * 3. EVEN MOTORS (2, 4, 6, 8, 10, 12) cascade with 1.67-second stagger.
 * 4. WAIT for all even motors to finish.
 * 5. Pause ~8.3 seconds.
 * 6. Repeat endlessly.
 */

#include <avr/interrupt.h>
#include <avr/io.h>

#define NUM_MOTORS 12

// Per the Trivision Manual:
volatile uint8_t *stepPort[NUM_MOTORS] = {
    &PORTC, &PORTC, &PORTC,                // 1-3
    &PORTD,                                // 4
    &PORTG, &PORTG, &PORTG,                // 5-7
    &PORTL, &PORTL, &PORTL, &PORTL, &PORTL // 8-12
};

const uint8_t stepBit[NUM_MOTORS] = {2, 1, 0, 7, 2, 1, 0, 7, 6, 5, 4, 3};

const uint8_t dirPin[NUM_MOTORS] = {18, 19, 20, 21, 22, 23,
                                    24, 25, 26, 27, 28, 29};

// --- MICROSTEPPING TWEAK ---
#define MICRO_MULT 1

// 120 degrees base at 1/8 microstep is 1067 steps.
#define STEPS_120 (1067L * MICRO_MULT)

#define CCW LOW

// Stagger Timing — 20% faster
#define CASCADE_OFFSET_MS 1667 // was 2000
#define HOLD_MS 8333           // was 10000

// Timing in TICKS (1 tick = 50µs)
// v05: 0.5 RPM cruise = 750 ticks.  ÷ 1.2 = 625 ticks → 0.6 RPM
// v05: ramp start     = 2000 ticks. ÷ 1.2 = 1667 ticks
#define RAMP_STEPS 200L
#define RAMP_START_TK (1667U / MICRO_MULT)
#define CRUISE_TK (625U / MICRO_MULT)
#define SPEED_RANGE_TK (RAMP_START_TK - CRUISE_TK)

unsigned int rampUpTable[RAMP_STEPS];
unsigned int rampDownTable[RAMP_STEPS];

void buildRampTables() {
  for (long i = 0; i < RAMP_STEPS; i++) {
    long t2 = i * i;
    long denom = RAMP_STEPS * RAMP_STEPS;
    rampUpTable[i] =
        RAMP_START_TK - (unsigned int)((t2 * (long)SPEED_RANGE_TK) / denom);

    long j = RAMP_STEPS - 1 - i;
    long j2 = j * j;
    rampDownTable[i] =
        RAMP_START_TK - (unsigned int)((j2 * (long)SPEED_RANGE_TK) / denom);
  }
}

// --- Per-motor state ---
struct MotorState {
  volatile long targetSteps;
  volatile long currentStep;
  volatile unsigned int ticksToNext;
  volatile unsigned int tickCounter;
  volatile bool active;
};

volatile MotorState motor[NUM_MOTORS];

// --- Timer1 ISR: fires every 50µs ---
ISR(TIMER1_COMPA_vect) {
  for (uint8_t m = 0; m < NUM_MOTORS; m++) {
    if (!motor[m].active)
      continue;

    motor[m].tickCounter--;

    if (motor[m].tickCounter == 0) {
      *stepPort[m] |= (1 << stepBit[m]); // HIGH

      __asm__ __volatile__("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
                           "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");

      *stepPort[m] &= ~(1 << stepBit[m]); // LOW

      motor[m].currentStep++;

      if (motor[m].currentStep >= motor[m].targetSteps) {
        motor[m].active = false;
        continue;
      }

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
  STATE_ODD_CASCADE,
  STATE_WAIT_ODD,
  STATE_EVEN_CASCADE,
  STATE_WAIT_EVEN,
  STATE_HOLD
};

State currentState = STATE_ODD_CASCADE;
int motorIndex = 0;
unsigned long lastLaunchTime = 0;
unsigned long holdTimer = 0;

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
    if (motor[m].active)
      return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  DDRC |= (1 << 2) | (1 << 1) | (1 << 0);
  DDRD |= (1 << 7);
  DDRG |= (1 << 2) | (1 << 1) | (1 << 0);
  DDRL |= (1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3);

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

  cli();
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11);
  OCR1A = 99;
  TIMSK1 = (1 << OCIE1A);
  sei();

  Serial.println(F("=== TrivisionChoreo v06 — ODD/EVEN CASCADE (0.6 RPM) ==="));
  Serial.print(F("Speed: 0.6 RPM CCW (+20% from v05)  Microstep Multiplier: "));
  Serial.println(MICRO_MULT);
  Serial.println(F("Stagger: 1667ms offset per motor.  Hold: 8333ms."));
  Serial.println();

  delay(3000); // 3-second safety wait on powerup

  motorIndex = 0;
  lastLaunchTime = millis();
  launchMotor(motorIndex, CCW);
  Serial.println(F(">>> STARTING ODD CASCADE >>>"));
  Serial.print(F("  Motor "));
  Serial.print(motorIndex + 1);
  Serial.println(F(" launched"));
  motorIndex += 2;
}

void loop() {
  switch (currentState) {

  case STATE_ODD_CASCADE:
    if (motorIndex < NUM_MOTORS) {
      if (millis() - lastLaunchTime >= CASCADE_OFFSET_MS) {
        launchMotor(motorIndex, CCW);
        Serial.print(F("  Motor "));
        Serial.print(motorIndex + 1);
        Serial.println(F(" launched"));
        motorIndex += 2;
        lastLaunchTime = millis();
      }
    } else {
      currentState = STATE_WAIT_ODD;
    }
    break;

  case STATE_WAIT_ODD:
    if (allDone()) {
      Serial.println(F("  Odd cascade finished. Commencing evens."));
      currentState = STATE_EVEN_CASCADE;
      motorIndex = 1;
      lastLaunchTime = millis();
      launchMotor(motorIndex, CCW);
      Serial.println(F(">>> STARTING EVEN CASCADE >>>"));
      Serial.print(F("  Motor "));
      Serial.print(motorIndex + 1);
      Serial.println(F(" launched"));
      motorIndex += 2;
    }
    break;

  case STATE_EVEN_CASCADE:
    if (motorIndex < NUM_MOTORS) {
      if (millis() - lastLaunchTime >= CASCADE_OFFSET_MS) {
        launchMotor(motorIndex, CCW);
        Serial.print(F("  Motor "));
        Serial.print(motorIndex + 1);
        Serial.println(F(" launched"));
        motorIndex += 2;
        lastLaunchTime = millis();
      }
    } else {
      currentState = STATE_WAIT_EVEN;
    }
    break;

  case STATE_WAIT_EVEN:
    if (allDone()) {
      Serial.println(F("  Even cascade finished. Holding 8.3 seconds."));
      holdTimer = millis();
      currentState = STATE_HOLD;
    }
    break;

  case STATE_HOLD:
    if (millis() - holdTimer >= HOLD_MS) {
      Serial.println();
      Serial.println(F("=== REPEATING SEQUENCE ==="));
      motorIndex = 0;
      lastLaunchTime = millis();
      launchMotor(motorIndex, CCW);
      Serial.println(F(">>> STARTING ODD CASCADE >>>"));
      Serial.print(F("  Motor "));
      Serial.print(motorIndex + 1);
      Serial.println(F(" launched"));
      motorIndex += 2;
      currentState = STATE_ODD_CASCADE;
    }
    break;
  }
}
