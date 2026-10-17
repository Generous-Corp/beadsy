# Beads DSP reconnaissance

Date: 2026-10-17

This report is read-only reconnaissance against Beads commit
`f4cfa7697095a28d292bff758beb51deb49b5a86` at
`/Users/danielraffel/Code/eurorack/beads` and Pulp `origin/main` commit
`0eadeac05ed8176abcce566edadbcd7cc2fafd29`. It does not copy source into
Pulp and does not modify shared Pulp files.

## Source and provenance

- The Beads repository has no top-level `LICENSE`, `NOTICE`, or `README` in
  the tracked tree. The C++ DSP files carry the MIT permission notice inline,
  attributed primarily to Emilie Gillet (with additional authors in
  `beads.cc`). Every tracked `dsp/**` C++/header file inspected has that
  notice; generated `resources.cc`/`resources.h` also carry it.
- `beads/.gitmodules` pins the external `stmlib` repository at submodule
  commit `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`. The local submodule is
  uninitialized (the directory exists but is empty), so a port must acquire
  that exact commit and record its repository and license separately. Do not
  silently substitute a current `stmlib` checkout. The pinned stmlib `LICENSE`
  states that its core is MIT but also contains separately licensed material,
  including GPL code under `programming/serial`; the plugin dependency manifest
  must include only the stmlib files reachable from the DSP subset and must
  exclude STM/bootloader/programming code.
- The tracked wavetable payload is `resources/waves.bin` (SHA-256
  `c672fccb061496d0229bfc5c2d55ec61f367e1539dda5870fd667a059c90f634`);
  generated C++ is `resources.cc` (SHA-256
  `65bd67ca50cbed0b45939d0f02b4932964d211abc4723e58544676b9315618d7`).
  The port needs a reproducible resource-generation/provenance step, not a
  hand-edited copy of the generated array.
- The Beads checkout contains untracked user-supplied `hardware_design/beads.dwg`
  and `beads.dxf`. They are outside this DSP source receipt and must not be
  included in an MIT code bundle without the separate artwork/mechanical-rights
  decision.

## Portable DSP boundary

For the requested instrument-first product, the most direct source path is the
original synth branch, not the external-audio effect branch:

- `dsp/synth.h:45-75` and `dsp/synth.cc:54-137` expose `Synth::Init` and
  `Synth::Render`. `Synth::Init` is fixed at 48 kHz and initializes the
  wavetable voice, 32-grain wavetable player, trigger generator, reverb, and
  limiters.
- `dsp/wavetable_voice.h:43-111` is the continuous wavetable oscillator. It
  maps `pitch.base` as semitones around `kWavetableRootNote = 130.81 Hz`,
  crossfades wavetable banks, and applies the original anti-aliasing/one-pole
  stages.
- `dsp/wavetable_granular_player.h:45-79`, `granular_player.h`, `wavetable_grain.h`,
  `grain.h`, `grain_envelope.h`, and `voice_stealing_envelope.h` implement the
  32-grain trigger/overlap path. `trigger_generator.cc/.h` supplies density,
  regularity, and gate generation. `randomized_parameter.h` and
  `modulated_parameter.h` preserve CV/randomization semantics.
- `dsp/fx/reverb.h` plus `dsp/fx/fx_engine.h`, `limiter.h`, and
  `resources.cc/.h` are needed by the synth render path. `frame.h` is the
  stereo frame type. These files are independent of STM32 peripherals once
  `stmlib` is available.
- `beads.cc:244-260` confirms the hardware selects `Synth::Render` only in
  synth mode and passes `Parameters`, density, mix, freeze, and seed arrays.
  It also confirms the hardware's input/effect branch is a separate
  `Processor::Process` path.

The effect branch is available later but should not be the first instrument
slice. It is centered on `dsp/processor.h/.cc` and the four
`GranularProcessor` instantiations. It hard-codes hardware-oriented quality
modes (`ENABLE_HARDWARE_24K` at `processor.h:46`), 24/32/48 kHz mode changes,
large delay buffers, and external audio input. Porting it first would obscure
the MIDI instrument contract and multiply sample-rate validation work.

## Pulp integration seam

Pulp's canonical seam is `pulp::format::Processor` in
`core/format/include/pulp/format/processor.hpp:131+`:

1. `descriptor()` declares an Instrument with no input bus, stereo output, and
   `accepts_midi = true`.
2. `define_parameters()` registers every automatable Beads control in
   `state::StateStore`.
3. `prepare(const PrepareContext&)` allocates the delay/reverb/resource storage
   and initializes the DSP at the host sample rate.
4. `process(BufferView<float>&, ..., MidiBuffer&, ..., ProcessContext&)` consumes
   sample-offset MIDI and renders without allocation, locks, I/O, or exceptions.

`examples/PulpSynth/pulp_synth.hpp` is the closest verified pattern: it sorts
`MidiBuffer`, applies note events at their `sample_offset`, and uses
`HeadlessHost` for deterministic tests. `core/format/include/pulp/format/headless.hpp`
supports explicit sample rate, block size, MIDI, transport, and parameter-event
runs. The audio-harness skill requires DSP acceptance through
`RenderScenario`/`AudioContract` and offline metrics/Doctor rather than a
hand-rolled FFT.

The original `Synth` API has no note-on/note-off concept: it receives a per-frame
`SeedButtonState`/`GateFlags` stream and a global `Parameters` object. A Pulp
adapter therefore needs a deliberate MIDI translation layer. Recommended first
contract:

- **Mono/global voice, hardware-faithful:** the newest active MIDI note sets the
  global `pitch.base`; note-on emits a seed/gate edge to the trigger generator;
  note-off releases the global gate and does not pretend the underlying 32 grains
  are per-note voices. Define last-note priority and behavior for overlapping
  notes explicitly.
- Preserve sample-accurate event offsets, channel filtering, note velocity
  policy, and reset behavior in tests. Polyphonic MIDI should be a separately
  named enhancement because the original `Synth` state is global.
- Do not use `Processor::Process` for the first instrument: it requires an input
  signal and its delay-mode state is not a MIDI voice engine.

`Synth::Render` is fixed to 48 kHz (`synth.cc:59`) while Pulp hosts can prepare
at arbitrary rates. The first implementation must either resample around a
48-kHz internal engine with a documented latency/quality contract or reject
unsupported rates; silently running the 48-kHz phase increments at 44.1/96 kHz
would change pitch and grain timing. This is a release gate, not an optimization.

The original code also uses variable-length stack arrays in `Processor::Process`
(and trigger arrays in the synth path). A Pulp port must move block scratch to
`prepare()`-sized storage or bounded fixed storage and prove the configured
resource limits. The large delay buffers are only needed for the effect branch;
the synth still needs the reverb buffer and its generated wavetable payload.

## Recommended next implementation slice

Create a standalone, independently branded **Beads-inspired instrument DSP
library** in the new plugin repository, carrying only the MIT-noticed synth
subset and exact `stmlib` submodule receipt. Keep it outside Pulp core initially:

1. Add a resource-generation script/manifest and `LICENSES.md` entries for
   Beads and `stmlib`; preserve every upstream notice.
2. Wrap the original `Synth` behind a small Pulp-facing adapter with explicit
   mono MIDI semantics and a 48-kHz internal/resampling decision.
3. Add a minimal `Processor` with parameters for time, size, shape, pitch,
   density, feedback/bank, dry-wet, reverb, and freeze/mono mode. Keep base
   parameter values host-automatable; modulation can layer later through the
   shared modulation API.
4. Add `HeadlessHost`/audio-harness tests for note-on audibility, note-off and
   overlap semantics, sample-offset partition invariance, finite/unclipped
   output, sample-rate matrix, reset, and deterministic seeded renders.
5. Only after this slice is green, evaluate whether any resampler, MIDI voice
   policy, or scratch-buffer utility is genuinely reusable enough to move into
   Pulp core. Generic core changes require the source-authority/routing review.

This slice is estimated as moderate rather than trivial: the DSP source is
portable, but exact `stmlib` acquisition, 48-kHz adaptation, MIDI semantics,
resource licensing, and real-time scratch ownership are the load-bearing work.
