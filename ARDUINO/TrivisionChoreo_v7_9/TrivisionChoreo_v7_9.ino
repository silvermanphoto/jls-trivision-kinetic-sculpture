/*
 * TrivisionChoreo v7.9
 *
 * Sequential Cascade — M1 through M12, one at a time
 * 1.0 RPM CW, S-curve ramps, seamless loop forever.
 * Each motor fires the instant the previous one finishes.
 *
 * FIX: Swapped CW/CCW polarity — dir HIGH wasn't working,
 * so CW is now LOW and CCW is HIGH on these TMC2209 drivers.
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

// === DIRECTION POLARITY (swapped — LOW=CW on these drivers) ===
#define CW  LOW
#define CCW HIGH

#define MICRO_MULT 1
#define STEPS_120 (1067L * MICRO_MULT)

// === SPEED: 1.0 RPM ===
#define RAMP_STEPS 200L
#define RAMP_START_TK 1000U
#define CRUISE_TK 375U
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

void waitMotor(int m) {
  while (motor[m].active) { /* spin */ }
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

  Serial.println(F("=== TrivisionChoreo v7.9 — SEQUENTIAL CASCADE ==="));
  Serial.println(F("1.0 RPM CW (polarity swapped), M1→M12, seamless loop"));
  Serial.println();

  delay(3000);
}

// === SEQUENTIAL CASCADE — loops forever ===
void loop() {
  for (int m = 0; m < NUM_MOTORS; m++) {
    launchMotor(m, CW);
    Serial.print(F("M"));
    Serial.print(m + 1);
    Serial.print(F(" "));
    waitMotor(m);
  }
  Serial.println(F("| cycle"));
}
