# Grainfield control-semantics audit

Audit date: 2026-10-18  
Grainfield base: `06f2b3271f2e673d981ce13f11e8203465f394c9`  
Beads source checkout: `/Users/danielraffel/Code/eurorack/beads` at commit `f4cfa7697095a28d292bff758beb51deb49b5a86`  
Documentation checkout: `/Users/danielraffel/Code/mutable-instruments-documentation/docs/modules/beads`

## Result

The Beads source and manual establish authoritative semantics for the 19 currently
provisional DXF entities, but they do **not** establish a handle-to-position map
for those entities. No provisional control was bound in this audit. Binding one by
matching a screenshot or by assuming DXF entity order would be an unverified
geometry guess and would weaken the existing provenance boundary.

The source-backed semantic set is:

| Source-defined control | Authoritative evidence | Current Grainfield status |
| --- | --- | --- |
| `TIME_AMOUNT` attenurandomizer | `drivers/pots_adc.h:47`; `beads.cc:179-184`; manual `manual.md:97-108` | semantic known; DXF handle unresolved |
| `SIZE_AMOUNT` attenurandomizer | `drivers/pots_adc.h:48`; `beads.cc:185-190`; manual `manual.md:97-108` | semantic known; DXF handle unresolved |
| `SHAPE_AMOUNT` attenurandomizer | `drivers/pots_adc.h:49`; `beads.cc:191-195`; manual `manual.md:97-108` | semantic known; DXF handle unresolved |
| `PITCH_AMOUNT` attenurandomizer | `drivers/pots_adc.h:50`; `beads.cc:197-201`; manual `manual.md:97-108` | semantic known; DXF handle unresolved |
| `FEEDBACK` | `drivers/pots_adc.h:44`; `ui.h:68-72`; `beads.cc:126-130,219-233`; manual `manual.md:123-124` | semantic known; DXF handle unresolved |
| `DRY_WET` | `drivers/pots_adc.h:45`; `ui.h:68-72`; `beads.cc:126-130,219-233`; manual `manual.md:125` | semantic known; DXF handle unresolved |
| `REVERB` | `drivers/pots_adc.h:46`; `ui.h:68-72`; `beads.cc:126-130,219-233`; manual `manual.md:127` | semantic known; DXF handle unresolved |

`DENSITY`, `TIME`, `PITCH`, `SIZE`, and `SHAPE` remain the only controls with a
parameter binding in `provenance/control-map.json`. Their source contracts are
also independently recorded in `drivers/pots_adc.h:52-58` and `beads.cc:178-211`.

## Why the handles remain unresolved

`provenance/control-map.json` records 24 DXF circles. The supplied DXF contains
centres, radii, layers, and handles, but no label text or net/component identity.
The source's `PotsAdcChannel` enum and `beads.cc` processing order identify
semantics, not the physical coordinates of the exported DXF entities. The manual
identifies the attenurandomizer and mix control families and their behavior, but
its raster/vector artwork does not provide a machine-readable correspondence to
DXF handles. Therefore neither source order nor visual proximity is sufficient
to bind `aux-*` or `small-*` entries.

The 12 `small-*` entries are intentionally left provisional as well. The manual
front image depicts twelve audio/CV jacks, while the DXF export does not label
entity type; reclassifying those entries as jacks would still require an
explicit DXF-to-panel component map. They must not be exposed as parameters.

## Safe next evidence

A future binding change may proceed only after one of these is available and
rights-cleared for use as provenance: (1) the original panel/PCB design with
component references or net labels tied to the DXF coordinate system, or (2) an
export manifest from the design tool that preserves each control's source ID,
label, and coordinates. At that point the seven source-defined semantics can be
bound and the twelve jack entities can be removed from the parameter candidate
set without inference.

Artwork and trademarks remain subject to the existing `pending_rights_review`
state. This audit does not authorize redistribution of Beads panel artwork or
documentation images.
