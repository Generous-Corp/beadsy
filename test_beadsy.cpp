#include "beads/dsp/trigger_generator.h"
#include "beadsy.hpp"
#include "stmlib/utils/random.h"
#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <pulp/audio/buffer.hpp>
#include <pulp/format/headless.hpp>
#include <pulp/midi/buffer.hpp>
#include <utility>
#include <vector>
#if defined(BEADSY_HAS_RT_ALLOCATION_PROBE)
#include "rt_allocation_probe.hpp"
#endif

TEST_CASE("Beadsy descriptor is an instrument") {
  auto proc = beadsy::create_beadsy();
  const auto desc = proc->descriptor();
  REQUIRE(desc.name == "Beadsy");
  REQUIRE(desc.manufacturer == "Independent Audio");
  REQUIRE(desc.category == pulp::format::PluginCategory::Instrument);
  REQUIRE(desc.accepts_midi);
  REQUIRE(desc.input_buses.size() == 2);
  REQUIRE(desc.input_buses[0].default_channels == 2);
  REQUIRE(desc.input_buses[0].optional);
  REQUIRE(desc.input_buses[1].name == "Sidechain");
  REQUIRE(desc.input_buses[1].default_channels == 2);
  REQUIRE(desc.input_buses[1].optional);
  REQUIRE(desc.output_buses.size() == 1);
}

TEST_CASE("Beadsy seed mode mapping is bounded and source-backed") {
  REQUIRE(beadsy::seed_button_state_from_value(
              -std::numeric_limits<float>::max()) ==
          beads::SEED_BUTTON_STATE_OFF);
  REQUIRE(beadsy::seed_button_state_from_value(0.49f) ==
          beads::SEED_BUTTON_STATE_OFF);
  REQUIRE(beadsy::seed_button_state_from_value(0.5f) ==
          beads::SEED_BUTTON_STATE_ON);
  REQUIRE(beadsy::seed_button_state_from_value(1.0f) ==
          beads::SEED_BUTTON_STATE_ON);
  REQUIRE(beadsy::seed_button_state_from_value(1.5f) ==
          beads::SEED_BUTTON_STATE_LOCKED);
  REQUIRE(beadsy::seed_button_state_from_value(2.0f) ==
          beads::SEED_BUTTON_STATE_LOCKED);
  REQUIRE(
      beadsy::seed_button_state_from_value(std::numeric_limits<float>::max()) ==
      beads::SEED_BUTTON_STATE_LOCKED);
  REQUIRE(beadsy::seed_button_state_from_value(
              std::numeric_limits<float>::quiet_NaN()) ==
          beads::SEED_BUTTON_STATE_OFF);
}

TEST_CASE("Beadsy seed states preserve the upstream trigger contract") {
  using Flags = stmlib::GateFlags;
  std::array<Flags, 8> no_external{};
  std::array<Flags, 8> merged{};
  std::array<beads::TriggerGenerator::Output, 8> output{};

  // With the manual-tap density midpoint, OFF stays idle while ON acts as
  // a held seed button. LOCKED deliberately suppresses manual taps; with a
  // supplied external stream it selects the upstream divider path instead.
  beads::TriggerGenerator off;
  beads::TriggerGenerator on;
  beads::TriggerGenerator locked;
  off.Init(48000.0f);
  on.Init(48000.0f);
  locked.Init(48000.0f);
  off.Render(beads::SEED_BUTTON_STATE_OFF, no_external.data(), 0.5f, 0.0f, 0.0f,
             output.data(), merged, output.size());
  REQUIRE_FALSE(output[0].trigger);
  REQUIRE_FALSE(output[0].gate);

  on.Render(beads::SEED_BUTTON_STATE_ON, no_external.data(), 0.5f, 0.0f, 0.0f,
            output.data(), merged, output.size());
  REQUIRE(output[0].trigger);
  REQUIRE(output[0].gate);

  locked.Render(beads::SEED_BUTTON_STATE_LOCKED, no_external.data(), 0.5f, 0.0f,
                0.0f, output.data(), merged, output.size());
  for (const auto &sample : output) {
    REQUIRE_FALSE(sample.trigger);
    REQUIRE_FALSE(sample.gate);
  }

  std::array<Flags, 8> external{};
  external[0] =
      static_cast<Flags>(stmlib::GATE_FLAG_HIGH | stmlib::GATE_FLAG_RISING);
  for (std::size_t i = 1; i < external.size(); ++i)
    external[i] = stmlib::GATE_FLAG_HIGH;
  locked.Init(48000.0f);
  locked.Render(beads::SEED_BUTTON_STATE_LOCKED, external.data(), 1.0f, 0.0f,
                0.0f, output.data(), merged, output.size());
  REQUIRE(output[0].trigger);
  REQUIRE(output[0].gate);
}

TEST_CASE("Beadsy routes the Seed Mode parameter to the adapted DSP") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);
  processor.prepare({.sample_rate = 48000.0,
                     .max_buffer_size = 8,
                     .input_channels = 0,
                     .output_channels = 2});

  pulp::midi::MidiBuffer midi_in;
  pulp::midi::MidiBuffer midi_out;
  pulp::audio::Buffer<float> output(2, 8);
  pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
  pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                       .num_samples = 8};
  const std::array<std::pair<float, beads::SeedButtonState>, 5> cases{{
      {-1.0f, beads::SEED_BUTTON_STATE_OFF},
      {0.0f, beads::SEED_BUTTON_STATE_OFF},
      {1.0f, beads::SEED_BUTTON_STATE_ON},
      {2.0f, beads::SEED_BUTTON_STATE_LOCKED},
      {99.0f, beads::SEED_BUTTON_STATE_LOCKED},
  }};
  for (const auto [value, expected] : cases) {
    store.set_value(beadsy::kSeedMode, value);
    output.clear();
    auto output_view = output.view();
    processor.process(output_view, empty_input, midi_in, midi_out, context);
    REQUIRE(processor.debug_seed_button_state() == expected);
  }
}

#if defined(BEADSY_HAS_RT_ALLOCATION_PROBE)
TEST_CASE("Beadsy prepared processing allocates no realtime memory",
          "[realtime][allocation]") {
  auto proc = beadsy::create_beadsy();
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

TEST_CASE("Beadsy prepared processing stays allocation-free across block sizes",
          "[realtime][allocation][matrix]") {
  for (const std::size_t block_size :
       {std::size_t{64}, std::size_t{256}, std::size_t{1024}}) {
    auto proc = beadsy::create_beadsy();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);
    pulp::format::PrepareContext prep{.sample_rate = 48000.0,
                                      .max_buffer_size =
                                          static_cast<int>(block_size),
                                      .input_channels = 0,
                                      .output_channels = 2};
    proc->prepare(prep);

    pulp::midi::MidiBuffer input;
    pulp::midi::MidiBuffer output_midi;
    input.add(pulp::midi::MidiEvent::note_on(0, 60, 100));
    pulp::audio::Buffer<float> output(2, block_size);
    auto view = output.view();
    pulp::audio::BufferView<const float> empty(nullptr, 0, 0);
    pulp::format::ProcessContext context{
        .sample_rate = 48000.0, .num_samples = static_cast<int>(block_size)};

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
    INFO("block_size=" << block_size << " allocation_count=" << allocation_count
                       << " allocated_bytes=" << allocated_bytes);
    REQUIRE(allocation_count == 0);
    REQUIRE(allocated_bytes == 0);
  }
}
#endif

TEST_CASE(
    "Beadsy exposes Beads semitone pitch range and maps MIDI note offsets") {
  auto render_note = [](std::uint8_t note) {
    auto proc = beadsy::create_beadsy();
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
    for (float sample : output.channel(0))
      energy += std::abs(sample);
    return energy;
  };

  auto proc = beadsy::create_beadsy();
  pulp::state::StateStore store;
  proc->set_state_store(&store);
  proc->define_parameters(store);
  REQUIRE(store.info(beadsy::kPitch)->range.min == -24.0f);
  REQUIRE(store.info(beadsy::kPitch)->range.max == 24.0f);
  REQUIRE(render_note(60) != render_note(72));
}

TEST_CASE("Beadsy maps MIDI velocity to deterministic output gain") {
  auto render_energy = [](std::uint8_t velocity) {
    stmlib::Random::Seed(0x13579bdu);
    auto proc = beadsy::create_beadsy();
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
    for (float sample : output.channel(0))
      energy += std::abs(sample);
    return energy;
  };
  const float full = render_energy(127);
  const float half = render_energy(64);
  REQUIRE(full > 0.0f);
  REQUIRE(half > 0.0f);
  REQUIRE(half < full);
}

TEST_CASE("Beadsy maps pitch bend to the Beads pitch offset") {
  auto render_energy = [](std::uint16_t bend) {
    stmlib::Random::Seed(0x2468aceu);
    auto proc = beadsy::create_beadsy();
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
    for (float sample : output.channel(0))
      energy += std::abs(sample);
    return energy;
  };
  const float center = render_energy(8192);
  const float up = render_energy(16383);
  REQUIRE(center > 0.0f);
  REQUIRE(up > 0.0f);
  REQUIRE(center != up);
}

TEST_CASE("Beadsy sustain and overlap handling is deterministic") {
  auto render_sequence = [](uint8_t midi_channel) {
    stmlib::Random::Seed(0x1020304u);
    auto proc = beadsy::create_beadsy();
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
    return std::vector<float>(output.channel(0).begin(),
                              output.channel(0).end());
  };
  const auto first = render_sequence(0);
  const auto second = render_sequence(0);
  const auto channel_fifteen = render_sequence(15);
  REQUIRE(first == second);
  REQUIRE(channel_fifteen == first);
  REQUIRE(std::all_of(first.begin(), first.end(),
                      [](float sample) { return std::isfinite(sample); }));
}

TEST_CASE("Beadsy MIDI gate transitions occur at event sample offsets") {
  beadsy::Beadsy processor;
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

TEST_CASE("Beadsy overlapping MIDI note-ons preserve retrigger edges") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);
  processor.prepare(pulp::format::PrepareContext{.sample_rate = 48000.0,
                                                 .max_buffer_size = 8,
                                                 .input_channels = 0,
                                                 .output_channels = 2});

  pulp::midi::MidiBuffer midi_in;
  pulp::midi::MidiBuffer midi_out;
  auto first = pulp::midi::MidiEvent::note_on(0, 60, 100);
  first.sample_offset = 0;
  auto retrigger = pulp::midi::MidiEvent::note_on(0, 64, 80);
  retrigger.sample_offset = 4;
  REQUIRE(midi_in.add(first));
  REQUIRE(midi_in.add(retrigger));

  pulp::audio::Buffer<float> output(2, 8);
  pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
  auto output_view = output.view();
  processor.process(
      output_view, empty_input, midi_in, midi_out,
      pulp::format::ProcessContext{.sample_rate = 48000.0, .num_samples = 8});
  const auto flags = processor.debug_seed_gate_flags();
  REQUIRE(flags.size() == 8);
  REQUIRE((flags[0] & stmlib::GATE_FLAG_RISING) != 0);
  REQUIRE((flags[4] & stmlib::GATE_FLAG_RISING) != 0);
  REQUIRE((flags[4] & stmlib::GATE_FLAG_HIGH) != 0);
}

TEST_CASE("Beadsy MIDI velocity gain changes at a mid-block note-on") {
  beadsy::Beadsy processor;
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
  for (std::size_t i = 0; i < 3; ++i)
    REQUIRE(gains[i] == 1.0f);
  for (std::size_t i = 3; i < gains.size(); ++i)
    REQUIRE(gains[i] == static_cast<float>(64.0f / 127.0f));
}

TEST_CASE("Beadsy MIDI note replacement restores the active note velocity") {
  beadsy::Beadsy processor;
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

TEST_CASE("Beadsy produces audio from one global MIDI trigger") {
  auto proc = beadsy::create_beadsy();
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

TEST_CASE("Beadsy is silent until synth MIDI or live input is selected") {
  auto proc = beadsy::create_beadsy();
  pulp::state::StateStore store;
  proc->set_state_store(&store);
  proc->define_parameters(store);
  proc->prepare({.sample_rate = 48000.0,
                 .max_buffer_size = 256,
                 .input_channels = 0,
                 .output_channels = 2});

  pulp::audio::Buffer<float> output(2, 256);
  pulp::midi::MidiBuffer midi_in;
  pulp::midi::MidiBuffer midi_out;
  pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
  pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                       .num_samples = 256};
  auto view = output.view();
  proc->process(view, empty_input, midi_in, midi_out, context);
  for (int channel = 0; channel < 2; ++channel)
    for (float sample : output.channel(channel))
      REQUIRE(sample == 0.0f);
}

TEST_CASE("Beadsy live-input mode processes stereo input") {
  auto proc = beadsy::create_beadsy();
  pulp::state::StateStore store;
  proc->set_state_store(&store);
  proc->define_parameters(store);
  store.set_value(beadsy::kMode, 1.0f);
  store.set_value(beadsy::kDryWet, 0.5f);
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
  const float *input_channels[] = {input.channel(0).data(),
                                   input.channel(1).data()};
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

TEST_CASE("Beadsy source mode selects the host sidechain bus") {
  const auto render = [](bool impulse_on_sidechain) {
    auto proc = beadsy::create_beadsy();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);
    store.set_value(beadsy::kMode, 2.0f);
    proc->prepare({.sample_rate = 48000.0,
                   .max_buffer_size = 128,
                   .input_channels = 2,
                   .output_channels = 2});
    pulp::audio::Buffer<float> main_input(2, 128);
    pulp::audio::Buffer<float> sidechain(2, 128);
    if (impulse_on_sidechain) {
      sidechain.channel(0)[0] = 1.0f;
      sidechain.channel(1)[0] = -1.0f;
    } else {
      main_input.channel(0)[0] = 1.0f;
      main_input.channel(1)[0] = -1.0f;
    }
    pulp::audio::Buffer<float> output(2, 128);
    const auto main_view =
        static_cast<const pulp::audio::Buffer<float> &>(main_input).view();
    const auto side_view =
        static_cast<const pulp::audio::Buffer<float> &>(sidechain).view();
    std::array<pulp::format::ProcessBusBufferView<const float>, 2> inputs{{
        {.info = {"Main In", 0, pulp::format::BusDirection::Input,
                  pulp::format::BusRole::Main, 2, true, true},
         .buffer = main_view},
        {.info = {"Sidechain", 1, pulp::format::BusDirection::Input,
                  pulp::format::BusRole::Sidechain, 2, true, true},
         .buffer = side_view},
    }};
    auto output_view = output.view();
    std::array<pulp::format::ProcessBusBufferView<float>, 1> outputs{{
        {.info = {"Main Out", 0, pulp::format::BusDirection::Output,
                  pulp::format::BusRole::Main, 2, false, true},
         .buffer = output_view},
    }};
    pulp::format::ProcessBuffers buffers{
        .inputs = pulp::format::ProcessBusBufferSet<const float>(inputs),
        .outputs = pulp::format::ProcessBusBufferSet<float>(outputs),
    };
    pulp::midi::MidiBuffer midi_in;
    pulp::midi::MidiBuffer midi_out;
    pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                         .num_samples = 128};
    proc->process(buffers, midi_in, midi_out, context);
    float energy = 0.0f;
    for (auto sample : output.channel(0)) {
      REQUIRE(std::isfinite(sample));
      energy += std::abs(sample);
    }
    return energy;
  };
  REQUIRE(render(true) > 0.0f);
  REQUIRE(render(false) == 0.0f);
}

TEST_CASE("Beadsy state round trips") {
  pulp::format::HeadlessHost host(beadsy::create_beadsy);
  host.prepare(48000, 512, 0, 2);
  host.state().set_value(beadsy::kGain, -12.0f);
  host.state().set_value(beadsy::kFreeze, 1.0f);
  const auto data = host.save_state();
  host.state().set_value(beadsy::kGain, 0.0f);
  host.state().set_value(beadsy::kFreeze, 0.0f);
  REQUIRE(host.load_state(data));
  REQUIRE(host.state().get_value(beadsy::kGain) == -12.0f);
  REQUIRE(host.state().get_value(beadsy::kFreeze) == 1.0f);
}

TEST_CASE("Beadsy freeze persistence is parameter-only") {
  // Pulp persists automatable parameters through StateStore.  The Beads
  // Synth wrapper exposes Init/Reset/Render only; its delay lines, grains,
  // envelopes, and reverb buffers remain private runtime state.  This
  // diagnostic pins the resulting boundary: the Freeze toggle is in the
  // host state, while the processor-owned payload is unchanged by it.
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);

  store.set_value(beadsy::kFreeze, 0.0f);
  const auto live_payload = processor.serialize_plugin_state();
  const auto live_state = store.serialize();
  store.set_value(beadsy::kFreeze, 1.0f);
  const auto frozen_payload = processor.serialize_plugin_state();
  const auto frozen_state = store.serialize();

  REQUIRE(store.get_value(beadsy::kFreeze) == 1.0f);
  REQUIRE(live_state != frozen_state);
  REQUIRE(live_payload == frozen_payload);
}

TEST_CASE("Beadsy LFO modulation leaves base automation intact and persists") {
  beadsy::Beadsy first;
  pulp::state::StateStore first_store;
  first.set_state_store(&first_store);
  first.define_parameters(first_store);
  first_store.set_value(beadsy::kDensity, 0.25f);
  first.set_lfo(
      beadsy::kDensity,
      {.enabled = true, .rate_hz = 2.0f, .depth = 0.1f, .phase = 0.0f});
  const auto data = first.serialize_plugin_state();
  REQUIRE(first_store.get_value(beadsy::kDensity) == 0.25f);
  beadsy::Beadsy restored_processor;
  pulp::state::StateStore restored_store;
  restored_processor.set_state_store(&restored_store);
  restored_processor.define_parameters(restored_store);
  REQUIRE(restored_processor.deserialize_plugin_state(data));
  REQUIRE(restored_store.deserialize(first_store.serialize()));
  REQUIRE(restored_store.get_value(beadsy::kDensity) == 0.25f);
  const auto restored = restored_processor.lfo(beadsy::kDensity);
  REQUIRE(restored.enabled);
  REQUIRE(restored.depth == 0.1f);
}

TEST_CASE("Beadsy LFO offsets knobs and button parameters during processing") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);
  processor.prepare({.sample_rate = 48000.0,
                     .max_buffer_size = 8,
                     .input_channels = 0,
                     .output_channels = 2});
  processor.set_lfo(beadsy::kDensity,
                    {.enabled = true, .rate_hz = 1.0f, .depth = 0.5f,
                     .phase = 0.25f});
  processor.set_lfo(beadsy::kFreeze,
                    {.enabled = true, .rate_hz = 1.0f, .depth = 0.5f,
                     .phase = 0.25f});
  pulp::midi::MidiBuffer midi_in;
  pulp::midi::MidiBuffer midi_out;
  pulp::audio::Buffer<float> output(2, 8);
  auto output_view = output.view();
  pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
  processor.process(output_view, empty_input, midi_in, midi_out,
                    {.sample_rate = 48000.0, .num_samples = 8});
  REQUIRE(store.get_modulated(beadsy::kDensity) > store.get_value(beadsy::kDensity));
  REQUIRE(store.get_modulated(beadsy::kFreeze) > store.get_value(beadsy::kFreeze));
  const auto density_display = store.displayed_modulation(beadsy::kDensity);
  const auto freeze_display = store.displayed_modulation(beadsy::kFreeze);
  REQUIRE(density_display.has_value());
  REQUIRE(freeze_display.has_value());
  REQUIRE(*density_display > store.get_value(beadsy::kDensity));
  REQUIRE(*freeze_display > store.get_value(beadsy::kFreeze));

  // Once the product LFO is disabled, the editor must stop showing a stale
  // played value even though the base parameter remains valid.
  processor.set_lfo(beadsy::kDensity, {});
  processor.set_lfo(beadsy::kFreeze, {});
  processor.process(output_view, empty_input, midi_in, midi_out,
                    {.sample_rate = 48000.0, .num_samples = 8});
  REQUIRE_FALSE(store.displayed_modulation(beadsy::kDensity).has_value());
  REQUIRE_FALSE(store.displayed_modulation(beadsy::kFreeze).has_value());
}

TEST_CASE("Beadsy exposes independent LFO 1 and LFO 2 state and shapes") {
  beadsy::Beadsy source;
  pulp::state::StateStore store;
  source.set_state_store(&store);
  source.define_parameters(store);
  source.set_lfo(beadsy::kDensity,
                 {.enabled = true, .rate_hz = 2.0f, .depth = 0.1f,
                  .phase = 0.0f, .shape = beadsy::LfoModulation::Shape::Triangle},
                 0);
  source.set_lfo(beadsy::kDensity,
                 {.enabled = true, .rate_hz = 3.0f, .depth = 0.2f,
                  .phase = 0.25f, .shape = beadsy::LfoModulation::Shape::Saw},
                 1);
  const auto payload = source.serialize_plugin_state();
  REQUIRE(payload[4] == 4);

  beadsy::Beadsy restored;
  pulp::state::StateStore restored_store;
  restored.set_state_store(&restored_store);
  restored.define_parameters(restored_store);
  REQUIRE(restored.deserialize_plugin_state(payload));
  const auto lane1 = restored.lfo(beadsy::kDensity, 0);
  const auto lane2 = restored.lfo(beadsy::kDensity, 1);
  REQUIRE(lane1.enabled);
  REQUIRE(lane2.enabled);
  REQUIRE(lane1.shape == beadsy::LfoModulation::Shape::Triangle);
  REQUIRE(lane2.shape == beadsy::LfoModulation::Shape::Saw);
  REQUIRE(lane1.rate_hz == 2.0f);
  REQUIRE(lane2.rate_hz == 3.0f);
}

TEST_CASE("Beadsy persists host route identities as rebinding hints") {
  beadsy::Beadsy source;
  source.set_route_identity(0, "track:piano");
  source.set_route_identity(1, "bus:sidechain");
  const auto payload = source.serialize_plugin_state();

  beadsy::Beadsy restored;
  REQUIRE(restored.deserialize_plugin_state(payload));
  REQUIRE(restored.route_identity(0) == "track:piano");
  REQUIRE(restored.route_identity(1) == "bus:sidechain");
}

TEST_CASE("Beadsy exposes every declared parameter as a bounded LFO target") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);

  for (pulp::state::ParamID id = beadsy::kFirstModulatableParam;
       id <= beadsy::kLastModulatableParam; ++id) {
    processor.set_lfo(
        id,
        {.enabled = true, .rate_hz = 2.0f, .depth = 0.25f, .phase = 0.125f});
    const auto modulation = processor.lfo(id);
    REQUIRE(modulation.enabled);
    REQUIRE(modulation.rate_hz == 2.0f);
    REQUIRE(modulation.depth == 0.25f);
    REQUIRE(modulation.phase == 0.125f);
  }

  // Slot zero and IDs beyond the declared range must not alias a real
  // parameter or grow the product-local modulation surface accidentally.
  processor.set_lfo(0, {.enabled = true, .depth = 1.0f});
  processor.set_lfo(beadsy::kLastModulatableParam + 1,
                    {.enabled = true, .depth = 1.0f});
  REQUIRE_FALSE(processor.lfo(0).enabled);
  REQUIRE_FALSE(processor.lfo(beadsy::kLastModulatableParam + 1).enabled);

  const auto payload = processor.serialize_plugin_state();
  beadsy::Beadsy restored;
  pulp::state::StateStore restored_store;
  restored.set_state_store(&restored_store);
  restored.define_parameters(restored_store);
  REQUIRE(restored.deserialize_plugin_state(payload));
  for (pulp::state::ParamID id = beadsy::kFirstModulatableParam;
       id <= beadsy::kLastModulatableParam; ++id)
    REQUIRE(restored.lfo(id).enabled);
}

TEST_CASE("Beadsy preserves centered Beads attenuverter law") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);

  REQUIRE(store.info(beadsy::kTimeAmount)->range.min == -1.0f);
  REQUIRE(store.info(beadsy::kTimeAmount)->range.max == 1.0f);
  REQUIRE(store.info(beadsy::kTimeAmount)->range.default_value == 0.0f);

  store.set_value(beadsy::kTimeAmount, -1.0f);
  store.set_value(beadsy::kSizeAmount, -0.25f);
  store.set_value(beadsy::kShapeAmount, 0.0f);
  store.set_value(beadsy::kPitchAmount, 0.875f);
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

TEST_CASE("Beadsy accepts legacy ten-slot modulation state") {
  beadsy::Beadsy source;
  pulp::state::StateStore source_store;
  source.set_state_store(&source_store);
  source.define_parameters(source_store);
  source.set_lfo(
      beadsy::kDensity,
      {.enabled = true, .rate_hz = 3.0f, .depth = 0.2f, .phase = 0.4f});
  constexpr std::size_t stride = 1 + sizeof(float) * 3;
  std::vector<std::uint8_t> legacy(6 + stride * 10, 0);
  legacy[0] = 'G'; legacy[1] = 'F'; legacy[2] = 'M'; legacy[3] = 'D';
  legacy[4] = 2;
  // The old format included slot zero before IDs 1..9; reconstruct that
  // layout explicitly instead of truncating the current ID-ordered form.
  const std::size_t at = 6 + static_cast<std::size_t>(beadsy::kDensity) * stride;
  legacy[at] = 1;
  const float values[] = {3.0f, 0.2f, 0.4f};
  std::memcpy(legacy.data() + at + 1, values, sizeof(values));

  beadsy::Beadsy restored;
  pulp::state::StateStore restored_store;
  restored.set_state_store(&restored_store);
  restored.define_parameters(restored_store);
  REQUIRE(restored.deserialize_plugin_state(legacy));
  REQUIRE(restored.lfo(beadsy::kDensity).enabled);
  REQUIRE(restored.lfo(beadsy::kDensity).depth == 0.2f);
  REQUIRE_FALSE(restored.lfo(beadsy::kMode).enabled);
}

TEST_CASE("Beadsy shared LFO settings and target assignments are independent") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);
  const beadsy::LfoModulation config{
      .enabled = true, .rate_hz = 2.5f, .depth = 0.4f, .phase = 0.125f,
      .shape = beadsy::LfoModulation::Shape::Square};
  processor.set_lfo_generator(0, config);
  processor.set_lfo_target_enabled(beadsy::kPitch, 0, true);

  // Visiting a new control must see the same running generator, while its
  // destination switch remains independent from the existing Pitch route.
  REQUIRE(processor.lfo_generator(0).enabled);
  REQUIRE_FALSE(processor.lfo_target_enabled(beadsy::kSize, 0));
  REQUIRE_FALSE(processor.lfo(beadsy::kSize, 0).enabled);
  REQUIRE(processor.lfo(beadsy::kSize, 0).rate_hz == 2.5f);
  REQUIRE(processor.lfo(beadsy::kSize, 0).shape == config.shape);
  processor.set_lfo_target_enabled(beadsy::kSize, 0, true);
  REQUIRE(processor.lfo(beadsy::kPitch, 0).enabled);
  REQUIRE(processor.lfo(beadsy::kSize, 0).enabled);
  processor.set_lfo_target_depth(beadsy::kPitch, 0, 0.8f);
  processor.set_lfo_target_depth(beadsy::kSize, 0, 0.2f);
  REQUIRE(processor.lfo(beadsy::kPitch, 0).depth == 0.8f);
  REQUIRE(processor.lfo(beadsy::kSize, 0).depth == 0.2f);
  processor.clear_lfo(beadsy::kSize, 0);
  REQUIRE_FALSE(processor.lfo_target_enabled(beadsy::kSize, 0));
  REQUIRE(processor.lfo(beadsy::kPitch, 0).enabled);
  REQUIRE(processor.lfo_generator(0).depth == 0.4f);
  processor.set_lfo(beadsy::kPitch, {});
  REQUIRE_FALSE(processor.lfo_target_enabled(beadsy::kPitch, 0));
  REQUIRE(processor.lfo_generator(0).enabled);
  REQUIRE(processor.lfo_generator(0).phase == 0.125f);
  REQUIRE(processor.lfo_target_depth(beadsy::kPitch, 0) == 0.8f);
  REQUIRE(processor.lfo_target_depth(beadsy::kSize, 0) == 0.2f);

  processor.set_lfo_target_enabled(beadsy::kSize, 0, true);
  auto disabled = processor.lfo_generator(0);
  disabled.enabled = false;
  processor.set_lfo_generator(0, disabled);
  REQUIRE(processor.lfo_target_enabled(beadsy::kSize, 0));
  REQUIRE_FALSE(processor.lfo(beadsy::kSize, 0).enabled);
  disabled.enabled = true;
  processor.set_lfo_generator(0, disabled);
  REQUIRE(processor.lfo(beadsy::kSize, 0).enabled);
}

TEST_CASE("Beadsy saves shared generators without routes and disabled routed lanes") {
  beadsy::Beadsy source;
  source.set_lfo_generator(0, {.enabled = true, .rate_hz = 4.0f,
                              .depth = 0.3f, .phase = 0.2f,
                              .shape = beadsy::LfoModulation::Shape::Saw});
  source.set_lfo_generator(1, {.enabled = false, .rate_hz = 7.0f,
                              .depth = 0.7f, .phase = 0.6f,
                              .shape = beadsy::LfoModulation::Shape::Triangle});
  source.set_lfo_target_enabled(beadsy::kSeedMode, 1, true);
  source.set_lfo_target_depth(beadsy::kSeedMode, 1, 0.9f);
  source.set_route_identity(0, "track:shared-lfo");
  const auto payload = source.serialize_plugin_state();
  beadsy::Beadsy restored;
  REQUIRE(restored.deserialize_plugin_state(payload));
  REQUIRE(restored.lfo_generator(0).enabled);
  REQUIRE(restored.lfo_generator(0).rate_hz == 4.0f);
  REQUIRE(restored.lfo_generator(0).depth == 0.3f);
  REQUIRE(restored.lfo_generator(0).phase == 0.2f);
  REQUIRE(restored.lfo_generator(0).shape == beadsy::LfoModulation::Shape::Saw);
  REQUIRE_FALSE(restored.lfo_target_enabled(beadsy::kSize, 0));
  REQUIRE_FALSE(restored.lfo_generator(1).enabled);
  REQUIRE(restored.lfo_generator(1).rate_hz == 7.0f);
  REQUIRE(restored.lfo_target_enabled(beadsy::kSeedMode, 1));
  REQUIRE(restored.lfo_target_depth(beadsy::kSeedMode, 1) == 0.9f);
  REQUIRE(restored.route_identity(0) == "track:shared-lfo");

  auto truncated = payload;
  truncated.pop_back();
  REQUIRE_FALSE(restored.deserialize_plugin_state(truncated));
  REQUIRE(restored.serialize_plugin_state() == payload);
  REQUIRE_FALSE(restored.deserialize_plugin_state(std::vector<std::uint8_t>{'G'}));
  REQUIRE(restored.serialize_plugin_state() == payload);
}

TEST_CASE("Beadsy shared oscillator advances once and displays scaled knob and seed values") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);
  processor.prepare({.sample_rate = 48000.0, .max_buffer_size = 8,
                     .input_channels = 0, .output_channels = 2});
  for (const auto id : {beadsy::kTime, beadsy::kSize, beadsy::kPitch,
                        beadsy::kQuality, beadsy::kSeedMode}) {
    const auto &range = store.info(id)->range;
    store.set_value(id, range.min);
    processor.set_lfo_target_enabled(id, 0, true);
  }
  processor.set_lfo_generator(0, {.enabled = true, .rate_hz = 3.0f,
                                 .depth = 0.5f, .phase = 0.25f});
  pulp::midi::MidiBuffer midi_in, midi_out;
  pulp::audio::Buffer<float> output(2, 8);
  auto output_view = output.view();
  pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
  const auto process = [&] {
    processor.process(output_view, empty_input, midi_in, midi_out,
                      {.sample_rate = 48000.0, .num_samples = 8});
  };
  process();
  REQUIRE(std::abs(processor.lfo_generator(0).phase - 0.2505f) < 0.000001f);
  for (const auto id : {beadsy::kTime, beadsy::kSize, beadsy::kPitch,
                        beadsy::kQuality, beadsy::kSeedMode}) {
    const auto &range = store.info(id)->range;
    const float expected = (range.min + range.max) * 0.5f;
    REQUIRE(store.get_value(id) == range.min);
    REQUIRE(std::abs(store.get_modulated(id) - expected) < 0.00001f);
    REQUIRE(store.displayed_modulation(id).has_value());
    REQUIRE(*store.displayed_modulation(id) ==
            range.normalize(store.get_modulated(id)));
  }
  REQUIRE(beadsy::seed_button_state_from_value(store.get_modulated(beadsy::kSeedMode))
          == beads::SEED_BUTTON_STATE_ON);

  processor.set_lfo_target_depth(beadsy::kTime, 0, 0.1f);
  processor.set_lfo_target_depth(beadsy::kSize, 0, 0.3f);
  auto shared = processor.lfo_generator(0);
  shared.phase = 0.25f;
  shared.depth = 0.0f; // Legacy generator depth does not scale destinations.
  processor.set_lfo_generator(0, shared);
  process();
  REQUIRE(std::abs(store.get_modulated(beadsy::kTime) - 0.1f) < 0.00001f);
  REQUIRE(std::abs(store.get_modulated(beadsy::kSize) - 0.3f) < 0.00001f);
  REQUIRE(store.get_modulated(beadsy::kSeedMode) == 1.0f);

  // Clearing one target immediately restores its base display without
  // stopping another assigned target or resetting the shared phase.
  processor.clear_lfo(beadsy::kSize);
  REQUIRE(store.get_modulated(beadsy::kSize) == store.get_value(beadsy::kSize));
  REQUIRE_FALSE(store.displayed_modulation(beadsy::kSize).has_value());
  REQUIRE(store.displayed_modulation(beadsy::kPitch).has_value());
  REQUIRE(processor.lfo_generator(0).enabled);
  auto negative = processor.lfo_generator(0);
  negative.phase = 0.75f;
  processor.set_lfo_generator(0, negative);
  process();
  REQUIRE(store.get_modulated(beadsy::kSeedMode) == 0.0f);
  REQUIRE(store.get_value(beadsy::kSeedMode) == 0.0f);
  REQUIRE(*store.displayed_modulation(beadsy::kSeedMode) == 0.0f);

  // Hosts may suspend processing when transport stops. Disabling the shared
  // generator must release its visible values without waiting for a block.
  negative.enabled = false;
  processor.set_lfo_generator(0, negative);
  REQUIRE(processor.lfo_target_enabled(beadsy::kSeedMode, 0));
  REQUIRE_FALSE(store.displayed_modulation(beadsy::kSeedMode).has_value());
  REQUIRE_FALSE(store.displayed_modulation(beadsy::kPitch).has_value());
}

TEST_CASE("Beadsy migrates old modulation layouts with deterministic shared settings") {
  const auto make_old = [](std::uint8_t version) {
    std::vector<std::uint8_t> data{'G', 'F', 'M', 'D', version, 0};
    const auto append = [&data](float value) {
      const auto *bytes = reinterpret_cast<const std::uint8_t *>(&value);
      data.insert(data.end(), bytes, bytes + sizeof(value));
    };
    const auto lanes = version == 3 ? 2 : 1;
    for (int lane = 0; lane < lanes; ++lane) {
      for (int id = 1; id <= 17; ++id) {
        const bool enabled = id == beadsy::kSize || id == beadsy::kPitch;
        data.push_back(enabled);
        if (version == 3)
          data.push_back(id == beadsy::kSize ? 2 : 3);
        append(id == beadsy::kSize ? 2.0f + lane : 9.0f);
        append(id == beadsy::kSize ? 0.4f : 0.8f);
        append(id == beadsy::kSize ? 0.25f : 0.75f);
      }
    }
    if (version >= 2)
      data.insert(data.end(), 4, 0); // Two empty host routing identities.
    return data;
  };
  for (const auto version : {1, 2, 3}) {
    beadsy::Beadsy restored;
    REQUIRE(restored.deserialize_plugin_state(make_old(version)));
    REQUIRE(restored.lfo_generator(0).enabled);
    REQUIRE(restored.lfo_generator(0).rate_hz == 2.0f);
    REQUIRE(restored.lfo_generator(0).depth == 0.4f);
    REQUIRE(restored.lfo_generator(0).phase == 0.25f);
    REQUIRE(restored.lfo_target_enabled(beadsy::kSize, 0));
    REQUIRE(restored.lfo_target_enabled(beadsy::kPitch, 0));
    REQUIRE(restored.lfo_target_depth(beadsy::kSize, 0) == 0.4f);
    REQUIRE(std::abs(restored.lfo_target_depth(beadsy::kPitch, 0) - 0.8f / 48.0f)
            < 0.000001f);
    REQUIRE_FALSE(restored.lfo_target_enabled(beadsy::kSeedMode, 0));
    if (version == 3) {
      REQUIRE(restored.lfo_generator(0).shape == beadsy::LfoModulation::Shape::Square);
      REQUIRE(restored.lfo_generator(1).rate_hz == 3.0f);
      REQUIRE(restored.lfo_target_enabled(beadsy::kPitch, 1));
    } else {
      REQUIRE(restored.lfo_generator(0).shape == beadsy::LfoModulation::Shape::Sine);
      REQUIRE_FALSE(restored.lfo_generator(1).enabled);
    }
  }
}

namespace {

struct RenderedBlocks {
  std::vector<float> left;
  std::vector<float> right;
};

RenderedBlocks render_seeded_note(int quality = 3, bool freeze = false) {
  stmlib::Random::Seed(0x12345678u);
  auto proc = beadsy::create_beadsy();
  pulp::state::StateStore store;
  proc->set_state_store(&store);
  proc->define_parameters(store);
  store.set_value(beadsy::kQuality, static_cast<float>(quality));
  store.set_value(beadsy::kFreeze, freeze ? 1.0f : 0.0f);

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
    rendered.left.insert(rendered.left.end(), output.channel(0).begin(),
                         output.channel(0).end());
    rendered.right.insert(rendered.right.end(), output.channel(1).begin(),
                          output.channel(1).end());
  }
  return rendered;
}

} // namespace

TEST_CASE("Beadsy seeded renders are deterministic") {
  const auto first = render_seeded_note();
  const auto second = render_seeded_note();
  REQUIRE(first.left == second.left);
  REQUIRE(first.right == second.right);
  REQUIRE(std::any_of(first.left.begin(), first.left.end(),
                      [](float sample) { return std::abs(sample) > 0.0001f; }));
}

TEST_CASE("Beadsy quality selects the four Beads synth resolutions") {
  std::array<RenderedBlocks, 4> renders;
  for (int quality = 0; quality < 4; ++quality) {
    renders[quality] = render_seeded_note(quality);
    REQUIRE(std::all_of(renders[quality].left.begin(),
                        renders[quality].left.end(),
                        [](float sample) { return std::isfinite(sample); }));
  }

  // The resolution paths quantize different stages of the source DSP. The
  // lowest and highest modes must therefore remain observably distinct.
  REQUIRE(renders[0].left != renders[3].left);
}

TEST_CASE("Beadsy freeze toggle reaches the internal Beads synth path") {
  const auto live = render_seeded_note(3, false);
  const auto frozen = render_seeded_note(3, true);
  REQUIRE(std::all_of(frozen.left.begin(), frozen.left.end(),
                      [](float sample) { return std::isfinite(sample); }));
  REQUIRE(frozen.left != live.left);
}

TEST_CASE("Beadsy sample-rate and mode matrix is finite and fail-closed") {
  auto render_at = [](double sample_rate, bool live_input) {
    auto proc = beadsy::create_beadsy();
    pulp::state::StateStore store;
    proc->set_state_store(&store);
    proc->define_parameters(store);
    store.set_value(beadsy::kMode, live_input ? 1.0f : 0.0f);

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
      input.channel(0)[i] = input.channel(1)[i] =
          std::sin(6.28318530718f * 440.0f * static_cast<float>(i) /
                   static_cast<float>(sample_rate));
    }
    auto input_view = input.view();
    pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
    const float *input_ptrs[] = {input_view.channel_ptr(0),
                                 input_view.channel_ptr(1)};
    auto out_view = output.view();
    pulp::format::ProcessContext context;
    context.sample_rate = sample_rate;
    context.num_samples = 128;
    pulp::audio::BufferView<const float> process_input =
        live_input ? pulp::audio::BufferView<const float>(input_ptrs, 2, 128)
                   : empty_input;
    proc->process(out_view, process_input, midi_in, midi_out, context);
    return output;
  };

  for (bool live_input : {false, true}) {
    const auto rendered = render_at(48000.0, live_input);
    bool has_signal = false;
    for (std::size_t channel = 0; channel < rendered.num_channels();
         ++channel) {
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
      for (float sample : rendered.channel(channel))
        REQUIRE(sample == 0.0f);
  }

  const auto unsupported = render_at(44100.0, false);
  for (std::size_t channel = 0; channel < unsupported.num_channels(); ++channel)
    for (float sample : unsupported.channel(channel))
      REQUIRE(sample == 0.0f);
}
