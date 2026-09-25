/*
 * TrivisionChoreo v7.13
 *
 * ALL 12 MOTORS IN SYNC — 0.75 RPM CW
 * All launch simultaneously, wait, repeat forever.
 *
 * Forked from v7.10 on 2026-09-25 (v7.11 and v7.12 are the waterfall line).
 * A revolution is 3200 microsteps, which does not split into three equal
 * faces. v7.10 turned 1067 every time, so the prisms crept one microstep per
 * revolution, about 3.6 degrees an hour. The three faces now take 1067, 1066
 * and 1067 steps, so every third turn lands exactly where the first began.
 * Speed is unchanged.
 */

#include <avr/interrupt.h>
#include <avr/io.h>

#define NUM_MOTORS 12

// === PIN MAPPING ===
volatile uint8_t *stepPort[NUM_MOTORS] = {
    &PORTC, &PORTC, &PORTC,
    &PORTD,
    &PORTG, &PORTG, &PORTG,
    &PORTL, &PORTL, &PORTL, &PORTL, &PORTL
};
const uint8_t stepBit[NUM_MOTORS] = {2, 1, 0, 7, 2, 1, 0, 7, 6, 5, 4, 3};
const uint8_t dirPin[NUM_MOTORS] = {18, 19, 20, 21, 22, 23,
                                    24, 25, 26, 27, 28, 29};

#define CCW LOW
#define CW  HIGH

#define MICRO_MULT 1
// 3200 / 3 is not whole: use 1067, 1066, 1067 so three faces total exactly one turn.
const long FACE_STEPS[3] = {1067L * MICRO_MULT, 1066L * MICRO_MULT, 1067L * MICRO_MULT};
uint8_t faceIdx[NUM_MOTORS];   // face each prism sits on, 0..2; zero at boot = square start

// === SPEED: 0.75 RPM ===
#define RAMP_STEPS 200L
#define RAMP_START_TK 1333U
#define CRUISE_TK 500U
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

// === MOTOR STATE ===
struct MotorState {
  volatile long targetSteps;
  volatile long currentStep;
  volatile unsigned int ticksToNext;
  volatile unsigned int tickCounter;
  volatile bool active;
};
volatile MotorState motor[NUM_MOTORS];

// === TIMER1 ISR — 50µs tick ===
ISR(TIMER1_COMPA_vect) {
  for (uint8_t m = 0; m < NUM_MOTORS; m++) {
    if (!motor[m].active) continue;
    motor[m].tickCounter--;
    if (motor[m].tickCounter == 0) {
      *stepPort[m] |= (1 << stepBit[m]);
      __asm__ __volatile__("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
                           "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
      *stepPort[m] &= ~(1 << stepBit[m]);
      motor[m].currentStep++;
      if (motor[m].currentStep >= motor[m].targetSteps) {
        motor[m].active = false;
        continue;
      }
      long i = motor[m].currentStep;
      long decelStart = motor[m].targetSteps - RAMP_STEPS;
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

// === HELPERS ===
void launchMotor(int m, uint8_t dir) {
  long steps;
  if (dir == CW) { steps = FACE_STEPS[faceIdx[m]]; faceIdx[m] = (faceIdx[m] + 1) % 3; }
  else           { faceIdx[m] = (faceIdx[m] + 2) % 3; steps = FACE_STEPS[faceIdx[m]]; }
  digitalWrite(dirPin[m], dir);
  delayMicroseconds(5);
  cli();
  motor[m].targetSteps = steps;
  motor[m].currentStep = 0;
  motor[m].ticksToNext = RAMP_START_TK;
  motor[m].tickCounter = RAMP_START_TK;
  motor[m].active = true;
  sei();
}

void waitForAll() {
  while (true) {
    bool done = true;
    for (int m = 0; m < NUM_MOTORS; m++) {
      if (motor[m].active) { done = false; break; }
    }
    if (done) return;
  }
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

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

  Serial.println(F("=== TrivisionChoreo v7.13 — ALL SYNC ==="));
  Serial.println(F("0.75 RPM CW, all 12 motors simultaneous, loop"));
  Serial.println();

  delay(3000);
}

// === ALL 12 IN SYNC — loops forever ===
void loop() {
  // Launch all 12 at once
  for (int m = 0; m < NUM_MOTORS; m++) {
    launchMotor(m, CW);
  }
  Serial.print(F("ALL GO... "));

  waitForAll();
  Serial.println(F("done."));
}
