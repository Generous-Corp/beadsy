# Seed mode mapping

Beads' `TriggerGenerator::Render` accepts the source enum
`SeedButtonState` from `third_party/beads_dsp/beads/dsp/trigger_generator.h`:

- `OFF` merges the external seed/gate stream without adding a button gate.
- `ON` treats the seed button as held and merges a constant high gate.
- `LOCKED` uses an external seed/gate stream as the clock/divider source. At
  the manual-tap density midpoint, the upstream implementation emits no taps
  while locked.

Beasy exposes those three source-backed states as the host parameter
`Seed Mode` (`kSeedMode`, values `0`, `1`, and `2`). The boundary helper rounds
and clamps host floating-point values and maps non-finite values to `OFF`.
The MIDI-derived gate stream remains the software equivalent of the external
seed input. The parameter is available for host automation; it is not yet
represented by a panel knob or a hardware-CV control.

This slice deliberately does not infer Beads' hardware-only combo actions,
output-gate selection, monophonic setting, freeze-buffer persistence, or an
independent random-seed control. Those behaviors require separate source and
host evidence.

Focused evidence is in `test_beasy.cpp`:

- `Beasy seed mode mapping is bounded and source-backed`
- `Beasy seed states preserve the upstream trigger contract`
- `Beasy routes the Seed Mode parameter to the adapted DSP`
