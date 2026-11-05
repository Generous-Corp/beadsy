#include "beadsy.hpp"
#include <pulp/format/vst3_entry.hpp>

#if defined(BEADSY_LAB_VARIANT) && defined(BEADSY_EFFECT_VARIANT)
PULP_VST3_PLUGIN(
    Steinberg::FUID(0x7A41C2E6, 0x9D3B4F08, 0xB15E6C27, 0x3F8A9D41),
    "Beadsy Lab FX",
    Steinberg::Vst::PlugType::kFx,
    "Independent Audio",
    "1.0.0",
    "",
    beadsy::create_beadsy
)
#elif defined(BEADSY_LAB_VARIANT)
PULP_VST3_PLUGIN(
    Steinberg::FUID(0x2C9E7B13, 0x6F0A4D52, 0x8E3B1A7C, 0xD47F26B9),
    "Beadsy Lab",
    Steinberg::Vst::PlugType::kInstrumentSynth,
    "Independent Audio",
    "1.0.0",
    "",
    beadsy::create_beadsy
)
#elif defined(BEADSY_EFFECT_VARIANT)
PULP_VST3_PLUGIN(
    Steinberg::FUID(0xB3D5F17A, 0x2C8E4D91, 0xA6E073B4, 0x9F1C52D8),
    "Beadsy FX",
    Steinberg::Vst::PlugType::kFx,
    "Independent Audio",
    "1.0.0",
    "",
    beadsy::create_beadsy
)
#else
PULP_VST3_PLUGIN(
    Steinberg::FUID(0xF59F853D, 0x51CDFD80, 0xF60C2686, 0x1D395D3C),
    "Beadsy",
    Steinberg::Vst::PlugType::kInstrumentSynth,
    "Independent Audio",
    "1.0.0",
    "",
    beadsy::create_beadsy
)
#endif
