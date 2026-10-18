#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <pulp/format/headless.hpp>
#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>
#include "beasy.hpp"
#include "stmlib/utils/random.h"
#if defined(BEASY_HAS_RT_ALLOCATION_PROBE)
#include "rt_allocation_probe.hpp"
#endif

TEST_CASE("Beasy descriptor is an instrument") {
    auto proc = beasy::create_beasy();
    const auto desc = proc->descriptor();
    REQUIRE(desc.name == "Beasy");
    REQUIRE(desc.manufacturer == "Independent Audio");
    REQUIRE(desc.category == pulp::format::PluginCategory::Instrument);
    REQUIRE(desc.accepts_midi);
    REQUIRE(desc.input_buses.size() == 1);
    REQUIRE(desc.input_buses[0].default_channels == 2);
    REQUIRE(desc.input_buses[0].optional);
    REQUIRE(desc.output_buses.size() == 1);
}

#if defined(BEASY_HAS_RT_ALLOCATION_PROBE)
TEST_CASE("Beasy prepared processing allocates no realtime memory",
          "[realtime][allocation]") {
    auto proc = beasy::create_beasy();
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

TEST_CASE("Beasy prepared processing stays allocation-free across block sizes",
          "[realtime][allocation][matrix]") {
    for (const std::size_t block_size : {std::size_t{64}, std::size_t{256},
                                         std::size_t{1024}}) {
        auto proc = beasy::create_beasy();
        pulp::state::StateStore store;
        proc->set_state_store(&store);
        proc->define_parameters(store);
        pulp::format::PrepareContext prep{.sample_rate = 48000.0,
                                          .max_buffer_size = static_cast<int>(block_size),
                                          .input_channels = 0,
                                          .output_channels = 2};
        proc->prepare(prep);

        pulp::midi::MidiBuffer input;
        pulp::midi::MidiBuffer output_midi;
        input.add(pulp::midi::MidiEvent::note_on(0, 60, 100));
        pulp::audio::Buffer<float> output(2, block_size);
        auto view = output.view();
        pulp::audio::BufferView<const float> empty(nullptr, 0, 0);
        pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                             .num_samples = static_cast<int>(block_size)};

        // Prime event and output-buffer state before arming the probe. The
        // actual steady-state callback is then exercised with an empty MIDI
        // buffer, so this assertion measures only realtime processing.
        proc->process(view, empty, input, output_midi, context);
        input.clear();
        output_midi.clear();

        std::size_t allocation_count = 0;
        std::size_t allocated_bytes = 0;
        {
            pulp::test::RtAllocationProbe probe;
            for (int block = 0; block < 32; ++block)
                proc->process(view, empty, input, output_midi, context);
            // Snapshot before the probe is destroyed; diagnostics below must
            // run outside its scope because Catch2 allocates while formatting.
            allocation_count = probe.allocation_count();
            allocated_bytes = probe.allocated_bytes();
        }
        INFO("block_size=" << block_size
             << " allocation_count=" << allocation_count
             << " allocated_bytes=" << allocated_bytes);
        REQUIRE(allocation_count == 0);
        REQUIRE(allocated_bytes == 0);
    }
}
#endif

TEST_CASE("Beasy exposes Beads semitone pitch range and maps MIDI note offsets") {
    auto render_note = [](std::uint8_t note) {
        auto proc = beasy::create_beasy();
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

    auto proc = beasy::create_beasy();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);
    REQUIRE(store.info(beasy::kPitch)->range.min == -24.0f);
    REQUIRE(store.info(beasy::kPitch)->range.max == 24.0f);
    REQUIRE(render_note(60) != render_note(72));
}

TEST_CASE("Beasy maps MIDI velocity to deterministic output gain") {
    auto render_energy = [](std::uint8_t velocity) {
        stmlib::Random::Seed(0x13579bdu);
        auto proc = beasy::create_beasy();
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

TEST_CASE("Beasy maps pitch bend to the Beads pitch offset") {
    auto render_energy = [](std::uint16_t bend) {
        stmlib::Random::Seed(0x2468aceu);
        auto proc = beasy::create_beasy();
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

TEST_CASE("Beasy sustain and overlap handling is deterministic") {
    auto render_sequence = [](uint8_t midi_channel) {
        stmlib::Random::Seed(0x1020304u);
        auto proc = beasy::create_beasy();
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
        add(pulp::midi::MidiEvent::note_on(midi_channel, 60, 100), 0);
        add(pulp::midi::MidiEvent::note_on(midi_channel, 64, 80), 48);
        add(pulp::midi::MidiEvent::cc(midi_channel, 64, 127), 96);
        add(pulp::midi::MidiEvent::note_off(midi_channel, 60), 128);
        add(pulp::midi::MidiEvent::note_off(midi_channel, 64), 160);
        add(pulp::midi::MidiEvent::cc(midi_channel, 64, 0), 192);
        pulp::audio::Buffer<float> output(2, 256);
        auto view = output.view();
        pulp::audio::BufferView<const float> empty(nullptr, 0, 0);
        pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                             .num_samples = 256};
        proc->process(view, empty, input, output_midi, context);
        return std::vector<float>(output.channel(0).begin(), output.channel(0).end());
    };
    const auto first = render_sequence(0);
    const auto second = render_sequence(0);
    const auto channel_fifteen = render_sequence(15);
    REQUIRE(first == second);
    REQUIRE(channel_fifteen == first);
    REQUIRE(std::all_of(first.begin(), first.end(), [](float sample) {
        return std::isfinite(sample);
    }));
}

TEST_CASE("Beasy MIDI gate transitions occur at event sample offsets") {
    beasy::Beasy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    pulp::format::PrepareContext prep{.sample_rate = 48000.0,
                                      .max_buffer_size = 8,
                                      .input_channels = 0,
                                      .output_channels = 2};
    processor.prepare(prep);

    pulp::midi::MidiBuffer midi_in;
    pulp::midi::MidiBuffer midi_out;
    auto note_on = pulp::midi::MidiEvent::note_on(0, 60, 100);
    note_on.sample_offset = 3;
    auto note_off = pulp::midi::MidiEvent::note_off(0, 60);
    note_off.sample_offset = 6;
    REQUIRE(midi_in.add(note_on));
    REQUIRE(midi_in.add(note_off));

    pulp::audio::Buffer<float> output(2, 8);
    auto output_view = output.view();
    pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
    pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                         .num_samples = 8};
    processor.process(output_view, empty_input, midi_in, midi_out, context);

    const auto flags = processor.debug_seed_gate_flags();
    REQUIRE(flags.size() == 8);
    for (std::size_t i = 0; i < 3; ++i)
        REQUIRE((flags[i] & stmlib::GATE_FLAG_HIGH) == 0);
    REQUIRE((flags[3] & stmlib::GATE_FLAG_HIGH) != 0);
    REQUIRE((flags[3] & stmlib::GATE_FLAG_RISING) != 0);
    REQUIRE((flags[4] & stmlib::GATE_FLAG_HIGH) != 0);
    REQUIRE((flags[5] & stmlib::GATE_FLAG_HIGH) != 0);
    REQUIRE((flags[6] & stmlib::GATE_FLAG_HIGH) == 0);
    REQUIRE((flags[6] & stmlib::GATE_FLAG_FALLING) != 0);
    REQUIRE((flags[7] & stmlib::GATE_FLAG_HIGH) == 0);
}

TEST_CASE("Beasy MIDI velocity gain changes at a mid-block note-on") {
    beasy::Beasy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    pulp::format::PrepareContext prep{.sample_rate = 48000.0,
                                      .max_buffer_size = 8,
                                      .input_channels = 0,
                                      .output_channels = 2};
    processor.prepare(prep);

    pulp::midi::MidiBuffer midi_in;
    pulp::midi::MidiBuffer midi_out;
    auto note_on = pulp::midi::MidiEvent::note_on(0, 60, 64);
    note_on.sample_offset = 3;
    REQUIRE(midi_in.add(note_on));

    pulp::audio::Buffer<float> output(2, 8);
    auto output_view = output.view();
    pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
    pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                         .num_samples = 8};
    processor.process(output_view, empty_input, midi_in, midi_out, context);

    const auto gains = processor.debug_velocity_gain_stream();
    REQUIRE(gains.size() == 8);
    for (std::size_t i = 0; i < 3; ++i) REQUIRE(gains[i] == 1.0f);
    for (std::size_t i = 3; i < gains.size(); ++i)
        REQUIRE(gains[i] == static_cast<float>(64.0f / 127.0f));
}

TEST_CASE("Beasy MIDI note replacement restores the active note velocity") {
    beasy::Beasy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    pulp::format::PrepareContext prep{.sample_rate = 48000.0,
                                      .max_buffer_size = 8,
                                      .input_channels = 0,
                                      .output_channels = 2};
    processor.prepare(prep);

    pulp::midi::MidiBuffer midi_in;
    pulp::midi::MidiBuffer midi_out;
    auto first = pulp::midi::MidiEvent::note_on(0, 60, 100);
    first.sample_offset = 0;
    auto replacement = pulp::midi::MidiEvent::note_on(0, 64, 32);
    replacement.sample_offset = 2;
    auto release = pulp::midi::MidiEvent::note_off(0, 64);
    release.sample_offset = 5;
    REQUIRE(midi_in.add(first));
    REQUIRE(midi_in.add(replacement));
    REQUIRE(midi_in.add(release));

    pulp::audio::Buffer<float> output(2, 8);
    auto output_view = output.view();
    pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
    pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                         .num_samples = 8};
    processor.process(output_view, empty_input, midi_in, midi_out, context);

    const auto gains = processor.debug_velocity_gain_stream();
    REQUIRE(gains.size() == 8);
    for (std::size_t i = 0; i < 2; ++i)
        REQUIRE(gains[i] == static_cast<float>(100.0f / 127.0f));
    for (std::size_t i = 2; i < 5; ++i)
        REQUIRE(gains[i] == static_cast<float>(32.0f / 127.0f));
    for (std::size_t i = 5; i < gains.size(); ++i)
        REQUIRE(gains[i] == static_cast<float>(100.0f / 127.0f));
}

TEST_CASE("Beasy produces audio from one global MIDI trigger") {
    auto proc = beasy::create_beasy();
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

TEST_CASE("Beasy live-input mode processes stereo input") {
    auto proc = beasy::create_beasy();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);
    store.set_value(beasy::kMode, 1.0f);
    store.set_value(beasy::kDryWet, 0.5f);
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

TEST_CASE("Beasy state round trips") {
    pulp::format::HeadlessHost host(beasy::create_beasy);
    host.prepare(48000, 512, 0, 2);
    host.state().set_value(beasy::kGain, -12.0f);
    host.state().set_value(beasy::kFreeze, 1.0f);
    const auto data = host.save_state();
    host.state().set_value(beasy::kGain, 0.0f);
    host.state().set_value(beasy::kFreeze, 0.0f);
    REQUIRE(host.load_state(data));
    REQUIRE(host.state().get_value(beasy::kGain) == -12.0f);
    REQUIRE(host.state().get_value(beasy::kFreeze) == 1.0f);
}

TEST_CASE("Beasy freeze persistence is parameter-only") {
    // Pulp persists automatable parameters through StateStore.  The Beads
    // Synth wrapper exposes Init/Reset/Render only; its delay lines, grains,
    // envelopes, and reverb buffers remain private runtime state.  This
    // diagnostic pins the resulting boundary: the Freeze toggle is in the
    // host state, while the processor-owned payload is unchanged by it.
    beasy::Beasy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    store.set_value(beasy::kFreeze, 0.0f);
    const auto live_payload = processor.serialize_plugin_state();
    const auto live_state = store.serialize();
    store.set_value(beasy::kFreeze, 1.0f);
    const auto frozen_payload = processor.serialize_plugin_state();
    const auto frozen_state = store.serialize();

    REQUIRE(store.get_value(beasy::kFreeze) == 1.0f);
    REQUIRE(live_state != frozen_state);
    REQUIRE(live_payload == frozen_payload);
}

TEST_CASE("Beasy LFO modulation leaves base automation intact and persists") {
    beasy::Beasy first;
    pulp::state::StateStore first_store;
    first.set_state_store(&first_store);
    first.define_parameters(first_store);
    first_store.set_value(beasy::kDensity, 0.25f);
    first.set_lfo(beasy::kDensity,
                             {.enabled = true, .rate_hz = 2.0f, .depth = 0.1f, .phase = 0.0f});
    const auto data = first.serialize_plugin_state();
    REQUIRE(first_store.get_value(beasy::kDensity) == 0.25f);
    beasy::Beasy restored_processor;
    pulp::state::StateStore restored_store;
    restored_processor.set_state_store(&restored_store);
    restored_processor.define_parameters(restored_store);
    REQUIRE(restored_processor.deserialize_plugin_state(data));
    REQUIRE(restored_store.deserialize(first_store.serialize()));
    REQUIRE(restored_store.get_value(beasy::kDensity) == 0.25f);
    const auto restored = restored_processor.lfo(beasy::kDensity);
    REQUIRE(restored.enabled);
    REQUIRE(restored.depth == 0.1f);
}

TEST_CASE("Beasy exposes every declared parameter as a bounded LFO target") {
    beasy::Beasy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    for (pulp::state::ParamID id = beasy::kFirstModulatableParam;
         id <= beasy::kLastModulatableParam; ++id) {
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
    processor.set_lfo(beasy::kLastModulatableParam + 1,
                      {.enabled = true, .depth = 1.0f});
    REQUIRE_FALSE(processor.lfo(0).enabled);
    REQUIRE_FALSE(processor.lfo(beasy::kLastModulatableParam + 1).enabled);

    const auto payload = processor.serialize_plugin_state();
    beasy::Beasy restored;
    pulp::state::StateStore restored_store;
    restored.set_state_store(&restored_store);
    restored.define_parameters(restored_store);
    REQUIRE(restored.deserialize_plugin_state(payload));
    for (pulp::state::ParamID id = beasy::kFirstModulatableParam;
         id <= beasy::kLastModulatableParam; ++id)
        REQUIRE(restored.lfo(id).enabled);
}

TEST_CASE("Beasy preserves centered Beads attenuverter law") {
    beasy::Beasy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    REQUIRE(store.info(beasy::kTimeAmount)->range.min == -1.0f);
    REQUIRE(store.info(beasy::kTimeAmount)->range.max == 1.0f);
    REQUIRE(store.info(beasy::kTimeAmount)->range.default_value == 0.0f);

    store.set_value(beasy::kTimeAmount, -1.0f);
    store.set_value(beasy::kSizeAmount, -0.25f);
    store.set_value(beasy::kShapeAmount, 0.0f);
    store.set_value(beasy::kPitchAmount, 0.875f);
    const auto values = processor.debug_randomized_parameters();
    REQUIRE(values[0].attenuverter == -1.0f);
    REQUIRE(values[1].attenuverter == -0.25f);
    REQUIRE(values[2].attenuverter == 0.0f);
    REQUIRE(values[3].attenuverter == 0.875f);
    REQUIRE_FALSE(values[0].has_cv);
    REQUIRE_FALSE(values[1].has_cv);
    REQUIRE_FALSE(values[2].has_cv);
    REQUIRE_FALSE(values[3].has_cv);
}

TEST_CASE("Beasy accepts legacy ten-slot modulation state") {
    beasy::Beasy source;
    pulp::state::StateStore source_store;
    source.set_state_store(&source_store);
    source.define_parameters(source_store);
    source.set_lfo(beasy::kDensity,
                   {.enabled = true, .rate_hz = 3.0f, .depth = 0.2f, .phase = 0.4f});
    constexpr std::size_t stride = 1 + sizeof(float) * 3;
    const auto current = source.serialize_plugin_state();
    std::vector<std::uint8_t> legacy(6 + stride * 10, 0);
    std::copy_n(current.begin(), 6, legacy.begin());
    // The old format included slot zero before IDs 1..9; reconstruct that
    // layout explicitly instead of truncating the current ID-ordered form.
    std::copy_n(current.begin() + 6 +
                    (static_cast<std::size_t>(beasy::kDensity) - 1) * stride,
                stride, legacy.begin() + 6 +
                    static_cast<std::size_t>(beasy::kDensity) * stride);

    beasy::Beasy restored;
    pulp::state::StateStore restored_store;
    restored.set_state_store(&restored_store);
    restored.define_parameters(restored_store);
    REQUIRE(restored.deserialize_plugin_state(legacy));
    REQUIRE(restored.lfo(beasy::kDensity).enabled);
    REQUIRE(restored.lfo(beasy::kDensity).depth == 0.2f);
    REQUIRE_FALSE(restored.lfo(beasy::kMode).enabled);
}


namespace {

struct RenderedBlocks {
    std::vector<float> left;
    std::vector<float> right;
};

RenderedBlocks render_seeded_note(int quality = 3, bool freeze = false) {
    stmlib::Random::Seed(0x12345678u);
    auto proc = beasy::create_beasy();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);
    store.set_value(beasy::kQuality, static_cast<float>(quality));
    store.set_value(beasy::kFreeze, freeze ? 1.0f : 0.0f);

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

TEST_CASE("Beasy seeded renders are deterministic") {
    const auto first = render_seeded_note();
    const auto second = render_seeded_note();
    REQUIRE(first.left == second.left);
    REQUIRE(first.right == second.right);
    REQUIRE(std::any_of(first.left.begin(), first.left.end(), [](float sample) {
        return std::abs(sample) > 0.0001f;
    }));
}

TEST_CASE("Beasy quality selects the four Beads synth resolutions") {
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

TEST_CASE("Beasy freeze toggle reaches the internal Beads synth path") {
    const auto live = render_seeded_note(3, false);
    const auto frozen = render_seeded_note(3, true);
    REQUIRE(std::all_of(frozen.left.begin(), frozen.left.end(),
                        [](float sample) { return std::isfinite(sample); }));
    REQUIRE(frozen.left != live.left);
}

TEST_CASE("Beasy sample-rate and mode matrix is finite and fail-closed") {
    auto render_at = [](double sample_rate, bool live_input) {
        auto proc = beasy::create_beasy();
        pulp::state::StateStore store;
        proc->set_state_store(&store);
        proc->define_parameters(store);
        store.set_value(beasy::kMode, live_input ? 1.0f : 0.0f);

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
