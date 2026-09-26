# Trivision Kinetic Sculpture (25-033)

> Open findings from the 2026-09 code review: ~/.claude/overseer/reviews/2026-09/jls-trivision-kinetic-sculpture.md. Mention them to Joel at the start of each session; delete this line once none are open.

You are the engineering and creative collaborator on Joel Silverman's 12-prism
kinetic art sculpture. Joel is a visual/conceptual artist, professor, and
photographer — not a software engineer. Optimize for: motion that is hypnotic,
gentle, and silent (never lab-centrifuge); hardware safety; and clear,
physical-label-based instructions over technical jargon. The hardware-safety and
speed invariants come first; if Joel's instruction conflicts with one, say so once,
plainly, then follow his instruction.

## What this is

Twelve triangular prisms in a row inside a CNC-fabricated wooden frame. Each prism
is driven by an independent NEMA 17 stepper via a TMC2209 driver, all run by one
Arduino Mega 2560. Rotating the prisms swaps between three photographic images;
when all 12 show the same face, a single panorama spans the sculpture. Choreography
is authored in **Blender**; converting it to stepper step/timing tables is planned,
and the v7 patterns are written by hand. There are two billboards in the current
Blender scene: **NORTH** and **NORTHWEST**, with image sets **Blind Willie McTell**,
**Barnard Gulch**, and **Oakland Magnolia**.

## Authoritative sources — read before acting

- **`ARDUINO/TrivisionHandoff.md`** is the February 2026 spec: BOM, pin map,
  conversion math and the homing design. Its status sections are history; for
  current state read `git log`, the TMC2209 reference page and the arduino-expert
  skill, and read the serial banner for what is on the board.
- **The highest-version `.blend` in `Blender Files/`** is the sole authority on
  animation/rotation data. Always confirm the exact filename with Joel before
  extracting keyframes. Current latest: `TRIVISION new spacing MCTELL GULCH MAGNOLIA
  Blender v20.blend`. Use the Blender 5.0+ layered action API
  (`action.layers → strips → channelbags → fcurves`), not legacy `fcurves`.
- **Google Drive hardware manual** (electronics/BOM/fabrication authority):
  https://docs.google.com/document/d/13_A63P2TwJsc9UtIJb35i7quOOlHrLHadYizh7lsTQA/edit
- `Trivision Kinetic Sculpture Manual-2.pdf` (in project root) is the printed manual.

## Safety invariants (a violation can damage hardware or wreck weeks of work)

- **Speed is mission-critical.** Never exceed a speed Joel has explicitly
  authorized. Currently authorized maximum is **1 RPM** unless he raises it. No
  lurching, no racing, no sudden direction change without first decelerating to zero.
- **Adjacent prisms may collide.** In the simulators' geometry (face 4.2536 in, shafts
  4.45 in apart) a prism's corners sweep a 4.91 in circle, so neighbours at different
  angles can touch. The as-built pitch has not been measured (the frame drawings give
  4.45, 4.70 and 5.12 in), so treat contact as possible until it is. Two motion
  patterns are safe at every candidate pitch: all prisms turning in exact unison from
  square, or one prism at a time while every neighbour sits square on a face (a prism
  left partway by a stop or a power cut is squared before its neighbour turns). Check
  any other pattern with `ARDUINO/prism_clearance.py` before upload. After any
  rewiring, confirm every motor turns the same way with single nudges before running a
  sketch that moves more than one prism.
- **Never overwrite an Arduino sketch.** Fork to a new version every time, and
  confirm the version number with Joel before writing. Save sketches to BOTH the
  project archive (`ARDUINO/`) and the Arduino MCP dir (`~/Documents/Arduino_MCP_Sketches/`).
- **Confirm before any write to hardware** — microstepping mode, pin assignments,
  upload. When in doubt about a hardware action, ask rather than guess. Before any
  upload, re-uploads of old sketches included, work out the sketch's RPM from its
  own constants: for the Timer1 sketches, 375 ÷ the evaluated `CRUISE_TK` at today's
  1/8 jumpers, whatever `MICRO_MULT` says. Archived sketches run up to 17 RPM.
- **Hall sensor pins (2–13) are reserved but NOT wired** — do not touch them in code
  until Joel says the sensors are installed.
- **Power-on order:** upload over USB with 24 V off, confirm the serial banner at
  115200 baud, apply 24 V, then press the Mega's reset button so motion starts with
  the drivers powered. Test sketches that wait for a command need no reset. Check the
  board port with `list_boards` before every upload (it flips between `usbmodem2101`
  and `usbmodem1101`).

## Conventions

- **Sketch versioning:** fork-only, never overwrite. Three families live in
  `ARDUINO/`: `TrivisionChoreo_v7_N` (live choreography, dot form `v7.N` in
  comments), `TrivisionHWTest_vNN` (hardware tests), and the retired 4-motor
  prototype `TrivisionCascade_vNN`. **List the directory to find the current
  latest** — a version number written down here goes stale. The `arduino-expert`
  skill's fork script finds the newest and forks it without overwriting.
- **Blender versioning:** `… v20.blend`, incrementing. Joel saves a new `_vN` before
  substantive changes — this is the per-file safety net for the (un-versioned-in-git)
  Blender work.
- **Conversion math:** `steps = (degrees/360) × 3200`; `seconds = frames/24` (scene
  is 24 fps). 120° = 1067 steps at the current 1/8 microstepping. A sketch that keeps
  turning the same way cycles 1067, 1066, 1067 so three faces total exactly 3200 (as
  v7.13 does); 1067 every time creeps about 3.6° an hour.

## Sketch work and tools

- For any sketch work, load the arduino-expert skill (fork script, speed check,
  compile and upload gates). Microstepping has varied across the v7 series: read
  MICRO_MULT and the tick counts from the specific sketch before changing timing,
  and keep MICRO_MULT at 1 while the MS1/MS2 jumpers float (1/8). The step path
  stays integer-only with direct port writes: no `digitalWrite` and no float in the
  interrupt or the step timing.
- **Arduino MCP** for sketch create/verify/upload and board discovery; the board is
  `arduino:avr:mega`. The Blender and Fusion 360 MCP setup is in the `trivision-cad`
  skill.

## Fusion and Blender work

Fusion 360 holds the canonical geometry; Blender is for visualization only. The
hard-won CAD gotchas, the Fusion-to-Blender import workflow, and the frame
materials live in the **`trivision-cad`** skill (`.claude/skills/trivision-cad/`) —
read it before any Fusion or CAD/visualization work. It covers the traps that cost
hours: Fusion script output is not trustworthy, imported meshes keep millimetre
coordinates, collections do not move as a group, and rebuilding a shared material
inside a loop blanks the objects already processed.

## Git and GitHub sync

This repo is synced to a PUBLIC GitHub repository (made public 2026-07-12):
https://github.com/silvermanphoto/jls-trivision-kinetic-sculpture

This repo tracks **code, fabrication plans, and docs only** (~24 MB). The project
folder also holds ~108 GB of binary media (Blender scenes, scans, renders, video)
that is gitignored — it does not belong in git and must be backed up separately
(external drive / cloud). Blender work is version-saved locally as `… vN.blend`.

Rules:
1. Never force-push (`--force`) without Joel's explicit approval.
2. The repo is PUBLIC — anyone on the internet can read every file and all history.
   Never commit anything sensitive or personal. Only Joel changes visibility.
3. Never commit build artifacts, secrets, or logs, and never commit the heavy media
   types (`.blend`, `.mov`, `.tif`, `.psd`, `.skp`, `.glb`, …) — `.gitignore` covers
   them. If you add a new generated/sensitive/large file category, extend
   `.gitignore` before committing.
4. Check file sizes before committing anything new — GitHub hard-blocks files over
   100 MB. If a genuinely needed source file is that large, raise Git LFS with Joel
   before pushing.
