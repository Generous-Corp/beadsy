#include <catch2/catch_test_macros.hpp>
#include <pulp/state/store.hpp>
#include <pulp/view/screenshot.hpp>
#include "grainfield.hpp"

#include <cstdlib>
#include <fstream>

TEST_CASE("Grainfield native editor captures a non-empty faithful panel frame") {
    grainfield::Grainfield processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    auto editor = processor.create_view();
    REQUIRE(editor != nullptr);
    const auto capture = pulp::view::capture_view(
        *editor, 201, 364, 2.0f, pulp::view::ScreenshotBackend::auto_select);
    REQUIRE(capture.ok);
    const bool has_frame = capture.used != pulp::view::ScreenshotBackend::default_backend || !capture.png.empty();
    REQUIRE(has_frame);
    REQUIRE(capture.png.size() > 1000);
    if (const char* path = std::getenv("GRAINFIELD_EDITOR_CAPTURE")) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }
}

TEST_CASE("Grainfield editor exposes a right-click modulation target seam") {
    grainfield::Grainfield processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<grainfield::GrainfieldEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE_FALSE(native->modulation_context_target().has_value());
    native->open_modulation_context(grainfield::kDensity);
    REQUIRE(native->modulation_context_target() == grainfield::kDensity);
    native->clear_modulation_context();
    REQUIRE_FALSE(native->modulation_context_target().has_value());
}

TEST_CASE("Grainfield modulation context menu updates and resets the selected LFO") {
    grainfield::Grainfield processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<grainfield::GrainfieldEditor*>(editor.get());
    REQUIRE(native != nullptr);

    auto* menu = native->show_modulation_context_menu(
        grainfield::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    REQUIRE(native->modulation_context_target() == grainfield::kDensity);
    REQUIRE(menu->items().size() == 7);
    REQUIRE_FALSE(menu->items()[0].enabled);
    REQUIRE(menu->items()[0].label == "LFO → Density");
    REQUIRE(menu->items()[1].label == "Enable LFO");

    menu->on_close(1);
    REQUIRE(processor.lfo(grainfield::kDensity).enabled);
    REQUIRE(processor.lfo(grainfield::kDensity).depth > 0.0f);
    REQUIRE_FALSE(native->modulation_context_target().has_value());

    menu = native->show_modulation_context_menu(
        grainfield::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    REQUIRE(menu->items()[1].label == "Disable LFO");
    const auto initial_rate = processor.lfo(grainfield::kDensity).rate_hz;
    const auto initial_depth = processor.lfo(grainfield::kDensity).depth;
    menu->on_close(4);
    REQUIRE(processor.lfo(grainfield::kDensity).rate_hz > initial_rate);
    menu = native->show_modulation_context_menu(
        grainfield::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    menu->on_close(6);
    REQUIRE(processor.lfo(grainfield::kDensity).depth > initial_depth);
    menu = native->show_modulation_context_menu(
        grainfield::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    menu->on_close(2);
    REQUIRE_FALSE(processor.lfo(grainfield::kDensity).enabled);
    REQUIRE(processor.lfo(grainfield::kDensity).depth == 0.0f);
    REQUIRE_FALSE(native->modulation_context_target().has_value());
}
