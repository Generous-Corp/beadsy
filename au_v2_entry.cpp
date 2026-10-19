#include "beadsy.hpp"
#if defined(BEADSY_EFFECT_VARIANT)
#include <pulp/format/au_v2_entry.hpp>
PULP_AU_MIDI_PLUGIN(BeadsyFXAU, beadsy::create_beadsy)
#else
#include <pulp/format/au_v2_instrument_entry.hpp>
PULP_AU_INSTRUMENT(BeadsyAU, beadsy::create_beadsy)
#endif
