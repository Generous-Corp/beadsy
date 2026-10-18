# Beasy adversarial review — current integration head

Reviewed at source head `908b887` on 2026-10-18. This review remains conservative: focused tests and artifact receipts do not close broader host, rights, UI, or release gates.

## Evidence that holds

- `ctest --test-dir build-beasy-renamed --output-on-failure`: 37/37 tests passed.
- Direct DSP fixtures remain byte-identical: 48 baseline fixtures and 144 seed/gate fixtures, max absolute error 0 and RMSE 0.
- Native editor capture and the source-backed Seed Mode button are proven. The Seed control cycles OFF → ON → LOCKED, updates its LED and accessibility label, and follows host state restoration.
- The standalone state CLI saves a 386-byte state file and loads it successfully in a fresh process.
- CLAP and VST3 host validation pass; governed GUI-bootstrap AU validation passes. The LV2 custom ABI probe now proves URID negotiation, 19 connected ports, finite audible MIDI output, and state save/restore.
- The importer hardening candidate is separately validated from current Pulp `origin/main`: six clean commits add validation-only preconverted SVG/control-map handling and explicit fail-closed Illustrator/AI/PDF refusal. It is not yet integrated into Pulp main or Beasy.
- Beasy is independently branded and MIT licensed, with explicit non-affiliation and attribution to Emilie Gillet and Mutable Instruments.

## Open or bounded claims

- No installed third-party LV2 host or validator is available; DAW-level LV2 integration remains unproven.
- AU installed-component discovery remains open even though the isolated governed `auval` path passes.
- AAX remains blocked by the unavailable proprietary SDK and host evidence.
- The editor is a clean-room procedural panel. Straight-on geometry, knobs, jacks, automation, LFO context menus, Freeze/Seed controls, compact I/O labels, and colored auxiliary controls are implemented. The current private comparison reports mean RGB error 54.58 and edge correlation 0.096; this is geometry-only evidence, not pixel-fidelity closure.
- Upstream panel artwork, CAD, photos, manuals, trademarks, and unresolved resource lineage remain rights gates. They are not redistributed.
- The importer candidate refuses unconverted AI/PDF rather than parsing them. A fresh Pulp integration worktree has validated the six-commit range with the focused and broad importer suites; the full gates script was inconclusive because the isolated worktree lacked codemodel/optional SDK/host configuration. The commits are not yet landed in Pulp main, and Beasy still requires a rights-bearing converted SVG/control-map sidecar.

## Required next review actions

1. Land or formally accept the current-main importer hardening range through Pulp's normal review process, then validate Beasy's converted SVG/control-map sidecar without bundling rights-sensitive source artwork.
2. Obtain a third-party LV2 host or a working REAPER project and capture audible MIDI plus state evidence.
3. Produce a rights-cleared straight-on visual comparison and resolve any remaining geometry deltas.
4. Complete the importer integration validation, then re-audit notices, attribution, package contents, signing, notarization, installation, and all format receipts before any release-complete claim.
