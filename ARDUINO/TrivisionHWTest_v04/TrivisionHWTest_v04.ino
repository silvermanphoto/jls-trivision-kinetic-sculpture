/*
 * TrivisionHWTest v04 — MOTOR IDENTIFICATION TOOL
 * ============================================================
 * Date: 2026-07-26
 *
 * PURPOSE
 *   After the 2026 rewiring, the driver channels may no longer match the
 *   physical left-to-right order of the prisms. This sketch lets you move
 *   ONE channel at a time on command, watch which prism actually turns,
 *   and record that pairing. When all twelve are recorded it prints the
 *   corrected pin tables, ready to paste into the choreography sketch.
 *
 * MOTION
 *   Same engine as the choreography (Timer1 interrupt, S-curve ramps,
 *   direct port writes) so movement feels identical to the finished piece.
 *   0.75 RPM. Every move is exactly one prism face (120 degrees), so a
 *   prism is never left at a partial angle.
 *
 * SAFETY
 *   Only one motor ever moves at a time. Hall sensor pins 2-13 untouched.
 *   Nothing is written to the board's permanent memory.
 *
 * HOW TO USE
 *   Open the Serial Monitor at 115200 baud, set line ending to Newline.
 *   Type "?" for the command list.
 * ============================================================
 */

#include <avr/interrupt.h>
#include <avr/io.h>

#define NUM_MOTORS 12

// === CHANNEL HARDWARE MAP (fixed by the shield — do not reorder) ===
// Channel 0 is the driver on STEP pin 35, channel 11 is STEP pin 46.
volatile uint8_t *chStepPort[NUM_MOTORS] = {
    &PORTC, &PORTC, &PORTC,
    &PORTD,
    &PORTG, &PORTG, &PORTG,
    &PORTL, &PORTL, &PORTL, &PORTL, &PORTL
};
const char chPortName[NUM_MOTORS][6] = {
    "PORTC", "PORTC", "PORTC",
    "PORTD",
    "PORTG", "PORTG", "PORTG",
    "PORTL", "PORTL", "PORTL", "PORTL", "PORTL"
};
const uint8_t chStepBit[NUM_MOTORS] = {2, 1, 0, 7, 2, 1, 0, 7, 6, 5, 4, 3};
const uint8_t chStepPinLabel[NUM_MOTORS] = {35, 36, 37, 38, 39, 40,
                                            41, 42, 43, 44, 45, 46};
const uint8_t chDirPin[NUM_MOTORS] = {18, 19, 20, 21, 22, 23,
                                      24, 25, 26, 27, 28, 29};

#define CCW LOW
#define CW  HIGH

#define MICRO_MULT 1
#define STEPS_120 (1067L * MICRO_MULT)

// === SPEED: 0.75 RPM (authorised ceiling is 1 RPM) ===
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

// === TIMER1 ISR — 50us tick ===
ISR(TIMER1_COMPA_vect) {
  for (uint8_t m = 0; m < NUM_MOTORS; m++) {
    if (!motor[m].active) continue;
    motor[m].tickCounter--;
    if (motor[m].tickCounter == 0) {
      *chStepPort[m] |= (1 << chStepBit[m]);
      __asm__ __volatile__("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
                           "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
      *chStepPort[m] &= ~(1 << chStepBit[m]);
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

// === IDENTIFICATION RECORD ===
// physicalOfChannel[c] = which prism position (1..12, left to right)
// actually turned when channel c was commanded. 0 means "not yet recorded".
uint8_t physicalOfChannel[NUM_MOTORS];

void printCorrectedTables();

// === MOVE HELPERS ===
void launchMotor(int m, uint8_t dir) {
  digitalWrite(chDirPin[m], dir);
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

void jog(int ch, uint8_t dir) {
  Serial.print(F("  Moving channel "));
  Serial.print(ch + 1);
  Serial.print(F("  (STEP pin "));
  Serial.print(chStepPinLabel[ch]);
  Serial.print(F(", DIR pin "));
  Serial.print(chDirPin[ch]);
  Serial.print(F(")  one face "));
  Serial.println(dir == CCW ? F("counter-clockwise...") : F("clockwise..."));

  launchMotor(ch, dir);
  waitMotor(ch);

  Serial.print(F("  Done. Which prism moved, counting from the left? Type  "));
  Serial.print(ch + 1);
  Serial.println(F("=<position>   for example  3=7"));
  Serial.println();
}

// === REPORTING ===
void printMapping() {
  Serial.println();
  Serial.println(F("--- WHAT YOU HAVE RECORDED SO FAR ---"));
  Serial.println(F("  channel  ->  prism position (from the left)"));
  uint8_t recorded = 0;
  for (uint8_t c = 0; c < NUM_MOTORS; c++) {
    Serial.print(F("     "));
    if (c + 1 < 10) Serial.print(' ');
    Serial.print(c + 1);
    Serial.print(F("     ->  "));
    if (physicalOfChannel[c] == 0) {
      Serial.println(F("not tested yet"));
    } else {
      Serial.println(physicalOfChannel[c]);
      recorded++;
    }
  }
  Serial.print(F("  "));
  Serial.print(recorded);
  Serial.println(F(" of 12 recorded."));

  // Duplicate check — two channels claiming the same prism means a mis-entry.
  bool dup = false;
  for (uint8_t a = 0; a < NUM_MOTORS; a++) {
    if (physicalOfChannel[a] == 0) continue;
    for (uint8_t b = a + 1; b < NUM_MOTORS; b++) {
      if (physicalOfChannel[a] == physicalOfChannel[b]) {
        if (!dup) {
          Serial.println();
          Serial.println(F("  CHECK THIS: the same prism is claimed twice."));
          dup = true;
        }
        Serial.print(F("    channels "));
        Serial.print(a + 1);
        Serial.print(F(" and "));
        Serial.print(b + 1);
        Serial.print(F(" both say prism "));
        Serial.println(physicalOfChannel[a]);
      }
    }
  }

  if (recorded == NUM_MOTORS && !dup) {
    printCorrectedTables();
  } else {
    Serial.println();
  }
}

void printCorrectedTables() {
  // Build the reverse lookup: for each prism position, which channel drives it.
  uint8_t channelOfPhysical[NUM_MOTORS];
  for (uint8_t p = 0; p < NUM_MOTORS; p++) channelOfPhysical[p] = 255;
  for (uint8_t c = 0; c < NUM_MOTORS; c++) {
    uint8_t p = physicalOfChannel[c];
    if (p >= 1 && p <= NUM_MOTORS) channelOfPhysical[p - 1] = c;
  }
  for (uint8_t p = 0; p < NUM_MOTORS; p++) {
    if (channelOfPhysical[p] == 255) {
      Serial.println(F("  Cannot build the tables yet — a prism is unclaimed."));
      return;
    }
  }

  Serial.println();
  Serial.println(F("=== ALL TWELVE RECORDED — CORRECTED TABLES BELOW ==="));
  Serial.println(F("Paste these three blocks over the matching lines near the"));
  Serial.println(F("top of the choreography sketch. They are in prism order,"));
  Serial.println(F("left to right, so prism 1 really is the leftmost."));
  Serial.println();

  Serial.println(F("volatile uint8_t *stepPort[NUM_MOTORS] = {"));
  Serial.print(F("    "));
  for (uint8_t p = 0; p < NUM_MOTORS; p++) {
    Serial.print('&');
    Serial.print(chPortName[channelOfPhysical[p]]);
    if (p < NUM_MOTORS - 1) Serial.print(F(", "));
    if (p == 5) { Serial.println(); Serial.print(F("    ")); }
  }
  Serial.println();
  Serial.println(F("};"));

  Serial.print(F("const uint8_t stepBit[NUM_MOTORS] = {"));
  for (uint8_t p = 0; p < NUM_MOTORS; p++) {
    Serial.print(chStepBit[channelOfPhysical[p]]);
    if (p < NUM_MOTORS - 1) Serial.print(F(", "));
  }
  Serial.println(F("};"));

  Serial.print(F("const uint8_t dirPin[NUM_MOTORS] = {"));
  for (uint8_t p = 0; p < NUM_MOTORS; p++) {
    Serial.print(chDirPin[channelOfPhysical[p]]);
    if (p < NUM_MOTORS - 1) Serial.print(F(", "));
  }
  Serial.println(F("};"));
  Serial.println();
  Serial.println(F("Copy the block above somewhere safe before unplugging."));
  Serial.println();
}

void printHelp() {
  Serial.println();
  Serial.println(F("=== COMMANDS ==="));
  Serial.println(F("  1 .. 12      move that channel one prism face, counter-clockwise"));
  Serial.println(F("  -1 .. -12    same, but clockwise (use this to put a prism back)"));
  Serial.println(F("  a            sweep every channel in turn, 1 through 12,"));
  Serial.println(F("               pausing between each so you can see the order"));
  Serial.println(F("  3=7          record it: channel 3 turns the 7th prism from the left"));
  Serial.println(F("  p            show what you have recorded so far"));
  Serial.println(F("  c            clear everything you have recorded and start over"));
  Serial.println(F("  ?            show this list again"));
  Serial.println();
  Serial.println(F("Nothing moves unless you ask it to, and only one prism moves"));
  Serial.println(F("at a time. Every move is exactly one face, so the prisms always"));
  Serial.println(F("land square."));
  Serial.println();
}

void sweepAll() {
  Serial.println();
  Serial.println(F("Sweeping channels 1 through 12, one at a time."));
  Serial.println(F("Watch the row and note the order the prisms move in."));
  Serial.println();
  for (uint8_t c = 0; c < NUM_MOTORS; c++) {
    Serial.print(F("  channel "));
    if (c + 1 < 10) Serial.print(' ');
    Serial.print(c + 1);
    Serial.println(F(" ..."));
    launchMotor(c, CCW);
    waitMotor(c);
    delay(1200);
  }
  Serial.println();
  Serial.println(F("Sweep finished. Record what you saw, for example  1=1"));
  Serial.println();
}

// === SERIAL INPUT ===
char inBuf[16];
uint8_t inLen = 0;

void handleLine(char *s) {
  while (*s == ' ') s++;          // trim leading spaces
  if (*s == '\0') return;

  if (s[0] == '?') { printHelp(); return; }
  if (s[0] == 'p' || s[0] == 'P') { printMapping(); return; }
  if (s[0] == 'a' || s[0] == 'A') { sweepAll(); return; }
  if (s[0] == 'c' || s[0] == 'C') {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) physicalOfChannel[i] = 0;
    Serial.println(F("  Cleared. Nothing recorded."));
    Serial.println();
    return;
  }

  // recording form:  <channel>=<prism position>
  char *eq = strchr(s, '=');
  if (eq != NULL) {
    *eq = '\0';
    int ch = atoi(s);
    int pos = atoi(eq + 1);
    if (ch < 1 || ch > NUM_MOTORS || pos < 1 || pos > NUM_MOTORS) {
      Serial.println(F("  Both numbers need to be between 1 and 12."));
      Serial.println();
      return;
    }
    physicalOfChannel[ch - 1] = (uint8_t)pos;
    Serial.print(F("  Recorded: channel "));
    Serial.print(ch);
    Serial.print(F(" turns the prism at position "));
    Serial.print(pos);
    Serial.println(F(" from the left."));
    printMapping();
    return;
  }

  // movement form: a number, optionally negative
  if (s[0] == '-' || (s[0] >= '0' && s[0] <= '9')) {
    int v = atoi(s);
    uint8_t dir = (v < 0) ? CW : CCW;
    int ch = (v < 0) ? -v : v;
    if (ch < 1 || ch > NUM_MOTORS) {
      Serial.println(F("  Channel numbers run from 1 to 12."));
      Serial.println();
      return;
    }
    jog(ch - 1, dir);
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
    pinMode(chDirPin[i], OUTPUT);
    digitalWrite(chDirPin[i], LOW);
    motor[i].active = false;
    physicalOfChannel[i] = 0;
  }

  buildRampTables();

  cli();
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11);
  OCR1A = 99;
  TIMSK1 = (1 << OCIE1A);
  sei();

  Serial.println();
  Serial.println(F("============================================================"));
  Serial.println(F("  TrivisionHWTest v04 — MOTOR IDENTIFICATION"));
  Serial.println(F("  Find out which driver channel turns which prism."));
  Serial.println(F("============================================================"));
  Serial.println(F("  0.75 RPM. One prism at a time. One face per move."));
  Serial.println(F("  Nothing moves until you type a command."));
  printHelp();
}

// === MAIN LOOP — waits for you, moves nothing on its own ===
void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inLen > 0) {
        inBuf[inLen] = '\0';
        handleLine(inBuf);
        inLen = 0;
      }
    } else if (inLen < sizeof(inBuf) - 1) {
      inBuf[inLen++] = c;
    }
  }
}
