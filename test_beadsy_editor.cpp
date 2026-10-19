#include <catch2/catch_test_macros.hpp>
#include <pulp/state/store.hpp>
#include <pulp/view/screenshot.hpp>
#include "beadsy.hpp"
#include "beadsy_editor.hpp"

#include <cstdlib>
#include <array>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string_view>

TEST_CASE("Beadsy clean-room panel scaffold names the product and bound controls") {
    const std::string_view status(kBeadsyPanelSourceStatus);
    const std::string_view panel(kBeadsyPanelSvg);
    REQUIRE((status == "independent scaffold; rights-gated artwork probe unavailable" ||
             status == "rights-gated authoring probe"));
    if (status == "rights-gated authoring probe") {
        // A rights-cleared authoring probe is intentionally passed through
        // unchanged; the scaffold assertions apply only to clean checkouts.
        return;
    }
    REQUIRE(panel.find("BEADSY") != std::string_view::npos);
    REQUIRE(panel.find("MIDI GRAIN SYNTH") != std::string_view::npos);
    for (const auto label : {"DENSITY", "TIME", "PITCH", "SIZE", "SHAPE"}) {
        REQUIRE(panel.find(label) != std::string_view::npos);
    }
    REQUIRE(panel.find("GRAIN") != std::string_view::npos);
    REQUIRE(panel.find("MIX / MOTION") != std::string_view::npos);
    REQUIRE(panel.find("PARAMETER CONTROLS") != std::string_view::npos);
    // Beadsy intentionally begins with the lineage name; reject a bare upstream label.
    REQUIRE(panel.find("Beads ") == std::string_view::npos);
    REQUIRE(panel.find("INDEPENDENT PRODUCT SCAFFOLD") != std::string_view::npos);
}

TEST_CASE("Beadsy knob skin preserves mapped DXF diameter") {
    constexpr float mapped_diameter = 9.25f * 2.0f * (201.0f / 70.8f);
    constexpr float layout_height = mapped_diameter * 1.25f;
    const float radius = beadsy::detail::PanelKnob::paint_radius_for_bounds(
        mapped_diameter, layout_height);
    // The outer shadow is still inside the mapped control bounds, while the
    // visible ring is within 2% of the DXF-derived radius.
    REQUIRE(radius * 1.14f <= mapped_diameter * 0.5f);
    REQUIRE(std::abs(radius - std::min(mapped_diameter, layout_height * 0.78f) * 0.43f) < 0.01f);
}

TEST_CASE("Beadsy source control exposes synth and live input modes") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE(native->source_mode_control() != nullptr);
    REQUIRE(native->source_mode_control()->mode() == 0);
    native->source_mode_control()->on_mouse_down({});
    REQUIRE(native->source_mode_control()->mode() == 1);
    REQUIRE(store.get_value(beadsy::kMode) == 1.0f);
    native->source_mode_control()->on_mouse_down({});
    REQUIRE(native->source_mode_control()->mode() == 0);
    REQUIRE(store.get_value(beadsy::kMode) == 0.0f);
}

TEST_CASE("Beadsy knob skin keeps measured cap and pointer proportions") {
    // Ratios are taken from the local straight-on material reference and are
    // checked as procedural geometry only; the reference image is never
    // embedded in the product.
    const float cap_to_outer = beadsy::detail::PanelKnob::kCapRadiusFraction /
                               beadsy::detail::PanelKnob::kOuterRadiusFraction;
    REQUIRE(cap_to_outer >= 0.53f);
    REQUIRE(cap_to_outer <= 0.61f);
    const float pointer_inner_to_outer =
        beadsy::detail::PanelKnob::kPointerInnerHalfWidthFraction /
        beadsy::detail::PanelKnob::kOuterRadiusFraction;
    REQUIRE(pointer_inner_to_outer >= 0.08f);
    REQUIRE(pointer_inner_to_outer <= 0.10f);
    REQUIRE(beadsy::detail::PanelKnob::kPointerOuterHalfWidthFraction <
            beadsy::detail::PanelKnob::kPointerInnerHalfWidthFraction);
}

TEST_CASE("Beadsy native editor captures a non-empty faithful panel frame") {
    beadsy::Beadsy processor;
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
    if (const char* path = std::getenv("BEADSY_EDITOR_CAPTURE")) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }
}

TEST_CASE("Beadsy explicit GPU capture reports its actual backend") {
    if (!std::getenv("BEADSY_GPU_CAPTURE_PROBE")) return;

    beadsy::Beadsy processor;
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
    if (const char* path = std::getenv("BEADSY_GPU_CAPTURE_OUTPUT")) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }
}

TEST_CASE("Beadsy procedural knob skin renders deterministic min mid max references") {
    const char* capture_dir = std::getenv("BEADSY_KNOB_SKIN_CAPTURE_DIR");
    if (!capture_dir) return;
    std::filesystem::create_directories(capture_dir);

    constexpr float diameter = 96.0f;
    constexpr float height = diameter * 1.25f;
    for (const auto& sample : std::array{std::pair{"min", 0.0f},
                                         std::pair{"mid", 0.5f},
                                         std::pair{"max", 1.0f}}) {
        pulp::view::View root;
        root.set_bounds({0.0f, 0.0f, diameter, height});
        auto knob = std::make_unique<beadsy::detail::PanelKnob>(
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
                          ("beadsy-knob-" + std::string(sample.first) + ".png");
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }
}

TEST_CASE("Beadsy editor exposes a right-click modulation target seam") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE_FALSE(native->modulation_context_target().has_value());
    native->open_modulation_context(beadsy::kDensity);
    REQUIRE(native->modulation_context_target() == beadsy::kDensity);
    native->clear_modulation_context();
    REQUIRE_FALSE(native->modulation_context_target().has_value());
}

TEST_CASE("Beadsy binds source-backed auxiliary controls and labels ports") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE(native->child_count() == 26);

    // Five primary controls plus seven PCB-net-backed auxiliary pots are
    // automatable. Ten remaining regions are input/control ports; output jacks
    // are intentionally omitted because host outputs are not patchable UI.
    for (std::size_t i = 0; i < 12; ++i) {
        auto* knob = dynamic_cast<beadsy::detail::PanelKnob*>(native->child_at(i));
        REQUIRE(knob != nullptr);
        REQUIRE(knob->enabled());
        REQUIRE(knob->access_disabled().empty());
    }
    const char* ports[] = {"TIME CV (J1)", "SIZE CV (J2)", "SHAPE CV (J3)",
                           "PITCH CV (J4)", "ASSIGN CV (J5)", "DENSITY CV (J6)",
                           "AUDIO IN L (J7)", "AUDIO IN R (J8)",
                           "FREEZE GATE (J9)", "SEED GATE (J10)"};
    for (std::size_t i = 0; i < 10; ++i) {
        auto* port = dynamic_cast<beadsy::PortRegion*>(native->child_at(i + 12));
        REQUIRE(port != nullptr);
        REQUIRE(port->access_label().find(ports[i]) != std::string::npos);
        REQUIRE(port->access_label().find("non-automatable") != std::string::npos);
    }
}

TEST_CASE("Beadsy Seed Mode button cycles the source-backed LED states") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);

    beadsy::SeedModeControl* seed = nullptr;
    for (std::size_t i = 0; i < native->child_count(); ++i)
        if ((seed = dynamic_cast<beadsy::SeedModeControl*>(native->child_at(i)))) break;
    REQUIRE(seed != nullptr);
    REQUIRE(seed->mode() == 0);
    REQUIRE(seed->access_label().find("off") != std::string::npos);

    seed->on_mouse_down({});
    REQUIRE(store.get_value(beadsy::kSeedMode) == 1.0f);
    REQUIRE(seed->mode() == 1);
    REQUIRE(seed->access_label().find("on") != std::string::npos);
    seed->on_mouse_down({});
    REQUIRE(store.get_value(beadsy::kSeedMode) == 2.0f);
    REQUIRE(seed->mode() == 2);
    REQUIRE(seed->access_label().find("locked") != std::string::npos);
    seed->on_mouse_down({});
    REQUIRE(store.get_value(beadsy::kSeedMode) == 0.0f);
    REQUIRE(seed->mode() == 0);
}

TEST_CASE("Beadsy top Freeze control is source-backed and laid out beside Seed") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    beadsy::FreezeControl* freeze = nullptr;
    beadsy::SeedModeControl* seed = nullptr;
    for (std::size_t i = 0; i < native->child_count(); ++i) {
        if (!freeze) freeze = dynamic_cast<beadsy::FreezeControl*>(native->child_at(i));
        if (!seed) seed = dynamic_cast<beadsy::SeedModeControl*>(native->child_at(i));
    }
    REQUIRE(freeze != nullptr);
    REQUIRE(seed != nullptr);
    REQUIRE_FALSE(freeze->pressed());
    REQUIRE(freeze->access_label().find("off") != std::string::npos);
    REQUIRE(seed->bounds().y < 80.0f);
    REQUIRE(freeze->bounds().y < 80.0f);
    REQUIRE(freeze->bounds().x < seed->bounds().x);

    freeze->on_mouse_down({});
    REQUIRE(store.get_value(beadsy::kFreeze) == 1.0f);
    REQUIRE(freeze->pressed());
    REQUIRE(freeze->access_label().find("on") != std::string::npos);
    freeze->on_mouse_down({});
    REQUIRE(store.get_value(beadsy::kFreeze) == 0.0f);
    REQUIRE_FALSE(freeze->pressed());
}

TEST_CASE("Beadsy modulation context menu updates and resets the selected LFO") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);

    auto* menu = native->show_modulation_context_menu(
        beadsy::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    REQUIRE(native->modulation_context_target() == beadsy::kDensity);
    REQUIRE(menu->items().size() == 7);
    REQUIRE_FALSE(menu->items()[0].enabled);
    REQUIRE(menu->items()[0].label == "LFO → Density");
    REQUIRE(menu->items()[1].label == "Enable LFO");

    menu->on_close(1);
    REQUIRE(processor.lfo(beadsy::kDensity).enabled);
    REQUIRE(processor.lfo(beadsy::kDensity).depth > 0.0f);
    REQUIRE_FALSE(native->modulation_context_target().has_value());

    menu = native->show_modulation_context_menu(
        beadsy::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    REQUIRE(menu->items()[1].label == "LFO Enabled");
    REQUIRE(menu->items()[1].checked);
    const auto initial_rate = processor.lfo(beadsy::kDensity).rate_hz;
    const auto initial_depth = processor.lfo(beadsy::kDensity).depth;
    menu->on_close(4);
    REQUIRE(processor.lfo(beadsy::kDensity).rate_hz > initial_rate);
    menu = native->show_modulation_context_menu(
        beadsy::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    menu->on_close(6);
    REQUIRE(processor.lfo(beadsy::kDensity).depth > initial_depth);
    menu = native->show_modulation_context_menu(
        beadsy::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    menu->on_close(2);
    REQUIRE_FALSE(processor.lfo(beadsy::kDensity).enabled);
    REQUIRE(processor.lfo(beadsy::kDensity).depth == 0.0f);
    REQUIRE_FALSE(native->modulation_context_target().has_value());
}

TEST_CASE("Beadsy bound editor knob emits a host automation gesture") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    std::vector<pulp::state::ParamID> begins;
    std::vector<pulp::state::ParamID> ends;
    store.set_gesture_callbacks(
        [&begins](pulp::state::ParamID id) { begins.push_back(id); },
        [&ends](pulp::state::ParamID id) { ends.push_back(id); });

    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();
    REQUIRE(native->child_count() >= 1);

    // Controls are added in the same order as the panel map. The first child
    // is the bound Density knob, so this exercises Beadsy's actual editor
    // binding rather than a standalone generic Knob fixture.
    auto* density = dynamic_cast<beadsy::detail::PanelKnob*>(
        native->child_at(0));
    REQUIRE(density != nullptr);
    REQUIRE(density->id() == "beadsy-Density");
    const float before = store.get_value(beadsy::kDensity);

    const auto b = density->local_bounds();
    const pulp::view::Point center{b.width * 0.5f, b.height * 0.5f};
    density->on_mouse_down(center);
    density->on_mouse_drag({center.x, center.y - 40.0f});
    density->on_mouse_up({center.x, center.y - 40.0f});

    REQUIRE(begins == std::vector<pulp::state::ParamID>{beadsy::kDensity});
    REQUIRE(ends == std::vector<pulp::state::ParamID>{beadsy::kDensity});
    REQUIRE(store.get_value(beadsy::kDensity) > before);
}
