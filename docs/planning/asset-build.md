# Clean checkout asset behavior

The converted Beads panel SVG under
`provenance/generated/beads-panel.svg` is an authoring probe with artwork
rights still pending. It remains ignored and is never required for the DSP
build or for installing a plugin.

When that probe exists, CMake embeds it into the native editor at configure
time. A clean checkout without the probe instead embeds a deterministic,
neutral panel rectangle with the same viewBox. This keeps headless/editor
compilation reproducible without copying or redistributing rights-gated
artwork. The neutral fallback is not visual fidelity evidence and does not
change the product's artwork-rights gate.
