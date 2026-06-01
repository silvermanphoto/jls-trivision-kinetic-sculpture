# TRIVISION KINETIC SCULPTURE — Agent Handoff Prompt

**Date:** February 23, 2026
**Author:** Joel Silverman (visual artist, college professor, photographer)
**Purpose:** Complete context transfer for an AI coding agent to continue development of Arduino-controlled kinetic art sculpture.

---

## SECTION 1: WHAT THIS PROJECT IS

A 12-prism kinetic sculpture. Twelve triangular prisms are mounted in a row inside a CNC-fabricated wooden frame. Each prism is driven by an independent NEMA 17 stepper motor via a TMC2209 driver, all controlled by a single Arduino Mega 2560. The prisms rotate to display three different photographic images — when all 12 prisms show the same face, a single panoramic photograph is visible across the entire sculpture. Choreographed rotation sequences create animated transitions between the three images.

The animation choreography is authored in Blender and must be converted to stepper motor commands that preserve the exact timing, rotation amounts, and interpolation character of the Blender keyframes. This is gallery art — the motion must be hypnotic, gentle, and silent. The audience should never perceive the machinery.

---

## SECTION 2: BEHAVIORAL RULES (NON-NEGOTIABLE)

These rules govern how you operate on this project. Violating them will cause hardware damage or destroy weeks of work.

### Code Versioning
- **NEVER overwrite Arduino sketches.** Always fork to a new version before modifying.
- Naming convention: `SketchName_v01`, `SketchName_v02`, etc.
- **Confirm the version number with the user before writing any file.**

### Arduino Code Output
- Save all sketches via the Arduino MCP `write_file` tool.
- MCP sketch directory: `~/Documents/Arduino_MCP_Sketches/`
- Project archive directory: `/Users/joelsilverman/Desktop/2025 Files/25-033 Trivision Kinetic Sculpture/ARDUINO/`
- Save to both locations.
- The main choreography sketch family is `TrivisionCascade`. Hardware test sketches are `TrivisionHWTest`.

### Authority Hierarchy
1. **The Blender file** is the sole authority on animation/rotation data.
   - Path: `/Users/joelsilverman/Desktop/2025 Files/25-033 Trivision Kinetic Sculpture/Blender Files/`
   - Always list this directory, identify the `.blend` file (NOT `.blend1`, `.blend2` backups) with the highest version suffix, and confirm the exact filename with the user before extracting animation data.
   - The current highest-version file as of this writing: `TRIVISION new spacing MCTELL GULCH MAGNOLIA Blender v19.blend`
2. **The Google Drive hardware manual** is the authority on electronics, BOM, and fabrication.
   - URL: https://docs.google.com/document/d/13_A63P2TwJsc9UtIJb35i7quOOlHrLHadYizh7lsTQA/edit
3. **The Google Drive Python script** is reference for Blender geometry/UV/materials only — its animation data is outdated.
   - URL: https://docs.google.com/document/d/1jP-eXVxYtKtYfGwvMH7EY039DtjzSabbf-j0JPjw7dc/edit

### Blender Extraction Pipeline
- When extracting keyframes from Blender files, use the **Blender 5.0+ layered action API** (`action.layers → strips → channelbags → fcurves`), NOT legacy `fcurves`.

### Safety Philosophy
- When in doubt — especially about microstepping mode, wiring pin assignments, or any action that writes to hardware — **ASK rather than guessing.** Confirm before writing to hardware.
- **Speed is MISSION CRITICAL.** Never exceed speeds the user has explicitly authorized. This is an art installation with heavy, delicate prisms — no lurching, no racing, no sudden direction changes without deceleration to zero first.
- The user has described this as "hypnotic and gentle, not a lab centrifuge." Treat every speed and acceleration parameter as safety-critical.

### User Preferences
- Never overwrite code without forking.
- Don't apologize when corrected — just fix it.
- Display web links as full readable URLs, not markdown link syntax.
- Preferred fonts: Futura, Gill Sans, Lato (never Arial).
- The user is a visual/conceptual artist, not a software engineer. Use clear, component-label-based instructions rather than technical abbreviations. When giving wiring instructions, reference physical labels on the boards.

---

## SECTION 3: HARDWARE SPECIFICATION

### Board
- **Arduino Mega 2560** with HCDC screw terminal shield
- FQBN: `arduino:avr:mega`
- Port: Check with `list_boards` before every upload — it can change between `usbmodem2101` and `usbmodem1101`
- Power: 9V from LM2596 buck converter to VIN, also USB during development

### Motors (×12)
- StepperOnline NEMA 17, model 17HM19-2004S
- 0.9° step angle = 400 full steps per revolution
- 2A rated current, 2.9V
- Bipolar, 4-wire

### Drivers (×12)
- Adafruit TMC2209 breakout boards (product 6121)
- StealthChop mode enabled (silent operation for gallery)
- MS1/MS2 floating = **1/8 microstepping** (this is the default and current operating mode)
- **3200 microsteps per revolution** (400 full steps × 8)

### Microstepping Math
- Each microstep = 0.1125°
- 120° rotation = 1067 steps (3200 / 3 = 1066.67, rounded to 1067)
- 240° rotation = 2133 steps
- 360° rotation = 3200 steps

### Pin Assignments (ALL CONFIRMED AND PHYSICALLY WIRED)

| Motor | STEP Pin | DIR Pin | Hall Pin (NOT WIRED) |
|-------|----------|---------|----------------------|
| 1     | 35       | 18      | 2                    |
| 2     | 36       | 19      | 3                    |
| 3     | 37       | 20      | 4                    |
| 4     | 38       | 21      | 5                    |
| 5     | 39       | 22      | 6                    |
| 6     | 40       | 23      | 7                    |
| 7     | 41       | 24      | 8                    |
| 8     | 42       | 25      | 9                    |
| 9     | 43       | 26      | 10                   |
| 10    | 44       | 27      | 11                   |
| 11    | 45       | 28      | 12                   |
| 12    | 46       | 29      | 13                   |

### Power
- **24V motor power:** Mean Well LRS-150-24 (fanless) → all driver VM/GND screw terminals
- **5V logic power:** Arduino Mega 5V → Electrocookie PCB bus rails → all driver VDD (pin 1) and GND (pin 2)
- **9V board power:** LM2596 buck converter (fed from 24V) → Arduino Mega VIN
- **Decoupling:** 100–470µF electrolytic cap on Electrocookie 5V/GND rails
- **Grounding:** All domains (Arduino, drivers, power supplies) share common ground

### Power-On Sequence
1. Upload sketch via USB first (Mega powered by USB alone)
2. Open Serial Monitor at 115200 baud, confirm startup banner
3. Then apply 24V (plug in Mean Well)
4. Mega can safely receive power from both USB and VIN simultaneously

### Hall Effect Sensors (NOT YET INSTALLED)
- A3144 (EPLZON brand), one per prism
- Digital output, active LOW when magnet detected
- Pins 2–13 reserved but **not wired and not to be touched in code** until the user says otherwise

---

## SECTION 4: CONVERSION MATH (BLENDER → ARDUINO)

### Rotation
```
steps = (degrees / 360) × 3200
```

### Timing
```
seconds = frames / 24    (Blender scene is 24 FPS)
```

### Interpolation Types
- **CONSTANT keyframes** = instant position holds (the prism teleports — in practice, move as fast as safely possible)
- **BEZIER keyframes** = acceleration-profiled moves (smooth ramp up, cruise, ramp down)

### Speed Reference
| RPM  | Steps/sec | Time for 1 rev | Time for 120° | Character                |
|------|-----------|----------------|---------------|--------------------------|
| 0.5  | 26.67     | 120 sec        | 40 sec        | Meditative, glacial      |
| 1.0  | 53.33     | 60 sec         | 20 sec        | Slow clock hand          |
| 2.0  | 106.67    | 30 sec         | 10 sec        | Gentle gallery motion    |
| 5.0  | 266.67    | 12 sec         | 4 sec         | Purposeful               |

**Currently authorized maximum: 1 RPM unless the user explicitly raises it.**

---

## SECTION 5: MISSION-CRITICAL REQUIREMENTS (FUTURE IMPLEMENTATION)

These are not yet implemented but define the target architecture. Do not implement without user direction — this section is reference for understanding the design intent.

### Homing Procedure
- Every power-on and periodically during operation, all 12 prisms must home to a known zero position using Hall Effect sensors.
- Drift is unacceptable — the images must align across all 12 prisms.

### Hold Position (Re-home Trigger)
- Defined as: all 12 motor step queues empty and no motor currently stepping.
- Occurs between choreography transitions when every prism is stationary at a target angle (0°, 120°, 240°, or 360°).
- Re-home window: interval between the last prism arriving at its hold angle and the first prism departing for the next transition.
- Re-home all prisms sequentially (not simultaneously) during this window.

### Graceful Sensor Failure Handling
- **Homing timeout:** If a prism doesn't find its Hall sensor within 1.5 full revolutions (540°), mark as FAULT, stop that motor, continue with the other 11. Report via Serial.
- **Sensor disconnect mid-run:** Open-loop step counting is primary position tracker during choreography. Hall sensors are for periodic drift correction only. A disconnected sensor means that prism loses drift correction but keeps running.
- **Periodic re-home:** Between choreography cycles, attempt a soft re-home — rotate slowly toward expected Hall trigger. If confirmed within ±5° of expected position, correct. If not found, flag drift warning but do not abort.
- **Status reporting:** Per-prism status array (OK, DRIFT_WARNING, SENSOR_FAULT). Print status on every re-home cycle via Serial.
- **Degraded mode:** Sculpture continues operating with any number of faulted sensors. Only a faulted motor (stall detection via TMC2209 diag pin) should stop that prism.

### Core Design Philosophy
Hall sensors correct drift, step counting drives position. The sculpture never depends on a sensor being present to know where it is — it always knows where it should be. The sensor just confirms it.

---

## SECTION 6: DEVELOPMENT HISTORY AND CURRENT STATE

### Sketch Families

**TrivisionCascade series (v01–v15):** Choreography development on a 4-motor prototype.
- v01: Single-motor hello world test. Raw `delayMicroseconds()` stepping with linear trapezoidal ramp. Proved basic hardware works.
- v02–v08: Progressive multi-motor cascade development using AccelStepper library. Encountered visible stuttering issues due to floating-point math in the step timing loop.
- v09–v14: Transition to timer-interrupt-driven architecture to eliminate jitter. Moved from `AccelStepper` polling to Timer1 ISR at 50µs tick rate with pre-computed integer ramp tables and direct port register manipulation for step pulses.
- **v15 (latest choreography sketch):** 4-motor cascade with zero-jitter Timer1 ISR. Pattern: cascade CW 180° (motors 1→2→3→4 with 1.25s stagger) → hold 3s → reverse CCW 180° (4→3→2→1) → hold 3s → repeat. Uses direct port writes (`PORTC`, `PORTG`) for ~125ns step pulses. Pre-computed S-curve ramp tables (250-step ramp, 70→22 tick range). This is the proven smooth-motion architecture.

**TrivisionHWTest series (v01–v03):** 12-motor hardware verification on the full sculpture.
- v01: Sequential test of all 12 motors — forward 2 revolutions, pause 15s, backward 1 revolution. 5-second stagger, 1 RPM, AccelStepper with trapezoidal ramp. All 12 motors confirmed operational.
- v02: Continuous forward rotation at 0.5 RPM, all 12 motors, endless loop.
- **v03 (currently uploaded and running):** Each prism rotates BACKWARD 120° at 0.5 RPM, pauses 10 seconds, repeats forever. 5-second stagger. AccelStepper with acceleration profiling.

### Key Technical Lessons Learned
1. **Floating-point math in step-generation loops causes visible stuttering.** The Arduino's FPU-less AVR takes variable time to compute float operations, creating timing jitter that manifests as audible and visible motion artifacts. Solution: pre-compute all timing values as integer tick counts in lookup tables, drive stepping from a fixed-rate timer ISR.
2. **AccelStepper's `run()` function works well at slow speeds (≤2 RPM)** where the polling interval is long relative to the `loop()` cycle time. At higher speeds or with many motors, the polling approach introduces jitter. For the final choreography at gallery speeds, the Timer1 ISR approach from v15 is the proven solution.
3. **Direct port register manipulation** (`PORTx |= (1 << bit)`) generates step pulses in ~125ns vs. `digitalWrite()` at ~6µs. Critical for the ISR approach where multiple motors must be serviced within a single 50µs tick.
4. **The stagger timing bug:** When the sketch boots via USB upload but motors don't have 24V power yet, the stagger countdown runs during the power-off period. By the time 24V arrives, all stagger windows have passed and all motors start simultaneously. Solution: either reset the Mega after applying 24V, or add a "wait for first step command" gate.

### What's Been Verified on Hardware (as of Feb 23, 2026)
- All 12 motors respond to STEP/DIR signals ✓
- All 12 TMC2209 drivers operational in StealthChop mode ✓
- Direction control (CW/CCW) works on all 12 ✓
- 3200 steps = 1 revolution confirmed ✓
- 1067 steps = 120° confirmed ✓
- Smooth acceleration ramp (no lurching) at 0.5 and 1.0 RPM ✓
- 24V power distribution to all 12 drivers ✓
- 5V logic distribution to all 12 drivers ✓
- No thermal issues observed during extended operation ✓

### What Has NOT Been Tested Yet
- Speeds above 1 RPM on the full 12-motor assembly
- Timer1 ISR approach on 12 motors (only tested on 4-motor prototype)
- Hall effect sensor homing (sensors not physically installed)
- Choreography playback from Blender keyframe data on 12 motors
- Extended multi-hour continuous operation

---

## SECTION 7: ARCHITECTURE NOTES FOR 12-MOTOR ISR EXPANSION

The v15 Timer1 ISR architecture was proven on 4 motors using direct port register writes. Expanding to 12 motors requires mapping all 12 STEP pins to their AVR port registers:

| Motor | STEP Pin | AVR Port | Bit |
|-------|----------|----------|-----|
| 1     | 35       | PORTC.2  | 2   |
| 2     | 36       | PORTC.1  | 1   |
| 3     | 37       | PORTC.0  | 0   |
| 4     | 38       | PORTD.7  | 7   |
| 5     | 39       | PORTG.2  | 2   |
| 6     | 40       | PORTG.1  | 1   |
| 7     | 41       | PORTG.0  | 0   |
| 8     | 42       | PORTL.7  | 7   |
| 9     | 43       | PORTL.6  | 6   |
| 10    | 44       | PORTL.5  | 5   |
| 11    | 45       | PORTL.4  | 4   |
| 12    | 46       | PORTL.3  | 3   |

**IMPORTANT:** Verify these port mappings against the Arduino Mega 2560 pinout diagram before using. The pin-to-port mapping above was derived from the ATmega2560 datasheet and should be confirmed with the uploaded `A000067fullpinout.pdf` in the project files.

The ISR must service all 12 motors within the 50µs tick window. At 16MHz, that's 800 CPU cycles per tick. Each motor's ISR body is roughly 20–30 cycles (decrement counter, conditional branch, port write, table lookup), so 12 motors ≈ 240–360 cycles — well within budget.

---

## SECTION 8: REFERENCE DOCUMENTS

### In This Claude Project (uploaded PDFs)
- `TMC2209_datasheet_rev1_09.pdf` — Driver IC datasheet
- `A000067datasheet.pdf` — Arduino Mega 2560 datasheet
- `A000067fullpinout.pdf` — Mega 2560 complete pinout diagram
- `A000067schematics.pdf` — Mega 2560 board schematics
- `17HM192004S_Full_Datasheet.pdf` — NEMA 17 motor specs
- `17HM192004S_Torque_Curve.pdf` — Motor torque curves
- `lrs150.pdf` — Mean Well LRS-150-24 power supply datasheet
- `EPLZON_3Pins_A3144...` — Hall effect sensor specs
- `Screenshot_20260222_at_8_11_12_PM.png` — Photo of HCDC screw terminal shield

### On Google Drive
- Hardware Manual: https://docs.google.com/document/d/13_A63P2TwJsc9UtIJb35i7quOOlHrLHadYizh7lsTQA/edit
- Blender Generation Script (geometry/UV reference only, animation data outdated): https://docs.google.com/document/d/1jP-eXVxYtKtYfGwvMH7EY039DtjzSabbf-j0JPjw7dc/edit

### On Filesystem
- Blender files: `/Users/joelsilverman/Desktop/2025 Files/25-033 Trivision Kinetic Sculpture/Blender Files/`
- Arduino sketches: `/Users/joelsilverman/Desktop/2025 Files/25-033 Trivision Kinetic Sculpture/ARDUINO/`
- MCP sketches: `~/Documents/Arduino_MCP_Sketches/` (TrivisionCascade_v01–v15, TrivisionHWTest_v01–v03)

### Web References
- Arduino Mega 2560 docs: https://docs.arduino.cc/hardware/mega-2560/
- Motor product page: https://www.omc-stepperonline.com/nema-17-bipolar-0-9deg-46ncm-65-1oz-in-2a-2-9v-42x42x48mm-4-wires-17hm19-2004s
- TMC2209 breakout (Adafruit 6121): https://www.adafruit.com/product/6121
- HCDC screw terminal shield: https://www.amazon.com/dp/B0F1M24KG3

---

## SECTION 9: NEXT STEPS (PROBABLE)

These are likely upcoming tasks based on the project trajectory. The user will direct which to pursue:

1. **Extract keyframe data from the Blender v19 file** and convert to a step/timing table for all 12 prisms.
2. **Expand the Timer1 ISR architecture from v15 to 12 motors** with correct port register mappings for all 12 STEP pins.
3. **Build a choreography playback engine** that reads the Blender-derived step/timing table and executes it on all 12 motors with the proven zero-jitter ISR approach.
4. **Install and integrate Hall effect sensors** for homing and periodic drift correction, following the graceful failure architecture described in Section 5.
5. **Long-duration reliability testing** — the sculpture must run unattended for hours in a gallery setting.

---

*End of handoff document.*
