# Beadsy delivery plan

This is the actionable plan for the independently branded, MIT-licensed Beadsy
instrument. The plan keeps the original Beads behavior as the compatibility
reference while treating host routing, artwork provenance, and format packaging
as separate acceptance gates.

## Phase 1 — shared Pulp capability

Land the host-owned `AudioRouteCatalog` and bus-role API in Pulp. The catalog
provides stable entries for physical inputs, project tracks, instruments, buses,
files, and internal sources. It is populated and changed on the control thread;
the audio callback consumes already-routed buffers. A plug-in cannot enumerate
arbitrary Logic or REAPER tracks by itself, so real DAW menus remain a host
acceptance gate. The route branch is currently at
`00c8c0a5f4cef8d67845d06476a9c99b28ca97b3`; its focused route and processor
provider tests pass. The reusable format seam is now an allocation-free,
pre-bound `AudioInputBusProvider`: `ProcessorNode` materializes declared
auxiliary/sidechain buses from host-borrowed buffers and the rich
`Processor::process(ProcessBuffers&, ...)` path receives every bus while legacy
processors remain source-compatible. The current branch has passed the focused
format suites and manifest checks; the overflow host refused the local
configure/source-selftest lanes, so those remain CI evidence. This phase is
still **partial**: the catalog has stable route entries, refresh behavior, bus
roles, selection snapshot/restore, and a provider seam, but no Beadsy
standalone/Forge host yet binds selected stable IDs to actual graph buffers.
That binding and unavailable/hot-plug behavior are required before this is an
end-to-end audio routing feature.

## Phase 2 — Beadsy behavior and UI

- Keep Beadsy as a MIDI instrument with optional host-routed stereo input for
  live granular processing, plus the companion effect for ordinary insert use.
- Preserve sample-offset MIDI retriggers, seed modes, freeze behavior, and the
  direct DSP parity fixtures before changing audio semantics.
- Keep the faceplate straight-on and procedural until artwork rights are
  cleared; retain the measured knob geometry and remove decorative output jacks.
- Keep resizing, automation gestures, keyboard/context-menu behavior, and LFO
  controls aligned with the shared Pulp/Spectr interaction contract.
- Add a visible source selector whose choices come from the host route catalog;
  the catalog may contain physical interface inputs, project tracks, buses,
  instruments, files, and internal sources. The selector must show the
  host-provided display name and availability, keep a stable selection across
  catalog refreshes, and call the host binding callback on selection.
  selecting a project track, bus, instrument, file, or physical input must
  change the host connection rather than synthesize a private track browser.
- Install the provider before `ProcessorNode::prepare()` and update its
  host-owned snapshot atomically when the selected route or catalog refreshes;
  the audio callback must never enumerate routes, allocate, lock, or resolve
  stable IDs.
- Keep the picker visibly disabled when no host catalog is injected; do not
  imply that an AU/VST3/CLAP/LV2 plug-in can enumerate the DAW project itself.

## Phase 3 — evidence

- Run the full Release build and tests, then the Beadsy editor, state, route,
  and audio-harness suites.
- Maintain byte-identical direct DSP fixtures and extend parity through the
  Processor and adapter paths at supported sample rates and block sizes.
- Capture independent AU, VST3, CLAP, and LV2 host receipts. A real Logic or
  REAPER project must prove that the host-populated source menu selects a track,
  bus, instrument, and physical input where the host supports each one.
- Add a route identity/state receipt proving selected stable IDs survive a state
  round-trip, catalog refresh, unavailable source, and editor reopen.
- Keep the route-enabled Beadsy editor build against the route-capability SDK
  as a required check; the current receipt is 158 assertions in 15 cases.
- Keep allocation, realtime, MIDI expressive, and state-restore probes as
  explicit receipts rather than inferring them from a successful scan.

## Phase 4 — provenance and release

- Validate only rights-cleared SVG/control-map inputs through the landed static
  importer. Do not redistribute the source AI, DWG, CAD, panel photo, or
  upstream artwork without a file-level license decision.
- Re-audit MIT attribution, non-affiliation language, third-party notices,
  package contents, product-name collision results, and trademark handling.
- Close standalone/LV2 packaging, AU installation discovery, signing,
  notarization, and AAX SDK/Pro Tools evidence independently. AAX stays planned
  until its proprietary SDK and host validator are available.

## Closeout review

Ultra/adversarial review is the final gate. It must inspect the exact landed
heads, current receipts, host-route behavior, DSP null/parity results, straight-on
visual comparison, and package provenance. The Beadsy goal remains active until
every required gate is `pass` or an explicitly recorded, intentional scope
decision exists.
