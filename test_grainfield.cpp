#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include <pulp/format/headless.hpp>
#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>
#include "grainfield.hpp"
#include "stmlib/utils/random.h"

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

TEST_CASE("Grainfield LFO modulation leaves base automation intact and persists") {
    grainfield::Grainfield first;
    pulp::state::StateStore first_store;
    first.set_state_store(&first_store);
    first.define_parameters(first_store);
    first_store.set_value(grainfield::kDensity, 0.25f);
    first.set_lfo(grainfield::kDensity,
                             {.enabled = true, .rate_hz = 2.0f, .depth = 0.1f, .phase = 0.0f});
    const auto data = first.serialize_plugin_state();
    REQUIRE(first_store.get_value(grainfield::kDensity) == 0.25f);
    grainfield::Grainfield restored_processor;
    pulp::state::StateStore restored_store;
    restored_processor.set_state_store(&restored_store);
    restored_processor.define_parameters(restored_store);
    REQUIRE(restored_processor.deserialize_plugin_state(data));
    REQUIRE(restored_store.deserialize(first_store.serialize()));
    REQUIRE(restored_store.get_value(grainfield::kDensity) == 0.25f);
    const auto restored = restored_processor.lfo(grainfield::kDensity);
    REQUIRE(restored.enabled);
    REQUIRE(restored.depth == 0.1f);
}


namespace {

struct RenderedBlocks {
    std::vector<float> left;
    std::vector<float> right;
};

RenderedBlocks render_seeded_note() {
    stmlib::Random::Seed(0x12345678u);
    auto proc = grainfield::create_grainfield();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);

    pulp::format::PrepareContext prep;
    prep.sample_rate = 48000.0;
    prep.max_buffer_size = 256;
    prep.input_channels = 0;
    prep.output_channels = 2;
    proc->prepare(prep);

    pulp::midi::MidiBuffer midi_in;
    pulp::midi::MidiBuffer midi_out;
    midi_in.add(pulp::midi::MidiEvent::note_on(0, 60, 100));
    pulp::audio::Buffer<float> output(2, 256);
    pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
    pulp::format::ProcessContext context;
    context.sample_rate = 48000.0;
    context.num_samples = 256;

    RenderedBlocks rendered;
    for (int block = 0; block < 2; ++block) {
        output.clear();
        auto out_view = output.view();
        proc->process(out_view, empty_input, midi_in, midi_out, context);
        midi_in.clear();
        rendered.left.insert(rendered.left.end(), output.channel(0).begin(), output.channel(0).end());
        rendered.right.insert(rendered.right.end(), output.channel(1).begin(), output.channel(1).end());
    }
    return rendered;
}

} // namespace

TEST_CASE("Grainfield seeded renders are deterministic") {
    const auto first = render_seeded_note();
    const auto second = render_seeded_note();
    REQUIRE(first.left == second.left);
    REQUIRE(first.right == second.right);
    REQUIRE(std::any_of(first.left.begin(), first.left.end(), [](float sample) {
        return std::abs(sample) > 0.0001f;
    }));
}

TEST_CASE("Grainfield 48 kHz contract is finite and rejects other rates") {
    auto render_at = [](double sample_rate) {
        auto proc = grainfield::create_grainfield();
        pulp::state::StateStore store;
        proc->set_state_store(&store);
        proc->define_parameters(store);

        pulp::format::PrepareContext prep;
        prep.sample_rate = sample_rate;
        prep.max_buffer_size = 128;
        prep.input_channels = 0;
        prep.output_channels = 2;
        proc->prepare(prep);

        pulp::midi::MidiBuffer midi_in;
        pulp::midi::MidiBuffer midi_out;
        midi_in.add(pulp::midi::MidiEvent::note_on(0, 60, 100));
        pulp::audio::Buffer<float> output(2, 128);
        pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
        auto out_view = output.view();
        pulp::format::ProcessContext context;
        context.sample_rate = sample_rate;
        context.num_samples = 128;
        proc->process(out_view, empty_input, midi_in, midi_out, context);
        return output;
    };

    const auto supported = render_at(48000.0);
    for (std::size_t channel = 0; channel < supported.num_channels(); ++channel)
        for (float sample : supported.channel(channel)) REQUIRE(std::isfinite(sample));

    const auto unsupported = render_at(44100.0);
    for (std::size_t channel = 0; channel < unsupported.num_channels(); ++channel)
        for (float sample : unsupported.channel(channel)) REQUIRE(sample == 0.0f);
}
