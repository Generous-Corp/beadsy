#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>
#include "beadsy.hpp"
#include "beadsy_editor.hpp"
#include <pulp/view/overlay_dismissal.hpp>

#include <cmath>

namespace {
float rms(const pulp::audio::Buffer<float> &buffer, std::size_t n) {
  double sum = 0.0;
  for (std::size_t ch = 0; ch < 2; ++ch)
    for (std::size_t i = 0; i < n; ++i)
      sum += double(buffer.view().channel(ch)[i]) * buffer.view().channel(ch)[i];
  return static_cast<float>(std::sqrt(sum / double(2 * n)));
}

struct Rig {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  pulp::midi::MidiBuffer midi_in, midi_out;
  pulp::audio::Buffer<float> output{2, 480};
  pulp::audio::BufferView<const float> empty_input{nullptr, 0, 0};
  Rig() {
    processor.set_state_store(&store);
    processor.define_parameters(store);
    processor.prepare({.sample_rate = 48000.0, .max_buffer_size = 480,
                       .input_channels = 0, .output_channels = 2});
    store.set_value(beadsy::kMode, 0.0f);
    store.set_value(beadsy::kDryWet, 0.0f);  // continuous oscillator only
  }
  float block() {
    auto view = output.view();
    processor.process(view, empty_input, midi_in, midi_out,
                      {.sample_rate = 48000.0, .num_samples = 480});
    midi_in.clear();
    return rms(output, 480);
  }
};
}  // namespace

TEST_CASE("Beadsy Lab identifies itself as its own instrument with both inputs") {
  beadsy::Beadsy processor;
  const auto descriptor = processor.descriptor();
  REQUIRE(descriptor.name == "Beadsy Lab");
  REQUIRE(descriptor.bundle_id == "com.independent_audio.beadsy-lab");
  REQUIRE(descriptor.category == pulp::format::PluginCategory::Instrument);
  REQUIRE(descriptor.accepts_midi);
  REQUIRE(descriptor.input_buses.size() == 2);
  REQUIRE(descriptor.input_buses[0].name == "Main In");
  REQUIRE(descriptor.input_buses[1].name == "Sidechain");
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);
  REQUIRE(store.info(beadsy::kNoteGate) != nullptr);
  REQUIRE(store.get_value(beadsy::kNoteGate) == 1.0f);
  REQUIRE(store.get_value(beadsy::kNoteRelease) == 250.0f);
  REQUIRE(store.info(beadsy::kMode)->range.max == 2.0f);
}

TEST_CASE("Beadsy Lab note gate follows the held keys on the internal synth") {
  Rig rig;
  // Silent until a note arrives (the instrument stays quiet on open).
  REQUIRE(rig.block() == 0.0f);
  rig.midi_in.add(pulp::midi::MidiEvent::note_on(0, 60, 100));
  float held = 0.0f;
  for (int i = 0; i < 20; ++i) held = std::max(held, rig.block());
  REQUIRE(held > 0.01f);

  rig.midi_in.add(pulp::midi::MidiEvent::note_off(0, 60));
  // 250 ms release = 25 blocks of 10 ms; after 60 blocks the tail is gone.
  float after_release = 0.0f;
  for (int i = 0; i < 60; ++i) after_release = rig.block();
  REQUIRE(after_release < held * 0.01f);

  // The engine kept running: the next note resumes without a restart gap
  // and with the attack opening within the first block.
  rig.midi_in.add(pulp::midi::MidiEvent::note_on(0, 64, 100));
  REQUIRE(rig.block() > 0.005f);
}

TEST_CASE("Beadsy Lab with the gate off keeps the oscillator ringing like the hardware") {
  Rig rig;
  rig.store.set_value(beadsy::kNoteGate, 0.0f);
  rig.midi_in.add(pulp::midi::MidiEvent::note_on(0, 60, 100));
  float held = 0.0f;
  for (int i = 0; i < 20; ++i) held = std::max(held, rig.block());
  REQUIRE(held > 0.01f);
  rig.midi_in.add(pulp::midi::MidiEvent::note_off(0, 60));
  float later = 0.0f;
  for (int i = 0; i < 60; ++i) later = rig.block();
  REQUIRE(later > held * 0.25f);
  REQUIRE(rig.processor.note_gate_level() == 1.0f);
}

TEST_CASE("Beadsy Lab panel menu switches the source and the gate") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);
  auto editor = processor.create_view();
  auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
  REQUIRE(native != nullptr);
  native->set_bounds({0, 0, 350, 551});
  native->layout_children();
  auto *menu = native->show_panel_menu({175.0f, 505.0f});
  REQUIRE(menu != nullptr);
  const auto find = [&](std::string_view label) -> pulp::view::ContextMenu::Item & {
    auto &items = const_cast<std::vector<pulp::view::ContextMenu::Item> &>(menu->items());
    for (auto &item : items)
      if (item.label == label) return item;
    FAIL("missing row " << label);
    return items.front();
  };
  auto &source = find("Source");
  REQUIRE(source.choices.size() == 3);
  source.set_choice_index(2);
  REQUIRE(store.get_value(beadsy::kMode) == 2.0f);
  find("Note Gate").set_toggled(false);
  REQUIRE(store.get_value(beadsy::kNoteGate) == 0.0f);
  find("Release").set_normalized_value(1.0f);
  REQUIRE(store.get_value(beadsy::kNoteRelease) == Catch::Approx(4000.0f));
  // A right-click on bare faceplate reaches the same menu through the root.
  native->simulate_click({175.0f, 505.0f});  // dismiss
  pulp::view::route_context_press(*native, {175.0f, 505.0f});
  REQUIRE(native->modulation_menu() != nullptr);
}
