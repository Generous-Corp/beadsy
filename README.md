# Grainfield

Grainfield is an independent, open-source Pulp software instrument inspired by the published Mutable Instruments Beads architecture. It is not affiliated with, endorsed by, or sponsored by Mutable Instruments. The product name, branding, and UI implementation are independent.

The project is being built in phases. The current phase is provenance and source-port preparation; no Beads artwork or firmware source is redistributed until each file has a recorded license and attribution decision.

## License

Original Grainfield source is MIT licensed. Third-party source, artwork, and resources retain their original licenses; see [LICENSES.md](LICENSES.md), [ATTRIBUTION.md](ATTRIBUTION.md), and [provenance/source-manifest.json](provenance/source-manifest.json).

## Build

This repository uses the Pulp SDK. From a configured Pulp checkout:

```sh
pulp build
pulp test
```

The first product milestone targets CLAP, VST3, AU, and standalone. AAX remains opt-in and blocked until the Avid SDK, entry point, validator, and Pro Tools evidence are available.

To inspect the formats declared by `pulp_add_plugin` against a particular build
tree, generate the machine-readable receipt with:

```sh
python3 scripts/validate_formats.py --output provenance/format-status.json
```

The receipt records artifact hashes and Pulp target presence only. It leaves
host validation as `not_run`; a missing artifact is reported as `blocked`.
