# Third-party licenses

This file is intentionally explicit about boundaries. Beadsy's original source is MIT licensed under `LICENSE`.

- Beads software files carry Emilie Gillet MIT notices where copied or adapted; retain the notices beside any accepted port.
- `stmlib` is tracked at the exact source commit recorded in `provenance/source-manifest.json`; its internal third-party directories must be audited separately before use.
- The panel artwork is adapted from the Beads front panel (Mutable Instruments, CC BY-SA 3.0) and is distributed under CC BY-SA 3.0, not under the MIT license; the attribution and the list of changes are in THIRD_PARTY_NOTICES.md.
- Mutable Instruments names, logos, and trademarks are not Beadsy branding.
- Griddy-MIDI-Effect-Plugin is GPLv3 and is a UX reference only; no Griddy code or GPL-derived resource is included here.

## Locally recorded origins

- Adapted Beads DSP origin: `/Users/danielraffel/Code/eurorack/beads/dsp/synth.cc` (SHA-256 `2244105032dcb2b3592a94348f50b96bfcfb64548db6e75ec88f8e413cd9b6cb`). The copied notice inventory is `third_party/beads_dsp/NOTICE.md` (SHA-256 `59c593dcdcf95a05305f74a408bf3eb978871d8a5251908e5138a5578ebe9d75`).
- `waves.bin` origin: `/Users/danielraffel/Code/eurorack/beads/resources/waves.bin` (SHA-256 `c672fccb061496d0229bfc5c2d55ec61f367e1539dda5870fd667a059c90f634`). Its local generator inputs are `resources/resources.py` and `resources/wavetables.py`; lineage is recorded, while notice and redistribution review remain open.
