# Beads documentation review

Reviewed 2026-10-18 against the local Mutable Instruments documentation tree:
`/Users/danielraffel/Code/mutable-instruments-documentation/docs/modules/beads`.
This is a behavioral and visual reference record. It does not copy or
relicense the documentation or its images.

## Product behavior confirmed by the manual

Beads is primarily a live stereo granular processor. It continuously records
incoming audio into a RAM buffer, supports automatic mono/stereo switching,
automatic input gain, freeze and freeze-buffer restore, and four quality modes:
48 kHz/16-bit, 32 kHz/12-bit, 24 kHz/12-bit, and 24 kHz/8-bit µ-law. The manual
describes three grain-generation modes (latched, gated, and clocked), up to 30
overlapping replay heads, and grain parameters sampled when each grain starts:
TIME, SIZE, SHAPE, and PITCH.

Each of those four parameters has an attenurandomizer with direct CV, CV-driven
randomization, or internal randomization. DENSITY/TIME/SEED are repurposed for
delay and beat-slicing behavior when SIZE is set to an endless grain. FEEDBACK,
DRY/WET, and REVERB form the stereo mix path. With no audio input, the module
falls back after a delay to eight internal Plaits-derived wavetable banks.

The current Beasy product implements the documented internal wavetable
synth branch only. Its explicit boundaries are global MIDI triggering, five
bound panel parameters, a fixed 48 kHz engine, and no live audio-input path.
The missing input/freeze/quality/attenurandomizer/delay-mode behavior remains a
planned parity phase and must not be described as complete Beads emulation.

## Visual and control references

The manual and index identify the panel controls as DENSITY, TIME, PITCH, SIZE,
SHAPE, four attenurandomizer rows, FEEDBACK, DRY/WET, and REVERB, with LEDs and
buttons for quality, FREEZE, SEED, and CV assignment. The supplied front and
manual images are useful for straight-on knob material, pointer shape, labels,
button treatment, and relative scale. `signal_flow.png`, `quality_table.png`,
`wavetable_quality_table.png`, `delay_density.png`, `delay_div.png`,
`ar_cv.png`, and `ar_internal.png` are behavioral diagrams rather than runtime
assets.

Image hashes and dimensions were inventoried from the documentation checkout on
the review date. The primary reference is:

| Asset | Dimensions | SHA-256 |
|---|---:|---|
| `images/front.jpg` | 593 x 1080 | `4346619e9d47cb8d8ac9604d3661776b7ce5dcc37cfe74f36d3d7f3d62b031bf` |
| `images/manual.png` | 447 x 600 | `cac1417d67262373a3ca3f4813ff0943cbb5ffa7fbcf0f9f6de643155b37f8bcf` |
| `images/signal_flow.png` | 546 x 175 | `fb711e9efd2f55ff3ef64f2fd1030e2a282d2ba81095278d39df5399cb151cc8` |
| `images/quality_table.png` | 554 x 252 | `f9befef4fad07f01dfbd7f2154dbac0c3de4792fae4d0167b495c3bc9dbb51fa` |
| `images/wavetable_quality_table.png` | 554 x 108 | `6f95e755fb140f39bf5ed45b475f57ed13c3964d7c574cd9ca28b808f492e705` |

The full inventory remains in the source checkout; no documentation image is
embedded in the Beasy build. Artwork and trademarks remain rights-gated.

## Fidelity impact

The documentation confirms that a faithful future version needs an audio-input
processor mode, quality-dependent DSP, freeze state, gate/clock scheduling,
attenurandomizer semantics, and delay mode. The current synth-only scope is a
valid first product slice, but the release definition must retain these as
explicit gates rather than inferring them from the existing Beads DSP subset.
