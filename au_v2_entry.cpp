#include "beadsy.hpp"
// The factory symbols are the shipping identities; the Lab configuration is
// the product now and does not get its own.
#if defined(BEADSY_EFFECT_VARIANT)
#include <pulp/format/au_v2_entry.hpp>
PULP_AU_MIDI_PLUGIN(BeadsyFXAU, beadsy::create_beadsy)
#else
#include <pulp/format/au_v2_instrument_entry.hpp>
PULP_AU_INSTRUMENT(BeadsyAU, beadsy::create_beadsy)
#endif
