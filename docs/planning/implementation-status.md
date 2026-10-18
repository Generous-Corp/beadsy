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
- Formats declared by the scaffold: VST3, CLAP, LV2, AU, standalone. AAX is
  declared only as a future validation lane; it is not a release claim.

## Work in flight

1. Complete the exact stmlib link boundary (`units.cc` and `random.cc`) and
   make the clean project build green.
2. Add seeded, finite-output, unsupported-rate, and MIDI overlap tests through
   Pulp's headless/audio test surfaces.
3. Remove variable-length real-time scratch from the imported render path by
   preparing bounded storage before the callback.
4. Materialize the panel only through an authoring-time AI/PDF-compatible SVG
   conversion. Keep the DXF as placement geometry and derive knob material and
   pointer treatment from the straight-on reference image.
5. Build the native Skia/Dawn editor from the control-map sidecar, then add
   automation and the Spectr-compatible right-click modulation interaction
   after its API boundary is proven.

## Release gates

The project cannot claim source-faithful completion until the DSP audio
contract, visual geometry/knob comparison, state and modulation persistence,
host automation, format load/editor/audio/state receipts, and rights review
all have explicit `pass` or `blocked` evidence. Artwork remains rights-gated;
the project MIT license does not relicense third-party panel assets.

