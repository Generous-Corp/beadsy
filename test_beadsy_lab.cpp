#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>
#include "beadsy.hpp"
#include "beadsy_editor.hpp"
#include <pulp/view/overlay_dismissal.hpp>
#include <pulp/view/screenshot.hpp>
#include <filesystem>
#include <string_view>

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
  // Source is three radio rows; picking one makes it the live source and the
  // menu rebuilds with exactly that row checked.
  REQUIRE(find("Internal synth").checked);
  REQUIRE_FALSE(find("Side Chain").checked);
  find("Side Chain").set_toggled(true);
  REQUIRE(store.get_value(beadsy::kMode) == 2.0f);
  REQUIRE(find("Side Chain").checked);
  REQUIRE_FALSE(find("Internal synth").checked);
  find("Main In").set_toggled(true);
  REQUIRE(store.get_value(beadsy::kMode) == 1.0f);
  // Unchecking the current row keeps the selection: there is always a source.
  find("Main In").set_toggled(false);
  REQUIRE(store.get_value(beadsy::kMode) == 1.0f);
  REQUIRE(find("Main In").checked);
  find("Internal synth").set_toggled(true);
  REQUIRE(store.get_value(beadsy::kMode) == 0.0f);
  find("Note Gate").set_toggled(false);
  REQUIRE(store.get_value(beadsy::kNoteGate) == 0.0f);
  find("Release").set_normalized_value(1.0f);
  REQUIRE(store.get_value(beadsy::kNoteRelease) == Catch::Approx(4000.0f));
  // A right-click on bare faceplate reaches the same menu through the root.
  native->simulate_click({175.0f, 505.0f});  // dismiss
  pulp::view::route_context_press(*native, {175.0f, 505.0f});
  REQUIRE(native->modulation_menu() != nullptr);
}

TEST_CASE("Beadsy Lab source chip sits in the band and opens the source menu on a left click") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);
  auto editor = processor.create_view();
  auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
  REQUIRE(native != nullptr);
  native->set_bounds({0, 0, 350, 551});
  native->layout_children();
  auto *chip = native->source_chip();
  auto *help = native->help_button();
  REQUIRE(chip != nullptr);
  REQUIRE(help != nullptr);
  const auto cb = chip->bounds();
  const auto hb = help->bounds();
  // Both live in the band under the divider, beside the grip, and the chip
  // is centred.
  const float unit = 350.0f / 70.8f;
  const float band_top = 551.0f - beadsy::ResizeGrip::size_for_unit(unit) - 0.6f * unit;
  REQUIRE(cb.y >= band_top);
  REQUIRE(cb.y + cb.height <= 551.0f);
  REQUIRE(hb.y >= band_top);
  REQUIRE(hb.x < cb.x);
  REQUIRE(cb.x + cb.width * 0.5f == Catch::Approx(175.0f).margin(0.5f));
  REQUIRE(std::string_view(chip->label_provider()) == "SOURCE  INTERNAL");
  store.set_value(beadsy::kMode, 2.0f);
  REQUIRE(std::string_view(chip->label_provider()) == "SOURCE  SIDE CHAIN");
  // A primary press on the chip opens the same Source menu a secondary press
  // on bare faceplate does.
  REQUIRE(native->modulation_menu() == nullptr);
  native->simulate_click({cb.x + cb.width * 0.5f, cb.y + cb.height * 0.5f});
  auto *menu = native->modulation_menu();
  REQUIRE(menu != nullptr);
  bool has_main_in = false;
  for (const auto &item : menu->items())
    has_main_in = has_main_in || item.label == "Main In";
  REQUIRE(has_main_in);
}

TEST_CASE("Beadsy Lab help button opens the acknowledgement panel and Escape closes it") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);
  auto editor = processor.create_view();
  auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
  REQUIRE(native != nullptr);
  native->set_bounds({0, 0, 350, 551});
  native->layout_children();
  const auto &sections = beadsy::help_sections();
  REQUIRE(sections.size() >= 5);
  const std::string_view about = sections.front().body;
  REQUIRE(about.find("Mutable Instruments") != std::string_view::npos);
  REQUIRE(about.find("Emilie Gillet") != std::string_view::npos);
  REQUIRE(about.find("MIT license") != std::string_view::npos);
  REQUIRE(about.find("not affiliated") != std::string_view::npos);

  REQUIRE(native->help_panel() == nullptr);
  const auto hb = native->help_button()->bounds();
  native->simulate_click({hb.x + hb.width * 0.5f, hb.y + hb.height * 0.5f});
  auto *panel = native->help_panel();
  REQUIRE(panel != nullptr);
  REQUIRE(panel->bounds().width == Catch::Approx(350.0f));
  REQUIRE(panel->bounds().height == Catch::Approx(551.0f));
  // Painting lays the text out; the wrapped copy is taller than the card, so
  // the wheel scrolls it and never past the end.
  const auto png = std::filesystem::temp_directory_path() / "beadsy-lab-help.png";
  REQUIRE(pulp::view::render_to_file(*native, 350, 551, png.string(), 1.0f));
  REQUIRE(panel->content_height() > 551.0f);
  pulp::view::MouseEvent wheel;
  wheel.is_wheel = true;
  wheel.scroll_delta_y = 100.0f;
  wheel.position = {175.0f, 275.0f};
  panel->on_mouse_event(wheel);
  REQUIRE(panel->scroll_y() == Catch::Approx(100.0f));
  wheel.scroll_delta_y = 1.0e6f;
  panel->on_mouse_event(wheel);
  REQUIRE(panel->scroll_y() < panel->content_height());
  // Escape goes through the framework overlay path, the same one a menu uses.
  pulp::view::View::dismiss_active_overlay(*native);
  REQUIRE(native->help_panel() == nullptr);
  // Reopen, then the close mark dismisses it too.
  native->show_help();
  REQUIRE(native->help_panel() != nullptr);
  pulp::view::MouseEvent press;
  press.is_down = true;
  press.position = {350.0f - 18.0f - 30.0f + 13.0f, 36.0f + 4.0f + 13.0f};
  native->help_panel()->on_mouse_event(press);
  REQUIRE(native->help_panel() == nullptr);
}
