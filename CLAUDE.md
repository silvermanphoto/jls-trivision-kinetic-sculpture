# Trivision Kinetic Sculpture (25-033)

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

- **Sketch versioning:** fork-only, never overwrite. The live choreography family
  is **`TrivisionChoreo_v7_N`** (dot form `v7.N` in comments) — **latest is
  `TrivisionChoreo_v7_11`** (Feb 26 2026). Hardware-test family is `TrivisionHWTest`;
  the older 4-motor prototype family is `TrivisionCascade_v01–v15`.
- **Save every new sketch to BOTH** the Arduino MCP dir
  (`~/Documents/Arduino_MCP_Sketches/`, where the MCP writes) **and** the project
  `ARDUINO/` archive, then commit + push. As of 2026-06-01 the archive was
  re-synced from the MCP dir, so `ARDUINO/` now mirrors the full sketch history
  (`TrivisionCascade_v01–v15`, `TrivisionChoreo_v01–v07` and `v7_0–v7_11`,
  `TrivisionHWTest_v01–v03`). Keep them in sync going forward.
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

## Fusion / CAD gotchas (learned 2026-06-24)

- **A broken third-party MCP shim spams script stdout.** A failing AuraFriday
  "Control your Mac" / `mcp-link-server` reconnect loop injects ~32 KB of
  `[MCP] [RECONNECT]…` log noise into the stdout captured from Fusion `execute`
  scripts (one run ballooned to 190 KB / "exceeds max tokens"). Workaround: have the
  Fusion Python **write results to a file** (`open('/tmp/foo.json','w')`) and read /
  process them locally with Bash, or print unique `MARK_…` lines and grep them out.
  Don't rely on clean stdout from Fusion scripts.
- **Importing a STEP and moving it (two-pass).** STEP comes in as dumb BRep (no
  parametric tree). To place an import at a known offset without fighting parametric
  move-features: import once into a temp `addNewComponent(identity)` to measure its
  native bounding box, `deleteMe()` the temp, then re-import into
  `root.occurrences.addNewComponent(T)` where `T` is the desired translation — the
  transform is baked at creation, so no Move feature is needed. Use
  `importManager.createSTEPImportOptions(path)` + `importToTarget2(opts, comp)`.
- **External labels** = a sketch on `xYConstructionPlane` with
  `sketchTexts.createInput2(text, height_cm)` + `input.setAsMultiLine(p1, p2, hAlign,
  vAlign, spacing)`, then `texts.add(input)`.
- **CAD version diff approach.** With no feature tree, compare geometry: OD bounding
  box, body inventory, planar-face Z/Y levels, and cylindrical faces (holes) grouped
  by axis + diameter, all referenced to each part's own min corner. Cluster holes
  into rows/columns to recover pitch. (2026-06-24: compared `Trivision PLS.step` as
  "Version 1" against the live "06.24.26 Trivision Billboard v2" — V1 is the smaller
  earlier billboard; V2 widened +5.8 in / deepened +1.0 in / same height, re-pitched
  the centered 12-station hole grid +0.42 in per station, and added 12 bottom tabs +
  4 one-inch cross-bores.)

## Git and GitHub sync

This repo is synced to a PRIVATE GitHub repository:
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
3. The repo is PRIVATE. Do not change its visibility.
4. Never commit build artifacts, secrets, or logs, and never commit the heavy media
   types (`.blend`, `.mov`, `.tif`, `.psd`, `.skp`, `.glb`, …) — `.gitignore` covers
   them. If you add a new generated/sensitive/large file category, extend
   `.gitignore` before committing.
5. Check file sizes before committing anything new — GitHub hard-blocks files over
   100 MB. If a genuinely needed source file is that large, raise Git LFS with Joel
   before pushing.

## Working style

- **Scope:** make the change asked for and what it clearly requires; surface
  worthwhile extras as suggestions rather than building them unasked.
- **Verbosity:** concise by default; expand only for genuinely complex work or when
  Joel asks for depth.
- Don't apologize when corrected — just fix it. Display web links as full readable
  URLs. Fonts: Futura, Gill Sans, Lato — never Arial.
