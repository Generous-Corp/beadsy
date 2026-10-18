# Grainfield adversarial review

**Review date:** 2026-10-18  
**Grainfield checkout:** `/Users/danielraffel/Code/agent-worktrees/grainfield-current-integration-20261018`  
**Grainfield HEAD observed:** `80759092d7683be3706bcee2ff9afa437cfae06f`  
**Beads source reviewed:** `/Users/danielraffel/Code/eurorack/beads`, commit `f4cfa7697095a28d292bff758beb51deb49b5a86`  
**Beads documentation reviewed:** `/Users/danielraffel/Code/mutable-instruments-documentation`, commit `14d04a75ad54cd97e5e5f7b8f5a2ec7bb23046d5`

This is a read-only implementation and evidence audit. The only mutation is this report. The conclusion is deliberately stricter than the existing focused test receipts: current work is a useful, buildable Grainfield prototype, but it is not yet a faithful Beads replacement or a release-ready product.

## What is complete enough to show

The native editor exists and produces screenshots. The current source creates a Skia/Dawn-compatible `GrainfieldEditor` with five primary controls, three mix controls, four auxiliary CV-amount controls, twelve named port regions, parameter bindings, host gesture callbacks, and a product-local right-click LFO menu. `grainfield_editor.hpp:192-422` is the current implementation.

The exact files currently present are:

- [Native scaffold capture](/Users/danielraffel/Code/agent-worktrees/grainfield-current-integration-20261018/build-full-current/grainfield-editor-full-current-20261018.png), 79,075 bytes, SHA-256 `e5e71a9e08be0c96019084b091f63949c0d9becdaa39168a99fbd836c846f64a`.
- [Procedural port capture](/Users/danielraffel/Code/agent-worktrees/grainfield-current-integration-20261018/build-bridge-current/grainfield-editor-ports-current.png), 86,493 bytes, SHA-256 `125e09482a8d9d4738454da20d7bee81d60d5dbe733cca683d641e61cb2ec890`.
- [GPU editor audit](/Users/danielraffel/Code/agent-worktrees/grainfield-current-integration-20261018/provenance/gpu-editor-capture-audit-20261018.json).
- [Procedural jack audit](/Users/danielraffel/Code/agent-worktrees/grainfield-current-integration-20261018/provenance/procedural-jack-render-audit-20261018.json).
- [Current UX audit](/Users/danielraffel/Code/agent-worktrees/grainfield-current-integration-20261018/provenance/ui-fidelity-current-audit-20261018.json).

The editor tests cover construction, accessibility labels, knob rendering, automation gesture brackets, and the context-menu lifecycle. The current integration receipt records 133 editor assertions in 9 cases. These prove native rendering and interaction seams; they do not prove visual fidelity against the original panel or host automation in a DAW.

## UX verdict

**Status: implemented prototype; visual fidelity open.** The screenshots are visibly a clean-room Grainfield scaffold: dark blue-black panel, Grainfield title, sparse section rules, procedural black annulus/off-white caps, and procedural jack collars. The original straight-on photograph at `/Users/danielraffel/Code/mutable-instruments-documentation/docs/modules/beads/images/front.jpg` is a white panel with the Beads/Mutable Instruments identity, FREEZE and SEED controls, quality and mode indicators, LEDs, dotted signal/attenurandomizer relationships, four rows of labeled controls, and a distinct jack arrangement. The current capture intentionally omits those source artwork elements and has no exact pixel or overlay comparison.

This is a good rights-safe fallback while artwork rights are unresolved. It must not be described as “faithful” until a rights-cleared reference or an independently authored, approved visual specification is used to close geometry, label, material, and typography residuals. The current `ui-fidelity-current-audit-20261018.json` correctly leaves that gate open.

The current editor also has no visible UI for declared `Gain`, `Quality`, `Freeze`, or `Mode` parameters (`grainfield.hpp:102-130`; `grainfield_editor.hpp:326-352`). The original manual documents a quality selector, FREEZE and SEED buttons, LED state, and hidden/alternate behavior. These may be intentionally deferred in the synth-only product, but the omission is a real UX scope difference and must stay explicit.

## DSP and behavior findings

### 1. Direct source-port parity is real but narrow

`provenance/source-port-comparison-v2.json` reports 49 byte-identical source/port fixtures across 48 kHz synth renders, four quality values, four parameter states, and block sizes 32/64/128. `provenance/source-port-comparison-matrix.json` reports the same kind of internal reference result. This is useful A/B evidence for the copied `beads::Synth` subset.

The receipts also say exactly what they do not prove: the fixture binaries live outside this repository, the upstream `beads_test` render calls are disabled, and the comparison does not cover the Pulp MIDI adapter, live-input effect behavior, hardware CV semantics, delay mode, freeze-buffer restore, host behavior, or rights. Both receipts are bound to earlier Grainfield project heads (`73163d15d43a42cf6b0b8059f38de3713f68132f` / `03b46cd3c99fe4ebe92f88616b3c7bf4b20d79bd`) rather than the observed `80759092d7683be3706bcee2ff9afa437cfae06f`. Treat them as historical internal evidence until regenerated at the current head.

**A/B answer:** yes, a deterministic source-vs-port A/B harness already exists and has passed for the narrow synth subset. It is not yet a current-head plugin-vs-original proof, and it cannot validate a physical Beads module without a controlled recording or hardware capture. The next A/B run should build both sides from pinned source commits, inject identical RNG seeds, parameter values, gate/MIDI event streams, sample rates, and block partitions, then emit byte, max-absolute-error, RMS, and spectral receipts. Keep the source fixture outside redistribution unless rights permit it.

### 2. Attenurandomizer semantics are not faithful

The original `PotChannel::LAW_ATTENUVERTER` maps a centered physical pot to approximately `-1..+1` with a dead zone (`/Users/danielraffel/Code/eurorack/beads/cv_reader.h:108-113`). `RandomizedParameter::value()` then uses the sign to select CV/randomization behavior and a peaked versus uniform internal distribution (`dsp/randomized_parameter.h:96-141`).

Grainfield declares each `* CV Amount` parameter as `0..1` (`grainfield.hpp:122-129`) and `randomized()` passes that positive value directly as `attenuverter` while hard-coding `has_cv=false` and `cv=0` (`grainfield.hpp:395-410`). This makes the negative/peaked half of the original control unreachable, and it calls the controls “CV Amount” even though no host/CV source is supplied. The focused test at `test_grainfield.cpp:457-476` only verifies that positive values are copied through; it does not test the original centered law, negative side, CV side, or random distribution.

This should be classified **behavioral parity gap**, not a cosmetic difference. A faithful software control needs an explicit centered parameter contract (or a deliberate product-local redesign) and tests for negative, zero, positive, with-CV and no-CV paths.

### 3. Pitch law differs from the hardware control

The original `PotChannel::LAW_DETUNE` quantizes the PITCH pot around virtual intervals `{0, 1, 3, 4, 7, 12, 24}` with smoothing (`/Users/danielraffel/Code/eurorack/beads/cv_reader.h:114-130`). Grainfield exposes a continuous linear `-24..+24` semitone host parameter (`grainfield.hpp:106`) and only tests that MIDI note 60 and 72 differ (`test_grainfield.cpp:110-144`). Continuous host pitch may be a reasonable software enhancement, but it is not the original pot response. Decide explicitly whether to preserve the notched law for the panel knob while allowing a separate continuous automation parameter, or document this as a deliberate deviation.

### 4. Seed/gate modes and external controls are reduced

The original synth receives `SeedButtonState` (`OFF`, `ON`, or `LOCKED`) plus a gate stream. Grainfield always passes `SEED_BUTTON_STATE_OFF` (`grainfield.hpp:356` and `361`) and derives the gate only from MIDI note events. It has no SEED button/clock input, latch mode, gated mode, clock-divider/probability behavior, or output-gate selection. Those are central documented behaviors in `manual.md` (“Latched”, “Clocked”, and “Gated and triggered”). They are outside the declared synth-only MIDI slice, but they prevent a full Beads behavior claim.

### 5. MIDI event timing is only partly sample accurate

The gate flag expansion is sample-offset aware (`grainfield.hpp:246-323`), and the focused test verifies note-on at offset 3 and note-off at offset 6 (`test_grainfield.cpp:253-292`). However, `current_note_`, `velocity_gain_`, and `pitch_bend_semitones_` are updated while scanning the whole block (`grainfield.hpp:250-308`), then one `parameters` object and one final `velocity_gain_` are applied to every output sample (`grainfield.hpp:325-374`). A velocity or pitch-bend event at offset 128 therefore affects the preceding samples in that block. Same-offset note-on/note-off ordering is also collapsed into OR-ed rising/falling flags (`grainfield.hpp:261-280`), so ordering cannot be represented.

The existing receipt calls the MIDI contract deterministic, but it does not prove sample-accurate velocity or pitch-bend. Add partition-invariance and offset-sensitive fixtures for each expressive event before claiming the full event contract.

### 6. Freeze and live input are not full hardware parity

The manual describes continuous stereo recording, automatic gain/mono detection, freeze-gate behavior, ten-second freeze-buffer autosave/restore, four media quality paths, and delay mode. Grainfield's v1 documentation honestly calls live input experimental/deferred and freeze persistence unavailable. The source contains a product-local `kMode` path into `beads::Processor`, but `effect_media_` is fixed to `MEDIA_DIGITAL` and host support is fixed to 48 kHz (`grainfield.hpp:203-224`, `457-467`). This is useful experimental coverage, not the original 24/32/48 kHz hardware quality behavior or AGC/auto-detection contract.

## Provenance and receipt integrity

There is a current-head consistency problem that must be fixed before closeout:

- Observed checkout HEAD is `80759092d7683be3706bcee2ff9afa437cfae06f`.
- `provenance/current-integration-validation-20261018.json` and the current gate matrix are bound to `80015bea33a68137874831a0000bf23ffc3e46e2`.
- `provenance/current-provenance-reconciliation-20261018.json` is bound to `4117f8a128b61776eff3c98e6c8d40d91da154ce`.
- Source-port receipts are bound to older project heads, and many format/editor receipts are historical.
- `tools/validate_control_map.py --control-map provenance/control-map.json --skins provenance/knob-skin-manifest.json` passes while reporting **24 controls; 5 bound; 19 provisional**. The current editor source binds 12 controls (five primary + feedback/dry-wet/reverb + four CV amounts), and `provenance/control-map-source-audit-20261018.json` reports seven auxiliary pots as source-backed. `control-map.json`, `tools/build_control_map.py`, the planning docs, and the current editor therefore disagree.
- The clean checkout does not contain `provenance/generated/beads-panel.svg`; `tools/validate_assets.py` fails closed when asked to validate it. That is correct for rights safety, but the screenshot is a scaffold capture rather than a panel-art fidelity capture.

All receipt claims should be regenerated or marked `historical` at one common Grainfield/Pulp/Beads head. In particular, do not let the stale 5-bound/19-provisional map appear as current evidence for the 12-bound editor.

## Proposed adversarial closeout gates

1. **Exact-head gate.** Inventory every receipt, artifact, test binary, and screenshot with the observed Grainfield HEAD, Pulp SDK head, Beads/stmlib heads, tool versions, and SHA-256. Any mismatch is historical until rerun.
2. **Source-vs-port A/B gate.** Regenerate the source and port harnesses from pinned commits. Cover all four synth resolutions, neutral/extreme parameter combinations, freeze transitions, density trigger modes, fixed seeds, 32/64/128/256/1024 partitions, and reset. Compare raw bytes plus max error, RMS, and spectral metrics. Keep source fixtures internal unless licensed.
3. **Adapter MIDI gate.** Add offset-sensitive fixtures for note-on/off, overlap/retrigger, velocity, pitch bend, sustain, same-offset ordering, all-channel policy, and reset. Verify that event timing affects only samples at/after the event and that block partitioning does not change the declared contract.
4. **Parameter-law gate.** Implement or explicitly reject the original attenuverter law, pitch detune notches, CV/no-CV randomization, density exponential FM, and freeze/seed state machine. Test both signs and boundaries instead of only positive pass-through.
5. **UI fidelity gate.** Use a rights-cleared straight-on reference or an independently authored approved spec. Compare normalized geometry, labels, typography, knob cap/annulus/pointer, jack collars, buttons, LEDs, and resize behavior at 1x/2x/3x. Require an image overlay/diff and a negative control proving no hidden upstream bitmap was embedded. Keep the current scaffold as the explicit fallback until this passes.
6. **Host/state gate.** For each claimed format, load the current exact artifact and observe editor, audio, automation, modulation, state save/restore, and clean shutdown. Keep AU/AAX/LV2/standalone and signing states partial or blocked where evidence is missing.
7. **Rights gate.** Close file-level Beads DSP/stmlib/waves.bin notices, panel/CAD/photo permission and CC BY-SA share-alike handling, and trademark/non-affiliation review. MIT for Grainfield source does not clear upstream material.

The release decision should remain **blocked** until these gates are either passed with current evidence or explicitly recorded as unsupported with a user-approved scope. The current work is sufficiently real to continue UX and A/B development, but it has not crossed the adversarial bar for “faithful” or “complete.”
