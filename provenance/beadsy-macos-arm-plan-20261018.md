# Beadsy macOS ARM and host-routing delivery plan

Reviewed against the exact Beadsy head and PR state on 2026-10-19.

The product contract is a MIDI instrument with optional host-routed audio. The
normal insert workflow is provided by the companion **Beadsy FX** target. A
Beadsy instrument does not invent a project-wide track picker: the host owns
the route list and supplies the selected audio bus. Pulp standalone and Forge
hosts can provide the same list through `AudioRouteCatalog`.

## Phase 0: host route capability (core Pulp)

- Land [PR 10575](https://github.com/Generous-Corp/pulp/pull/10575), then
  refresh all Beadsy receipts against the landed `origin/main` head.
- Keep `AudioRouteCatalog` as a UI/control-plane API. It exposes physical
  inputs, project tracks, and buses with stable IDs, kind labels, channel
  counts, availability, refresh notifications, and a connection callback.
- Preserve selected stable IDs across refreshes, but reject connection attempts
  while a route is absent or unavailable. This allows the menu to show a saved
  choice without claiming that audio is currently connected.
- Keep route enumeration, allocation, locking, and topology mutation off the
  realtime callback. The adapter publishes the selected input buffer to the
  processor; Beadsy only reads it during `process()`.
- AUv3 is the primary instrument-side sidechain path. AUv2 remains the legacy
  instrument path without input buses. VST3, CLAP, LV2, and standalone expose
  the arrangements supported by their host adapters. Beadsy FX exposes the
  ordinary main-input effect path.
- Add host receipts for Logic/AUv3, REAPER/VST3 or CLAP, and standalone/Forge
  once each host is available. A passing catalog unit test alone does not prove
  that project audio reaches the declared input bus.

## Phase 1: importer landing

- Land PR [#10552](https://github.com/Generous-Corp/pulp/pull/10552) through the normal merge queue. Its exact head is `30265dd1f40399228ae1d906268b10e09f702bbc`; GitHub reports all required checks successful and `mergeStateStatus: CLEAN`.
- After landing, refresh the receipt against the landed `origin/main` head.
- Re-run `python3 tools/import-validation/test_source_contracts.py` (29/29 expected) and the Vellum expansion watch check.

## Phase 2: clean Apple Silicon build

- Use a fresh worktree from the landed `origin/main`.
- Run the governed Pulp Release build; verify Release configuration and `arm64` Mach-O output for AU, VST3, CLAP, LV2, and Standalone.
- Run Beadsy CTest (38/38 expected) and the exact-head format verifier.

## Phase 3: host proofs on the primary Mac

- Run AU validation with `auval -v aumu Bead Inde`.
- Run the CLAP validator and VST3 pluginval at strictness 5.
- Exercise the standalone state bridge and bounded CoreAudio/MIDI/editor launch.
- Verify the route picker presents `Input`, `Track`, and `Bus` entries with
  checkmarks, unavailable-state treatment, keyboard navigation, outside-tap
  dismissal, and stable-ID state restoration.
- Verify an actual selected physical input, project track, and bus reaches the
  processor through the host adapter. Record each as a separate receipt.
- Keep installation, signing, and notarization as separate gates.

## Phase 4: second ARM lane on M5s

- Do not build until load and disk preflight pass. Current M5s is reachable and arm64 but reports approximately 93% disk use and load averages above 10.
- Once quiescent, repeat the governed build, CTest, artifact architecture/hash audit, and host proofs.
- Run ARM AddressSanitizer/UndefinedBehaviorSanitizer and realtime allocation probes using the Pulp harness.
- Run the headless audio matrix at 44.1/48/96 kHz and 64/128/256/1024-frame blocks; require finite, audible output and deterministic DSP fixtures.

## Phase 5: DSP and UX parity review

- Extend the internal Beads comparison fixtures through the Processor adapter,
  MIDI ordering, CV/pitch/octave behavior, freeze persistence, Beadsy FX, and
  supported sample-rate/block-size matrices.
- Compare the straight-on faceplate against a rights-cleared reference at
  fixed dimensions. Validate knob diameter, cap, indicator, typography,
  spacing, resize handle, and removal of decorative output jacks.
- Exercise Spectr-compatible contextual LFO controls: right-click/tap,
  toggles, arrow keys, Return, outside dismissal, automation, and state
  round-trip.

## Phase 6: adversarial release review

Keep these explicit until authoritative evidence exists: rights and share-alike decisions for panel/CAD/reference inputs, generated wavetable-resource policy, pixel-level UI comparison, independent third-party LV2 host validation, signed/notarized installation, and AAX SDK/host validation.

Release remains blocked until every open gate has a current exact-head receipt;
the route catalog and its unit tests do not by themselves close DAW routing,
artwork rights, installed discovery, signing, notarization, or AAX validation.
