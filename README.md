# Beasy

Beasy is an independent, open-source Pulp software instrument inspired by the published Mutable Instruments Beads architecture. It is not affiliated with, endorsed by, or sponsored by Mutable Instruments. The product name, branding, and UI implementation are independent.

With deep respect and gratitude to **Emilie Gillet**, the GOAT whose pioneering
work and generous open-source releases made this lineage possible. Thank you,
Emilie, for showing what instrument design and open hardware can be. Emilie and
Mutable Instruments are not affiliated with or endorsing Beasy; their names
and marks remain their own.

The project is being built in phases. The current phase is provenance and source-port preparation; no Beads artwork or firmware source is redistributed until each file has a recorded license and attribution decision.

## License

Original Beasy source is MIT licensed. Third-party source, artwork, and resources retain their original licenses; see [LICENSES.md](LICENSES.md), [ATTRIBUTION.md](ATTRIBUTION.md), and [provenance/source-manifest.json](provenance/source-manifest.json).

## Build

This repository uses the Pulp SDK. From a configured Pulp checkout:

```sh
pulp build
pulp test
```

The first product milestone declares CLAP, VST3, AU, LV2, and standalone.
Current validation is intentionally scoped: CLAP and VST3 host validation pass;
AU passes the governed GUI-bootstrap `auval` path while installed-component
discovery remains open; LV2 has a passing binary/TTL probe but no external host
or state round-trip proof; and standalone state save/load passes through the
bounded CLI bridge. AAX remains blocked until the Avid SDK, entry point,
validator, and Pro Tools evidence are available. See the current [format
receipt](provenance/beasy-format-status-current.json) and [progress
ledger](provenance/beasy-current-progress-20261018.json) before treating any
format as release-ready.

To inspect the formats declared by `pulp_add_plugin` against a particular build
tree, generate the machine-readable receipt with:

```sh
python3 scripts/validate_formats.py --output provenance/format-status.json
```

The receipt records artifact hashes and Pulp target presence only. It leaves
host validation as `not_run`; a missing artifact is reported as `blocked`.
