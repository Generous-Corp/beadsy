#include <catch2/catch_test_macros.hpp>
#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>
#include "grainfield.hpp"

TEST_CASE("Grainfield descriptor is an instrument") {
    auto proc = grainfield::create_grainfield();
    const auto desc = proc->descriptor();
    REQUIRE(desc.name == "Grainfield");
    REQUIRE(desc.manufacturer == "Independent Audio");
    REQUIRE(desc.category == pulp::format::PluginCategory::Instrument);
    REQUIRE(desc.accepts_midi);
    REQUIRE(desc.input_buses.empty());
    REQUIRE(desc.output_buses.size() == 1);
}

TEST_CASE("Grainfield produces audio from one global MIDI trigger") {
    auto proc = grainfield::create_grainfield();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);

    pulp::format::PrepareContext prep;
    prep.sample_rate = 48000.0;
    prep.max_buffer_size = 512;
    prep.input_channels = 0;
    prep.output_channels = 2;
    proc->prepare(prep);

    pulp::midi::MidiBuffer midi_in;
    pulp::midi::MidiBuffer midi_out;
    midi_in.add(pulp::midi::MidiEvent::note_on(0, 60, 100));
    pulp::audio::Buffer<float> output(2, 512);
    pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
    auto out_view = output.view();
    pulp::format::ProcessContext context;
    context.sample_rate = 48000.0;
    context.num_samples = 512;
    proc->process(out_view, empty_input, midi_in, midi_out, context);

    bool audible = false;
    for (float sample : output.channel(0))
        audible |= std::isfinite(sample) && std::abs(sample) > 0.0001f;
    REQUIRE(audible);
}

TEST_CASE("Grainfield state round trips") {
    pulp::format::HeadlessHost host(grainfield::create_grainfield);
    host.prepare(48000, 512, 0, 2);
    host.state().set_value(grainfield::kGain, -12.0f);
    const auto data = host.save_state();
    host.state().set_value(grainfield::kGain, 0.0f);
    REQUIRE(host.load_state(data));
    REQUIRE(host.state().get_value(grainfield::kGain) == -12.0f);
}
