# LV2 host capability audit

Status: **blocked by host capability**, independent of Grainfield's bundle metadata.

The current Grainfield LV2 bundle declares `urid:map` as a required feature and
includes an atom MIDI input. The Pulp host path used by the validation command
has two independent gaps:

1. `core/host/src/plugin_slot_lv2.cpp` calls the plugin descriptor's
   `instantiate` callback with a null feature list. Grainfield therefore receives
   no `LV2_URID__map` feature and correctly refuses instantiation in
   `core/format/include/pulp/format/lv2_entry.hpp`.
2. The same host slot only discovers and connects `AudioPort` and `ControlPort`
   entries. Its source comment explicitly says atom/event ports are not
   connected and host MIDI is discarded. Supplying a URID map alone would still
   be insufficient for a synth with an atom MIDI input; the host must provide a
   stable URID map and connect/encode the LV2 atom sequence MIDI port.

The recorded probe is therefore an honest host limitation, not evidence that
Grainfield's LV2 descriptor or DSP is invalid:

- Command: `pulp host build/LV2/Grainfield.lv2 --format lv2`
- Receipt: `provenance/lv2-host.txt`
- Receipt SHA-256: `1da496986ae49210ee362fe4a03b3df842ed76aac9ad809c6764649dd5410ddd`
- Recorded result: the host did not provide `LV2_URID__map`; instantiation was
  refused.

Closing this gate requires a separately reviewed Pulp host change that owns
feature negotiation, URID lifetime, atom sequence buffer sizing, MIDI event
encoding, and cleanup. Until that lands and is revalidated against a real LV2
host, Grainfield must continue to report LV2 as partial/blocked.
