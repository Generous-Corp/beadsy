# Quality mode slice

This slice exposes the four quality paths already present in the copied Beads
synth source through Beadsy's `Quality` parameter:

| Value | Upstream `beads::Synth::Resolution` |
| ---: | --- |
| 0 | `RESOLUTION_4_BIT` |
| 1 | `RESOLUTION_16_BIT_6_BIT` |
| 2 | `RESOLUTION_7_BIT` |
| 3 | `RESOLUTION_16_BIT` |

The mapping is product-local adapter code. The enum and DSP implementation
remain under `third_party/beads_dsp`, including their Emilie Gillet MIT notice;
the adapted source is not relicensed by this change. The parameter is an
integer, defaults to the full 16-bit path, and is clamped before entering the
upstream enum.

The deterministic test proves that all four paths render finite output and
that the lowest and highest modes are observably distinct for the same seeded
MIDI render. This does not implement Beads' external-audio processor quality
modes, 24/32/48 kHz switching, freeze, or input path.
