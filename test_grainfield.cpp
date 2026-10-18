#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <pulp/format/headless.hpp>
#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>
#include "grainfield.hpp"
#include "stmlib/utils/random.h"
#if defined(GRAINFIELD_HAS_RT_ALLOCATION_PROBE)
#include "rt_allocation_probe.hpp"
#endif

TEST_CASE("Grainfield descriptor is an instrument") {
    auto proc = grainfield::create_grainfield();
    const auto desc = proc->descriptor();
    REQUIRE(desc.name == "Grainfield");
    REQUIRE(desc.manufacturer == "Independent Audio");
    REQUIRE(desc.category == pulp::format::PluginCategory::Instrument);
    REQUIRE(desc.accepts_midi);
    REQUIRE(desc.input_buses.size() == 1);
    REQUIRE(desc.input_buses[0].default_channels == 2);
    REQUIRE(desc.output_buses.size() == 1);
}

#if defined(GRAINFIELD_HAS_RT_ALLOCATION_PROBE)
TEST_CASE("Grainfield prepared processing allocates no realtime memory",
          "[realtime][allocation]") {
    auto proc = grainfield::create_grainfield();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);
    pulp::format::PrepareContext prep{.sample_rate = 48000.0,
                                      .max_buffer_size = 256,
                                      .input_channels = 0,
                                      .output_channels = 2};
    proc->prepare(prep);

    pulp::midi::MidiBuffer input;
    pulp::midi::MidiBuffer output_midi;
    input.add(pulp::midi::MidiEvent::note_on(0, 60, 100));
    pulp::audio::Buffer<float> output(2, 256);
    auto view = output.view();
    pulp::audio::BufferView<const float> empty(nullptr, 0, 0);
    pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                         .num_samples = 256};

    // Warm up state transitions before observing the steady-state callback.
    proc->process(view, empty, input, output_midi, context);

    pulp::test::RtAllocationProbe probe;
    for (int block = 0; block < 32; ++block)
        proc->process(view, empty, input, output_midi, context);

    REQUIRE_FALSE(probe.saw_allocation());
}
#endif

TEST_CASE("Grainfield exposes Beads semitone pitch range and maps MIDI note offsets") {
    auto render_note = [](std::uint8_t note) {
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
        pulp::midi::MidiBuffer input;
        pulp::midi::MidiBuffer output_midi;
        input.add(pulp::midi::MidiEvent::note_on(0, note, 100));
        pulp::audio::Buffer<float> output(2, 512);
        auto view = output.view();
        pulp::audio::BufferView<const float> empty(nullptr, 0, 0);
        pulp::format::ProcessContext context;
        context.sample_rate = 48000.0;
        context.num_samples = 512;
        proc->process(view, empty, input, output_midi, context);
        float energy = 0.0f;
        for (float sample : output.channel(0)) energy += std::abs(sample);
        return energy;
    };

    auto proc = grainfield::create_grainfield();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);
    REQUIRE(store.info(grainfield::kPitch)->range.min == -24.0f);
    REQUIRE(store.info(grainfield::kPitch)->range.max == 24.0f);
    REQUIRE(render_note(60) != render_note(72));
}

TEST_CASE("Grainfield maps MIDI velocity to deterministic output gain") {
    auto render_energy = [](std::uint8_t velocity) {
        stmlib::Random::Seed(0x13579bdu);
        auto proc = grainfield::create_grainfield();
        pulp::state::StateStore store;
        proc->set_state_store(&store);
        proc->define_parameters(store);
        pulp::format::PrepareContext prep{.sample_rate = 48000.0,
                                          .max_buffer_size = 256,
                                          .input_channels = 0,
                                          .output_channels = 2};
        proc->prepare(prep);
        pulp::midi::MidiBuffer input;
        pulp::midi::MidiBuffer output_midi;
        input.add(pulp::midi::MidiEvent::note_on(0, 60, velocity));
        pulp::audio::Buffer<float> output(2, 256);
        auto view = output.view();
        pulp::audio::BufferView<const float> empty(nullptr, 0, 0);
        pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                             .num_samples = 256};
        proc->process(view, empty, input, output_midi, context);
        float energy = 0.0f;
        for (float sample : output.channel(0)) energy += std::abs(sample);
        return energy;
    };
    const float full = render_energy(127);
    const float half = render_energy(64);
    REQUIRE(full > 0.0f);
    REQUIRE(half > 0.0f);
    REQUIRE(half < full);
}

TEST_CASE("Grainfield maps pitch bend to the Beads pitch offset") {
    auto render_energy = [](std::uint16_t bend) {
        stmlib::Random::Seed(0x2468aceu);
        auto proc = grainfield::create_grainfield();
        pulp::state::StateStore store;
        proc->set_state_store(&store);
        proc->define_parameters(store);
        pulp::format::PrepareContext prep{.sample_rate = 48000.0,
                                          .max_buffer_size = 256,
                                          .input_channels = 0,
                                          .output_channels = 2};
        proc->prepare(prep);
        pulp::midi::MidiBuffer input;
        pulp::midi::MidiBuffer output_midi;
        input.add(pulp::midi::MidiEvent::pitch_bend(0, bend));
        input.add(pulp::midi::MidiEvent::note_on(0, 60, 127));
        pulp::audio::Buffer<float> output(2, 256);
        auto view = output.view();
        pulp::audio::BufferView<const float> empty(nullptr, 0, 0);
        pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                             .num_samples = 256};
        proc->process(view, empty, input, output_midi, context);
        float energy = 0.0f;
        for (float sample : output.channel(0)) energy += std::abs(sample);
        return energy;
    };
    const float center = render_energy(8192);
    const float up = render_energy(16383);
    REQUIRE(center > 0.0f);
    REQUIRE(up > 0.0f);
    REQUIRE(center != up);
}

TEST_CASE("Grainfield sustain and overlap handling is deterministic") {
    auto render_sequence = [] {
        stmlib::Random::Seed(0x1020304u);
        auto proc = grainfield::create_grainfield();
        pulp::state::StateStore store;
        proc->set_state_store(&store);
        proc->define_parameters(store);
        pulp::format::PrepareContext prep{.sample_rate = 48000.0,
                                          .max_buffer_size = 256,
                                          .input_channels = 0,
                                          .output_channels = 2};
        proc->prepare(prep);
        pulp::midi::MidiBuffer input;
        pulp::midi::MidiBuffer output_midi;
        auto add = [&input](pulp::midi::MidiEvent event, int offset) {
            event.sample_offset = offset;
            REQUIRE(input.add(event));
        };
        add(pulp::midi::MidiEvent::note_on(0, 60, 100), 0);
        add(pulp::midi::MidiEvent::note_on(0, 64, 80), 48);
        add(pulp::midi::MidiEvent::cc(0, 64, 127), 96);
        add(pulp::midi::MidiEvent::note_off(0, 60), 128);
        add(pulp::midi::MidiEvent::note_off(0, 64), 160);
        add(pulp::midi::MidiEvent::cc(0, 64, 0), 192);
        pulp::audio::Buffer<float> output(2, 256);
        auto view = output.view();
        pulp::audio::BufferView<const float> empty(nullptr, 0, 0);
        pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                             .num_samples = 256};
        proc->process(view, empty, input, output_midi, context);
        return std::vector<float>(output.channel(0).begin(), output.channel(0).end());
    };
    const auto first = render_sequence();
    const auto second = render_sequence();
    REQUIRE(first == second);
    REQUIRE(std::all_of(first.begin(), first.end(), [](float sample) {
        return std::isfinite(sample);
    }));
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

TEST_CASE("Grainfield live-input mode processes stereo input") {
    auto proc = grainfield::create_grainfield();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);
    store.set_value(grainfield::kMode, 1.0f);
    store.set_value(grainfield::kDryWet, 0.5f);
    pulp::format::PrepareContext prep{.sample_rate = 48000.0,
                                      .max_buffer_size = 256,
                                      .input_channels = 2,
                                      .output_channels = 2};
    proc->prepare(prep);

    pulp::audio::Buffer<float> input(2, 256);
    for (std::size_t i = 0; i < 256; ++i) {
        input.channel(0)[i] = 0.25f;
        input.channel(1)[i] = -0.125f;
    }
    pulp::audio::Buffer<float> output(2, 256);
    pulp::midi::MidiBuffer midi_in;
    pulp::midi::MidiBuffer midi_out;
    const float* input_channels[] = {input.channel(0).data(), input.channel(1).data()};
    pulp::audio::BufferView<const float> input_view(input_channels, 2, 256);
    auto output_view = output.view();
    pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                         .num_samples = 256};
    for (int block = 0; block < 32; ++block)
        proc->process(output_view, input_view, midi_in, midi_out, context);

    bool audible = false;
    for (std::size_t i = 0; i < 256; ++i) {
        REQUIRE(std::isfinite(output.channel(0)[i]));
        REQUIRE(std::isfinite(output.channel(1)[i]));
        audible |= std::abs(output.channel(0)[i]) > 0.0001f ||
                   std::abs(output.channel(1)[i]) > 0.0001f;
    }
    REQUIRE(audible);
}

TEST_CASE("Grainfield state round trips") {
    pulp::format::HeadlessHost host(grainfield::create_grainfield);
    host.prepare(48000, 512, 0, 2);
    host.state().set_value(grainfield::kGain, -12.0f);
    host.state().set_value(grainfield::kFreeze, 1.0f);
    const auto data = host.save_state();
    host.state().set_value(grainfield::kGain, 0.0f);
    host.state().set_value(grainfield::kFreeze, 0.0f);
    REQUIRE(host.load_state(data));
    REQUIRE(host.state().get_value(grainfield::kGain) == -12.0f);
    REQUIRE(host.state().get_value(grainfield::kFreeze) == 1.0f);
}

TEST_CASE("Grainfield freeze persistence is parameter-only") {
    // Pulp persists automatable parameters through StateStore.  The Beads
    // Synth wrapper exposes Init/Reset/Render only; its delay lines, grains,
    // envelopes, and reverb buffers remain private runtime state.  This
    // diagnostic pins the resulting boundary: the Freeze toggle is in the
    // host state, while the processor-owned payload is unchanged by it.
    grainfield::Grainfield processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    store.set_value(grainfield::kFreeze, 0.0f);
    const auto live_payload = processor.serialize_plugin_state();
    const auto live_state = store.serialize();
    store.set_value(grainfield::kFreeze, 1.0f);
    const auto frozen_payload = processor.serialize_plugin_state();
    const auto frozen_state = store.serialize();

    REQUIRE(store.get_value(grainfield::kFreeze) == 1.0f);
    REQUIRE(live_state != frozen_state);
    REQUIRE(live_payload == frozen_payload);
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

TEST_CASE("Grainfield exposes every declared parameter as a bounded LFO target") {
    grainfield::Grainfield processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    for (pulp::state::ParamID id = grainfield::kFirstModulatableParam;
         id <= grainfield::kLastModulatableParam; ++id) {
        processor.set_lfo(id, {.enabled = true, .rate_hz = 2.0f,
                                .depth = 0.25f, .phase = 0.125f});
        const auto modulation = processor.lfo(id);
        REQUIRE(modulation.enabled);
        REQUIRE(modulation.rate_hz == 2.0f);
        REQUIRE(modulation.depth == 0.25f);
        REQUIRE(modulation.phase == 0.125f);
    }

    // Slot zero and IDs beyond the declared range must not alias a real
    // parameter or grow the product-local modulation surface accidentally.
    processor.set_lfo(0, {.enabled = true, .depth = 1.0f});
    processor.set_lfo(grainfield::kLastModulatableParam + 1,
                      {.enabled = true, .depth = 1.0f});
    REQUIRE_FALSE(processor.lfo(0).enabled);
    REQUIRE_FALSE(processor.lfo(grainfield::kLastModulatableParam + 1).enabled);

    const auto payload = processor.serialize_plugin_state();
    grainfield::Grainfield restored;
    pulp::state::StateStore restored_store;
    restored.set_state_store(&restored_store);
    restored.define_parameters(restored_store);
    REQUIRE(restored.deserialize_plugin_state(payload));
    for (pulp::state::ParamID id = grainfield::kFirstModulatableParam;
         id <= grainfield::kLastModulatableParam; ++id)
        REQUIRE(restored.lfo(id).enabled);
}

TEST_CASE("Grainfield accepts legacy ten-slot modulation state") {
    grainfield::Grainfield source;
    pulp::state::StateStore source_store;
    source.set_state_store(&source_store);
    source.define_parameters(source_store);
    source.set_lfo(grainfield::kDensity,
                   {.enabled = true, .rate_hz = 3.0f, .depth = 0.2f, .phase = 0.4f});
    constexpr std::size_t stride = 1 + sizeof(float) * 3;
    const auto current = source.serialize_plugin_state();
    std::vector<std::uint8_t> legacy(6 + stride * 10, 0);
    std::copy_n(current.begin(), 6, legacy.begin());
    // The old format included slot zero before IDs 1..9; reconstruct that
    // layout explicitly instead of truncating the current ID-ordered form.
    std::copy_n(current.begin() + 6 +
                    (static_cast<std::size_t>(grainfield::kDensity) - 1) * stride,
                stride, legacy.begin() + 6 +
                    static_cast<std::size_t>(grainfield::kDensity) * stride);

    grainfield::Grainfield restored;
    pulp::state::StateStore restored_store;
    restored.set_state_store(&restored_store);
    restored.define_parameters(restored_store);
    REQUIRE(restored.deserialize_plugin_state(legacy));
    REQUIRE(restored.lfo(grainfield::kDensity).enabled);
    REQUIRE(restored.lfo(grainfield::kDensity).depth == 0.2f);
    REQUIRE_FALSE(restored.lfo(grainfield::kMode).enabled);
}


namespace {

struct RenderedBlocks {
    std::vector<float> left;
    std::vector<float> right;
};

RenderedBlocks render_seeded_note(int quality = 3, bool freeze = false) {
    stmlib::Random::Seed(0x12345678u);
    auto proc = grainfield::create_grainfield();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);
    store.set_value(grainfield::kQuality, static_cast<float>(quality));
    store.set_value(grainfield::kFreeze, freeze ? 1.0f : 0.0f);

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

TEST_CASE("Grainfield quality selects the four Beads synth resolutions") {
    std::array<RenderedBlocks, 4> renders;
    for (int quality = 0; quality < 4; ++quality) {
        renders[quality] = render_seeded_note(quality);
        REQUIRE(std::all_of(renders[quality].left.begin(), renders[quality].left.end(),
                            [](float sample) { return std::isfinite(sample); }));
    }

    // The resolution paths quantize different stages of the source DSP. The
    // lowest and highest modes must therefore remain observably distinct.
    REQUIRE(renders[0].left != renders[3].left);
}

TEST_CASE("Grainfield freeze toggle reaches the internal Beads synth path") {
    const auto live = render_seeded_note(3, false);
    const auto frozen = render_seeded_note(3, true);
    REQUIRE(std::all_of(frozen.left.begin(), frozen.left.end(),
                        [](float sample) { return std::isfinite(sample); }));
    REQUIRE(frozen.left != live.left);
}

TEST_CASE("Grainfield sample-rate and mode matrix is finite and fail-closed") {
    auto render_at = [](double sample_rate, bool live_input) {
        auto proc = grainfield::create_grainfield();
        pulp::state::StateStore store;
        proc->set_state_store(&store);
        proc->define_parameters(store);
        store.set_value(grainfield::kMode, live_input ? 1.0f : 0.0f);

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
        pulp::audio::Buffer<float> input(2, 128);
        for (std::size_t i = 0; i < 128; ++i) {
            input.channel(0)[i] = input.channel(1)[i] = std::sin(
                6.28318530718f * 440.0f * static_cast<float>(i) /
                static_cast<float>(sample_rate));
        }
        auto input_view = input.view();
        pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
        const float* input_ptrs[] = {input_view.channel_ptr(0), input_view.channel_ptr(1)};
        auto out_view = output.view();
        pulp::format::ProcessContext context;
        context.sample_rate = sample_rate;
        context.num_samples = 128;
        pulp::audio::BufferView<const float> process_input = live_input
            ? pulp::audio::BufferView<const float>(input_ptrs, 2, 128)
            : empty_input;
        proc->process(out_view, process_input, midi_in, midi_out, context);
        return output;
    };

    for (bool live_input : {false, true}) {
        const auto rendered = render_at(48000.0, live_input);
        bool has_signal = false;
        for (std::size_t channel = 0; channel < rendered.num_channels(); ++channel) {
            for (float sample : rendered.channel(channel)) {
                REQUIRE(std::isfinite(sample));
                has_signal = has_signal || std::abs(sample) > 1.0e-7f;
            }
        }
        REQUIRE(has_signal);
    }

    for (double sample_rate : {24000.0, 32000.0}) {
        const auto rendered = render_at(sample_rate, false);
        for (std::size_t channel = 0; channel < rendered.num_channels(); ++channel)
            for (float sample : rendered.channel(channel)) REQUIRE(sample == 0.0f);
    }

    const auto unsupported = render_at(44100.0, false);
    for (std::size_t channel = 0; channel < unsupported.num_channels(); ++channel)
        for (float sample : unsupported.channel(channel)) REQUIRE(sample == 0.0f);
}
