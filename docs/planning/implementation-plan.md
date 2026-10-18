# Independent Beads-inspired Pulp instrument

Status: implementation plan, Ultra-reviewed 2026-10-17
Planning head: Pulp `origin/main` `5a359c9`
Reference source head: Beads `f4cfa7697095a28d292bff758beb51deb49b5a86`
Documentation reference: [`beads-documentation-review.md`](beads-documentation-review.md),
based on the local Mutable Instruments manual/index and image set.

This project will be an independent MIT-licensed Pulp instrument inspired by the Mutable Instruments Beads architecture and panel layout. It must not imply endorsement or affiliation. The product name is provisional until the rights review; use an independent name such as **Grainfield** rather than “Beads,” “Mutable,” or a confusing abbreviation. The README and editor About view will state the relationship accurately and link to the upstream source and licenses.

## Product boundary

The first release is a fixed-topology MIDI instrument implemented as a Pulp `Processor`, with native Skia/Dawn/ViewBridge UI and host parameter automation. It models the documented internal wavetable synthesis/granular branch. Live audio acquisition, freeze, quality switching, attenurandomizers, and delay mode remain explicit follow-on parity phases. It is not a per-note polyphonic instrument: the initial MIDI contract is mono/global and documented as follows.

- A MIDI note-on updates the pitch mapping and triggers the global voice/granular event.
- Note-off follows the declared gate behavior; it does not create an independent release envelope per note.
- Pitch bend, channel pressure, sustain, and overlapping notes are either implemented and tested or explicitly reported unsupported.
- “Seed” is a trigger/gate event, not a random-seed parameter. Deterministic tests receive an injected/resettable RNG seam.
- Beads’ internal DSP modes are 24/32/48 kHz. Pulp’s 44.1/48/88.2/96 kHz support requires an explicit resampling strategy and a reference comparison; it cannot be assumed equivalent.
- Audio input/effect mode, polyphony, alternate MIDI semantics, and an AAX AudioSuite path are later extensions, not hidden scope.

The UI will reproduce the straight-on panel composition and interaction hierarchy. It will use the supplied panel art for static geometry and custom native knob skins for the physical appearance. It will not ship a 3D view.

## Rights and provenance contract

Before copying or distributing any source or asset, create `source-manifest.json` with SHA-256 hashes, origin URLs/paths, commit IDs, per-file notices, and a rights state.

- Beads software notices and the exact `stmlib` commit `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec` must be acquired and recorded.
- Beads hardware/panel artwork is identified as CC-BY-SA 3.0 material. Its attribution and share-alike obligations must remain visible; it must not be silently relicensed as MIT.
- Mutable Instruments names, logos, and trademarks are removed from independent product branding unless explicit permission is obtained.
- Griddy-MIDI-Effect-Plugin is GPLv3. It is an architectural and UX reference only; no code, data, or GPL-derived resources may enter this MIT project.
- Keep `LICENSE` (project MIT), `LICENSES.md` (third-party notices), `ATTRIBUTION.md`, and an artwork-rights decision record separate.
- The build must fail closed when provenance or a required notice is missing.

## Worktree and ownership rules

Use a new worktree from current `origin/main` for each mutation. The current planning worktree is:

`/Users/danielraffel/Code/beads-pulp-instrument-plan`

All CMUX workspaces/tabs and `sr codex` sessions use Beads-related names. Workstreams claim paths before editing; shared Pulp importer/core files are serial integration territory. Generic framework/rendering changes must pass the Pulp/Vellum source-authority route check before implementation.

## Phases and parallel workstreams

### W0 — Lead contract and repository skeleton

Freeze the two source heads, choose the independent name, define the mono MIDI contract, list supported formats, and create the new project skeleton. Add the MIT license, notice files, contributor guidance, provenance schema, and a machine-readable workstream ledger.

Receipts: `source-manifest.json`, active worktree-lineage records, `route-report.json`, and a signed decision record for name, scope, and rights.

### W1 — Source, artwork, and asset pipeline

In a dedicated “Beads assets” worktree, preserve the AI, DWG, DXF, source SVG conversion, panel PNG, and hashes. Convert Illustrator/PDF-compatible AI to clean SVG at authoring time, outline or embed required fonts, record converter version, and retain unsupported-feature diagnostics. Treat DXF as mechanical geometry (centres/diameters), not as the knob appearance. Use the straight-on photograph to calibrate highlights, pointer, rim, and material.

Receipts: source hashes and notices, clean SVG, outlined-text SVG, reference PNG, converter report, and artwork-rights decision.

### W2 — Geometry and knob skins

Build `control-map.json` from SVG coordinates plus DXF centres/diameters. It must name every parameter, bounds, anchor, hit target, automation ID, and source node. Create `knob-skin-manifest.json` with large ornate and small black knob families, pointer/value states, material colors, scale rules, and min/mid/max reference renders. Add a debug overlay that shows panel coordinates, knob centres, and hit regions.

Receipts: control map, knob manifest, geometry debug PNG, and source/reference hashes. A control is not considered bound because a path merely resembles a knob.

### W3 — DSP and MIDI

Acquire the exact stmlib commit and isolate portable Beads DSP from STM32 drivers, ADC/CV, codec, GPIO, LEDs, calibration, watchdog, bootloader, and persistence. Implement the Pulp Processor, parameter store, MIDI event handling, deterministic RNG seam, and rate conversion. Use the Pulp audio harness for offline renders and real-time safety probes.

Receipts: `audio-contract.json` (sample rates, block sizes, finite output, allocation/RT probes, mono semantics), source-vs-port comparison renders, and seeded reference WAVs.

### W4 — Reusable vector-art import

In a separate Pulp worktree, extend the design importer through a versioned `DesignIR` vector-art source adapter. The initial adapters are AI/PDF-compatible Illustrator input, SVG, and existing Figma/HTML lanes. Normalize paths, groups, transforms, text, fills, strokes, clips, and diagnostics. Unsupported Illustrator features must produce explicit fail-closed diagnostics. Conversion occurs at authoring/build time; installed plugins never depend on Affinity, Illustrator, Inkscape, or a browser service.

Do not block the instrument on the generalized importer: a reviewed clean SVG may be used first. Add Figma/AI/SVG/PDF fixtures and deterministic output hashes. Check source-authority routing before generic Pulp changes.

Receipts: DesignIR envelope, SVG output, unsupported-feature report, deterministic hash, and native screenshot.

### W5 — Native editor and modulation

Build the shipped editor with Skia/Dawn/ViewBridge. Load static panel art first, then overlay parameter-bound native controls. Implement custom knob skins, drag gestures, keyboard/focus behavior, resize rules, state save/restore, and content-first/prewarm lifecycle.

Add the reusable modulation interaction proven by Spectr:

- Right-click a parameter knob opens a contextual menu.
- Attach/detach an LFO and configure shape, rate, depth, and target.
- Draw modulation state on the knob and preserve the base parameter’s host automation.
- Persist modulation state and tear it down cleanly on detach/reload.
- Keep modulation in the shared core only when the Beads and Spectr use cases share an API and tests; otherwise keep it product-local.

Receipts: screenshot and interaction/state evidence, named-control binding report, balanced gesture traces, host-automation tests, and modulation teardown tests.

### W6 — Validation matrix

Run the audio matrix across supported rates and block sizes, MIDI event order, reset/freeze/seed behavior, allocation and real-time probes, and source-vs-port comparisons. Run UI geometry, presence, material, interaction, and reload checks using Pulp’s importer and screenshot tools. Use a substitution control to prove no captured bitmap is silently supplying native pixels.

Validate CLAP, VST3, AUv2, and standalone first. Treat AUv3 as an explicit additional lane. AAX remains `blocked` until an Avid SDK, a complete `aax_entry.cpp`, DigiShell/AAX Validator results, and Pro Tools editor/audio evidence exist. AudioSuite is not claimed without its offline path.

Receipts: offline WAV renders, screenshot/interaction/state reports, and per-format load/editor/audio/state receipts. Every skipped or blocked lane includes exact missing evidence and a same-instrument negative control that exits non-zero.

### W7 — Packaging and documentation

Package the independent MIT project with presets, reproducible asset build steps, attribution, notices, non-affiliation language, install instructions, and format-specific metadata. Verify checksums, signing, notarization, and installed behavior where distribution is intended. Keep CC-BY-SA artwork attribution/share-alike obligations separate from MIT source licensing.

Receipts: signed installed artifacts, checksums, notarization/install evidence, README, license/notice files, and attribution audit.

### W8 — Adversarial closeout

Ultra or an equivalent independent review checks every claim against an exact receipt. Confirm current heads, provenance, rights, negative controls, and that unavailable/not-run/inconclusive states are represented honestly. Re-run the route report after any generic Pulp change. Only then mark gates complete.

## Dependency graph and parallel schedule

W1, W2, W3, and W4 reconnaissance can run concurrently after W0. W5 depends on W1 and W2; W6 depends on W3 and W5; W7 depends on W6 and rights resolution; W8 follows all implementation and packaging work. W4 can feed W5 but is not a prerequisite for the first clean SVG-based editor.

Use named CMUX workspaces such as “Beads assets,” “Beads geometry,” “Beads DSP,” “Beads vector importer,” “Beads native editor,” and “Beads validation.” Each session reports its worktree, claimed paths, exact commit, tests, receipts, blockers, and one next action.

## Core-hardening rule

A capability moves into Pulp core only after two concrete consumers (initially Beads and Spectr), a stable versioned API, a focused test plus a negative control, no source-authority conflict, and evidence that product-local code would otherwise duplicate behavior. Candidate shared work includes vector-art normalization, control maps, parameter binding, contextual modulation, automation gestures, knob skin primitives, provenance diagnostics, and screenshot/interaction gates.

## Definition of done

The project is complete only when the independent product is buildable from a clean worktree, its DSP and UI are validated against declared Beads behavior, its panel/knob fidelity has geometry and screenshot evidence, automation and modulation survive host/state tests, supported formats have installed load/editor/audio/state receipts, rights and attribution are auditable, and any unsupported format or behavior is explicitly marked with its missing evidence.


## W4 implementation map and routing gate

The current importer already has a DesignIR provenance/asset surface. The first vector-art slice should follow these seams: `core/view/include/pulp/view/design_ir.hpp`, `core/view/src/design_import.cpp`, `core/view/src/design_ir_json.cpp`, `core/view/src/anchor_strategy.cpp`, `tools/import-design/pulp_import_design.cpp`, and `tools/import-design/CMakeLists.txt`. New adapter and fixture files should land before shared enum/dispatch edits; the enum, serialization, CLI, CMake, and test-manifest changes are one serial integration slice so switches cannot drift. Existing `svg_probe`, `svg_shape_lowering`, `fidelity_diff`, `layout_parity`, `material_audit`, and `check_panel_presence` remain the proof tools.

Before touching those paths, run the Pulp/Vellum route report. The active framework-authoritative-transferred rule may require a reviewed Pulp-only inheritance event and append-only watch record. A vector-art change spanning DesignIR, design import, and import fixtures is expected to declare the exact capability families `design-ir-contract`, `design-output-and-packaging`, and `design-source-ingest` (plus visual-proof coverage if applicable). This route evidence is a prerequisite, not an after-the-fact documentation step.

## Additional source-faithful and format-specific constraints

The v1 product is synth-only: it renders the global Beads voice/granular path and does not claim the original hardware’s external audio-input/effect path until that path has its own parity evidence. The provenance manifest must include `resources/waves.bin` and its BRAIDS/PLAITS resource lineage, every initialized submodule, and third-party license boundaries inside stmlib (including GPL components that must not be copied into the MIT product). Preserve the relevant authors and notices from the source files.

The MIDI contract must define note-on velocity, note-off, overlapping notes, retrigger timing, channel filtering, pitch bend range, pressure, sustain, and reset behavior. The default is one global voice: the latest accepted note controls pitch, note-on retriggers the global gate, and note-off only changes the gate when it releases the currently held note according to the documented overlap policy. Tests must exercise each policy and use sample-offset events where the host format exposes them. VST3, AUv3, and CLAP event offsets must be honored; AUv2 and AAX block-rate limitations must be documented and tested separately.

The format matrix must identify the product as an instrument: AUv2 `aumu`/instrument metadata, AUv3 instrument metadata, CLAP instrument features, and VST3 instrument bus/category. AAX remains blocked without the SDK, entry point, validator, and Pro Tools evidence. The modulation lane depends on a landed, reviewed Spectr-compatible API; a context menu or modulated indicator alone is not evidence that LFO assignment is available.

Every receipt uses one common envelope containing schema version, exact Pulp/Beads/project heads, input hashes, tool versions, dimensions/sample rates/block sizes, status (`pass`, `blocked`, `not-run`, or `inconclusive`), and artifact paths. Negative controls are recorded alongside positive results.

The vector-art core slice must also claim `core/view/cmake/PulpDesignUnits.cmake`, which is the explicit source manifest for core design-import translation units. It must preserve the existing `faithful_svg`/`svg_asset_id`/`asset_manifest`/`interactive_elements` lowering path rather than inventing a renderer. Run the design-unit boundary selftest after adding a parser TU. Native parser, CLI, and top-level design-import test manifests are separate ownership units and must be edited serially.

The DXF declares millimetre units (`$INSUNITS=4`); preserve that unit in `control-map.json` and convert only at the explicit panel scale. The AI conversion receipt must include the exact `/opt/homebrew/bin/pdftocairo` version and output hash. Spectr’s right-click modulation implementation is currently product-local (reference commit `0f4ce3332ac7cfaf93e23e083e4e4afd4bc0d178`); treat it as a source reference and require an extraction boundary plus current tests before declaring a shared Pulp LFO API proven.
