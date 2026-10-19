# Beadsy

Beadsy is an independent, open-source Pulp software instrument inspired by the published Mutable Instruments Beads architecture. It is not affiliated with, endorsed by, or sponsored by Mutable Instruments. The product name, branding, and UI implementation are independent.

With deep respect and gratitude to **Emilie Gillet**—the GOAT of this
instrument lineage—whose pioneering work and generous open-source releases
made it possible. Thank you, Emilie, for showing what instrument design and
open hardware can be. Emilie and Mutable Instruments are not affiliated with
or endorsing Beadsy; their names and marks remain their own.

The current product supports MIDI-driven internal wavetable synthesis and
optional host-routed audio for live granular processing. AUv3, VST3, and CLAP
expose the declared main/sidechain buses; the AUv2 instrument remains the
compatibility path without live input. Host routing supplies the audio input;
the editor exposes the source choice without pretending to enumerate tracks on
its own.
The live-input path is still a prototype until independent AU, VST3, and CLAP
host receipts are current. The project is being built in phases, and no Beads
artwork or firmware source is redistributed until each file has a recorded
license and attribution decision.

## License

Original Beadsy source is MIT licensed. Third-party source, artwork, and resources retain their original licenses; see [LICENSES.md](LICENSES.md), [ATTRIBUTION.md](ATTRIBUTION.md), and [provenance/source-manifest.json](provenance/source-manifest.json).

## Build

This repository uses the Pulp SDK. From a configured Pulp checkout:

```sh
pulp build
pulp test
```

The first product milestone declares CLAP, VST3, AU, AUv3, LV2, and standalone.
Current validation is intentionally scoped: CLAP and VST3 host validation pass;
AU passes the governed GUI-bootstrap `auval` path while installed-component
discovery remains open; LV2 has a passing binary/TTL probe but no external host
or state round-trip proof; and standalone state save/load passes through the
bounded CLI bridge. AAX remains blocked until the Avid SDK, entry point,
validator, and Pro Tools evidence are available. See the current [format
receipt](provenance/beadsy-format-status-current.json) and [progress
ledger](provenance/beadsy-current-progress-20261018.json) before treating any
format as release-ready.

To inspect the formats declared by `pulp_add_plugin` against a particular build
tree, generate the machine-readable receipt with:

```sh
python3 scripts/validate_formats.py --output provenance/format-status.json
```

The receipt records artifact hashes and Pulp target presence only. It leaves
host validation as `not_run`; a missing artifact is reported as `blocked`.

The current phased delivery plan, including host-routed source selection and
the remaining evidence gates, is in
[provenance/beadsy-plan-20261019.md](provenance/beadsy-plan-20261019.md).
