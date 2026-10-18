# Freeze mode slice

Grainfield's product-local `Freeze` toggle supplies a constant per-sample
freeze stream to the adapted `beads::Synth::Render` path. This reuses the
upstream synth behavior: frozen grains stop accepting new trigger work and the
wavetable voice holds its freeze-controlled motion while the existing output
path continues to render.

The toggle is part of the normal Pulp parameter state, so saving and restoring
a plugin state restores the requested freeze mode. The upstream internal synth
does not expose its delay/grain buffer contents or a snapshot/restore API;
Grainfield therefore does not claim Beads' freeze-buffer restore behavior.

The adapted Beads source retains its Emilie Gillet MIT notices under
`third_party/beads_dsp`. This change adds only the independent adapter
parameter and deterministic tests; it does not copy or relicense upstream
artwork or the external-audio processor.
