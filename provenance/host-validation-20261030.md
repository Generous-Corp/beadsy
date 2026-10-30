# Beadsy host validation — 2026-10-30

## Exact candidate

- Beadsy product code source: `266cb12547e8b27e3c8f77bb5e2af2c391352bbf` (later commits update this receipt only)
- Beadsy version: `1.0.7`
- Pulp SDK source: `26a4ae85e09cab39d4eef015123e6f835be7bc8b`
- AU executable SHA-256: `e9c42e29f9a10fede9029bdabdb84379087fcbfffa8a69382edc92a929decf66`
- VST3 executable SHA-256: `b193ca735ace1b67e451407156a07f04f3c93be0926649b4478bf8c1f72f6d53`
- Installed test package SHA-256: `749d1585537e1b5b754f767821722b37f8dfe3bb32cbb9e4a44f45e90f1f4a6f`

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
- The effect-variant DSP/format test passed 137 assertions in 1 case. `auval -v aumf BdFX Inde` passed, including two-channel connection semantics and MIDI.
- The exact Pulp SDK view/widget suite passed 6,462 assertions in 752 cases, including root-routed context-menu selection, outside-click dismissal, Escape dismissal, scrolling, and the shared two-LFO hierarchy.
- A fresh REAPER `editor-open` smoke against the installed `Beadsy FX.vst3` (format-qualified, exact REAPER executable, 60-second cap) reached the FX-shown state but could not measure a floating editor window. Verdict: **INCONCLUSIVE**; cleanup completed and no temporary plugin was left behind. This confirms the correct effect variant was requested but does not close interactive REAPER acceptance.
- Logic exposed no window to the local accessibility or computer-use surface, and no fresh `AUHostingServiceXPC` process appeared. A graceful Logic quit returned `User canceled`; the session was left intact. Logic custom-editor and installed-inode validation remain **open**.
- The installed AU is an `aumu` instrument with no input bus. SOURCE only offers MIDI, and the L/R route fields are disabled without a Pulp host catalog. Real stereo input routing and route selection in external DAWs remain **open**. A selectable label without an audio connection would not meet this gate.
- The stereo-input variant is a separate, same-source product: `Beadsy FX` AU subtype `BdFX`, type `aumf`, and matching `Beadsy FX.vst3`. It is now installed alongside the MIDI instrument. `auval -v aumf BdFX Inde` passes, including two-channel connection semantics and MIDI. The instrument and effect are intentionally distinct format identities; neither is an old copy of the other.
- The headless menu capture shows the panel capped to editor height with scrollable rows. Selection, Escape/outside dismissal, and appearance in Logic/REAPER remain **open** until directly driven in a host.
- The REAPER VST3 editor smoke was inconclusive. Signed/notarized package verification remains **open**; the local package is unsigned.
- The root-owned stale system CLAP bundle (`Beadsy.clap` 1.0.6), old Beadsy package receipts, and prior `/Applications/Beadsy*` standalone/AUv3 bundles were removed through the approved administrator installer wrapper. The only remaining plug-in files are the four current 1.0.7 bundles: Beadsy AU/VST3 and Beadsy FX AU/VST3. The only remaining receipts are `com.independent-audio.beadsy`, `com.independent-audio.beadsy.current`, and `com.independent-audio.beadsy.duo`, all version 1.0.7.
- Current installed executable hashes: `Beadsy.component` AU `e9c42e29f9a10fede9029bdabdb84379087fcbfffa8a69382edc92a929decf66`; `Beadsy.vst3` `b193ca735ace1b67e451407156a07f04f3c93be0926649b4478bf8c1f72f6d53`; `Beadsy FX.component` AU `72316b40df9ae9491554fae5d67269329ae767e8636e8281ca03b03bccaae208`; `Beadsy FX.vst3` `064027eac9e0b69da8779cd83c05b17eaaf22b9316e3970790b9f2cc91ba8252`.
- The dual-variant test package SHA-256 is `a98680c219dec7c44e601ad2e0e7407f21c027fe477a2a24671ef4f9131dcf87`.
