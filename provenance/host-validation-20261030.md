# Beadsy host validation — 2026-10-30

## Exact candidate

- Beadsy source: `44af1ca1eb28e6a284262982793223fe02fe8d0f`
- Beadsy version: `1.0.7`
- Pulp SDK source: `26a4ae85e09cab39d4eef015123e6f835be7bc8b`
- AU executable SHA-256: `27bbe0ca5f914d7421d66a83b892594b327bbfde7d329e86be91427ec4e49cbe`
- VST3 executable SHA-256: `f55c19713bb88a170af81bf0915a788efba47be5b4509d7879c41ec87c9dd97e`
- Package SHA-256: `8e5871d6f4e7b80153081fd5ad36d216ea3dcbb0c7c8c7babf48159a1aaa55be`

Only these Beadsy locations are installed: `/Library/Audio/Plug-Ins/Components/Beadsy.component` and `/Library/Audio/Plug-Ins/VST3/Beadsy.vst3`. No Beadsy component or VST3 exists under the user Library paths.

## Passing evidence

- `Beadsy-editor-test`: 286 assertions / 32 cases.
- Shared context-menu tests: 206 assertions / 34 cases.
- `auval -v aumu Bead Inde`: PASS, including Test MIDI.
- Fresh editor capture: `panel-final.png`.
- Fresh modulation capture: `menu-final-26a4.png`.

## Logic cache boundary

Logic's existing `AUHostingServiceXPC_arrow` process had loaded an older Beadsy inode. Its loaded inode, size, and SHA did not match the on-disk 1.0.7 candidate. That process later exited; the evidence does not establish what ended it. A Logic visual proof is still required from a fresh host service; `auval`, a direct Cocoa factory probe, and REAPER do not close that gate.

The Logic fallback now writes UI edits through `AudioUnitSetParameter` before notifying the host. This keeps the UI-side synchronized model and the live AU processor state connected.

## Current acceptance check

- Recomputed installed AU SHA-256: `27bbe0ca5f914d7421d66a83b892594b327bbfde7d329e86be91427ec4e49cbe`; inode `926522278`, size 23,457,080 bytes. Installed VST3 SHA-256: `f55c19713bb88a170af81bf0915a788efba47be5b4509d7879c41ec87c9dd97e`.
- The latest editor test passed 308 assertions in 34 cases. `auval -v aumu Bead Inde` passed, including MIDI and parameter setting.
- Logic exposed no window to the local accessibility or computer-use surface, and no fresh `AUHostingServiceXPC` process appeared. A graceful Logic quit returned `User canceled`; the session was left intact. Logic custom-editor and installed-inode validation remain **open**.
- The installed AU is an `aumu` instrument with no input bus. SOURCE only offers MIDI, and the L/R route fields are disabled without a Pulp host catalog. Real stereo input routing and route selection in external DAWs remain **open**. A selectable label without an audio connection would not meet this gate.
- The headless menu capture shows the panel capped to editor height with scrollable rows. Selection, Escape/outside dismissal, and appearance in Logic/REAPER remain **open** until directly driven in a host.
- The REAPER VST3 editor smoke was inconclusive. Signed/notarized package verification remains **open**; the local package is unsigned.
