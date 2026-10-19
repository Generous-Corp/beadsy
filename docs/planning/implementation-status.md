# Beadsy implementation status

Updated 2026-10-18 against the integrated source head
`217920209270ede7ad356966e7a4c09f7f9fa2e5`; receipt updates are recorded in
`provenance/current-integration-validation-20261018.json`.

## Current executable slice

- Independent product name: Beadsy.
- Project license: MIT for original project code.
- Provenance: the source-to-port inventory is complete for the recorded port files, with hashes, local Beads origins, notices, and redistribution states; adapted DSP notice mappings, `waves.bin` lineage, and stmlib upstream license metadata are recorded. Hash verification records 29 Beads matches and 7 mismatches, plus 14 stmlib matches and 1 mismatch; all eight mismatches are classified as intentional portability adaptations with deterministic unified-diff metadata in `provenance/source-to-port.json`. The stmlib audit is metadata-only, and legal/source redistribution remains blocked. The existing Pulp `RtAllocationProbe` is linked into Beadsy-test. Release and alternate Debug arm64 AppleClang configurations pass focused 1/1 and ten-run 64/256/1024 matrices with zero allocations/bytes, but the prior one-allocation/192-byte observation remains unresolved; the allocation/realtime gate is inconclusive pending recurrence backtrace and broader cross-toolchain evidence. Pulp commit `f9752e4` proves fail-closed `illustrator`/`ai`/`pdf` aliases with preconverted-SVG validation-only behavior; this does not claim Illustrator conversion support. External `pdftocairo` feature diagnostics remain inconclusive.
- Release scope (`v1-synth-only-20261018`): the shipped first release is the
  global MIDI synth voice with no external audio-input requirement.
- DSP boundary: the Beads synth path is the v1 release path, with the reachable
  MIT-noticed source subset recorded under `third_party/beads_dsp`. The
  live-input effect path remains experimental/deferred validation code; its
  receipts are retained for future parity work and do not expand the v1 claim.
- Host contract: v1 uses global/mono MIDI voice semantics and accepts 48 kHz
  only; non-48 kHz hosts remain fail-closed because the port's synth and
  processor initialization are fixed at 48 kHz. Stereo
  Main In/live-input behavior is exercised only by the deferred experimental
  path.
- Documentation alignment: the local Beads manual and image set are recorded
  in `docs/planning/beads-documentation-review.md`; the historical validation set includes a tested live-input path, but that path
  is experimental/deferred and is not part of the v1 release claim; freeze-buffer restore, attenurandomizer,
  and full delay parity remain unclaimed.
- MIDI pitch: the bound PITCH parameter is now -24 to +24 semitones, with MIDI
  note 60 as the neutral reference and note offsets applied to the synth path.
  MIDI events from all channels are accepted; channel filtering is not implemented.
- Quality: the product-local QUALITY parameter now selects all four upstream
  Beads synth `Resolution` paths (4-bit, 16/6-bit, 7-bit, and 16-bit). This is
  deterministic coverage of the internal wavetable branch; the external-input
  processor has historical 24/32/48 kHz matrix receipts; they are not v1 host
  support because the synth path is fixed at 48 kHz.
- Freeze: the product-local FREEZE toggle now feeds the upstream synth's
  per-sample freeze input and persists through the normal Pulp parameter state.
  Freeze-buffer serialization/restore remains unavailable because the
  internal `beads::Synth` exposes no buffer snapshot API.
- Native editor prototype: the current build uses a deterministic native
  Skia/Dawn editor with procedural segmented knobs, off-white caps, broad
  pointers, DXF-derived placement, five primary controls, seven source-mapped
  auxiliary controls, labeled non-automatable jack regions, host automation
  gestures, and contextual LFO enable/rate/depth/reset actions. Current-head
  editor tests pass 132 assertions in 9 cases, and min/mid/max knob captures
  are hash-recorded in the integrated validation receipt. The captures prove
  native rendering and interaction coverage; exact visual fidelity remains
  open because the straight-on reference and panel artwork are rights-gated.
- Formats declared by the scaffold: VST3, CLAP, LV2, AU, standalone. The
  integrated LV2 bundle loads with 12 parameters and has an exact-head host
  audio render receipt (7,200 frames at 48 kHz; zero NaN, Inf, or clipped
  samples). The standalone state test passes 5/5 assertions. VST3, CLAP, AU,
  AAX, and remaining standalone/editor/state claims retain the explicit
  partial or unavailable statuses in the packaging reconciliation; this is not
  a release claim.

## Work in flight

1. Add the remaining source-vs-port audio comparison and MIDI overlap receipts; expose allocation counts, capture a backtrace on recurrence, and rerun 64/256/1024 across supported toolchains.
2. **Host-route seam is implemented and focused-tested:** Pulp commits `573ef3a6b5` and `0356f60bd2` add static `BusRole` metadata plus host-owned `AudioRouteCatalog`; Beadsy now exposes stable MIDI Synth/Main Input/Sidechain modes and a distinct main-vs-sidechain impulse test passes. The catalog may enumerate Pulp-host sources; external DAWs remain responsible for their own project/input menus. Remaining work is SDK integration and real AU/VST3/CLAP host receipts.
3. Calibrate the native panel capture against a rights-cleared straight-on
   reference, including ornate knob faces, pointer angles, labels, and pixel
   residuals.
4. Complete host editor/audio/state receipts where the current matrix is partial
   or blocked, retaining explicit negative evidence for AU/LV2/Standalone gaps,
   VST3 moduleinfo/Steinberg validator, and AAX. AUv2 `aumu` live input remains
   unavailable unless a separate `aumf` identity is added.
   Current exact-head format checks: Beadsy CLAP builds and passes clap-validator
   (17 passed, 3 skipped, 0 failed); AU `aumu` builds and passes `auval`, but
   explicitly reports zero input buses, so its Sidechain/Main Input selector is
   not yet reachable through AU host routing. The VST3 artifact builds; no local
   Steinberg validator is installed, so VST3 bus activation remains unverified.
   The new Beadsy FX companion builds as AU `aumf`, CLAP, and VST3; its focused
   processor test passes 131 assertions, AU `auval` reports two input buses and
   succeeds, and CLAP validation passes 17/20 tests. A real DAW project-route
   receipt is still open.
5. Resolve panel artwork permission and trademark clearance; complete legal/source redistribution review and external converter diagnostics (currently inconclusive) after the classified portability adaptations are accepted. Keep the Pulp importer fail-closed and validation-only boundary from commit `f9752e4`.

## Release gates

The project cannot claim source-faithful completion until the v1 synth DSP
contract, visual geometry/knob comparison, state and modulation persistence,
host automation, format load/editor/audio/state receipts, and rights review
all have explicit `pass` or `blocked` evidence. Artwork remains rights-gated;
the project MIT license does not relicense third-party panel assets.

