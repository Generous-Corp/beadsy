#define BEADSY_EFFECT_VARIANT 1
#include "beadsy.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Beadsy FX is an audio-input companion") {
    auto processor = beadsy::create_beadsy();
    const auto descriptor = processor->descriptor();
    REQUIRE(descriptor.category == pulp::format::PluginCategory::Effect);
    REQUIRE(descriptor.input_buses.size() == 2);
    REQUIRE(descriptor.input_buses[0].name == "Main In");
    REQUIRE(descriptor.input_buses[0].default_channels == 2);
    REQUIRE_FALSE(descriptor.input_buses[0].optional);
    REQUIRE(descriptor.input_buses[1].name == "Sidechain");
    REQUIRE(descriptor.input_buses[1].optional);

    pulp::state::StateStore store;
    processor->set_state_store(&store);
    processor->define_parameters(store);
    REQUIRE(store.get_value(beadsy::kMode) == 1.0f);

    processor->prepare({.sample_rate = 48000.0, .max_buffer_size = 128,
                        .input_channels = 2, .output_channels = 2});
    pulp::audio::Buffer<float> input(2, 128);
    input.channel(0)[0] = 1.0f;
    input.channel(1)[0] = -1.0f;
    pulp::audio::Buffer<float> output(2, 128);
    const auto input_view = static_cast<const pulp::audio::Buffer<float>&>(input).view();
    auto output_view = output.view();
    pulp::midi::MidiBuffer midi_in;
    pulp::midi::MidiBuffer midi_out;
    processor->process(output_view, input_view, midi_in, midi_out,
                       {.sample_rate = 48000.0, .num_samples = 128});
    float energy = 0.0f;
    for (auto sample : output.channel(0)) {
        REQUIRE(std::isfinite(sample));
        energy += std::abs(sample);
    }
    REQUIRE(energy > 0.0f);
}
