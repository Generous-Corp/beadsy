#include "beadsy.hpp"
#include <pulp/format/vst3_entry.hpp>

// Class IDs are the shipping identities, so sessions saved under the
// earlier name keep loading; only the display names changed.
#if defined(BEADSY_EFFECT_VARIANT)
PULP_VST3_PLUGIN(
    Steinberg::FUID(0xB3D5F17A, 0x2C8E4D91, 0xA6E073B4, 0x9F1C52D8),
    "Meru FX",
    Steinberg::Vst::PlugType::kFx,
    "Independent Audio",
    "1.0.0",
    "",
    beadsy::create_beadsy
)
#else
PULP_VST3_PLUGIN(
    Steinberg::FUID(0xF59F853D, 0x51CDFD80, 0xF60C2686, 0x1D395D3C),
    "Meru",
    Steinberg::Vst::PlugType::kInstrumentSynth,
    "Independent Audio",
    "1.0.0",
    "",
    beadsy::create_beadsy
)
#endif
