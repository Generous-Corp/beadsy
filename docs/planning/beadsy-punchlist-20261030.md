# Beadsy punch list — 2026-10-30

This is the acceptance list for the Beadsy instrument and Beadsy FX companion.
Each item remains open until it has both a focused test and fresh host or
capture evidence from the exact installed build.

## Controls and artwork

- [ ] Move the black button above the three source icons left so its center is
  vertically aligned with the dashed guide below it.
- [ ] Make that station a single colored circular button. Remove the separate
  LED and cycle the whole button face through the destination colors.
- [ ] Apply the same single-button/state-color treatment to the center quality
  station; remove the separate quality LED.
- [ ] Keep the four small CV controls interactive, centered over their symbols,
  and free of construction circles or clipped icons.
- [ ] Align `IN R` over the right route field and verify the lower border is
  continuous across the panel.
- [ ] Make the AUv2 resize grip draggable in Logic and verify proportional
  resizing at standard, narrow, and wide sizes.

## Phased completion plan

Work proceeds in this order. Later phases do not count as complete while an
earlier phase lacks direct host evidence.

### Phase 1 — prove LFO animation and modulation

- [ ] Attach LFO 1 and LFO 2 to a large knob, small knob, and button/toggle.
- [ ] Verify visible movement/state changes in a fresh REAPER editor instance.
- [ ] Verify the rendered parameter/audio value changes over time, not only the
  menu state.
- [ ] Repeat the focused check in AUv2 after Logic loads the exact installed
  component. Keep polyphony out of this phase.

### Phase 2 — make source and bus routing clear and functional

- [ ] Make `SOURCE` visibly switch between MIDI, `MAIN IN`, and `SIDECHAIN`.
- [ ] Expose and validate stereo main-input and sidechain buses in AUv2, VST3,
  and CLAP-capable hosts.
- [ ] Decide and document the L/R control contract: use them only when a host
  supplies a real route catalog; otherwise remove or clearly disable them and
  direct track selection to the host routing UI.
- [ ] Prove external audio enters, is granularly processed, and exits with
  numeric evidence in each supported format.

### Phase 3 — add concise contextual help

- [ ] Add a bottom-right `?` button after source and modulation behavior are
  stable.
- [ ] Use the Spectr-style compact scrollable popup with stable sizing,
  dismissal, and readable text.
- [ ] Ground the text in the Beads manual at
  `/Users/danielraffel/Code/mutable-instruments-documentation/docs/modules/beads`
  and explain Beadsy-specific source/bus behavior succinctly.

### Phase 4 — decide polyphony

- [ ] Revisit polyphonic MIDI only after the external-audio effect path and
  host routing are accepted. Treat it as a separate design decision.

## Source selection and real-time audio

- [ ] `Source` must be a real selectable dropdown with an affordance, popup
  dismissal, and persistent selection.
- [ ] `L IN` and `R IN` remain only if a host-owned route catalog can populate
  and connect them; otherwise remove them from the visible UI rather than
  presenting disabled controls that imply track selection.
- [ ] Beadsy FX must expose host-routed stereo input and process external audio
  in real time in Main In and Sidechain modes.
- [ ] Prove left/right route selection, including the same route on both
  channels, with a host-owned catalog and numeric DSP evidence.
- [ ] Keep Beadsy (`aumu`, MIDI instrument) and Beadsy FX (`aumf`, audio-input
  effect) claims separate in every host report.

## Shared Spectr-style modulation menu

- [ ] Use a highlight color that does not collide with switch thumbs or slider
  tracks; preserve readable light-on-dark contrast.
- [ ] Size the menu from a stable maximum width so edit-lane changes do not
  resize the popup. Prevent toggle text overlap and slider-thumb truncation.
- [ ] Expose a hand cursor over toggles and sliders through the shared Pulp core
  widget behavior.
- [ ] Make pointer taps on a slider jump the thumb to the tapped position.
- [ ] Present LFO 1/LFO 2 edit selection as stable stacked tabs or equivalent
  controls; changing lanes must not change popup width.
- [ ] Render waveform icons with selected-state styling.
- [ ] Preserve two independent LFO toggles, progressive target disclosure,
  target assignment, rate/depth values, keyboard navigation, Escape/outside
  dismissal, and one-popup-at-a-time behavior.

## Verification gates

- [ ] Run the complete editor/DSP/FX suite after each semantic slice.
- [ ] Capture standard, narrow, and wide editor frames from the exact installed
  binary and compare against the authored Beads source artwork.
- [ ] Verify current AU/VST3 executable inode, SHA, source SHA, and host/helper
  PID before every Logic or REAPER claim.
- [ ] Freshly load Beadsy in Logic and REAPER and perform direct pointer checks
  for buttons, dropdowns, small knobs, modulation sliders, and resize.
- [ ] Keep PR #11248 and the shared SDK capability dependency visible in the
  final handoff; do not claim completion while the required core API is only a
  local temporary SDK artifact.

## Current evidence

The latest exact-head local evidence is under `provenance/` in this worktree.
The current installed package is a test artifact, not a signed release.

## 2026-10-30 implementation checkpoint

- Implemented in the current source head: CV assign and quality are single
  stateful circular buttons with full-face destination/quality colors; the
  assign button is aligned to the authored dashed guide.
- Implemented in the shared SDK source head: modulation switches and slider
  thumbs use contrast-safe colors, the popup reserves a stable minimum width,
  slider/choice rows advertise the pointer cursor, and Shape choices render
  waveform glyphs.
- Focused Beadsy editor, DSP, and FX tests pass (329, 2,821, and 137
  assertions respectively). Both installed AU variants pass `auval`.
- Installed exact-head package: `Beadsy-1.0.7.pkg` (SHA-256
  `9bbeb75c252b39f5bbe5ef21b741220f3d8adbbdb763a256860f6caffba23fd9`).
- Still open until fresh direct host evidence: route popup interaction in Logic
  or REAPER, visible modulation-menu pointer/slider behavior, Logic resize,
  exact artwork alignment captures, and numeric external-audio-through proof.
