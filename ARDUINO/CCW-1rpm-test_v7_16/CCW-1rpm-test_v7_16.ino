/*
 * CCW-1rpm-test v7.16 — ALL TWELVE TOGETHER, ONE GENTLE TURN AT 1 RPM CCW
 *
 * Forked from TrivisionChoreo v7.15 on 2026-10-17, as a first test of all
 * twelve prisms turning in sync. Nothing moves until you type a command.
 *
 *   g   one full turn CCW: eases up over 45 degrees (about 20 s) to 1 RPM,
 *       turns, eases down over the last 45 degrees and lands square on the
 *       face it started from. About 85 seconds.
 *   anything typed while the prisms move eases them all to a stop together
 *       (as gently as they started). They stop at the same angle, which is
 *       safe, and the sketch says how far round they are.
 *   f   finish a stopped turn (CCW, to square)
 *   b   back a stopped turn out (CW, to where it started)
 *   ?   this list
 *
 * Speed: cruise 375 ticks = 1.00 RPM, the authorised ceiling; start and
 * finish 1333 ticks = 0.28 RPM; no step is more than 1.3% faster or slower
 * than the one before. All twelve launch on the same interrupt tick.
 *
 * Before typing g: square every prism on a face by hand, check the direction
 * table below (all LOW until TrivisionHWTest's identification sweep says
 * otherwise), 24 V on. This sketch waits for a command, so no reset press is
 * needed after the 24 V goes on.
 *
 * CCW here is the code's name for one direction-wire level. Which way that
 * looks from the front is settled by watching the first turn.
 */

#include <avr/interrupt.h>
#include <avr/io.h>

#define NUM_MOTORS 12

// === PIN MAPPING ===
// These four tables go together, one entry per prism, left to right. After
// the identification sweep in TrivisionHWTest, paste all four blocks it prints
// over all four tables here, never one alone: the printout is in prism order,
// and a table left in channel order pairs each entry with the wrong motor.
volatile uint8_t *stepPort[NUM_MOTORS] = {
    &PORTC, &PORTC, &PORTC,
    &PORTD,
    &PORTG, &PORTG, &PORTG,
    &PORTL, &PORTL, &PORTL, &PORTL, &PORTL
};
const uint8_t stepBit[NUM_MOTORS] = {2, 1, 0, 7, 2, 1, 0, 7, 6, 5, 4, 3};
const uint8_t dirPin[NUM_MOTORS] = {18, 19, 20, 21, 22, 23,
                                    24, 25, 26, 27, 28, 29};
// Level on each direction wire for a CCW turn; a CW turn writes the opposite.
// The fourth of the four blocks above. Its R marks compare each prism with
// the first one tested, so check one prism by eye too: if every prism turns
// the wrong way, swap every LOW and HIGH.
const uint8_t dirCCW[NUM_MOTORS] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW};

#define CCW LOW
#define CW  HIGH

#define MICRO_MULT 1
#define TURN_STEPS (3200L * MICRO_MULT)   // one full revolution at 1/8 microstepping

// === SPEED: 1.00 RPM cruise, eased in and out over 400 steps (45 degrees) ===
#define RAMP_STEPS 400L
#define RAMP_START_TK 1333U
#define CRUISE_TK 375U
#define SPEED_RANGE_TK (RAMP_START_TK - CRUISE_TK)

// Refuse to build anything over the authorised 1 RPM (50 us tick, 1/8 jumpers).
static_assert(MICRO_MULT == 1, "drivers are jumpered for 1/8 microstepping");
static_assert((long)CRUISE_TK * MICRO_MULT >= 375L, "CRUISE_TK exceeds the 1 RPM ceiling");
static_assert(RAMP_START_TK > CRUISE_TK, "ramp would accelerate backwards");

// One table. Slowing down reads it from the other end, counting the steps
// still to go, so starts and stops are mirror images.
unsigned int rampUpTable[RAMP_STEPS];

void buildRampTables() {
  for (long i = 0; i < RAMP_STEPS; i++) {
    rampUpTable[i] = RAMP_START_TK -
        (unsigned int)((i * i * (long)SPEED_RANGE_TK) / (RAMP_STEPS * RAMP_STEPS));
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

// === TIMER1 ISR — 50us tick ===
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
      // The slower of the speed allowed by the steps taken and by the steps
      // left, so every move, long or short, eases in and out.
      long i = motor[m].currentStep;
      long r = motor[m].targetSteps - 1 - i;   // steps left after the next one
      unsigned int a = (i < RAMP_STEPS) ? rampUpTable[i] : CRUISE_TK;
      unsigned int b = (r < RAMP_STEPS) ? rampUpTable[r] : CRUISE_TK;
      motor[m].ticksToNext = (a > b) ? a : b;
      motor[m].tickCounter = motor[m].ticksToNext;
    }
  }
}

// === HELPERS ===
// pos: how far every prism has turned CCW from the square start, in microsteps.
// All twelve move together, so one number describes them all.
long pos = 0;

bool anyActive() {
  for (int m = 0; m < NUM_MOTORS; m++) {
    if (motor[m].active) return true;
  }
  return false;
}

// All twelve start on the same interrupt tick.
void launchAll(uint8_t dir, long steps) {
  for (int m = 0; m < NUM_MOTORS; m++) {
    digitalWrite(dirPin[m], (dir == CCW) ? dirCCW[m] : !dirCCW[m]);
  }
  delayMicroseconds(5);
  cli();
  for (int m = 0; m < NUM_MOTORS; m++) {
    motor[m].targetSteps = steps;
    motor[m].currentStep = 0;
    motor[m].ticksToNext = RAMP_START_TK;
    motor[m].tickCounter = RAMP_START_TK;
    motor[m].active = true;
  }
  sei();
}

// A stop request: bring the end of the move forward so the ramp down starts
// now, from whatever speed the prisms are doing. Same new end for all twelve.
void easeToStop() {
  cli();
  long i = motor[0].currentStep;
  long left = motor[0].targetSteps - 1 - i;      // steps left after the next one
  if (motor[0].active && left > 0) {
    long j = (i < left) ? i : left;              // where the move is on its ramp
    if (j > RAMP_STEPS - 1) j = RAMP_STEPS - 1;
    long newEnd = i + 1 + j;
    for (int m = 0; m < NUM_MOTORS; m++) motor[m].targetSteps = newEnd;
  }
  sei();
}

// Runs one move to its end, or to a gentle stop if anything is typed.
// Returns false if it was stopped.
bool runAll(uint8_t dir, long steps) {
  launchAll(dir, steps);
  bool stopped = false;
  while (anyActive()) {
    if (!stopped && Serial.available()) {
      while (Serial.available()) Serial.read();
      stopped = true;
      easeToStop();
      Serial.println(F("  stopping gently..."));
    }
  }
  cli();
  long done = motor[0].currentStep;
  sei();
  pos += (dir == CCW) ? done : -done;
  return !stopped;
}

void reportPosition() {
  if (pos == 0 || pos == TURN_STEPS) {
    pos = 0;
    Serial.println(F("  Done. Every prism is square on the face it started from."));
  } else {
    Serial.print(F("  Stopped "));
    Serial.print((pos * 360L) / TURN_STEPS);
    Serial.println(F(" degrees into the turn. All twelve sit at the same angle."));
    Serial.println(F("  Type  f  to finish the turn, or  b  to go back to the start."));
  }
  Serial.println();
}

void printHelp() {
  Serial.println(F("  g  one full turn CCW at 1 RPM, eased in and out (about 85 s)"));
  Serial.println(F("     anything typed while moving stops all twelve gently"));
  Serial.println(F("  f  finish a stopped turn     b  back a stopped turn out"));
  Serial.println();
}

void handleCommand(char c) {
  if (c == '?') { printHelp(); return; }
  if (c == 'g' || c == 'G') {
    if (pos != 0) { Serial.println(F("  The prisms are part way round. Type f or b first.")); Serial.println(); return; }
    Serial.println(F("  One full turn CCW, all twelve together. Type anything to stop."));
    runAll(CCW, TURN_STEPS);
    reportPosition();
    return;
  }
  if (c == 'f' || c == 'F' || c == 'b' || c == 'B') {
    if (pos == 0) { Serial.println(F("  Nothing to finish: the prisms are square.")); Serial.println(); return; }
    bool fin = (c == 'f' || c == 'F');
    Serial.println(fin ? F("  Finishing the turn...") : F("  Going back to the start..."));
    if (fin) runAll(CCW, TURN_STEPS - pos); else runAll(CW, pos);
    reportPosition();
    return;
  }
  Serial.println(F("  Not a command I know. Type ? for the list."));
  Serial.println();
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

  Serial.println(F("=== CCW-1rpm-test v7.16 — ALL TWELVE TOGETHER, 1 RPM CCW ==="));
  Serial.print(F("Reversed in software: "));
  bool anyRev = false;
  for (int m = 0; m < NUM_MOTORS; m++) {
    if (dirCCW[m] == HIGH) { Serial.print(F("prism ")); Serial.print(m + 1); Serial.print(' '); anyRev = true; }
  }
  Serial.println(anyRev ? F("") : F("none"));
  Serial.println(F("Nothing moves until you type a command. Square every prism first."));
  Serial.println();
  printHelp();
}

// === MAIN LOOP — waits for you, moves nothing on its own ===
void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == '\n' || c == '\r' || c == ' ') return;   // line endings of any setting
  delay(5);                                           // let the rest of the line arrive
  while (Serial.available()) Serial.read();           // and drop it, or its line ending
  handleCommand(c);                                   // would stop the move it starts
}
