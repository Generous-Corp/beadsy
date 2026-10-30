# Beadsy host validation — 2026-10-30

## Exact candidate

- Beadsy source: `44af1ca1eb28e6a284262982793223fe02fe8d0f`
- Beadsy version: `1.0.7`
- Pulp SDK source: `26a4ae85e09cab39d4eef015123e6f835be7bc8b`
- AU executable SHA-256: `e9c42e29f9a10fede9029bdabdb84379087fcbfffa8a69382edc92a929decf66`
- VST3 executable SHA-256: `b193ca735ace1b67e451407156a07f04f3c93be0926649b4478bf8c1f72f6d53`
- Package SHA-256: `8e5871d6f4e7b80153081fd5ad36d216ea3dcbb0c7c8c7babf48159a1aaa55be`

Only these Beadsy locations are installed: `/Library/Audio/Plug-Ins/Components/Beadsy.component` and `/Library/Audio/Plug-Ins/VST3/Beadsy.vst3`. No Beadsy component or VST3 exists under the user Library paths.

## Passing evidence

- `Beadsy-editor-test`: 308 assertions / 34 cases.
- Shared context-menu tests: 206 assertions / 34 cases.
- `auval -v aumu Bead Inde`: PASS, including Test MIDI.
- Fresh editor capture: `panel-final.png`.
- Fresh modulation capture: `menu-final-26a4.png`.

## Logic cache boundary

Logic's existing `AUHostingServiceXPC_arrow` process had loaded an older Beadsy inode. Its loaded inode, size, and SHA did not match the on-disk 1.0.7 candidate. That process later exited; the evidence does not establish what ended it. A Logic visual proof is still required from a fresh host service; `auval`, a direct Cocoa factory probe, and REAPER do not close that gate.

The Logic fallback now writes UI edits through `AudioUnitSetParameter` before notifying the host. This keeps the UI-side synchronized model and the live AU processor state connected.

## Current acceptance check

- Recomputed installed AU SHA-256: `e9c42e29f9a10fede9029bdabdb84379087fcbfffa8a69382edc92a929decf66`; inode `926683367`, size 23,457,080 bytes. Installed VST3 SHA-256: `b193ca735ace1b67e451407156a07f04f3c93be0926649b4478bf8c1f72f6d53`.
- The latest editor test passed 308 assertions in 34 cases. `auval -v aumu Bead Inde` passed, including MIDI and parameter setting.
- Logic exposed no window to the local accessibility or computer-use surface, and no fresh `AUHostingServiceXPC` process appeared. A graceful Logic quit returned `User canceled`; the session was left intact. Logic custom-editor and installed-inode validation remain **open**.
- The installed AU is an `aumu` instrument with no input bus. SOURCE only offers MIDI, and the L/R route fields are disabled without a Pulp host catalog. Real stereo input routing and route selection in external DAWs remain **open**. A selectable label without an audio connection would not meet this gate.
- The headless menu capture shows the panel capped to editor height with scrollable rows. Selection, Escape/outside dismissal, and appearance in Logic/REAPER remain **open** until directly driven in a host.
- The REAPER VST3 editor smoke was inconclusive. Signed/notarized package verification remains **open**; the local package is unsigned.
- A stale system CLAP bundle remains at `/Library/Audio/Plug-Ins/CLAP/Beadsy.clap`, version 1.0.6; it is root-owned and ordinary removal was denied. It is not part of the current package and still needs the authorized administrator removal path.
