# Beads source reconnaissance

Captured 2026-10-17 from local repositories; this is planning evidence, not a redistribution of the source or artwork.

- Beads repository: `/Users/danielraffel/Code/eurorack/beads`
- Beads commit: `f4cfa7697095a28d292bff758beb51deb49b5a86`
- stmlib submodule: `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec` (working tree is not initialized locally)
- Illustrator panel: `hardware_design/panel/beads.ai`
  - SHA-256: `bdc4e394703fa99e6e7997683e1b4dee2bada5cc6eff95af36d40086f6f86c5e`
- Panel DWG: `hardware_design/panel/beads.dwg`
  - SHA-256: `82f184f0cfef7f5f09cda6f2aa76aa4642a47304dd54051b6fa2509161c38ed7`
- Converted DXF: `hardware_design/beads.dxf`
  - SHA-256: `caa755bf680939221683d9b3113fa467a4f1cf382780e80b074c32e785639667`
- Straight-on reference image: `/Users/danielraffel/Documents/beads-panel.png`
  - SHA-256: `6ba7ede50ad6baab8d6c96bb41eb51355026ac9a3fd012fc6fe0ff96214664db`

The Beads source files examined during planning carry Emilie Gillet MIT notices. Hardware artwork has a separate rights review requirement; the source manifest must classify every file before packaging. The DXF is mechanical geometry evidence and does not establish the visual knob material, pointer, or highlight. The photograph is the visual reference for those properties. AI conversion must record converter/version and unsupported-feature diagnostics.


- Shared wavetable resource: `resources/waves.bin`
  - SHA-256: `c672fccb061496d0229bfc5c2d55ec61f367e1539dda5870fd667a059c90f634`
  - The source tooling identifies it as BRAIDS_WAVES and it is byte-identical to Braids/Plaits resources; record that lineage and its notice before redistribution.

The AI is PDF 1.5 with a 200.693 x 364.252 pt page. `pdfinfo` reports the embedded title `190305_clouds mk2` and Illustrator CC 23.1 metadata; preserve this as provenance evidence and do not assume the file is Beads-original artwork until the rights review resolves it.
