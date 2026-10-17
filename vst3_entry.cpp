#include "grainfield.hpp"
#include <pulp/format/vst3_entry.hpp>

PULP_VST3_PLUGIN(
    Steinberg::FUID(0xF59F853D, 0x51CDFD80, 0xF60C2686, 0x1D395D3C),
    "Grainfield",
    Steinberg::Vst::PlugType::kInstrumentSynth,
    "Independent Audio",
    "1.0.0",
    "",
    grainfield::create_grainfield
)
