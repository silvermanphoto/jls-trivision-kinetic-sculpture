---
name: trivision-cad
description: >-
  Fusion 360 and Blender working knowledge for the Trivision sculpture — the CAD
  gotchas learned the hard way and the workflow for bringing Fusion geometry into
  Blender for visualization. Use whenever the work touches Fusion 360, CAD
  geometry, STEP or STL files, the billboard housings, comparing two versions of a
  part, importing or placing imported geometry in Blender, texturing imported
  meshes, or the wood materials on the sculpture frame. Reach for it even when the
  tools are not named — "why did the import land in the wrong place", "compare the
  new billboard to the old one", "why did all my materials go grey", "make the
  frame look like plywood" are all this skill. It carries traps that will
  otherwise cost hours: Fusion script output is not trustworthy, imported meshes
  keep millimetre coordinates, and collections do not move as a group.
---

# Trivision CAD and Visualization

Fusion 360 holds the **canonical** geometry. Blender is for **visualization only**,
never CAD or CAM. Both can run at once — Fusion's bridge listens on port 27182 (the
bare root address returns nothing; the working address ends in `/mcp`), Blender's on
port 9876, started from the BlenderMCP panel inside Blender.

Fusion's internal units are **centimetres** — divide by 2.54 for inches.

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

## Fusion + Blender together (visualization workflow, 2026-06-25)

Workflow to bring Fusion parts into Blender:

- **Export mesh from Fusion, import to Blender.** Blender has no STEP importer.
  Export STL from Fusion (`exportManager.createSTLExportOptions(geom, path)`, geom =
  an Occurrence / Component / BRepBody; `MeshRefinementMedium`, `isBinaryFormat`).
  Fusion STL is in **millimeters**.
- **`wm.stl_import(filepath, global_scale=0.001)`** brings mm → metres, but the 0.001
  goes onto the **object scale** — the **mesh vertices stay in mm**. So `Object`
  texture coordinates and any raw `mesh.vertices.co` are in mm; box-projection mapping
  scale must be `meters_per_unit / tile_metres` (mm mesh → mpu 0.001).
- **Placement-drift gotcha.** After `E.location = center` you MUST call
  `view_layer.update()` before reading `E.matrix_world` for
  `o.matrix_parent_inverse = E.matrix_world.inverted()`. Skip it and the inverse is
  stale (identity), so the child doesn't recenter and lands at its import coords.
- **STL import welds touching bodies** (merges coincident verts), so connected-
  component analysis can NOT recover the individual Fusion bodies from a single STL.
  And per-body STL export gave **inconsistent (body-local) coordinates** for some
  occurrences (V2 per-body union read 74 in vs the true 68.25 in), so the bodies
  can't be reassembled in world space either. To texture **regions** of an imported
  housing, classify **faces by the mesh's own local geometry** (outer-shell faces vs
  inset recess walls) and set `polygon.material_index` — don't rely on per-body data.
- **Material-clobber bug.** Don't `bpy.data.materials.remove(m)`+recreate a *shared*
  material inside a per-object loop — it blanks the slots of objects processed
  earlier (they go default-grey). Build shared materials **once**, then assign.
- **Texturing without UVs.** STL imports have no UVs. Use an Image Texture node with
  `projection='BOX'` fed by `TexCoord.Object → Mapping`, so no unwrap is needed and
  grain stays consistent across faces.

**Collections are NOT transform parents.** Selecting a collection ("folder") and
moving only moves the objects that happen to be selected — there's no rigid-body
container. To make a named group move/rotate/scale as one, add a root **Empty** and
parent the top-level objects to it (Joel's `..._ROOT` pattern; the deleted billboards
used `*_CNC_Frame_ROOT`). "Only part of it moves" = no root parent, or you grabbed a
sub-empty that carries just its branch.

**Original-assembly frame materials** (collection `TRIVISION VERSION 1 `, note the
trailing space — the kinetic assembly, distinct from the imported Fusion housings
named `… (FUSION)`): OUTSIDE FRAME = per-rail **Baltic Birch plywood** + birch back
panel (image `Baltic Birch Plywood Texture 63.5x40.tiff` in `Blender Files/`);
INSIDE FRAME = **oak veneer** (`oak_veneer_01_*`, packed as `/var/folders` temp files
— `file_exists=False` but `has_data=True`, so they still render). The recess interior
is the birch back panel, not black.

## Related

The Fusion billboards and the kinetic assembly are transform-locked at Joel's
hand-set positions — never reposition them. Follow the `blender-projects` skill for
outliner and collection discipline, and `creative-tool-mcp` for the rule about
verifying operator and parameter names against the live tool before writing code.
