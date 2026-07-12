# Trivision Kinetic Sculpture

Twelve triangular prisms in a row, each carrying strips of three photographic
images, each turned by its own stepper motor inside a CNC-fabricated wooden
frame. As the prisms rotate — slowly, silently, in choreographed patterns — the
sculpture dissolves between three photographs; when all twelve prisms land on
the same face, a single panorama spans the full width.

The mechanism borrows from the trivision billboards of roadside advertising,
rebuilt at gallery scale and choreographed like a musical score. A kinetic
artwork by [Joel Silverman](https://joelsilverman.com), Atlanta. Current image
sets: *Blind Willie McTell*, *Barnard Gulch*, and *Oakland Magnolia*.

## How it works

**Choreography is authored in Blender.** Each prism is a keyframed object in a
Blender scene; the animation curves are extracted and converted into integer
step/timing tables (`steps = degrees/360 × 3200` at 1/8 microstepping, 24 fps
timebase) that compile straight into the Arduino firmware.

**One Arduino Mega 2560 drives all twelve motors.** Each prism has a NEMA 17
stepper (0.9°/step) on a TMC2209 silent driver. The proven motion architecture
runs a 50 µs Timer1 interrupt with integer S-curve acceleration tables and
direct port-register step pulses — no floating point and no `digitalWrite` in
the hot loop — so twelve axes stay smooth and the gallery stays quiet. Motion
is deliberately gentle: the authorized ceiling is 1 RPM.

**Power:** Mean Well LRS-150-24 supplies the motor rail; a buck converter feeds
the Mega. Full electronics detail, pin assignments, and the development history
live in `ARDUINO/TrivisionHandoff.md` — the canonical spec for this project.

## Repository contents

```
ARDUINO/                     all firmware, forked by version, never overwritten:
                             TrivisionChoreo_v7_* (live choreography family),
                             TrivisionHWTest_* (bring-up tests),
                             TrivisionCascade_* (4-motor prototype era),
                             plus motion simulators and TrivisionHandoff.md
CNC Fabrication Plans/       DXF/SVG cut files for the frame
ILLUSTRATOR FILES FOR CNC/   vector source for the cut files
Woodworking Frame Plans/     frame joinery drawings
3D Printing Files/           printed brackets and fittings
Stepper Motor 17HM19-2004S/  motor documentation and CAD
Trivision Kinetic Sculpture Manual-2.pdf   the printed manual
```

This repository tracks code, fabrication plans, and documentation. The heavy
project media (Blender scenes, photographic scans, renders, video) lives
outside git.

The wooden housings were cut with a companion tool also on GitHub:
[CNC-boxcreator](https://github.com/silvermanphoto/CNC-boxcreator), which
generates the CNC toolpath SVGs for the frame boxes.

## Status

A 4-motor prototype has verified the full motion architecture end to end
(smooth S-curve starts and stops at sub-1-RPM speeds, no resonance, inaudible
at gallery distance). The 12-motor firmware expansion and hall-sensor homing
are the active fronts; see `ARDUINO/TrivisionHandoff.md` for the roadmap.
