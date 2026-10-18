# External audio and effect mode analysis

Date: 2026-10-17
Head reviewed: `d5cc980` (`fix(editor): match knob skins to mapped panel geometry`)

This is a source and integration review of the vendored Beads live-input
processor. It does not enable external audio, change the plugin descriptor, or
change the existing synth/audio receipts. The conclusion is **blocked pending
a dedicated effect-mode port**.

## What the vendored API provides

`beads::Processor` is a stereo effect processor, separate from
`beads::Synth`. Its public seam is `Init(uint8_t* buffers[], uint16_t*
reverb_buffer)`, `Configure(Media, bool)`, and `Process(..., const
FloatFrame* in, FloatFrame* out, size_t size)` (`third_party/beads_dsp/beads/dsp/processor.h:53-73,155-164`). The four media modes are dirty tape, clean tape, vintage digital, and digital (`processor.h:58-63`). The implementation initializes all four granular processors before selecting digital by default (`processor.cc:43-67`).

The effect owns a large delay store: `kDelayBufferSizeBits` is `768 * 1000 *
8`, described as roughly four seconds of 48 kHz, 16-bit stereo delay storage
(`processor.h:50-51`). Four `GranularProcessor` members partition that storage
at different bit resolutions (`processor.h:166-189`), and the reverb requires
a caller-owned `uint16_t` buffer (`processor.cc:43-55`). A port therefore needs
an explicit prepare-time memory budget and ownership plan; constructing the
class alone is not enough.

## Why it cannot be enabled in the current slice

1. **It is not in the build.** Beadsy's DSP target currently compiles
   `synth.cc`, `trigger_generator.cc`, resources, and two reachable stmlib
   units. It does not compile `processor.cc` or the effect dependency graph
   (`CMakeLists.txt:27-37`).
2. **The callback is not currently real-time safe for the Pulp contract.**
   `Processor::Process` creates four arrays whose sizes are the current block
   length on the audio stack (`processor.cc:124-126,163`). These must become
   prepare-sized storage or bounded scratch before an adapter can call this
   code from Pulp's process seam.
3. **Quality modes have different internal rates.** With
   `ENABLE_HARDWARE_24K`, the mode table is 24 kHz dirty tape, 48 kHz clean
   tape, 32 kHz vintage digital, and 48 kHz digital (`processor.h:78-85`).
   Initialization confirms those rates (`processor.cc:46-49`). Beadsy's
   current host contract accepts only 48 kHz (`beadsy.hpp:132-143`), so
   enabling all modes requires an explicit resampling and latency/quality
   policy. Silently running the internal mode at the host rate would be
   incorrect.
4. **The host format is currently an instrument with no input bus.** The
   descriptor declares `input_buses = {}`, stereo output, and MIDI acceptance
   (`beadsy.hpp:45-55`). Adding live stereo input changes host metadata and
   product scope; it cannot be hidden behind a parameter while retaining an
   honest descriptor.
5. **Effect state is a separate product seam.** The processor has delay-mode
   transition state, four media-specific processors, freeze input, dry/wet,
   reverb, limiter, and optional output-gate behavior (`processor.cc:102-119,
   131-170,206-251`). The current Beadsy state and synth adapter do not
   define reset, snapshot, or automation semantics for that branch.

The existing reconnaissance reaches the same boundary: the effect branch is
documented as a later phase because it combines external input, large delay
buffers, and 24/32/48 kHz hardware-oriented modes
(`docs/planning/beads-dsp-recon.md:63-68`). The current product status
explicitly describes the implemented slice as the internal wavetable branch
and does not claim live-input processor quality modes
(`docs/planning/implementation-status.md`, Quality and Documentation entries).

## Required design for a future slice

1. Decide whether Beadsy becomes a dual-purpose instrument/effect with a
   declared stereo input bus, or whether live input gets a separately
   identified effect product/bundle. Record the decision before changing the
   descriptor.
2. Add `processor.cc` and every reachable effect source to an isolated DSP
   target, preserving the upstream MIT notices and dependency provenance.
3. Move the four variable-length callback arrays into prepare-sized storage;
   allocate delay and reverb memory during prepare, enforce a maximum block
   size, and reject or safely handle larger blocks.
4. Start with `MEDIA_DIGITAL` at 48 kHz, or add tested host-rate conversion
   around the 24/32 kHz variants. The selected policy must include latency and
   quality receipts.
5. Add deterministic stereo-input tests for finite/unclipped output, dry
   passthrough, wet response, block partition invariance, reset/configure,
   freeze, and the `parameters.size > 0.98` delay-mode transition.
6. Add Pulp audio-harness and host-format receipts before claiming live-input
   support. Keep the existing synth path and the new effect path separate in
   documentation; this work would not establish full Beads hardware parity.
