# External input prototype

This isolated prototype starts at Grainfield head `33417e1` and adds a real
stereo live-input path beside the existing MIDI wavetable synth. It is a
development slice, not a claim of full Beads hardware parity.

## Contract

- The descriptor declares one required stereo `Main In` bus and one stereo
  `Main Out` bus.
- The product-local `Mode` parameter selects the existing synth (`0`) or live
  input (`1`). MIDI remains accepted in both modes; live input currently uses
  the input signal as its source and does not use MIDI as a second audio voice.
- Only `MEDIA_DIGITAL` at 48 kHz is enabled. The upstream 24/32 kHz media
  modes remain out of scope until an explicit host-rate conversion contract is
  implemented.
- The effect's delay banks, reverb storage, frame conversion, and callback
  scratch are allocated during `prepare()`. The processor reports a resource
  estimate and fails closed when host limits reject it.

## Upstream adaptation

The vendored Beads effect sources now include `processor.cc`, `delay_controller.cc`,
`delay_clock.cc`, and `mu_law.cc`. `Processor::Process` accepts a stereo
`FloatFrame` input and uses a product-local `Processor::Scratch` prepared to the
host maximum block size. This removes the top-level VLA scratch arrays from the
effect callback. The upstream `GranularProcessor` still contains VLA code in
its ratio-2 branch; this prototype never configures those modes and therefore
does not claim them safe.

## Validation

The focused test sends a sustained stereo signal through live-input mode and
checks finite, nonzero output. Existing synth, MIDI, state, freeze, quality,
editor, and 48 kHz tests remain in the suite. A full format/host receipt is
still required before product integration.
