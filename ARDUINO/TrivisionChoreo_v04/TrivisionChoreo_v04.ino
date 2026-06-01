/*
 * TrivisionChoreo_v04.ino
 *
 * 12-Motor Exact Unison Sequence
 *
 * ANTI-STUTTER: Timer1 interrupt drives stepping at fixed 50µs tick.
 * All step timing is quantized to 50µs increments — perfectly uniform.
 * No polling jitter, no digitalWrite overhead in critical path.
 *
 * Sequence:
 * ALL 12 Motors rotate backward 120° in exact unison.
 * Pause 10 seconds.
 * Repeat endlessly.
 *
 * This sequence natively physically avoids collisions by ensuring 0 offset
 * between sweeps.
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

// 120 degrees at 3200 steps/rev (1/8 microstepping)
#define STEPS_120 1067L
#define CCW LOW

// Stagger Timing
#define HOLD_MS 10000 // 10 second hold when all 12 are done

// Timing in TICKS (1 tick = 50µs)
// 0.5 RPM = 3 degrees per sec.
// 120 degrees = 40 sec.
// 1067 steps / 40 sec = 26.67 steps/sec
#define RAMP_STEPS 200L
#define RAMP_START_TK 2000U // Start very slow: 100,000µs between steps
#define CRUISE_TK 750U      // 37,500µs / 50µs = 750 ticks (0.5 RPM)
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
// For exact unison, we iterate through all 12 within the 50us budget
ISR(TIMER1_COMPA_vect) {
  for (uint8_t m = 0; m < NUM_MOTORS; m++) {
    if (!motor[m].active)
      continue;

    motor[m].tickCounter--;

    if (motor[m].tickCounter == 0) {
      *stepPort[m] |= (1 << stepBit[m]); // HIGH

      // We don't need a huge NOP delay for each if we are pulsing all 12 in the
      // ISR loop; however, doing NOP per motor is safer for pulse width.
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
enum State { STATE_SWEEP, STATE_HOLD };

State currentState = STATE_SWEEP;
unsigned long holdTimer = 0;

void launchAllMotors(uint8_t dir) {
  // Set all directions first
  for (int m = 0; m < NUM_MOTORS; m++) {
    digitalWrite(dirPin[m], dir);
  }
  delayMicroseconds(5);

  // Launch them all in exact unison
  cli();
  for (int m = 0; m < NUM_MOTORS; m++) {
    motor[m].targetSteps = STEPS_120;
    motor[m].currentStep = 0;
    motor[m].ticksToNext = RAMP_START_TK;
    motor[m].tickCounter = RAMP_START_TK;
    motor[m].active = true;
  }
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

  Serial.println(F("=== TrivisionChoreo v04 — UNISON ==="));
  Serial.println(F("Speed: 0.5 RPM CCW     Distance: 120 deg"));
  Serial.println(F("Stagger: EXACT UNISON (0 offsets) to prevent collisions."));
  Serial.println();

  delay(3000); // 3-second safety wait on powerup

  Serial.println(F(">>> FIRING ALL PRISMS UNISON (CCW) >>>"));
  launchAllMotors(CCW);
}

void loop() {
  switch (currentState) {

  case STATE_SWEEP:
    if (allDone()) {
      Serial.println(F("  All 12 completed. Holding 10 seconds."));
      holdTimer = millis();
      currentState = STATE_HOLD;
    }
    break;

  case STATE_HOLD:
    if (millis() - holdTimer >= HOLD_MS) {
      Serial.println();
      Serial.println(F("=== REPEATING UNISON SWEEP ==="));
      Serial.println(F(">>> FIRING ALL PRISMS UNISON (CCW) >>>"));
      launchAllMotors(CCW);
      currentState = STATE_SWEEP;
    }
    break;
  }
}
