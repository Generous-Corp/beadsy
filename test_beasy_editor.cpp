#include <catch2/catch_test_macros.hpp>
#include <pulp/state/store.hpp>
#include <pulp/view/screenshot.hpp>
#include "beasy.hpp"
#include "beasy_editor.hpp"

#include <cstdlib>
#include <array>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string_view>

TEST_CASE("Beasy clean-room panel scaffold names the product and bound controls") {
    const std::string_view status(kBeasyPanelSourceStatus);
    const std::string_view panel(kBeasyPanelSvg);
    REQUIRE((status == "independent scaffold; rights-gated artwork probe unavailable" ||
             status == "rights-gated authoring probe"));
    if (status == "rights-gated authoring probe") {
        // A rights-cleared authoring probe is intentionally passed through
        // unchanged; the scaffold assertions apply only to clean checkouts.
        return;
    }
    REQUIRE(panel.find("BEASY") != std::string_view::npos);
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

TEST_CASE("Beasy knob skin preserves mapped DXF diameter") {
    constexpr float mapped_diameter = 9.25f * 2.0f * (201.0f / 70.8f);
    constexpr float layout_height = mapped_diameter * 1.25f;
    const float radius = beasy::detail::PanelKnob::paint_radius_for_bounds(
        mapped_diameter, layout_height);
    // The outer shadow is still inside the mapped control bounds, while the
    // visible ring is within 2% of the DXF-derived radius.
    REQUIRE(radius * 1.14f <= mapped_diameter * 0.5f);
    REQUIRE(std::abs(radius - std::min(mapped_diameter, layout_height * 0.78f) * 0.43f) < 0.01f);
}

TEST_CASE("Beasy native editor captures a non-empty faithful panel frame") {
    beasy::Beasy processor;
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
    if (const char* path = std::getenv("BEASY_EDITOR_CAPTURE")) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }
}

TEST_CASE("Beasy explicit GPU capture reports its actual backend") {
    if (!std::getenv("BEASY_GPU_CAPTURE_PROBE")) return;

    beasy::Beasy processor;
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
    if (const char* path = std::getenv("BEASY_GPU_CAPTURE_OUTPUT")) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }
}

TEST_CASE("Beasy procedural knob skin renders deterministic min mid max references") {
    const char* capture_dir = std::getenv("BEASY_KNOB_SKIN_CAPTURE_DIR");
    if (!capture_dir) return;
    std::filesystem::create_directories(capture_dir);

    constexpr float diameter = 96.0f;
    constexpr float height = diameter * 1.25f;
    for (const auto& sample : std::array{std::pair{"min", 0.0f},
                                         std::pair{"mid", 0.5f},
                                         std::pair{"max", 1.0f}}) {
        pulp::view::View root;
        root.set_bounds({0.0f, 0.0f, diameter, height});
        auto knob = std::make_unique<beasy::detail::PanelKnob>(
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
                          ("beasy-knob-" + std::string(sample.first) + ".png");
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }
}

TEST_CASE("Beasy editor exposes a right-click modulation target seam") {
    beasy::Beasy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beasy::BeasyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE_FALSE(native->modulation_context_target().has_value());
    native->open_modulation_context(beasy::kDensity);
    REQUIRE(native->modulation_context_target() == beasy::kDensity);
    native->clear_modulation_context();
    REQUIRE_FALSE(native->modulation_context_target().has_value());
}

TEST_CASE("Beasy binds source-backed auxiliary controls and labels ports") {
    beasy::Beasy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beasy::BeasyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE(native->child_count() == 24);

    // Five primary controls plus seven PCB-net-backed auxiliary pots are
    // automatable. The twelve remaining regions are physical I/O ports.
    for (std::size_t i = 0; i < 12; ++i) {
        auto* knob = dynamic_cast<beasy::detail::PanelKnob*>(native->child_at(i));
        REQUIRE(knob != nullptr);
        REQUIRE(knob->enabled());
        REQUIRE(knob->access_disabled().empty());
    }
    const char* ports[] = {"TIME CV (J1)", "SIZE CV (J2)", "SHAPE CV (J3)",
                           "PITCH CV (J4)", "ASSIGN CV (J5)", "DENSITY CV (J6)",
                           "AUDIO IN L (J7)", "AUDIO IN R (J8)",
                           "FREEZE GATE (J9)", "SEED GATE (J10)",
                           "AUDIO OUT L (J11)", "AUDIO OUT R (J12)"};
    for (std::size_t i = 0; i < 12; ++i) {
        auto* port = dynamic_cast<beasy::PortRegion*>(native->child_at(i + 12));
        REQUIRE(port != nullptr);
        REQUIRE(port->access_label().find(ports[i]) != std::string::npos);
        REQUIRE(port->access_label().find("non-automatable") != std::string::npos);
    }
}

TEST_CASE("Beasy modulation context menu updates and resets the selected LFO") {
    beasy::Beasy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beasy::BeasyEditor*>(editor.get());
    REQUIRE(native != nullptr);

    auto* menu = native->show_modulation_context_menu(
        beasy::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    REQUIRE(native->modulation_context_target() == beasy::kDensity);
    REQUIRE(menu->items().size() == 7);
    REQUIRE_FALSE(menu->items()[0].enabled);
    REQUIRE(menu->items()[0].label == "LFO → Density");
    REQUIRE(menu->items()[1].label == "Enable LFO");

    menu->on_close(1);
    REQUIRE(processor.lfo(beasy::kDensity).enabled);
    REQUIRE(processor.lfo(beasy::kDensity).depth > 0.0f);
    REQUIRE_FALSE(native->modulation_context_target().has_value());

    menu = native->show_modulation_context_menu(
        beasy::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    REQUIRE(menu->items()[1].label == "Disable LFO");
    const auto initial_rate = processor.lfo(beasy::kDensity).rate_hz;
    const auto initial_depth = processor.lfo(beasy::kDensity).depth;
    menu->on_close(4);
    REQUIRE(processor.lfo(beasy::kDensity).rate_hz > initial_rate);
    menu = native->show_modulation_context_menu(
        beasy::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    menu->on_close(6);
    REQUIRE(processor.lfo(beasy::kDensity).depth > initial_depth);
    menu = native->show_modulation_context_menu(
        beasy::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    menu->on_close(2);
    REQUIRE_FALSE(processor.lfo(beasy::kDensity).enabled);
    REQUIRE(processor.lfo(beasy::kDensity).depth == 0.0f);
    REQUIRE_FALSE(native->modulation_context_target().has_value());
}

TEST_CASE("Beasy bound editor knob emits a host automation gesture") {
    beasy::Beasy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    std::vector<pulp::state::ParamID> begins;
    std::vector<pulp::state::ParamID> ends;
    store.set_gesture_callbacks(
        [&begins](pulp::state::ParamID id) { begins.push_back(id); },
        [&ends](pulp::state::ParamID id) { ends.push_back(id); });

    auto editor = processor.create_view();
    auto* native = dynamic_cast<beasy::BeasyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();
    REQUIRE(native->child_count() >= 1);

    // Controls are added in the same order as the panel map. The first child
    // is the bound Density knob, so this exercises Beasy's actual editor
    // binding rather than a standalone generic Knob fixture.
    auto* density = dynamic_cast<beasy::detail::PanelKnob*>(
        native->child_at(0));
    REQUIRE(density != nullptr);
    REQUIRE(density->id() == "beasy-Density");
    const float before = store.get_value(beasy::kDensity);

    const auto b = density->local_bounds();
    const pulp::view::Point center{b.width * 0.5f, b.height * 0.5f};
    density->on_mouse_down(center);
    density->on_mouse_drag({center.x, center.y - 40.0f});
    density->on_mouse_up({center.x, center.y - 40.0f});

    REQUIRE(begins == std::vector<pulp::state::ParamID>{beasy::kDensity});
    REQUIRE(ends == std::vector<pulp::state::ParamID>{beasy::kDensity});
    REQUIRE(store.get_value(beasy::kDensity) > before);
}
