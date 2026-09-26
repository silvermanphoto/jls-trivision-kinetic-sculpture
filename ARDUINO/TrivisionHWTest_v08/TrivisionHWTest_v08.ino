/*
 * TrivisionHWTest v08 — POST-REWIRE SMOKE TEST AND MOTOR IDENTIFICATION
 * ============================================================
 * Date: 2026-09-26 (forked from v07, 2026-09-25)
 *
 * WHAT CHANGED FROM v07
 *   - Short moves are smooth. Each step runs at the slower of the speed
 *     allowed by the steps already taken and by the steps still to go, so
 *     a finish or back-out shorter than two ramps speeds up only as far as
 *     it can slow down again, and always ends at the starting crawl. v07
 *     could stop dead from nearly full speed, or drop to under half speed
 *     in one step, on any move under 400 steps (under 50 for a nudge).
 *     Longer moves, full face turns included, run exactly as before.
 *   - With the Serial Monitor's line ending set to "Both NL & CR", the
 *     newline that follows a command's carriage return is taken with it.
 *     v07 read that newline as a stop request, so every move stopped one
 *     step in.
 *   - The corrected-tables printout says four blocks, not three, and
 *     names the choreography sketches that read the fourth (direction).
 *   - The sketch refuses to compile if its speed constants would pass the
 *     authorised 1 RPM.
 *
 * WHAT CHANGED FROM v06 — STOP RECOVERY
 *   - A stopped move is now remembered as one count: how far that prism
 *     sits from the square position it started from, adding every leg it
 *     actually turned. "b" drives the count back to zero; "f" drives it to
 *     the next face, or back to zero for a nudge, which has no next face.
 *     v06 remembered only the leg that was stopped, so finishing a stopped
 *     nudge's first leg, or backing out its return leg, left the prism ten
 *     degrees off while saying it was square.
 *   - While a prism sits off square, every command that moves anything
 *     other than "f" and "b" is refused, naming the channel to square
 *     first, so a neighbour never turns past it and a second stop can never
 *     overwrite the first.
 *   - "That prism is square again" is printed only after checking the
 *     count is back at zero or on the next face.
 *
 * PURPOSE
 *   Joel re-landed all four wires of all twelve motors onto A+ A- B+ B-.
 *   Two questions follow, and this sketch answers both in one pass:
 *     1. Does each motor turn smoothly, and which way round is it?
 *     2. Which prism, counting from the left, does each channel drive?
 *   When all twelve are recorded it prints corrected pin tables AND a
 *   direction table, ready to paste into the choreography sketch.
 *
 * WHAT CHANGED FROM v04
 *   - A plain channel number now does a short NUDGE: about ten degrees
 *     out and straight back, roughly seven seconds for the round trip.
 *     A dead or mis-paired motor shows itself immediately, and the prism
 *     ends exactly where it started. The whole row takes about two
 *     minutes instead of eight. The full 120-degree face turn is still
 *     there, on the "t" commands.
 *   - Any move can be STOPPED. Send anything on the serial line and the
 *     motor eases to a halt in about a second. If that leaves a prism at
 *     a partial angle, "f" finishes the turn and "b" backs it out.
 *   - Rotation direction is recorded alongside position, so a motor whose
 *     coil pair went on backwards is caught here rather than later.
 *
 * MOTION
 *   Same engine as the choreography (Timer1 interrupt, integer S-curve
 *   ramps, direct port writes). Cruise speed 0.75 RPM, unchanged from
 *   v04 and below the authorised 1 RPM ceiling.
 *
 * BEFORE YOU RUN THIS — SET THE CURRENT DIALS
 *   Measure DC volts between ground and the dial's single leg (the one
 *   facing the green screw terminal — the plastic body and slot are
 *   connected to nothing). Aim for 1.00 V, about 1.3 A per phase, which
 *   is ample at this speed. Twelve motors standing still then draw
 *   roughly 80 W of the 156 W supply.
 *
 *   The dial's voltage is generated inside the driver chip and needs 24 V
 *   present. The coloured lights on the board run off the Arduino's 5 V
 *   and prove nothing about the motor supply.
 *
 *   Correction, 29 July 2026: an earlier note here said the dials could
 *   reach 2 A and that twelve motors at rest drew 155 W. Adafruit's
 *   schematic shows the dial physically tops out near 1.5 A, so the real
 *   figure at maximum is closer to 110 W. Full working in
 *   "TMC2209 Wiring and Power Reference.html" in the project root.
 *
 * SAFETY
 *   Only one motor ever moves at a time, and while a stopped prism sits
 *   off square nothing else moves. Hall sensor pins 2-13 untouched.
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
#define STEPS_NUDGE (89L * MICRO_MULT)   // about 10 degrees — visible, quick, reversible

// === SPEED: 0.75 RPM cruise (authorised ceiling is 1 RPM) ===
#define RAMP_STEPS 200L
#define NUDGE_RAMP_STEPS 25L
#define RAMP_START_TK 1333U
#define CRUISE_TK 500U
#define SPEED_RANGE_TK (RAMP_START_TK - CRUISE_TK)

// Refuse to build anything over the authorised 1 RPM (50 us tick, 1/8 jumpers).
static_assert(MICRO_MULT == 1, "drivers are jumpered for 1/8 microstepping");
static_assert((long)CRUISE_TK * MICRO_MULT >= 375L, "CRUISE_TK exceeds the 1 RPM ceiling");
static_assert(RAMP_START_TK > CRUISE_TK, "ramp would accelerate backwards");

// How much slower each step gets while stopping. 40 ticks per step walks the
// speed from cruise back to the starting crawl in about twenty steps — roughly
// one second — so a stop is a gentle ease-off, never a jolt.
#define ABORT_DECEL_TK 40U

// One table per ramp. Slowing down reads the same table from the other end,
// counting the steps still to go, so no separate down table is needed.
unsigned int rampUpTable[RAMP_STEPS];
unsigned int nudgeUpTable[NUDGE_RAMP_STEPS];

static void fillRamp(unsigned int *up, long n) {
  for (long i = 0; i < n; i++) {
    long denom = n * n;
    up[i] = RAMP_START_TK - (unsigned int)((i * i * (long)SPEED_RANGE_TK) / denom);
  }
}

void buildRampTables() {
  fillRamp(rampUpTable, RAMP_STEPS);
  fillRamp(nudgeUpTable, NUDGE_RAMP_STEPS);
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

// Which ramp profile the move in progress is using, and whether a stop has
// been asked for. Only one motor ever moves in this sketch, so one set of
// these serves the whole engine.
volatile long moveRampSteps = RAMP_STEPS;
volatile bool useNudgeRamp = false;
volatile bool abortReq = false;

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
      if (abortReq) {
        // Ease off: lengthen the gap between steps until the motor is back at
        // its crawling start speed, then stop. Continuous from whatever speed
        // it was doing, so there is no lurch.
        unsigned int t = motor[m].ticksToNext;
        if (t + ABORT_DECEL_TK >= RAMP_START_TK) {
          motor[m].active = false;
          continue;
        }
        motor[m].ticksToNext = t + ABORT_DECEL_TK;
        motor[m].tickCounter = motor[m].ticksToNext;
        continue;
      }
      // The slower of the speed allowed by the steps taken and by the steps
      // left, so a move too short for a full ramp up and down never stops
      // hard or jumps in speed. Moves of two ramps or more are unchanged.
      long i = motor[m].currentStep;
      long r = motor[m].targetSteps - 1 - i;             // steps left after the next one
      const unsigned int *up = useNudgeRamp ? nudgeUpTable : rampUpTable;
      unsigned int a = (i < moveRampSteps) ? up[i] : CRUISE_TK;
      unsigned int b = (r < moveRampSteps) ? up[r] : CRUISE_TK;
      motor[m].ticksToNext = (a > b) ? a : b;
      motor[m].tickCounter = motor[m].ticksToNext;
    }
  }
}

// === IDENTIFICATION RECORD ===
// physicalOfChannel[c] = which prism position (1..12, left to right)
// actually turned when channel c was commanded. 0 means "not yet recorded".
uint8_t physicalOfChannel[NUM_MOTORS];
// reversedOfChannel[c] = true if that prism turned the opposite way to the
// first one you tested. A coil pair landed the other way round does this. It is
// harmless only once the printed direction table is in a choreography sketch
// that reads it (v7.14 and later).
bool reversedOfChannel[NUM_MOTORS];

void printCorrectedTables();
bool waitMotorLaunch(int ch, uint8_t dir, long steps, bool isNudge);

// === A PRISM LEFT OFF SQUARE ===
// If a move is stopped part-way the prism is at an angle, which the rest of
// this sketch promises never to leave. offCh is that channel (-1 while every
// prism is square). offSteps is how far it sits from the square position the
// move started from, in microsteps, positive for CCW, summed over every leg it
// actually turned. offDest is where "f" takes it: the next face for a face
// turn, zero for a nudge. While offCh is set, only f and b move anything.
int  offCh = -1;
long offSteps = 0;
long offDest = 0;
bool offNudge = false;

// === MOVE HELPERS ===
void launchMotor(int m, uint8_t dir, long steps, bool nudge) {
  digitalWrite(chDirPin[m], dir);
  delayMicroseconds(5);
  cli();
  useNudgeRamp = nudge;
  moveRampSteps = nudge ? NUDGE_RAMP_STEPS : RAMP_STEPS;
  abortReq = false;
  motor[m].targetSteps = steps;
  motor[m].currentStep = 0;
  motor[m].ticksToNext = RAMP_START_TK;
  motor[m].tickCounter = RAMP_START_TK;
  motor[m].active = true;
  sei();
}

// Waits out a move, watching the serial line the whole time. Anything arriving
// stops the motor. Adds the steps the motor actually took, with their
// direction, to offSteps. Returns true if the move finished, false if it was
// stopped (even when the ease-off happened to carry it to the end).
bool waitMotor(int m, uint8_t dir) {
  bool stopped = false;
  while (motor[m].active) {
    if (!stopped && Serial.available()) {
      while (Serial.available()) Serial.read();
      abortReq = true;
      stopped = true;
    }
  }
  cli();
  long done = motor[m].currentStep;
  sei();
  abortReq = false;
  offSteps += (dir == CCW) ? done : -done;
  return !stopped;
}

// Every move starts from a square prism: start its count at zero.
void beginOp(int ch, long dest, bool isNudge) {
  offCh = ch;
  offSteps = 0;
  offDest = dest;
  offNudge = isNudge;
}

int offDegrees() {
  long a = (offSteps < 0) ? -offSteps : offSteps;
  return (int)((a * 120L) / STEPS_120);
}

// After a stop: keep the record while the prism is off square, and say so.
void reportStop() {
  Serial.println();
  Serial.print(F("  STOPPED. Channel "));
  Serial.print(offCh + 1);
  if (offSteps == 0 || offSteps == offDest) {
    Serial.println(F(" was already square, so there is nothing to finish."));
    Serial.println();
    offCh = -1;
    return;
  }
  Serial.print(F(" is sitting about "));
  Serial.print(offDegrees());
  Serial.println(F(" degrees off square."));
  if (offNudge) {
    Serial.println(F("  Type  f  or  b  to bring it back to where it started."));
  } else {
    Serial.println(F("  Type  f  to finish the turn, or  b  to back it out."));
  }
  Serial.println(F("  Nothing else will move until you do."));
  Serial.println();
}

// Every command that moves a prism asks this first.
bool refuseWhileOff() {
  if (offCh < 0) return false;
  Serial.print(F("  Channel "));
  Serial.print(offCh + 1);
  Serial.print(F(" is still sitting about "));
  Serial.print(offDegrees());
  Serial.println(F(" degrees off square."));
  Serial.println(F("  Type  f  or  b  to square it before moving anything else."));
  Serial.println();
  return true;
}

// A short out-and-back. The prism ends exactly where it started, so this is
// safe to run on every channel in a row.
void nudge(int ch, uint8_t dir) {
  Serial.print(F("  Nudging channel "));
  Serial.print(ch + 1);
  Serial.print(F("  (step wire on terminal "));
  Serial.print(chStepPinLabel[ch]);
  Serial.print(F(", direction wire on terminal "));
  Serial.print(chDirPin[ch]);
  Serial.println(F(")  ten degrees and back..."));

  beginOp(ch, 0, true);
  if (!waitMotorLaunch(ch, dir, STEPS_NUDGE, true)) { reportStop(); return; }
  delay(400);
  if (!waitMotorLaunch(ch, (dir == CCW) ? CW : CCW, STEPS_NUDGE, true)) { reportStop(); return; }
  offCh = -1;

  Serial.print(F("  Back where it started. Which prism moved, counting from the"));
  Serial.println(F(" left?"));
  Serial.print(F("  Type  "));
  Serial.print(ch + 1);
  Serial.print(F("=<position>   for example  "));
  Serial.print(ch + 1);
  Serial.println(F("=7 . Add R if it turned the opposite way to the first one."));
  Serial.println();
}

// Launch plus wait, so the callers above read cleanly.
bool waitMotorLaunch(int ch, uint8_t dir, long steps, bool isNudge) {
  launchMotor(ch, dir, steps, isNudge);
  return waitMotor(ch, dir);
}

// The full 120 degree face change — the old v04 behaviour, kept for when a
// prism genuinely needs to land on a different picture.
void faceTurn(int ch, uint8_t dir) {
  Serial.print(F("  Turning channel "));
  Serial.print(ch + 1);
  Serial.print(F(" a full face "));
  Serial.print(dir == CCW ? F("one way") : F("the other way"));
  Serial.println(F(" — about 38 seconds. Send anything to stop it."));

  beginOp(ch, (dir == CCW) ? STEPS_120 : -STEPS_120, false);
  if (!waitMotorLaunch(ch, dir, STEPS_120, false)) { reportStop(); return; }
  offCh = -1;
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
      Serial.print(physicalOfChannel[c]);
      Serial.println(reversedOfChannel[c] ? F("   (turns the opposite way)")
                                          : F(""));
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
  Serial.println(F("Paste these four blocks over the matching lines near the"));
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

  // The level to write on the direction wire to get the reference rotation.
  // A motor whose coil pair went on backwards needs the opposite level.
  Serial.print(F("const uint8_t dirCCW[NUM_MOTORS] = {"));
  bool anyReversed = false;
  for (uint8_t p = 0; p < NUM_MOTORS; p++) {
    bool rev = reversedOfChannel[channelOfPhysical[p]];
    if (rev) anyReversed = true;
    Serial.print(rev ? F("HIGH") : F("LOW"));
    if (p < NUM_MOTORS - 1) Serial.print(F(", "));
  }
  Serial.println(F("};"));

  Serial.println();
  if (anyReversed) {
    Serial.println(F("Some prisms turn the opposite way to the rest. The fourth"));
    Serial.println(F("block above cancels that out in software, so there is no"));
    Serial.println(F("need to re-wire, but only choreography v7.14 and later read"));
    Serial.println(F("it. An older sketch would turn those prisms into their"));
    Serial.println(F("neighbours."));
  } else {
    Serial.println(F("Every prism turns the same way. Nothing to correct."));
  }
  Serial.println(F("R compares each prism with the first one you tested, so also"));
  Serial.println(F("check one prism by eye: if it turns the wrong way in the"));
  Serial.println(F("choreography, swap every LOW and HIGH in the fourth block."));
  Serial.println();
  Serial.println(F("Copy the blocks above somewhere safe before unplugging."));
  Serial.println();
}

void printHelp() {
  Serial.println();
  Serial.println(F("=== COMMANDS ==="));
  Serial.println(F("  1 .. 12      nudge that channel: ten degrees out and straight"));
  Serial.println(F("               back, about seven seconds. The prism ends exactly"));
  Serial.println(F("               where it started. This is the safe everyday one."));
  Serial.println(F("  -1 .. -12    same nudge, starting the other way"));
  Serial.println(F("  n            nudge every channel in turn, 1 through 12"));
  Serial.println(F("               (about two minutes for the whole row)"));
  Serial.println();
  Serial.println(F("  t1 .. t12    full 120 degree face change, about 38 seconds"));
  Serial.println(F("  t-1 .. t-12  the same face change the other way round"));
  Serial.println(F("  a            face-change every channel in turn (about 8 minutes)"));
  Serial.println();
  Serial.println(F("  3=7          record it: channel 3 turns the 7th prism from the left"));
  Serial.println(F("  3=7R         same, but that prism turned the OPPOSITE way to the"));
  Serial.println(F("               first one you tested"));
  Serial.println(F("  p            show what you have recorded so far"));
  Serial.println(F("  f            finish a turn that you stopped part-way (a stopped"));
  Serial.println(F("               nudge goes back to where it started)"));
  Serial.println(F("  b            back out a turn that you stopped part-way"));
  Serial.println(F("  c            clear everything you have recorded and start over"));
  Serial.println(F("  ?            show this list again"));
  Serial.println();
  Serial.println(F("TO STOP A MOVE: send anything at all — the empty Send button will"));
  Serial.println(F("do. The motor eases to a halt in about a second. Until you type"));
  Serial.println(F("f or b, nothing else will move."));
  Serial.println();
  Serial.println(F("Nothing moves unless you ask it to, and only one prism ever moves"));
  Serial.println(F("at a time."));
  Serial.println();
}

void nudgeAll() {
  Serial.println();
  Serial.println(F("Nudging channels 1 through 12, one at a time, ten degrees each."));
  Serial.println(F("Watch the row. Every prism ends where it started."));
  Serial.println(F("Send anything to stop."));
  Serial.println();
  for (uint8_t c = 0; c < NUM_MOTORS; c++) {
    Serial.print(F("  channel "));
    if (c + 1 < 10) Serial.print(' ');
    Serial.print(c + 1);
    Serial.println(F(" ..."));
    beginOp(c, 0, true);
    if (!waitMotorLaunch(c, CCW, STEPS_NUDGE, true)) { reportStop(); return; }
    delay(400);
    if (!waitMotorLaunch(c, CW, STEPS_NUDGE, true)) { reportStop(); return; }
    offCh = -1;
    delay(700);
  }
  Serial.println();
  Serial.println(F("Row finished. Anything that stayed still, buzzed, or shuddered"));
  Serial.println(F("instead of turning is the one to look at."));
  Serial.println(F("Record what you saw, for example  1=1  or  1=1R"));
  Serial.println();
}

void sweepAll() {
  Serial.println();
  Serial.println(F("Full face change on channels 1 through 12, one at a time."));
  Serial.println(F("This takes about eight minutes. Send anything to stop."));
  Serial.println();
  for (uint8_t c = 0; c < NUM_MOTORS; c++) {
    Serial.print(F("  channel "));
    if (c + 1 < 10) Serial.print(' ');
    Serial.print(c + 1);
    Serial.println(F(" ..."));
    beginOp(c, STEPS_120, false);
    if (!waitMotorLaunch(c, CCW, STEPS_120, false)) { reportStop(); return; }
    offCh = -1;
    delay(1200);
  }
  Serial.println();
  Serial.println(F("Sweep finished. Record what you saw, for example  1=1"));
  Serial.println();
}

// === PICKING UP A STOPPED MOVE ===
// f drives the count to offDest (the next face, or zero for a nudge); b drives
// it to zero. Stopping one of these keeps the record, so f or b can follow.
void resumePartial(bool finish) {
  if (offCh < 0) {
    Serial.println(F("  There is no half-finished turn to deal with."));
    Serial.println();
    return;
  }
  int ch = offCh;
  long target = finish ? offDest : 0;
  long delta = target - offSteps;
  uint8_t dir = (delta > 0) ? CCW : CW;
  long steps = (delta > 0) ? delta : -delta;
  Serial.print(finish ? F("  Finishing") : F("  Backing out"));
  Serial.print(F(" channel "));
  Serial.print(ch + 1);
  Serial.println(F("..."));
  if (steps > 0 && !waitMotorLaunch(ch, dir, steps, offNudge)) { reportStop(); return; }
  if (offSteps == 0 || offSteps == offDest) {
    offCh = -1;
    Serial.println(F("  That prism is square again."));
  } else {
    Serial.print(F("  Channel "));
    Serial.print(ch + 1);
    Serial.println(F(" did not reach a square position. Nothing else will move."));
  }
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
  if (s[0] == 'n' || s[0] == 'N') { if (!refuseWhileOff()) nudgeAll(); return; }
  if (s[0] == 'a' || s[0] == 'A') { if (!refuseWhileOff()) sweepAll(); return; }
  if (s[0] == 'f' || s[0] == 'F') { resumePartial(true); return; }
  if (s[0] == 'b' || s[0] == 'B') { resumePartial(false); return; }
  if (s[0] == 'c' || s[0] == 'C') {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
      physicalOfChannel[i] = 0;
      reversedOfChannel[i] = false;
    }
    Serial.println(F("  Cleared. Nothing recorded."));
    Serial.println();
    return;
  }

  // recording form:  <channel>=<prism position>   with an optional R for reversed
  char *eq = strchr(s, '=');
  if (eq != NULL) {
    *eq = '\0';
    int ch = atoi(s);
    char *rhs = eq + 1;
    int pos = atoi(rhs);
    bool rev = (strchr(rhs, 'R') != NULL) || (strchr(rhs, 'r') != NULL);
    if (ch < 1 || ch > NUM_MOTORS || pos < 1 || pos > NUM_MOTORS) {
      Serial.println(F("  Both numbers need to be between 1 and 12."));
      Serial.println();
      return;
    }
    physicalOfChannel[ch - 1] = (uint8_t)pos;
    reversedOfChannel[ch - 1] = rev;
    Serial.print(F("  Recorded: channel "));
    Serial.print(ch);
    Serial.print(F(" turns the prism at position "));
    Serial.print(pos);
    Serial.print(F(" from the left"));
    Serial.println(rev ? F(", the opposite way round.") : F("."));
    printMapping();
    return;
  }

  // full face change:  t3  or  t-3
  if (s[0] == 't' || s[0] == 'T') {
    int v = atoi(s + 1);
    uint8_t dir = (v < 0) ? CW : CCW;
    int ch = (v < 0) ? -v : v;
    if (ch < 1 || ch > NUM_MOTORS) {
      Serial.println(F("  Channel numbers run from 1 to 12, so t1 through t12."));
      Serial.println();
      return;
    }
    if (refuseWhileOff()) return;
    faceTurn(ch - 1, dir);
    return;
  }

  // nudge: a bare number, optionally negative
  if (s[0] == '-' || (s[0] >= '0' && s[0] <= '9')) {
    int v = atoi(s);
    uint8_t dir = (v < 0) ? CW : CCW;
    int ch = (v < 0) ? -v : v;
    if (ch < 1 || ch > NUM_MOTORS) {
      Serial.println(F("  Channel numbers run from 1 to 12."));
      Serial.println();
      return;
    }
    if (refuseWhileOff()) return;
    nudge(ch - 1, dir);
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
    reversedOfChannel[i] = false;
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
  Serial.println(F("  TrivisionHWTest v08 — SMOKE TEST AND MOTOR IDENTIFICATION"));
  Serial.println(F("  Check every motor turns, and find out which prism each"));
  Serial.println(F("  driver channel actually drives."));
  Serial.println(F("============================================================"));
  Serial.println(F("  0.75 RPM. One prism at a time. Nothing moves until you"));
  Serial.println(F("  type a command, and anything you send stops a move."));
  Serial.println();
  Serial.println(F("  BEFORE YOU START — set the current dials."));
  Serial.println(F("  Unplug that motor, put 24V on, and measure DC volts"));
  Serial.println(F("  between a ground terminal and the dial's SINGLE leg, the"));
  Serial.println(F("  one pointing toward the green screw terminal. Aim for"));
  Serial.println(F("  1.00 volt, which is about 1.3 amps per phase."));
  Serial.println(F("  The dial's plastic body and slot are connected to nothing,"));
  Serial.println(F("  so touching those reads zero however right everything else"));
  Serial.println(F("  is. The little coloured lights are fed from the Arduino,"));
  Serial.println(F("  not from 24V, so they do NOT prove the motor supply is on."));
  printHelp();
}

// === MAIN LOOP — waits for you, moves nothing on its own ===
void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      // "Both NL & CR" sends a newline straight after the carriage return.
      // Take it now, with its command; left waiting, it would stop the move
      // the command starts. A later newline is still a stop request.
      if (c == '\r') {
        delay(2);
        if (Serial.peek() == '\n') Serial.read();
      }
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
