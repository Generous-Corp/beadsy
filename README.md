# Beadsy

Beadsy is a granular texture instrument and effect for macOS, built on the
[Pulp](https://github.com/Generous-Corp/pulp) SDK. It is inspired by Beads by
Mutable Instruments and adapts its MIT-licensed firmware; Beadsy is an
independent project, not affiliated with or endorsed by Mutable Instruments or
Emilie Gillet.

## Download

Signed, notarized installers are on the
[releases page](https://github.com/Generous-Corp/beadsy/releases). Each release
ships two packages:

- **Beadsy** – the instrument and the FX companion (standalone apps, AU, VST3, CLAP).
- **Beadsy Lab** – exploration builds installed beside them, with a selectable
  audio source (internal synth, main input, side chain) and a note gate.

Install the `.pkg`, then restart your DAW. Logic may need a plug-in re-scan.

## Using it

Right-click any knob or button for its modulation menu (two LFOs, routable to
every control). The **?** at the bottom-left of the Lab editor opens the help
panel, which explains the controls and credits the sources.

## Build from source

Requires a Pulp SDK install and CMake with Ninja.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DPulp_DIR=<pulp-sdk>/lib/cmake/Pulp
cmake --build build
ctest --test-dir build
```

Bundles land in `build/` (`Beadsy.app`, `AU/`, `VST3/`, `CLAP/`).

## License and credits

Beadsy's own source is MIT licensed ([LICENSE](LICENSE)). The adapted Beads
firmware and stmlib files are MIT and keep their original notices; the panel
artwork is CC BY-SA 3.0 and is not part of the MIT grant. See
[LICENSES.md](LICENSES.md), [ATTRIBUTION.md](ATTRIBUTION.md) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Thank you, Emilie Gillet, for the instrument and for publishing its source.
