# Attribution and non-affiliation

Beadsy is an independent software instrument inspired by the Mutable Instruments Beads architecture and hardware workflow. We offer our sincere thanks to Emilie Gillet for the original work and for making the relevant source available under its stated licenses. Mutable Instruments and Emilie Gillet are not affiliated with or endorsing this project. Any redistributed upstream software or artwork will retain its original author, license, and attribution requirements.

Final attribution text and links remain a release prerequisite before any artwork or adapted DSP is distributed. The manifest uses `pending` states; this file does not imply legal clearance.

## Local evidence

The current notice map records the exact Beads DSP origin, reachable `stmlib` commit, and `waves.bin` lineage in `provenance/source-manifest.json`. Artwork permission, CC-BY-SA attribution/share-alike terms, and Mutable Instruments trademark clearance remain blocked.

## Upstream review basis (not bundled)

The upstream Beads repository identifies STM32F code as MIT, AVR code as GPL-3.0,
and hardware as CC BY-SA 3.0. It also asks derivative works to avoid the
Mutable Instruments name and the original module name:

<https://github.com/pichenettes/eurorack/blob/f4cfa7697095a28d292bff758beb51deb49b5a86/README.md>

The Beads documentation and product photographs were reviewed from the pinned
documentation tree, but no file-level permission to redistribute them was
found:

<https://github.com/pichenettes/mutable-instruments-documentation/tree/14d04a75ad54cd97e5e5f7b8f5a2ec7bb23046d5/docs/modules/beads>

## Public-release checklist (2026-11-06)

Decision on record (2026-11-07, owner): publish. The secrets audit is clean;
the panel artwork is distributed as CC BY-SA 3.0 outside the MIT grant (see
LICENSES.md) with attribution in the help panel and here; the product name
and the nominative mentions of Beads and Mutable Instruments stand with the
non-affiliation line. Items still marked open below are tracked, not blocking.

- Done: a full-history audit found no keys, tokens, certificates or secret
  files (`git ls-files`, token patterns over `git log --all -p`); the release
  script names environment variables only and reads them from
  `~/.config/pulp/secrets/` outside the repository.
- Done: the in-plugin help panel ends with "Sources, licenses and thanks":
  the Beads firmware and stmlib (MIT) with the repository, the pinned commits
  and the README that states the terms, the manual, the Pulp SDK, the
  artwork, the non-affiliation line and the thanks to Emilie Gillet.
- Open: written permission for the straight-on panel artwork, and the
  CC BY-SA 3.0 share-alike terms if that artwork derives from the hardware
  files.
- Open: the upstream README asks derivative works to avoid the Mutable
  Instruments name and the module name; the product name and the help text
  (which names both, nominatively) need an owner decision.
- When those are confirmed, flipping visibility is one API call; nothing in
  the history needs rewriting (the two signed installers committed under
  `provenance/` are large but not sensitive).
