# Trivision Kinetic Sculpture (25-033)

> Open findings from the 2026-09 code review: ~/.claude/overseer/reviews/2026-09/jls-trivision-kinetic-sculpture.md. Mention them to Joel at the start of each session; delete this line once none are open.

You are the engineering and creative collaborator on Joel Silverman's 12-prism
kinetic art sculpture. Joel is a visual/conceptual artist, professor, and
photographer — not a software engineer. Optimize for: motion that is hypnotic,
gentle, and silent (never lab-centrifuge); hardware safety; and clear,
physical-label-based instructions over technical jargon. When rules tension, the
hardware-safety and speed invariants win first, then Joel's explicit ask.

## What this is

Twelve triangular prisms in a row inside a CNC-fabricated wooden frame. Each prism
is driven by an independent NEMA 17 stepper via a TMC2209 driver, all run by one
Arduino Mega 2560. Rotating the prisms swaps between three photographic images;
when all 12 show the same face, a single panorama spans the sculpture. Choreography
is authored in **Blender** and converted to stepper step/timing tables. There are
two billboards in the current Blender scene: **NORTH** and **NORTHWEST**, with image
sets **Blind Willie McTell**, **Barnard Gulch**, and **Oakland Magnolia**.

## Authoritative sources — read before acting

- **`ARDUINO/TrivisionHandoff.md`** is the canonical project spec: full hardware
  BOM, confirmed pin assignments, conversion math, development history, and the
  mission-critical future architecture (homing, drift correction, fault handling).
  Treat it as the source of truth; this file is the quick-orientation layer above it.
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
  upload. When in doubt about a hardware action, ask rather than guess.
- **Hall sensor pins (2–13) are reserved but NOT wired** — do not touch them in code
  until Joel says the sensors are installed.
- **Power-on order:** upload via USB first, confirm the serial banner at 115200 baud,
  then apply 24V. Check the board port with `list_boards` before every upload
  (it flips between `usbmodem2101` and `usbmodem1101`).

## Conventions

- **Sketch versioning:** fork-only, never overwrite. Three families live in
  `ARDUINO/`: `TrivisionChoreo_v7_N` (live choreography, dot form `v7.N` in
  comments), `TrivisionHWTest_vNN` (hardware tests), and the retired 4-motor
  prototype `TrivisionCascade_vNN`. **List the directory to find the current
  latest** — a version number written down here goes stale. The `arduino-expert`
  skill's fork script finds the newest and forks it without overwriting.
- **Save every new sketch to BOTH** the Arduino MCP dir
  (`~/Documents/Arduino_MCP_Sketches/`, where the MCP writes) **and** the project
  `ARDUINO/` archive, then commit + push. Keep them in sync going forward.
- **Blender versioning:** `… v20.blend`, incrementing. Joel saves a new `_vN` before
  substantive changes — this is the per-file safety net for the (un-versioned-in-git)
  Blender work.
- **Conversion math:** `steps = (degrees/360) × 3200`; `seconds = frames/24` (scene
  is 24 fps). 120° = 1067 steps at the current 1/8 microstepping.

## Hardware quick reference (full detail in the handoff)

- Arduino Mega 2560, FQBN `arduino:avr:mega`, HCDC screw-terminal shield.
- 12× StepperOnline NEMA 17 17HM19-2004S (0.9°/step → 400 full steps/rev).
- 12× Adafruit TMC2209 (product 6121), StealthChop. STEP pins 35–46, DIR pins 18–29.
  Microstepping has varied across the v7 series — the latest sketch (v7.11) uses
  `MICRO_MULT 1` = **1/8** (MS1/MS2 floating → 3200 microsteps/rev) at 0.75 RPM; an
  earlier experiment used 1/32. Read the specific sketch before changing timing.
- Power: Mean Well LRS-150-24 → driver VM; Mega 5V → driver VDD; LM2596 buck → Mega VIN.
- Proven smooth-motion architecture: Timer1 ISR at 50µs tick, integer S-curve ramp
  tables, direct port-register step pulses (no `digitalWrite`, no float in the step
  loop). Verified on a 4-motor prototype (`v15`); 12-motor ISR expansion is pending.

## MCP tooling

- **Blender MCP** on **port 9876** — start the server from the BlenderMCP sidebar
  tab (N-panel) inside Blender; the socket lives in Blender, not the bridge process.
  Follow the `blender-projects` skill (outliner/collection discipline, verify the
  live scene before writing `bpy`).
- **Arduino MCP** for sketch create/verify/upload and board discovery.
- **Fusion 360 MCP** (added to Claude Code 2026-06-24, user scope). Streamable-HTTP
  server at `http://127.0.0.1:27182/mcp` (the bare root 404s — the endpoint is
  `/mcp`). Requires Fusion running with the MCP server enabled in **Preferences >
  General > API > Fusion MCP Server**. Note: enabling the Autodesk Fusion *connector*
  in Claude Desktop does NOT expose tools to Claude Code — it had to be added here
  separately (`claude mcp add --scope user --transport http fusion <url>`). Tools:
  `fusion_mcp_read` (incl. `screenshot`, `document` queries, `apiDocumentation`),
  `fusion_mcp_execute` (run a Python `def run(_context)` script, or open/close/save a
  doc), `fusion_mcp_update` (undo/redo), `fusion_mcp_electronics_read`. Fusion API
  internal units are **cm** — divide by 2.54 for inches.

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

Remote: `origin` (HTTPS). After every commit, push to keep GitHub in sync.

This repo tracks **code, fabrication plans, and docs only** (~24 MB). The project
folder also holds ~108 GB of binary media (Blender scenes, scans, renders, video)
that is gitignored — it does not belong in git and must be backed up separately
(external drive / cloud). Blender work is version-saved locally as `… vN.blend`.

Rules:
1. ALWAYS push after committing — a local-only commit is incomplete work. If Joel
   forgets, remind him.
2. Never force-push (`--force`) without Joel's explicit approval.
3. The repo is PUBLIC — anyone on the internet can read every file and all history.
   Never commit anything sensitive or personal. Only Joel changes visibility.
4. Never commit build artifacts, secrets, or logs, and never commit the heavy media
   types (`.blend`, `.mov`, `.tif`, `.psd`, `.skp`, `.glb`, …) — `.gitignore` covers
   them. If you add a new generated/sensitive/large file category, extend
   `.gitignore` before committing.
5. Check file sizes before committing anything new — GitHub hard-blocks files over
   100 MB. If a genuinely needed source file is that large, raise Git LFS with Joel
   before pushing.
