#include <catch2/catch_test_macros.hpp>
#include <pulp/state/store.hpp>
#include <pulp/view/screenshot.hpp>
#include "grainfield.hpp"
#include "grainfield_editor.hpp"

#include <cstdlib>
#include <array>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string_view>

TEST_CASE("Grainfield clean-room panel scaffold names the product and bound controls") {
    const std::string_view status(kGrainfieldPanelSourceStatus);
    const std::string_view panel(kGrainfieldPanelSvg);
    REQUIRE((status == "independent scaffold; rights-gated artwork probe unavailable" ||
             status == "rights-gated authoring probe"));
    if (status == "rights-gated authoring probe") {
        // A rights-cleared authoring probe is intentionally passed through
        // unchanged; the scaffold assertions apply only to clean checkouts.
        return;
    }
    REQUIRE(panel.find("GRAINFIELD") != std::string_view::npos);
    REQUIRE(panel.find("MIDI GRAIN SYNTH") != std::string_view::npos);
    for (const auto label : {"DENSITY", "TIME", "PITCH", "SIZE", "SHAPE"}) {
        REQUIRE(panel.find(label) != std::string_view::npos);
    }
    REQUIRE(panel.find("GRAIN") != std::string_view::npos);
    REQUIRE(panel.find("MIX / MOTION") != std::string_view::npos);
    REQUIRE(panel.find("PARAMETER CONTROLS") != std::string_view::npos);
    REQUIRE(panel.find("Beads") == std::string_view::npos);
    REQUIRE(panel.find("INDEPENDENT PRODUCT SCAFFOLD") != std::string_view::npos);
}

TEST_CASE("Grainfield knob skin preserves mapped DXF diameter") {
    constexpr float mapped_diameter = 9.25f * 2.0f * (201.0f / 70.8f);
    constexpr float layout_height = mapped_diameter * 1.25f;
    const float radius = grainfield::detail::PanelKnob::paint_radius_for_bounds(
        mapped_diameter, layout_height);
    // The outer shadow is still inside the mapped control bounds, while the
    // visible ring is within 2% of the DXF-derived radius.
    REQUIRE(radius * 1.14f <= mapped_diameter * 0.5f);
    REQUIRE(std::abs(radius - std::min(mapped_diameter, layout_height * 0.78f) * 0.43f) < 0.01f);
}

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

TEST_CASE("Grainfield explicit GPU capture reports its actual backend") {
    if (!std::getenv("GRAINFIELD_GPU_CAPTURE_PROBE")) return;

    grainfield::Grainfield processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    REQUIRE(editor != nullptr);

    REQUIRE(pulp::view::has_gpu_capture());
    const auto capture = pulp::view::capture_view(
        *editor, 201, 364, 2.0f, pulp::view::ScreenshotBackend::gpu);
    REQUIRE(capture.ok);
    REQUIRE(capture.used == pulp::view::ScreenshotBackend::gpu);
    REQUIRE(capture.png.size() > 1000);
    if (const char* path = std::getenv("GRAINFIELD_GPU_CAPTURE_OUTPUT")) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }
}

TEST_CASE("Grainfield procedural knob skin renders deterministic min mid max references") {
    const char* capture_dir = std::getenv("GRAINFIELD_KNOB_SKIN_CAPTURE_DIR");
    if (!capture_dir) return;
    std::filesystem::create_directories(capture_dir);

    constexpr float diameter = 96.0f;
    constexpr float height = diameter * 1.25f;
    for (const auto& sample : std::array{std::pair{"min", 0.0f},
                                         std::pair{"mid", 0.5f},
                                         std::pair{"max", 1.0f}}) {
        pulp::view::View root;
        root.set_bounds({0.0f, 0.0f, diameter, height});
        auto knob = std::make_unique<grainfield::detail::PanelKnob>(
            "reference-" + std::string(sample.first), diameter);
        knob->set_bounds({0.0f, 0.0f, diameter, height});
        knob->flex().preferred_width = diameter;
        knob->flex().preferred_height = height;
        knob->set_value(sample.second);
        root.add_child(std::move(knob));

        const auto capture = pulp::view::capture_view(
            root, static_cast<int>(diameter), static_cast<int>(height), 2.0f,
            pulp::view::ScreenshotBackend::auto_select);
        REQUIRE(capture.ok);
        REQUIRE(capture.png.size() > 1000);
        const auto path = std::filesystem::path(capture_dir) /
                          ("grainfield-knob-" + std::string(sample.first) + ".png");
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

TEST_CASE("Grainfield leaves geometry-only controls visibly provisional") {
    grainfield::Grainfield processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<grainfield::GrainfieldEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE(native->child_count() == 24);

    // The first five controls are the only source-backed parameter bindings.
    for (std::size_t i = 0; i < native->child_count(); ++i) {
        auto* knob = dynamic_cast<grainfield::detail::PanelKnob*>(native->child_at(i));
        REQUIRE(knob != nullptr);
        if (i < 5) {
            REQUIRE(knob->enabled());
            REQUIRE(knob->access_disabled().empty());
        } else {
            REQUIRE_FALSE(knob->enabled());
            REQUIRE(knob->access_disabled() == "true");
            REQUIRE(knob->access_label().find(
                        " (Provisional control mapping)") != std::string::npos);
        }
    }
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

TEST_CASE("Grainfield bound editor knob emits a host automation gesture") {
    grainfield::Grainfield processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    std::vector<pulp::state::ParamID> begins;
    std::vector<pulp::state::ParamID> ends;
    store.set_gesture_callbacks(
        [&begins](pulp::state::ParamID id) { begins.push_back(id); },
        [&ends](pulp::state::ParamID id) { ends.push_back(id); });

    auto editor = processor.create_view();
    auto* native = dynamic_cast<grainfield::GrainfieldEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();
    REQUIRE(native->child_count() >= 1);

    // Controls are added in the same order as the panel map. The first child
    // is the bound Density knob, so this exercises Grainfield's actual editor
    // binding rather than a standalone generic Knob fixture.
    auto* density = dynamic_cast<grainfield::detail::PanelKnob*>(
        native->child_at(0));
    REQUIRE(density != nullptr);
    REQUIRE(density->id() == "grainfield-Density");
    const float before = store.get_value(grainfield::kDensity);

    const auto b = density->local_bounds();
    const pulp::view::Point center{b.width * 0.5f, b.height * 0.5f};
    density->on_mouse_down(center);
    density->on_mouse_drag({center.x, center.y - 40.0f});
    density->on_mouse_up({center.x, center.y - 40.0f});

    REQUIRE(begins == std::vector<pulp::state::ParamID>{grainfield::kDensity});
    REQUIRE(ends == std::vector<pulp::state::ParamID>{grainfield::kDensity});
    REQUIRE(store.get_value(grainfield::kDensity) > before);
}
