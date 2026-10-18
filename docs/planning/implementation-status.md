# Grainfield implementation status

Updated 2026-10-18 against Grainfield source head
`ced81b323a0fd1ddc11a2c1c56bf5b947c915ba`.

## Current executable slice

- Independent product name: Grainfield.
- Project license: MIT for original project code.
- Provenance: the source-to-port inventory is complete for the recorded port files, with hashes, local Beads origins, notices, and redistribution states; adapted DSP notice mappings, `waves.bin` lineage, and stmlib upstream license metadata are recorded. Hash verification now records 29 Beads matches and 7 mismatches, plus 14 stmlib matches and 1 mismatch; mismatch paths remain explicit in `provenance/source-to-port.json`. The stmlib audit is metadata-only, and copied-byte/internal-license redistribution remains blocked.
- DSP boundary: the Beads synth path only, with the reachable MIT-noticed
  source subset recorded under `third_party/beads_dsp`; the live-input effect
  path is now wired through the same noticed processor subset.
- Host contract: required stereo Main In/Main Out, global/mono MIDI voice
  semantics, live-input effect mode, and tested 24/32/48 kHz media selection.
  Other host rates remain fail-closed.
- Documentation alignment: the local Beads manual and image set are recorded
  in `docs/planning/beads-documentation-review.md`; this release slice is
  includes a tested live-input path; freeze-buffer restore, attenurandomizer,
  and full delay parity remain unclaimed.
- MIDI pitch: the bound PITCH parameter is now -24 to +24 semitones, with MIDI
  note 60 as the neutral reference and note offsets applied to the synth path.
- Quality: the product-local QUALITY parameter now selects all four upstream
  Beads synth `Resolution` paths (4-bit, 16/6-bit, 7-bit, and 16-bit). This is
  deterministic coverage of the internal wavetable branch; the external-input
  processor has separate finite 24/32/48 kHz matrix receipts.
- Freeze: the product-local FREEZE toggle now feeds the upstream synth's
  per-sample freeze input and persists through the normal Pulp parameter state.
  Freeze-buffer serialization/restore remains unavailable because the
  internal `beads::Synth` exposes no buffer snapshot API.
- Native editor prototype: the converted panel SVG is embedded at build time,
  all 24 DXF-derived knob placements are rendered at the calibrated centres
  and diameters, and five controls are parameter-bound. The bound knobs have
  working Pulp context menus for LFO enable/reset, host automation bindings,
  and a visible modulation arc. A headless capture is recorded at 402 x 728
  pixels. Knob material remains provisional; the straight-on reference is the
  calibration source. Clean checkouts without the rights-gated panel probe use
  a deterministic neutral fallback for compilation; that fallback is not
  visual fidelity evidence.
- Formats declared by the scaffold: VST3, CLAP, LV2, AU, standalone. VST3
  builds against the installed development SDK and passes pluginval 1.0.4; the
  CLAP and VST3 host load/audio probes pass. AU packaging passes but AU
  registration/host load is blocked; LV2 host instantiation is blocked by missing
  usable audio ports/URID mapping; the Standalone launch/editor probe is partial
  pending state/audio/clean-shutdown evidence. VST3 moduleinfo and Steinberg
  validator evidence remain open. AAX is declared only as a future validation
  lane; it is not a release claim.

## Work in flight

1. Add the remaining source-vs-port audio comparison, real-time allocation
   probe, and MIDI overlap receipts.
2. Calibrate the native panel capture against a rights-cleared straight-on
   reference, including ornate knob faces, pointer angles, labels, and pixel
   residuals.
3. Complete host editor/audio/state receipts where the current matrix is partial
   or blocked, retaining explicit negative evidence for AU/LV2/Standalone gaps,
   VST3 moduleinfo/Steinberg validator, and AAX.
4. Resolve panel artwork permission and trademark clearance; resolve the seven Beads and one stmlib hash gaps, verify copied stmlib bytes/per-file notices, complete legal redistribution review, and resolve converter diagnostics (currently inconclusive).

## Release gates

The project cannot claim source-faithful completion until the DSP audio
contract, visual geometry/knob comparison, state and modulation persistence,
host automation, format load/editor/audio/state receipts, and rights review
all have explicit `pass` or `blocked` evidence. Artwork remains rights-gated;
the project MIT license does not relicense third-party panel assets.

