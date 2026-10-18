# Beads asset and geometry reconnaissance

Status: read-only reconnaissance, 2026-10-17. This report records facts from the supplied source files and a proposed mapping contract. It does not relicense, copy, or distribute the supplied artwork.

## Source receipts

| Source | Type and dimensions | SHA-256 |
|---|---|---|
| `/Users/danielraffel/Code/eurorack/beads/hardware_design/panel/beads.ai` | PDF-compatible Illustrator CC 23.1; PDF 1.5; 1 page; 200.693 x 364.252 pt; metadata says maximum 70.800136 x 128.500011 mm | `bdc4e394703fa99e6e7997683e1b4dee2bada5cc6eff95af36d40086f6f86c5e` |
| `/Users/danielraffel/Code/eurorack/beads/hardware_design/panel/beads.dwg` | AutoCAD 2018/2019/2020 | `82f184f0cfef7f5f09cda6f2aa76aa4642a47304dd54051b6fa2509161c38ed7` |
| `/Users/danielraffel/Code/eurorack/beads/hardware_design/beads.dwg` | AutoCAD 2018/2019/2020 | `747ff909602a9214fdaaa1f48b0c28cee296cefc326d236f4ee1c98eb641e81f` |
| `/Users/danielraffel/Code/eurorack/beads/hardware_design/beads.dxf` | AutoCAD Drawing Exchange Format, AC1015/2000; `$INSUNITS=4` (millimetres); extents `(-36.474411, -26.849318)` to `(142.28, 158.816421)` because dimensions/construction geometry are included | `caa755bf680939221683d9b3113fa467a4f1cf382780e80b074c32e785639667` |
| `/Users/danielraffel/Documents/beads-panel.png` | RGB PNG, 837 x 1518 px, 300 dpi metadata; Affinity Photo 2.6.5 appears in XMP history | `6ba7ede50ad6baab8d6c96bb41eb51355026ac9a3fd012fc6fe0ff96214664db` |

The Beads checkout was at `f4cfa7697095a28d292bff758beb51deb49b5a86`; its `stmlib` submodule is pinned at `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec` but is locally uninitialized. The DWG/DXF files are untracked in that checkout, so their provenance must remain an explicit input receipt.

## AI/PDF conversion probe

`file` identifies `beads.ai` as a PDF. `pdfinfo` reports Adobe Illustrator CC 23.1 (Macintosh), Adobe PDF library 15.00, PDF 1.5, no encryption, no JavaScript, no text pages, and a 200.693 x 364.252 pt page. Poppler `pdftocairo version 26.07.0` converted it successfully with:

```text
pdftocairo -svg beads.ai beads-panel.svg
pdftocairo -png -r 144 beads.ai beads-panel-render
```

The SVG has `viewBox="0 0 200.693 364.252"` and retains vector paths and red cutout geometry. The PNG probe is 402 x 729 px at 144 dpi. Conversion is suitable for authoring/build-time normalization, subject to a source-feature diagnostic and an affine fit. The shipped plug-in must consume a checked-in normalized asset; it must not depend on Affinity, Illustrator, or Poppler at runtime.

## DXF geometry findings

The DXF inventory used by the checked-in map has 24 positive-radius `CIRCLE` entities on layer `Knobs` (the earlier reconnaissance count of 27 included construction/reference circles):

- 3 major ornate positions, radius 9.25 mm (diameter 18.5 mm): density `(35.735,104.4)`, time `(11.915,90.1)`, pitch `(59.515,90.1)`.
- 2 major positions, radius 7.2 mm (diameter 14.4 mm): size `(24.615,67.9)`, shape `(46.865,67.9)`.
- 12 small positions, radius 4.4 mm (diameter 8.8 mm), in two six-position rows at y=31.999 and y=17.4 mm.
- 7 auxiliary positions, radius 3.0 mm (diameter 6.0 mm): `(18.565,47.9)`, `(30.015,47.9)`, `(41.465,47.9)`, `(52.915,47.9)`, `(7.115,67.9)`, `(64.365,67.9)`, `(64.365,51.7)`.

The zero-radius `Knobs` circles are construction/reference points and must not be rendered as controls. The DXF also contains panel, PCB, construction, dimension, scale-help, and proxy entities; it is a mechanical/placement source, not a knob material or shading source.

The complete machine-readable inventory is in [`provenance/control-map.json`](../../provenance/control-map.json), generated from the retained [`provenance/control-map.draft.json`](../../provenance/control-map.draft.json). The accompanying [`provenance/knob-skin-manifest.json`](../../provenance/knob-skin-manifest.json) keeps visual material calibration separate from placement geometry; both remain rights-gated. The DXF panel dimensions and AI page dimensions establish the initial affine transform: x maps linearly by `200.693 / 70.8` pt/mm and y maps by `(128.5 - y_mm) * (364.252 / 128.5)` pt, so the UI uses a top-left origin after the explicit vertical flip. A screenshot overlay must still verify residuals before shipping; semantic IDs for small/auxiliary controls remain provisional.

## Proposed reusable control-map contract

`pulp.beads.control-map.v1` should be a source-neutral mapping artifact with:

- `source` receipt: original path, SHA-256, units, coordinate system, conversion tool/version;
- `panel_mm`: physical width/height and affine transform to view pixels;
- `controls[]`: stable parameter ID, semantic role, geometry center/radius, hitbox, z-order, visual skin key, and interaction policy;
- `normalization`: explicit x/y origin and y-axis direction, never an implicit image flip;
- `skins[]`: separate visual recipes for ornate, large, small, and auxiliary knobs (pointer angle, cap/ring colors, texture source, shadow/highlight, value/modulation indicators);
- `validation`: expected screenshot dimensions, geometry tolerance, and source/artifact hashes.

A control entry should distinguish `geometry_ref` (DXF/SVG placement) from `skin_ref` (photograph/material calibration). This prevents the common error of treating red panel cutouts or DXF circles as knob appearance.

## Knob-skin recommendation

Use a front-facing, parameterized Skia skin rather than embedding a 3-D model. Capture from the straight-on module photograph: outer silhouette, cap/ring colors, pointer shape and angle, highlight/shadow profile, and visual scale. Render a static panel vector layer first, then overlay native parameter-bound knobs at the fitted centers. Keep the supplied photo as a calibration/reference input unless its redistribution rights are separately resolved.

Acceptance evidence for W2 is a debug overlay showing DXF centers and radii over the converted AI/SVG and a pixel-diff report against the panel reference. Material/knob fidelity is a separate evidence item and cannot be inferred from geometry alone.

## Rights boundary

Source code files inspected in the Beads tree carry Emilie Gillet MIT notices. No repository-level license or artwork license was found in the inspected root. The AI/DWG/DXF/panel artwork therefore remains rights-sensitive: preserve attribution and provenance, do not put it under a new project MIT license by assumption, and do not ship Mutable Instruments marks or artwork without a documented permission/license decision. This report is an internal planning artifact and does not grant redistribution rights.
