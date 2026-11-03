#include <catch2/catch_test_macros.hpp>
#include <pulp/view/screenshot.hpp>
#include "beadsy.hpp"
#include "beadsy_editor.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>

TEST_CASE("Beadsy shipping editor keeps modulation focused and renders its menu") {
  beadsy::Beadsy processor;
  pulp::state::StateStore store;
  processor.set_state_store(&store);
  processor.define_parameters(store);
  auto editor = processor.create_view();
  auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
  REQUIRE(native != nullptr);
  REQUIRE(native->source_mode_control() != nullptr);
  REQUIRE_FALSE(native->source_mode_control()->visible());
  native->set_bounds({0, 0, 350, 634});
  native->layout_children();
  processor.set_lfo(beadsy::kPitch, {.enabled = true, .rate_hz = 2, .depth = 0.25f});
  auto *menu = native->show_modulation_context_menu(beadsy::kSize, {24, 120});
  REQUIRE(menu->layout().max_scroll == 0);
  const auto row = [&](std::string_view name) {
    const auto &items = menu->items();
    const auto it = std::find_if(items.begin(), items.end(),
                                [&](const auto &item) { return item.label == name; });
    REQUIRE(it != items.end());
    return static_cast<std::size_t>(it - items.begin());
  };
  const auto click = [&](std::string_view name) {
    const auto bounds = menu->layout().rows[row(name)];
    native->simulate_click({bounds.x + bounds.width * 0.5f,
                            bounds.y + bounds.height * 0.5f});
  };
  REQUIRE(menu->items()[row("LFO 1")].checked);
  REQUIRE_FALSE(menu->items()[row("LFO 1 -> Size")].checked);
  click("LFO 1 -> Size");
  REQUIRE(processor.lfo_target_enabled(beadsy::kSize, 0));
  REQUIRE(processor.lfo_target_enabled(beadsy::kPitch, 0));
  REQUIRE(menu->items().size() == 13);
  REQUIRE_FALSE(menu->items()[row("All targets…")].checked);

  const auto capture = [&](const char *name) {
    const auto shot = pulp::view::capture_view(*editor, 350, 634, 2.0f,
                                              pulp::view::ScreenshotBackend::coregraphics);
    REQUIRE(shot.ok);
    REQUIRE(shot.png.size() > 1000);
    if (const auto *directory = std::getenv("BEADSY_PRODUCT_MENU_CAPTURE_DIR")) {
      std::filesystem::create_directories(directory);
      std::ofstream out(std::filesystem::path(directory) / name, std::ios::binary);
      out.write(reinterpret_cast<const char *>(shot.png.data()),
                static_cast<std::streamsize>(shot.png.size()));
      REQUIRE(static_cast<bool>(out));
    }
    return shot.png;
  };
  const auto focused = capture("beadsy-size-focused.png");
  click("All targets…");
  REQUIRE(menu->items()[row("All targets…")].checked);
  REQUIRE(menu->items().size() > 13);
  REQUIRE(menu->items()[row("LFO 1 -> Pitch")].checked);
  const auto expanded = capture("beadsy-size-all-targets.png");
  REQUIRE(expanded != focused);
  menu = native->show_modulation_context_menu(beadsy::kPitch, {24, 120});
  REQUIRE(menu->items().size() == 13);
  REQUIRE(menu->items()[row("LFO 1")].checked);
  REQUIRE(menu->items()[row("LFO 1 -> Pitch")].checked);
  REQUIRE_FALSE(menu->items()[row("All targets…")].checked);
  capture("beadsy-pitch-focused.png");
}
