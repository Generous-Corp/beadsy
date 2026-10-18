# Illustrator control-geometry evidence

`provenance/ai-control-geometry-evidence.json` records a reproducible comparison between the supplied Illustrator file (which is PDF-compatible) and the existing DXF-derived control map. The source files remain outside this repository; no Beads artwork is embedded or redistributed.

The Illustrator PDF has no extractable text: its labels are flattened outlines. A 600 dpi `pdftocairo` render was used only to detect the red geometric markers. All 24 existing control-map candidates have a corresponding marker. The measured marker centres differ from the DXF/viewBox centres by roughly 1 point (mean 1.0113 pt, maximum 1.0765 pt), which is a source/raster calibration discrepancy to resolve during visual calibration. It does not provide parameter semantics.

The five existing parameter bindings have explicit nearby outlined labels: `DENSITY`, `TIME`, `PITCH`, `SIZE`, and `SHAPE`. Three auxiliary markers are visually adjacent to `FREEZE`, `SEED`, and a duplicate `DENSITY` label, but the flattened artwork cannot establish machine-readable association or DSP semantics. The remaining candidates are deliberately left unlabelled/provisional. No bindings were changed from the control semantics audit.

Before any rights-cleared artwork is used in the editor, calibrate the native view against a straight-on reference and independently verify the knob/pointer treatment. Keep the AI file outside the shipped source until redistribution rights are cleared.
