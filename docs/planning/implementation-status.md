# Grainfield implementation status

Updated 2026-10-17 against the current Grainfield `main` checkout.

## Current executable slice

- Independent product name: Grainfield.
- Project license: MIT for original project code.
- DSP boundary: the Beads synth path only, with the reachable MIT-noticed
  source subset recorded under `third_party/beads_dsp`.
- Host contract: stereo MIDI instrument, global/mono voice semantics, and an
  explicit 48 kHz support gate. Unsupported host rates must remain silent until
  a tested resampling layer exists.
- Documentation alignment: the local Beads manual and image set are recorded
  in `docs/planning/beads-documentation-review.md`; this release slice is
  explicitly the internal wavetable branch and does not claim the documented
  live-input, freeze, quality-mode, attenurandomizer, or delay behavior.
- MIDI pitch: the bound PITCH parameter is now -24 to +24 semitones, with MIDI
  note 60 as the neutral reference and note offsets applied to the synth path.
- Quality: the product-local QUALITY parameter now selects all four upstream
  Beads synth `Resolution` paths (4-bit, 16/6-bit, 7-bit, and 16-bit). This is
  deterministic coverage of the internal wavetable synth branch only; it does
  not claim the documented live-input processor's 24/32/48 kHz quality modes.
- Native editor prototype: the converted panel SVG is embedded at build time,
  all 24 DXF-derived knob placements are rendered at the calibrated centres
  and diameters, and five controls are parameter-bound. The bound knobs have
  working Pulp context menus for LFO enable/reset, host automation bindings,
  and a visible modulation arc. A headless capture is recorded at 402 x 728
  pixels. Knob material remains provisional; the straight-on reference is the
  calibration source. Clean checkouts without the rights-gated panel probe use
  a deterministic neutral fallback for compilation; that fallback is not
  visual fidelity evidence.
- Formats declared by the scaffold: VST3, CLAP, LV2, AU, standalone. AAX is
  declared only as a future validation lane; it is not a release claim.

## Work in flight

1. Add the remaining source-vs-port audio comparison, real-time allocation
   probe, and MIDI overlap receipts.
2. Calibrate the native panel capture against a rights-cleared straight-on
   reference, including ornate knob faces, pointer angles, labels, and pixel
   residuals.
3. Add host load/editor/audio/state receipts for each supported format and
   retain explicit negative evidence for VST3 and AAX.
4. Resolve panel artwork and knob reference rights before redistribution.

## Release gates

The project cannot claim source-faithful completion until the DSP audio
contract, visual geometry/knob comparison, state and modulation persistence,
host automation, format load/editor/audio/state receipts, and rights review
all have explicit `pass` or `blocked` evidence. Artwork remains rights-gated;
the project MIT license does not relicense third-party panel assets.

