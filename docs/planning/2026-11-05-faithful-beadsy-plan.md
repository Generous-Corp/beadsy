# Faithful Beadsy: plan of record (2026-11-05)

Goal: one Beads in software. Behaviour follows the Mutable Instruments Beads
manual and firmware (local references: the documentation tree under
`mutable-instruments-documentation/docs/modules/beads` and the firmware under
`eurorack/beads`), while the few things a DAW cannot do the hardware's way
(sense a patch cable, put a VCA after the module) get explicit, visible
software stand-ins. Respect for the original is a design constraint, not a
footnote: no claim of affiliation, every adapted file keeps its notice, and the
in-plugin help acknowledges the authors.

## What the hardware is (manual, in order)

1. A stereo granular processor over a continuously recorded buffer; the panel,
   quality modes, Freeze and the delay mode all assume audio in.
2. With both inputs unpatched for ten seconds it granulates eight internal
   wavetable banks (FEEDBACK = bank, DRY/WET = raw oscillator vs grains).
3. No notes: V/oct into PITCH, a gate into SEED. Grain modes: latched
   (continuous at DENSITY; SEED hold ≥ long press, `ui.cc:393-398`), gated
   (grains only while SEED/gate is high), clocked (SEED input is a clock,
   DENSITY is divider/probability).
4. Everything a rack adds after the module (VCA, envelope) is outside it.

## Product shape

- Two host entries of one engine, because DAWs need both doors:
  - Instrument entry = "nothing patched" by default. Note Gate on.
  - Effect entry (MIDI-controlled effect in Logic, plain effect elsewhere) =
    "audio patched" by default. Note Gate off.
- A visible Source switch (Internal synth / Main In / Side Chain) replaces the
  patch cable. The host routes audio; the plug-in only chooses whether to use
  it. Auto (synth after 10 s of silence) stays optional, never default.
- The Lab builds are the proving ground; once accepted they become the
  shipping bundles (1.1) under the existing names and identities.
- Instrument entries receive audio only where the host and format allow it;
  the AU instrument needs the Pulp SDK change tracked in
  `pulp-planning/2026-11-05-pulp-instrument-audio-inputs.md`.

## UX rules for the reclaimed band under the divider

The band is the grip's height and stays that height. Left: a "?" button
opening the help/acknowledgement panel. Centre: the Source chip, a tappable
pill that reads "SOURCE: MAIN IN" with a disclosure mark; a left click opens
the same menu a right click does. Right: the resize grip. Nothing else is
added to the faceplate; everything beyond the hardware's controls lives in
menus reachable from that band.

## Slices (each lands with tests, a capture, and a host check)

1. Band UX + help panel. Landed in 1.0.28 (`beadsy_help.hpp`): "?" at the
   divider's left edge opens the scrolling acknowledgement + help panel (a
   framework overlay, so Escape and outside-click close it); the Source chip
   in the band's centre names the live source and opens the Source menu on a
   primary press. Proven in the Lab standalone (GPU frames) and in REAPER.
2. Seed button fidelity, per `ui.cc`: a press sets `seed_button_state_ =
   true` for as long as it is held (and clears `seed_button_locked_`); a
   release after `long_press_duration` sets `locked`; a short release just
   drops the state. SEED+FREEZE together also lock. So in software: mouse
   down = momentary ON while held, hold beyond the long-press time = latch
   on release, and a press while latched unlatches. MIDI gate stays the SEED
   input. DENSITY semantics per state need no verification: the engine runs
   the firmware's own `TriggerGenerator` (the adapted copy differs only by a
   span in place of a VLA and by keeping the MIDI adapter's edge flags).
   Implemented in `SeedModeControl` (press/release with held time, injected
   clock for tests), FREEZE-while-held latch in the Freeze press handler,
   and the latched face breathing on the firmware's 2048 ms triangle.
3. Attenurandomizers: inherited. The four amounts are passed as the centred
   attenuverter value into the firmware's own `RandomizedParameter`
   (`beadsy.hpp` `randomized()`), so the peaky/uniform law is the module's.
   The hardware's CV path has no equivalent; the modulation menu's LFOs are
   the software's CV.
4. Delay mode: SIZE fully clockwise; DENSITY base time, SEED taps (MIDI notes)
   and host clock as the external clock, TIME multiplier, SHAPE envelope,
   PITCH shifter, FREEZE slicing (`dsp/delay_controller.h`,
   `dsp/delay_clock.*`). Inherited: the effect path runs the firmware's own
   `beads::Processor` (`effect_` in `beadsy.hpp`), and the SEED gate flags it
   receives carry MIDI-note taps, so tap tempo and the external clock work as
   on the module. What remains is a render test that pins the delay regime
   at SIZE fully clockwise against the firmware's thresholds.
5. Quality/media modes with resampling so the engine runs at any host rate
   (`Synth::Resolution` and `Processor` media paths); then auto input gain and
   limiter as an option; Freeze-buffer persistence if the port exposes it.
6. Fold Lab into the shipping bundles as 1.1; retire the Lab identities.

## Release and openness

- Repository: github.com/Generous-Corp/beadsy (private until the open-source
  pass). Releases carry the signed, notarized installer and its SHA-256.
- Before open-sourcing: this document stays; provenance receipts, notices and
  the help text are the public face of the attribution.
