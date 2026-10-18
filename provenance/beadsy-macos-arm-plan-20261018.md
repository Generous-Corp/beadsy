# Beadsy macOS ARM recovery plan

Reviewed against the exact Beadsy head and PR state on 2026-10-18.

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
- Keep installation, signing, and notarization as separate gates.

## Phase 4: second ARM lane on M5s

- Do not build until load and disk preflight pass. Current M5s is reachable and arm64 but reports approximately 93% disk use and load averages above 10.
- Once quiescent, repeat the governed build, CTest, artifact architecture/hash audit, and host proofs.
- Run ARM AddressSanitizer/UndefinedBehaviorSanitizer and realtime allocation probes using the Pulp harness.
- Run the headless audio matrix at 44.1/48/96 kHz and 64/128/256/1024-frame blocks; require finite, audible output and deterministic DSP fixtures.

## Phase 5: adversarial release review

Keep these explicit until authoritative evidence exists: rights and share-alike decisions for panel/CAD/reference inputs, generated wavetable-resource policy, pixel-level UI comparison, independent third-party LV2 host validation, signed/notarized installation, and AAX SDK/host validation.
