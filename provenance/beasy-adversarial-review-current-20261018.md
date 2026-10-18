# Beasy adversarial review — current integration head

Reviewed at source head `aa2cd6b` on 2026-10-18. This review is intentionally conservative: a passing focused test or artifact receipt does not close a broader host, rights, UI, or release gate.

## Evidence that holds

- `ctest --test-dir build-beasy-renamed --output-on-failure`: 35/35 tests passed after the source-backed seed mode and finite host-value hardening.
- `provenance/beasy-source-port-comparison-current-20261018.json`: 48/48 direct-DSP fixtures byte-identical, max absolute error 0, RMSE 0.
- `provenance/beasy-source-port-comparison-seed-current-20261018.json`: 144/144 direct-DSP seed/gate fixtures byte-identical, max absolute error 0, RMSE 0.
- `provenance/beasy-format-status-current.json`: current artifact hashes verify for CLAP, VST3, LV2, AU, and standalone; AAX is blocked by unavailable proprietary SDK and host evidence.
- The current source is independently branded Beasy, MIT licensed, and credits Emilie Gillet and Mutable Instruments with an explicit non-affiliation notice.

## Open or bounded claims

- The direct-DSP matrices do not prove Processor adapter MIDI ordering, velocity/pitch-bend behavior at every host boundary, CV/attenuverter projection, freeze-buffer persistence, effect-path parity, or unsupported-rate behavior.
- The editor is a procedural independent scaffold. It has deterministic knob/jack geometry, automation gestures, and right-click LFO seams, but pixel-faithful panel equivalence remains open and the Seed Mode parameter is host-automation-only pending a source-backed button/LED mapping.
- The 24-control map has 12 bound controls and 12 explicitly provisional socket regions. Ambiguous hardware semantics remain unbound.
- AU registration/auval, LV2 host state/MIDI behavior, VST3 validator/module-info evidence, signing/notarization/install behavior, and AAX remain release gates unless separately evidenced.
- Artwork/CAD redistribution, share-alike obligations, trademark clearance, and any `waves.bin` or resource lineage decisions remain rights gates. The project does not currently redistribute upstream panel artwork, CAD, photographs, or manuals.

## Required next review actions

1. Run adapter-level audio-harness fixtures for MIDI event order, velocity, pitch bend, CV, freeze, effect mode, and supported host rates.
2. Produce a current visual comparison against a rights-cleared straight-on reference and reconcile the remaining control-map regions.
3. Run current host validators and installed behavior checks for every claimed format; retain explicit blocked/not-run states where tooling or rights are unavailable.
4. Re-audit the clean release tree, notices, attribution, signatures, and package contents before any release-complete claim.
