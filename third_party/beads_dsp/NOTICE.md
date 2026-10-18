# Beads DSP source notice

This directory contains selected MIT-licensed DSP source adapted from Mutable Instruments Beads by Emilie Gillet, pinned to Beads commit `f4cfa7697095a28d292bff758beb51deb49b5a86`, and selected `stmlib` files pinned to `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`. Original per-file notices are retained.

Only the DSP-reachable `stmlib` files listed in `provenance/source-manifest.json` are included, including the compiled `dsp/units.cc` and `utils/random.cc` translation units. `dsp/dsp.h` carries the original MIT notice and a documented host-guard adaptation for this port. The `stmlib` repository contains separately licensed STM and GPL serial material; those directories are intentionally excluded.
