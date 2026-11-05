#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <pulp/state/store.hpp>
#include <pulp/view/screenshot.hpp>
#include <pulp/view/pointer_dispatch.hpp>
#include <pulp/view/overlay_dismissal.hpp>
#include "beadsy.hpp"
#include "beadsy_editor.hpp"

#include <cstdlib>
#include <array>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string_view>

#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
#include <array>
#endif

TEST_CASE("Beadsy clean-room panel scaffold names the product and bound controls") {
    const std::string_view status(kBeadsyPanelSourceStatus);
    const std::string_view panel(kBeadsyPanelSvg);
  REQUIRE((status == "independent scaffold; panel artwork opt-in disabled or unavailable" ||
           status == "reviewed Beads panel artwork (private opt-in)" ||
           status == "authorized Beads panel artwork" ||
           status == "authorized Beads panel artwork (rasterized Illustrator import)"));
    if (status == "authorized Beads panel artwork" ||
        status == "authorized Beads panel artwork (rasterized Illustrator import)" ||
        status == "reviewed Beads panel artwork (private opt-in)") {
        // A rights-cleared or private comparison probe is intentionally passed
        // through unchanged; scaffold assertions apply only to clean checkouts.
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

TEST_CASE("Beadsy source control exposes synth and host-routed input modes") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE(native->source_mode_control() != nullptr);
    REQUIRE(native->source_mode_control()->mode() == 0);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();
    auto *source = native->source_mode_control();
    const auto b = source->bounds();
    native->simulate_click({b.x + b.width * 0.5f,
                             b.y + b.height * 0.5f});
    REQUIRE(source->is_open());
    pulp::view::KeyEvent down{.key = pulp::view::KeyCode::down};
    REQUIRE(source->on_key_event(down));
    pulp::view::KeyEvent enter{.key = pulp::view::KeyCode::enter};
    REQUIRE(source->on_key_event(enter));
    REQUIRE(source->mode() == 1);
    REQUIRE(store.get_value(beadsy::kMode) == 1.0f);
    native->simulate_click({b.x + b.width * 0.5f,
                             b.y + b.height * 0.5f});
    REQUIRE(source->is_open());
    REQUIRE(source->on_key_event(down));
    REQUIRE(source->on_key_event(enter));
    REQUIRE(source->mode() == 2);
    REQUIRE(store.get_value(beadsy::kMode) == 2.0f);
    native->simulate_click({b.x + b.width * 0.5f,
                             b.y + b.height * 0.5f});
    REQUIRE(source->is_open());
    pulp::view::KeyEvent up{.key = pulp::view::KeyCode::up};
    REQUIRE(source->on_key_event(up));
    REQUIRE(source->on_key_event(up));
    REQUIRE(source->on_key_event(enter));
    REQUIRE(source->mode() == 0);
    REQUIRE(store.get_value(beadsy::kMode) == 0.0f);
}

TEST_CASE("Beadsy quality button cycles and follows restored state") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE(native->quality_control() != nullptr);
    REQUIRE(native->quality_control()->quality() == 3);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    const auto qb = native->quality_control()->bounds();
    const pulp::view::Point center{qb.x + qb.width * 0.5f,
                                   qb.y + qb.height * 0.5f};
    REQUIRE(native->hit_test(center) == native->quality_control());
    native->simulate_click(center);
    REQUIRE(store.get_value(beadsy::kQuality) == 0.0f);
    REQUIRE(native->quality_control()->quality() == 0);

    store.set_value(beadsy::kQuality, 2.0f);
    REQUIRE(native->quality_control()->quality() == 2);
    REQUIRE(native->quality_control()->access_label().find("7-bit") !=
            std::string::npos);
}

TEST_CASE("Beadsy quality and CV-assign buttons occupy separate original stations") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    auto* quality = native->quality_control();
    auto* assign = native->assign_control();
    REQUIRE(quality != nullptr);
    REQUIRE(assign != nullptr);
    // Quality [A] is beneath DENSITY; the CV destination [M] is left of the
    // four lower utility pots. They must not share the old guessed station.
    REQUIRE(quality->bounds().x > 70.0f);
    REQUIRE(quality->bounds().y > 90.0f);
    REQUIRE(assign->bounds().x < 30.0f);
    REQUIRE(assign->bounds().y > 180.0f);
    const auto before = assign->destination();
    const auto ab = assign->bounds();
    const pulp::view::Point center{ab.x + ab.width * 0.5f,
                                   ab.y + ab.height * 0.5f};
    REQUIRE(native->hit_test(center) == assign);
    native->simulate_click(center);
    REQUIRE(assign->destination() == (before + 1) % 3);
    REQUIRE(assign->access_label().find("Dry/Wet") != std::string::npos);
}

TEST_CASE("Beadsy CV assign station is a button with visible state feedback") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    auto* button = native->assign_control();
    REQUIRE(button != nullptr);
    REQUIRE(button->access_role() == pulp::view::View::AccessRole::button);
    const auto bounds = button->bounds();
    const pulp::view::Point center{bounds.x + bounds.width * 0.5f,
                                   bounds.y + bounds.height * 0.5f};
    REQUIRE(native->hit_test(center) == button);
    const auto before = pulp::view::capture_view(
        *editor, 201, 364, 1.0f, pulp::view::ScreenshotBackend::auto_select);
    REQUIRE(before.ok);
    const auto destination = button->destination();
    native->simulate_click(center);
    REQUIRE(button->destination() == (destination + 1) % 3);
    const auto after = pulp::view::capture_view(
        *editor, 201, 364, 1.0f, pulp::view::ScreenshotBackend::auto_select);
    REQUIRE(after.ok);
    REQUIRE(after.png != before.png);
}

TEST_CASE("Beadsy source control clamps values to the declared bus topology") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE(native->source_mode_control() != nullptr);

    native->source_mode_control()->set_mode(99);
    REQUIRE(native->source_mode_control()->mode() == beadsy::kAvailableSourceModeMax);
    native->source_mode_control()->set_mode(-99);
    REQUIRE(native->source_mode_control()->mode() == 0);
}

TEST_CASE("Beadsy source control is a real dropdown with keyboard dismissal") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    auto* source = native->source_mode_control();
    REQUIRE(source != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();
    REQUIRE(source->items().size() == 3);
    REQUIRE_FALSE(source->is_open());

    pulp::view::MouseEvent press;
    press.phase = pulp::view::MousePhase::press;
    press.is_down = true;
    press.position = {source->bounds().width * 0.5f,
                      source->bounds().height * 0.5f};
    source->on_mouse_event(press);
    REQUIRE(source->is_open());

    pulp::view::KeyEvent down{.key = pulp::view::KeyCode::down};
    REQUIRE(source->on_key_event(down));
    pulp::view::KeyEvent enter{.key = pulp::view::KeyCode::enter};
    REQUIRE(source->on_key_event(enter));
    REQUIRE_FALSE(source->is_open());
    REQUIRE(source->mode() == 1);
    REQUIRE(store.get_value(beadsy::kMode) == 1.0f);

    source->set_mode(0);
    store.set_value(beadsy::kMode, 0.0f);
    source->on_mouse_event(press);
    REQUIRE(source->is_open());
    pulp::view::KeyEvent escape{.key = pulp::view::KeyCode::escape};
    REQUIRE(source->on_key_event(escape));
    REQUIRE_FALSE(source->is_open());
    REQUIRE(source->mode() == 0);
    REQUIRE(store.get_value(beadsy::kMode) == 0.0f);
}

TEST_CASE("Beadsy source dropdown is reachable through root pointer dispatch") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();
    auto* source = native->source_mode_control();
    REQUIRE(source != nullptr);
    REQUIRE_FALSE(source->is_open());
    const auto b = source->bounds();
    native->simulate_click({b.x + b.width * 0.5f,
                             b.y + b.height * 0.5f});
    REQUIRE(source->is_open());
    pulp::view::KeyEvent down{.key = pulp::view::KeyCode::down};
    REQUIRE(source->on_key_event(down));
    pulp::view::KeyEvent enter{.key = pulp::view::KeyCode::enter};
    REQUIRE(source->on_key_event(enter));
    REQUIRE_FALSE(source->is_open());
    REQUIRE(source->mode() == 1);
    REQUIRE(store.get_value(beadsy::kMode) == 1.0f);
}

#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
namespace {
class EditorRouteCatalog final : public pulp::host::AudioRouteCatalog {
public:
    std::vector<pulp::host::AudioRouteInfo> enumerate() const override { return routes; }
    void set_change_callback(ChangeCallback callback) override { callback_ = std::move(callback); }
    bool connect(std::string_view stable_id, std::size_t bus_index) override {
        return connect_channel(stable_id, bus_index, 0);
    }
    bool connect_channel(std::string_view stable_id, std::size_t bus_index,
                         std::size_t channel) override {
        if (bus_index > 1 || channel > 1 || stable_id != "track:piano")
            return false;
        selected_[bus_index] = std::string(stable_id);
        selected_channels_[bus_index][channel] = std::string(stable_id);
        return true;
    }
    std::string connected_route(std::size_t bus_index) const override {
        return bus_index < selected_.size() ? selected_[bus_index] : std::string{};
    }
    std::string connected_route_channel(std::size_t bus_index,
                                        std::size_t channel) const override {
        return bus_index < selected_channels_.size() && channel < 2
                   ? selected_channels_[bus_index][channel]
                   : std::string{};
    }
    void notify() { if (callback_) callback_(); }
    void clear_selection() {
        selected_.fill({});
        for (auto& channels : selected_channels_)
            channels.fill({});
    }
    std::vector<pulp::host::AudioRouteInfo> routes{
        {"input:mic", "Mic", pulp::host::AudioRouteKind::PhysicalInput, 1, true},
        {"track:piano", "Piano", pulp::host::AudioRouteKind::Track, 2, true},
        {"bus:offline", "Offline Bus", pulp::host::AudioRouteKind::Bus, 2, false},
    };
private:
    ChangeCallback callback_;
    std::array<std::string, 2> selected_{};
    std::array<std::array<std::string, 2>, 2> selected_channels_{};
};

class DistinctBusRouteCatalog final : public pulp::host::AudioRouteCatalog {
public:
    std::vector<pulp::host::AudioRouteInfo> enumerate() const override {
        return routes;
    }
    void set_change_callback(ChangeCallback callback) override {
        callback_ = std::move(callback);
    }
    bool connect(std::string_view stable_id, std::size_t bus_index) override {
        return connect_channel(stable_id, bus_index, 0);
    }
    bool connect_channel(std::string_view stable_id, std::size_t bus_index,
                         std::size_t channel) override {
        bool known_route = false;
        for (const auto &route : routes)
            known_route = known_route || route.stable_id == stable_id;
        if (bus_index > 1 || channel > 1 || !known_route)
            return false;
        selected_[bus_index] = std::string(stable_id);
        selected_channels_[bus_index][channel] = std::string(stable_id);
        return true;
    }
    std::string connected_route(std::size_t bus_index) const override {
        return bus_index < selected_.size() ? selected_[bus_index]
                                            : std::string{};
    }
    std::string connected_route_channel(std::size_t bus_index,
                                        std::size_t channel) const override {
        return bus_index < selected_channels_.size() && channel < 2
                   ? selected_channels_[bus_index][channel]
                   : std::string{};
    }
    void notify() {
        if (callback_)
            callback_();
    }
    std::vector<pulp::host::AudioRouteInfo> routes{
        {"main:left", "Main Left", pulp::host::AudioRouteKind::Track, 1,
         true},
        {"main:right", "Main Right", pulp::host::AudioRouteKind::Track, 1,
         true},
        {"side:left", "Side Left", pulp::host::AudioRouteKind::Bus, 1, true},
        {"side:right", "Side Right", pulp::host::AudioRouteKind::Bus, 1,
         true},
    };

private:
    ChangeCallback callback_;
    std::array<std::string, 2> selected_{};
    std::array<std::array<std::string, 2>, 2> selected_channels_{};
};
}

TEST_CASE("Beadsy route picker consumes host catalog on the control thread") {
    EditorRouteCatalog catalog;
    beadsy::AudioRoutePicker picker;
    picker.set_catalog(&catalog, true);
    REQUIRE(picker.catalog() == &catalog);
    REQUIRE(picker.items().size() == 4);
    REQUIRE(picker.items()[1] == "Input: Mic (1 ch)");
    REQUIRE(picker.items()[3].find("unavailable") != std::string::npos);
    REQUIRE(picker.enabled() == false); // MIDI synth mode does not use host input.

    picker.set_source_mode(1);
    REQUIRE(picker.enabled());
    picker.set_selected(2);
    REQUIRE(catalog.connected_route(0) == "track:piano");

    catalog.routes[1].display_name = "Piano Updated";
    catalog.notify();
    REQUIRE(picker.items()[2] == "Track: Piano (2 ch)");
    picker.refresh_if_needed();
    REQUIRE(picker.items()[2] == "Track: Piano Updated (2 ch)");
}

TEST_CASE("Beadsy clears a dropped route identity after catalog refresh") {
    EditorRouteCatalog catalog;
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_audio_route_catalog(&catalog, true);
    native->source_mode_control()->set_mode(1);
    auto* picker = native->audio_route_picker();
    REQUIRE(picker != nullptr);
    picker->set_source_mode(1);
    picker->set_selected(2);
    REQUIRE(processor.route_identity(0) == "track:piano");

    catalog.clear_selection();
    catalog.notify();
    native->layout_children();
    REQUIRE(processor.route_identity(0).empty());
}

TEST_CASE("Beadsy exposes independent stereo host route pickers") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE(native->audio_route_picker() != nullptr);
    REQUIRE(native->audio_route_picker_right() != nullptr);
    REQUIRE(native->audio_route_picker()->bus_index() == 0);
    REQUIRE(native->audio_route_picker_right()->bus_index() == 0);
    REQUIRE(native->audio_route_picker()->access_label().find("Input L") != std::string::npos);
    REQUIRE(native->audio_route_picker_right()->access_label().find("Input R") != std::string::npos);

    EditorRouteCatalog catalog;
    native->set_audio_route_catalog(&catalog, true);
    native->source_mode_control()->set_mode(1);
    native->audio_route_picker()->set_source_mode(1);
    native->audio_route_picker_right()->set_source_mode(1);
    native->audio_route_picker()->set_selected(2);
    native->audio_route_picker_right()->set_selected(2);
    REQUIRE(catalog.connected_route_channel(0, 0) == "track:piano");
    REQUIRE(catalog.connected_route_channel(0, 1) == "track:piano");
}

TEST_CASE("Beadsy stereo route menus select the same host source through root clicks") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    EditorRouteCatalog catalog;
    native->set_audio_route_catalog(&catalog, true);
    native->source_mode_control()->set_mode(1);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    for (auto* picker : {native->audio_route_picker(),
                         native->audio_route_picker_right()}) {
        REQUIRE(picker != nullptr);
        picker->set_source_mode(1);
        REQUIRE(picker->enabled());
        const auto b = picker->bounds();
        const pulp::view::Point center{b.x + b.width * 0.5f,
                                       b.y + b.height * 0.5f};
        REQUIRE(native->hit_test(center) == picker);
        native->simulate_click(center);
        REQUIRE(picker->is_open());
        pulp::view::KeyEvent down{.key = pulp::view::KeyCode::down};
        REQUIRE(picker->on_key_event(down));
        REQUIRE(picker->on_key_event(down));
        pulp::view::KeyEvent enter{.key = pulp::view::KeyCode::enter};
        REQUIRE(picker->on_key_event(enter));
        REQUIRE_FALSE(picker->is_open());
    }
    REQUIRE(catalog.connected_route_channel(0, 0) == "track:piano");
    REQUIRE(catalog.connected_route_channel(0, 1) == "track:piano");
}

TEST_CASE("Beadsy refreshes distinct sidechain channel identities") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
    REQUIRE(native != nullptr);

    DistinctBusRouteCatalog catalog;
    native->set_audio_route_catalog(&catalog, true);
    native->source_mode_control()->set_mode(1);
    auto *left = native->audio_route_picker();
    auto *right = native->audio_route_picker_right();
    REQUIRE(left != nullptr);
    REQUIRE(right != nullptr);
    left->set_source_mode(1);
    right->set_source_mode(1);
    left->set_selected(1);
    right->set_selected(2);
    native->layout_children();
    REQUIRE(processor.route_identity(0) == "main:left");
    REQUIRE(processor.route_identity(1) == "main:right");

    native->source_mode_control()->set_mode(2);
    left->set_source_mode(2);
    right->set_source_mode(2);
    left->set_selected(3);
    right->set_selected(4);
    catalog.notify();
    native->layout_children();
    REQUIRE(left->bus_index() == 1);
    REQUIRE(right->bus_index() == 1);
    REQUIRE(processor.route_identity(0) == "side:left");
    REQUIRE(processor.route_identity(1) == "side:right");
}
#endif

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

TEST_CASE("Beadsy utility pots stay black across editor scale") {
    REQUIRE(beadsy::detail::PanelKnob::is_utility_diameter(3.0f));
    REQUIRE(beadsy::detail::PanelKnob::is_utility_diameter(6.0f));
    REQUIRE_FALSE(beadsy::detail::PanelKnob::is_utility_diameter(7.2f));
    REQUIRE_FALSE(beadsy::detail::PanelKnob::is_utility_diameter(18.5f));
    constexpr float diameter = 17.0f;
    REQUIRE(beadsy::detail::PanelKnob::utility_paint_radius_for_bounds(
                diameter, diameter * 2.35f) >
            beadsy::detail::PanelKnob::paint_radius_for_bounds(
                diameter, diameter * 2.35f));
}

TEST_CASE("Beadsy auxiliary knobs are interactive at their painted centers") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    constexpr std::array<beadsy::ParamIDs, 7> auxiliary_ids = {
        beadsy::kFeedback, beadsy::kDryWet, beadsy::kReverb,
        beadsy::kTimeAmount, beadsy::kSizeAmount, beadsy::kShapeAmount,
        beadsy::kPitchAmount};
    for (std::size_t i = 0; i < auxiliary_ids.size(); ++i) {
        INFO("auxiliary index " << i);
        auto *knob = dynamic_cast<beadsy::detail::PanelKnob *>(
            native->child_at(i + 5));
        REQUIRE(knob != nullptr);
        REQUIRE(knob->enabled());
        REQUIRE_FALSE(knob->hit_slop().empty());
        const auto before = store.get_value(auxiliary_ids[i]);
        const auto b = knob->bounds();
        const pulp::view::Point center{b.x + b.width * 0.5f,
                                       b.y + b.height * 0.5f};
        native->simulate_drag(center, {center.x, center.y - 35.0f}, 3);
        REQUIRE(store.get_value(auxiliary_ids[i]) > before);
    }
}

TEST_CASE("Beadsy quality station uses one button and hides the legacy LED") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    beadsy::CenterIndicators *indicators = nullptr;
    for (std::size_t i = 0; i < native->child_count(); ++i) {
        if ((indicators = dynamic_cast<beadsy::CenterIndicators *>(
                 native->child_at(i))))
            break;
    }
    REQUIRE(indicators != nullptr);
    const auto b = indicators->bounds();
    REQUIRE(b.height > 20.0f);
  // The product panel now crops the inert routing bay, so the quality
  // station is intentionally higher in the reclaimed-height layout.
  REQUIRE(b.y > 80.0f);
    REQUIRE(b.y < 130.0f);
    REQUIRE_FALSE(indicators->wants_mouse_input());
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

TEST_CASE("Beadsy preserves panel aspect ratio on host-provided resize") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    auto editor = processor.create_view();
    auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
    REQUIRE(native != nullptr);

    // Hosts can briefly provide a wide rectangle while restoring a window.
    // The authored 200.693:364.252 faceplate must stay uniform and centered;
    // otherwise the small colored symbols and their native hit regions drift.
    native->set_bounds({0.0f, 0.0f, 320.0f, 364.0f});
    native->layout_children();
    const auto source = native->source_mode_control()->bounds();
    REQUIRE(source.x > 50.0f);
    REQUIRE(source.x + source.width < 270.0f);
    REQUIRE(source.height > 14.0f);

    const auto capture = pulp::view::capture_view(
        *editor, 320, 364, 1.0f, pulp::view::ScreenshotBackend::auto_select);
    REQUIRE(capture.ok);
    REQUIRE(capture.png.size() > 1000);
    if (const char *path = std::getenv("BEADSY_RESIZE_CAPTURE")) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }
}

TEST_CASE("Beadsy publishes the proportional default size to every host") {
    beadsy::Beadsy processor;
    const auto size = processor.view_size();

    REQUIRE(size.preferred_width == 350);
    // The width-locked faceplate cropped just below the trim labels
    // (111.5 of 128.5 mm) at 350 px wide.
    REQUIRE(size.preferred_height == 551);
    REQUIRE(size.min_width == size.preferred_width);
    REQUIRE(size.min_height == size.preferred_height);
    REQUIRE(size.max_width == size.preferred_width * 2);
    REQUIRE(size.max_height == size.preferred_height * 2);
    // The aspect field is the host-side resize lock. Leaving it at zero makes
    // AUv2/standalone accept a free-form drag even though the editor itself
    // lays out the authored panel uniformly.
    REQUIRE(std::abs(size.aspect_ratio - beadsy::kPanelAspectRatio) < 1.0e-9);
    REQUIRE(std::abs(size.aspect_ratio - 70.8 / 111.5) < 1.0e-6);
    REQUIRE(std::lround(size.preferred_width / size.aspect_ratio) ==
            size.preferred_height);
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

TEST_CASE("Beadsy small knob pointer visibly follows its value") {
    constexpr float diameter = 32.0f;
    constexpr float height = diameter * 2.35f;
    std::vector<std::uint8_t> previous;
    for (const float value : {0.0f, 0.5f, 1.0f}) {
        beadsy::detail::PanelKnob knob("utility-pointer", 6.0f);
        knob.set_bounds({0.0f, 0.0f, diameter, height});
        knob.set_value(value);
        const auto png = pulp::view::render_to_png(
            knob, static_cast<int>(diameter), static_cast<int>(height), 2.0f,
            pulp::view::ScreenshotBackend::coregraphics);
        REQUIRE(png.size() > 100);
        if (!previous.empty()) REQUIRE(png != previous);
        previous = png;
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

TEST_CASE("Beadsy knob opens modulation menu through root right-click dispatch") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    auto* density = dynamic_cast<beadsy::detail::PanelKnob*>(native->child_at(0));
    REQUIRE(density != nullptr);
    const auto b = density->bounds();
    REQUIRE(pulp::view::dispatch_context_menu(
        *native, {b.x + b.width * 0.5f, b.y + b.height * 0.5f}));

    REQUIRE(native->modulation_context_target() == beadsy::kDensity);
    REQUIRE(native->child_count() > 20);
    auto* menu = dynamic_cast<pulp::view::ContextMenu*>(native->child_at(native->child_count() - 1));
    REQUIRE(menu != nullptr);
    // Only the hovered target is shown until All targets is requested.
    REQUIRE(menu->items().size() == 13);

    if (const char *path = std::getenv("BEADSY_MENU_CAPTURE")) {
        const auto capture = pulp::view::capture_view(
            *editor, 201, 364, 2.0f, pulp::view::ScreenshotBackend::auto_select);
        REQUIRE(capture.ok);
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }

    // The shared menu owns keyboard navigation and dismissal; its keyboard
    // contract is covered by the focused ContextMenu tests below.
}

TEST_CASE("Beadsy buttons open modulation menus through secondary dispatch") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 350.0f, 558.0f});
    native->layout_children();
    for (const auto id : {beadsy::kSeedMode, beadsy::kFreeze, beadsy::kQuality}) {
      pulp::view::View *button = nullptr;
      for (std::size_t i = 0; i < native->child_count(); ++i) {
        auto *candidate = native->child_at(i);
        if ((id == beadsy::kSeedMode && dynamic_cast<beadsy::SeedModeControl *>(candidate)) ||
            (id == beadsy::kFreeze && dynamic_cast<beadsy::FreezeControl *>(candidate)) ||
            (id == beadsy::kQuality && dynamic_cast<beadsy::PushButtonControl *>(candidate))) {
          button = candidate;
          break;
        }
      }
      REQUIRE(button != nullptr);
      const auto b = button->bounds();
      REQUIRE(pulp::view::dispatch_context_menu(
          *native, {b.x + b.width * 0.5f, b.y + b.height * 0.5f}));
      REQUIRE(native->modulation_context_target() == id);
      native->clear_modulation_context();
    }
}

TEST_CASE("Beadsy binds source-backed auxiliary controls without decorative ports") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    // The visible source selector replaces the legacy secondary bus toggle;
    // the compatibility parameter remains state/automation-only.
    REQUIRE(native->child_count() ==
            21
    );

    // Five primary controls plus seven PCB-net-backed auxiliary pots are
    // automatable. Hardware jack regions are intentionally omitted: host
    // audio routing is selected through the injected route picker.
    for (std::size_t i = 0; i < 12; ++i) {
        auto* knob = dynamic_cast<beadsy::detail::PanelKnob*>(native->child_at(i));
        REQUIRE(knob != nullptr);
        REQUIRE(knob->enabled());
        REQUIRE(knob->access_disabled().empty());
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

TEST_CASE("Beadsy Seed Mode keeps its three states independent of input buses") {
    beadsy::SeedModeControl seed;
    seed.set_mode(2);
    REQUIRE(seed.mode() == 2);
    REQUIRE(seed.access_label().find("locked") != std::string::npos);
    seed.set_mode(99);
    REQUIRE(seed.mode() == 2);
    seed.set_mode(-1);
    REQUIRE(seed.mode() == 0);
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

TEST_CASE("Beadsy top buttons receive clicks through the editor root") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    for (const auto& [id, expected] :
         std::array<std::pair<std::string_view, pulp::state::ParamID>, 2>{
             {{"beadsy-freeze", beadsy::kFreeze},
              {"beadsy-seed-mode", beadsy::kSeedMode}}}) {
        pulp::view::View* button = nullptr;
        for (std::size_t i = 0; i < native->child_count(); ++i)
            if (native->child_at(i)->id() == id) button = native->child_at(i);
        REQUIRE(button != nullptr);
        const auto b = button->bounds();
        const pulp::view::Point center{b.x + b.width * 0.5f,
                                       b.y + b.height * 0.5f};
        REQUIRE(native->hit_test(center) == button);
        native->simulate_click(center);
        REQUIRE(store.get_value(expected) == 1.0f);
    }
}

TEST_CASE("Beadsy stereo route fields have distinct host-facing identities") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    std::vector<std::string> route_ids;
    for (std::size_t i = 0; i < native->child_count(); ++i) {
        auto* child = native->child_at(i);
        if (child && child->id().find("beadsy-audio-route-") == 0)
            route_ids.emplace_back(child->id());
    }
    REQUIRE(route_ids.size() == 2);
    REQUIRE(route_ids[0] != route_ids[1]);
}

namespace {
using MenuItem = pulp::view::ContextMenu::Item;
MenuItem menu_item(pulp::view::ContextMenu *menu, std::string_view label) {
    const auto it = std::find_if(menu->items().begin(), menu->items().end(),
                                [&](const auto &item) { return item.label == label; });
    REQUIRE(it != menu->items().end());
    return *it;
}
bool has_menu_item(pulp::view::ContextMenu *menu, std::string_view label) {
    return std::any_of(menu->items().begin(), menu->items().end(),
                       [&](const auto &item) { return item.label == label; });
}
void click_menu_row(beadsy::BeadsyEditor &editor, pulp::view::ContextMenu *menu,
                    std::string_view label) {
    const auto &items = menu->items();
    const auto it = std::find_if(items.begin(), items.end(),
                                [&](const auto &item) { return item.label == label; });
    REQUIRE(it != items.end());
    const auto row = menu->layout().rows[static_cast<std::size_t>(it - items.begin())];
    editor.simulate_click({row.x + row.width * 0.5f, row.y + row.height * 0.5f});
}
}

TEST_CASE("Beadsy keeps shared LFOs enabled while menus focus on another control") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 350.0f, 634.0f});
    native->layout_children();
    auto *menu = native->show_modulation_context_menu(beadsy::kPitch, {12, 12});
    REQUIRE(menu->items().front().label == "Reset Pitch");
    REQUIRE(menu->items().size() == 13);
    REQUIRE(menu->layout().max_scroll == 0.0f);
    REQUIRE_FALSE(has_menu_item(menu, "LIVE"));
    REQUIRE_FALSE(has_menu_item(menu, "Reset to Live"));
    click_menu_row(*native, menu, "LFO 1");
    REQUIRE(processor.lfo_generator(0).enabled);
    REQUIRE(processor.lfo_target_enabled(beadsy::kPitch, 0));
    REQUIRE(menu->items().size() == 13);
    REQUIRE_FALSE(has_menu_item(menu, "LFO 1 -> Size"));
    auto rate = menu_item(menu, "Rate");
    rate.set_normalized_value(0.2f);
    // Continuous root pointer dispatch must keep the slider captured across
    // multiple motion events (rebuilding rows on change used to stop drag).
    const auto rate_it = std::find_if(menu->items().begin(), menu->items().end(),
                                     [](const auto &item) { return item.label == "Rate"; });
    const auto rate_row = menu->layout().rows[static_cast<std::size_t>(rate_it - menu->items().begin())];
    const float left = rate_row.x + std::min(90.0f, rate_row.width * 0.45f);
    const float right = rate_row.x + rate_row.width - 52.0f;
    const float y = rate_row.y + rate_row.height * 0.5f;
    native->simulate_drag({left + (right - left) * 0.1f, y},
                         {left + (right - left) * 0.8f, y}, 8);
    REQUIRE(std::abs(processor.lfo_generator(0).rate_hz - (0.01f + 0.8f * 19.99f)) < 0.01f);
    menu->on_mouse_event({.position = {left + (right - left) * 0.2f, y}, .is_down = true});
    menu->on_mouse_event({.position = {left + (right - left) * 0.6f, y}, .is_down = false});
    REQUIRE(std::abs(processor.lfo_generator(0).rate_hz - (0.01f + 0.6f * 19.99f)) < 0.01f);
    menu->on_mouse_event({.position = {left + (right - left) * 0.6f, y}, .is_down = true});
    const auto released_rate = processor.lfo_generator(0).rate_hz;
    menu->on_mouse_event({.position = {left, y}, .phase = pulp::view::MousePhase::hover});
    REQUIRE(processor.lfo_generator(0).rate_hz == released_rate);
    const auto pitch_settings = processor.lfo_generator(0);

    menu = native->show_modulation_context_menu(beadsy::kSize, {12, 12});
    REQUIRE(native->modulation_context_target() == beadsy::kSize);
    REQUIRE(menu->items().front().label == "Reset Size");
    REQUIRE(menu_item(menu, "LFO 1").checked);
    REQUIRE_FALSE(menu_item(menu, "LFO 1 -> Size").checked);
    REQUIRE(processor.lfo_generator(0).rate_hz == pitch_settings.rate_hz);
    REQUIRE_FALSE(has_menu_item(menu, "LFO 1 -> Pitch"));
    const auto pitch_depth = processor.lfo_target_depth(beadsy::kPitch, 0);
    auto size_depth = menu_item(menu, "Depth");
    size_depth.set_normalized_value(0.8f);
    REQUIRE(processor.lfo_target_depth(beadsy::kSize, 0) == 0.8f);
    REQUIRE(processor.lfo_target_depth(beadsy::kPitch, 0) == pitch_depth);
    click_menu_row(*native, menu, "LFO 1 -> Size");
    REQUIRE(processor.lfo_target_enabled(beadsy::kPitch, 0));
    REQUIRE(processor.lfo_target_enabled(beadsy::kSize, 0));
    REQUIRE(menu->items().size() == 13);
    click_menu_row(*native, menu, "LFO 1 -> Size");
    REQUIRE_FALSE(processor.lfo_target_enabled(beadsy::kSize, 0));
    REQUIRE(processor.lfo_target_enabled(beadsy::kPitch, 0));
    REQUIRE(processor.lfo_generator(0).enabled);

    click_menu_row(*native, menu, "All targets…");
    REQUIRE(has_menu_item(menu, "LFO 1 -> Pitch"));
    REQUIRE(has_menu_item(menu, "LFO 1 -> Freeze"));
    REQUIRE(has_menu_item(menu, "LFO 1 -> Seed Mode"));
    auto pitch = menu_item(menu, "LFO 1 -> Pitch");
    pitch.set_toggled(false);
    REQUIRE_FALSE(processor.lfo_target_enabled(beadsy::kPitch, 0));
    menu = native->show_modulation_context_menu(beadsy::kTime, {12, 12});
    REQUIRE(menu_item(menu, "LFO 1").checked);
    REQUIRE_FALSE(menu_item(menu, "All targets…").checked);
    REQUIRE(menu->items().size() == 13);
}

TEST_CASE("Beadsy reset affects only the hovered control and its assignments") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    store.set_value(beadsy::kPitch, 9.0f);
    store.set_value(beadsy::kSize, 0.8f);
    processor.set_lfo(beadsy::kPitch, {.enabled = true, .rate_hz = 3, .depth = 0.4f});
    processor.set_lfo_target_enabled(beadsy::kSize, 0, true);
    processor.set_lfo(beadsy::kSize, {.enabled = true, .rate_hz = 2, .depth = 0.3f}, 1);
    auto editor = processor.create_view();
    auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
    native->set_bounds({0, 0, 350, 634});
    auto *menu = native->show_modulation_context_menu(beadsy::kSize, {12, 12});
    click_menu_row(*native, menu, "Reset Size");
    REQUIRE(store.get_value(beadsy::kSize) == store.info(beadsy::kSize)->range.default_value);
    REQUIRE(store.get_value(beadsy::kPitch) == 9.0f);
    REQUIRE_FALSE(processor.lfo_target_enabled(beadsy::kSize, 0));
    REQUIRE_FALSE(processor.lfo_target_enabled(beadsy::kSize, 1));
    REQUIRE(processor.lfo_target_enabled(beadsy::kPitch, 0));
    REQUIRE(processor.lfo_generator(0).enabled);
    REQUIRE(processor.lfo_generator(1).enabled);
    REQUIRE(processor.lfo_generator(0).rate_hz == 3.0f);
    REQUIRE_FALSE(native->modulation_context_target().has_value());
}

TEST_CASE("Beadsy modulation menu is reachable through root secondary dispatch") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0, 0, 350, 634});
    native->layout_children();
    auto *density = dynamic_cast<beadsy::detail::PanelKnob *>(native->child_at(0));
    REQUIRE(density != nullptr);
    const auto bounds = density->bounds();
    REQUIRE(pulp::view::dispatch_context_menu(*native,
        {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f}));
    auto *menu = dynamic_cast<pulp::view::ContextMenu *>(native->child_at(native->child_count() - 1));
    REQUIRE(menu != nullptr);
    REQUIRE(menu->items().front().label == "Reset Density");
    REQUIRE(menu->items().size() == 13);
    REQUIRE(has_menu_item(menu, "LFO 1 -> Density"));
    REQUIRE_FALSE(has_menu_item(menu, "LFO 1 -> Size"));
}

TEST_CASE("Beadsy shows named waveforms and edits the selected shared LFO") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
    native->set_bounds({0, 0, 350, 634});
    auto *menu = native->show_modulation_context_menu(beadsy::kSize, {12, 12});
    auto waveform = menu_item(menu, "Waveform");
    REQUIRE(waveform.choices == std::vector<std::string>{"Sine", "Triangle", "Square", "Saw"});
    waveform.set_choice_index(2);
    REQUIRE(processor.lfo_generator(0).shape == beadsy::LfoModulation::Shape::Square);
    const auto wave_it = std::find_if(menu->items().begin(), menu->items().end(),
                                     [](const auto &item) { return item.label == "Waveform"; });
    const auto wave_row = menu->layout().rows[static_cast<std::size_t>(wave_it - menu->items().begin())];
    for (const std::size_t shape : {3u, 0u, 2u}) {
        const auto icon = beadsy::detail::ModulationContextMenu::waveform_cell(wave_row, shape);
        native->simulate_click({icon.x + icon.width * 0.5f, icon.y + icon.height * 0.5f});
        REQUIRE(static_cast<std::size_t>(processor.lfo_generator(0).shape) == shape);
        REQUIRE(menu_item(menu, "Waveform").choice_index == shape);
    }
    click_menu_row(*native, menu, "EDIT LFO 2");
    waveform = menu_item(menu, "Waveform");
    waveform.set_choice_index(1);
    REQUIRE(processor.lfo_generator(1).shape == beadsy::LfoModulation::Shape::Triangle);
    REQUIRE(processor.lfo_generator(0).shape == beadsy::LfoModulation::Shape::Square);
    click_menu_row(*native, menu, "LFO 2");
    REQUIRE(processor.lfo_generator(1).enabled);
    REQUIRE(processor.lfo_target_enabled(beadsy::kSize, 1));
}

TEST_CASE("Beadsy paints played knob and button states without changing base values") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto *native = dynamic_cast<beadsy::BeadsyEditor *>(editor.get());
    native->set_bounds({0, 0, 350, 634});
    native->layout_children();
    auto *knob = dynamic_cast<beadsy::detail::PanelKnob *>(native->child_at(0));
    REQUIRE(knob != nullptr);
    const auto knob_base = knob->value();
    const auto before = pulp::view::render_to_png(*knob, 64, 80, 1.0f,
        pulp::view::ScreenshotBackend::coregraphics);
    REQUIRE(before.size() > 100);
    store.set_display_modulation(beadsy::kDensity, 0.95f);
    store.set_display_modulation(beadsy::kFreeze, 1.0f);
    store.set_display_modulation(beadsy::kSeedMode, 2.0f);
    store.set_display_modulation(beadsy::kQuality, 0.0f);
    store.pump_listeners();
    const auto after = pulp::view::render_to_png(*knob, 64, 80, 1.0f,
        pulp::view::ScreenshotBackend::coregraphics);
    REQUIRE(after != before);
    REQUIRE(knob->value() == knob_base);
    REQUIRE(knob->modulated_display_value() == 0.95f);
    REQUIRE(native->quality_control()->displayed_quality() == 0);
    REQUIRE(native->quality_control()->quality() == 3);
    bool saw_seed = false;
    bool saw_freeze = false;
    for (std::size_t i = 0; i < native->child_count(); ++i) {
        if (auto *seed = dynamic_cast<beadsy::SeedModeControl *>(native->child_at(i))) {
            REQUIRE(seed->displayed_mode() == 2);
            REQUIRE(seed->mode() == 0);
            saw_seed = true;
        }
        if (auto *freeze = dynamic_cast<beadsy::FreezeControl *>(native->child_at(i))) {
            REQUIRE(freeze->displayed_pressed());
            REQUIRE_FALSE(freeze->pressed());
            saw_freeze = true;
        }
    }
    REQUIRE(saw_seed);
    REQUIRE(saw_freeze);
    REQUIRE(store.get_value(beadsy::kFreeze) == 0.0f);
    REQUIRE(store.get_value(beadsy::kSeedMode) == 0.0f);
    REQUIRE(store.get_value(beadsy::kQuality) == 3.0f);
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

TEST_CASE("Beadsy resize grip forwards an accepted host resize request") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);

    uint32_t requested_width = 0;
    uint32_t requested_height = 0;
    processor.set_editor_resize_handler(
        [&](uint32_t width, uint32_t height) {
            requested_width = width;
            requested_height = height;
            return true;
        });

    const auto hints = processor.view_size();
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, static_cast<float>(hints.preferred_width),
                        static_cast<float>(hints.preferred_height)});
    native->layout_children();

    beadsy::ResizeGrip* grip = nullptr;
    for (std::size_t i = 0; i < native->child_count(); ++i) {
        if (!grip)
            grip = dynamic_cast<beadsy::ResizeGrip*>(native->child_at(i));
    }
    REQUIRE(grip != nullptr);
    const auto locked_height = [&](uint32_t width) {
        return static_cast<uint32_t>(
            std::lround(width / beadsy::kPanelAspectRatio));
    };

    pulp::view::MouseEvent press;
    press.phase = pulp::view::MousePhase::press;
    press.is_down = true;
    press.pointer_id = 1;
    press.window_position = {345.0f, 546.0f};
    grip->on_mouse_event(press);

    pulp::view::MouseEvent drag;
    drag.phase = pulp::view::MousePhase::drag;
    drag.is_down = true;
    drag.pointer_id = 1;
    drag.has_movement_delta = true;
    drag.movement_x = 40.0f;
    drag.movement_y = 4.0f;
    drag.window_position = {385.0f, 550.0f};
    grip->on_mouse_event(drag);
    REQUIRE(requested_width == 390);
    REQUIRE(requested_height == locked_height(390));

    // AUv2 hosts commonly report each pointer update as a delta. The second
    // update must remain anchored to the original press size rather than
    // jumping back to the first frame's width.
    drag.movement_x = 10.0f;
    drag.movement_y = 0.0f;
    drag.window_position = {395.0f, 550.0f};
    grip->on_mouse_event(drag);
    REQUIRE(requested_width == 400);
    REQUIRE(requested_height == locked_height(400));

    // A mostly vertical drag drives the same lock: the height moves with the
    // pointer and the width follows the artwork ratio.
    drag.movement_x = 0.0f;
    drag.movement_y = 100.0f;
    drag.window_position = {395.0f, 650.0f};
    grip->on_mouse_event(drag);
    REQUIRE(requested_height == locked_height(requested_width));
    REQUIRE(static_cast<double>(requested_width) /
                static_cast<double>(requested_height) ==
            Catch::Approx(beadsy::kPanelAspectRatio).epsilon(0.01));

    // The grip never requests a size the processor would refuse.
    drag.movement_x = 2000.0f;
    drag.movement_y = 0.0f;
    grip->on_mouse_event(drag);
    REQUIRE(requested_width == hints.max_width);
    REQUIRE(requested_height == locked_height(hints.max_width));
    drag.movement_x = -4000.0f;
    grip->on_mouse_event(drag);
    REQUIRE(requested_width == hints.min_width);
    REQUIRE(requested_height == locked_height(hints.min_width));

    pulp::view::MouseEvent release;
    release.phase = pulp::view::MousePhase::release;
    release.pointer_id = 1;
    grip->on_mouse_event(release);
    processor.set_editor_resize_handler({});
}

namespace {
pulp::view::View* beadsy_child(beadsy::BeadsyEditor& editor, std::string_view id) {
    for (std::size_t i = 0; i < editor.child_count(); ++i)
        if (editor.child_at(i)->id() == id) return editor.child_at(i);
    return nullptr;
}
pulp::view::Point beadsy_center(const pulp::view::View& view) {
    const auto b = view.bounds();
    return {b.x + b.width * 0.5f, b.y + b.height * 0.5f};
}
}  // namespace

TEST_CASE("Beadsy toggles answer the host pointer verbs at the default size") {
    // The macOS window and plug-in hosts deliver a left press through
    // deliver_mouse_down/deliver_mouse_up and a secondary press through
    // route_context_press. Drive those exact verbs rather than simulate_click
    // so the proof covers the same layer a real NSEvent reaches.
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    const auto size = processor.view_size();
    native->set_bounds({0.0f, 0.0f, static_cast<float>(size.preferred_width),
                        static_cast<float>(size.preferred_height)});
    native->layout_children();

    const auto press = [&](pulp::view::Point p) {
        auto* target = native->hit_test(p);
        REQUIRE(target != nullptr);
        REQUIRE(pulp::view::deliver_mouse_down(*native, target, p, 0, 1));
        pulp::view::deliver_mouse_up(*native, target, p, 0, 1, {});
    };

    auto* freeze = beadsy_child(*native, "beadsy-freeze");
    auto* seed = beadsy_child(*native, "beadsy-seed-mode");
    auto* quality = beadsy_child(*native, "beadsy-push-button");
    auto* assign = beadsy_child(*native, "beadsy-assign-button");
    REQUIRE(freeze != nullptr);
    REQUIRE(seed != nullptr);
    REQUIRE(quality != nullptr);
    REQUIRE(assign != nullptr);

    press(beadsy_center(*freeze));
    REQUIRE(store.get_value(beadsy::kFreeze) == 1.0f);
    press(beadsy_center(*freeze));
    REQUIRE(store.get_value(beadsy::kFreeze) == 0.0f);
    press(beadsy_center(*seed));
    REQUIRE(store.get_value(beadsy::kSeedMode) == 1.0f);
    press(beadsy_center(*quality));
    REQUIRE(store.get_value(beadsy::kQuality) == 0.0f);
    press(beadsy_center(*assign));
    REQUIRE(native->assign_control()->destination() == 1);

    const auto context = [&](pulp::view::View* view, pulp::state::ParamID expected) {
        pulp::view::route_context_press(*native, beadsy_center(*view));
        REQUIRE(native->modulation_context_target() == expected);
        auto* menu = dynamic_cast<pulp::view::ContextMenu*>(
            native->child_at(native->child_count() - 1));
        REQUIRE(menu != nullptr);
        REQUIRE(menu->items().front().label.rfind("Reset ", 0) == 0);
        menu->on_key_event({.key = pulp::view::KeyCode::escape, .is_down = true});
    };
    context(freeze, beadsy::kFreeze);
    context(seed, beadsy::kSeedMode);
    context(quality, beadsy::kQuality);
    context(assign, beadsy::kFeedback);
}

TEST_CASE("Beadsy controls survive a host-owned Yoga layout pass") {
    // Standalone and plug-in hosts place the editor inside their own root and
    // lay the whole window out with one Yoga pass. Yoga applies the editor's
    // rect first (which places the controls for that size) and then applies
    // its own rects to the editor's children; unless the editor owns its
    // children's layout those rects come from the styles captured before the
    // editor had a size, so every control lands at the 201 x 364 fallback.
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);

    pulp::view::View host_root;
    // The standalone chrome and the plug-in bridge both let the editor fill
    // the host root through flex-grow rather than an explicit size.
    editor->flex().flex_grow = 1.0f;
    host_root.add_child(std::move(editor));
    const auto size = processor.view_size();
    host_root.set_bounds({0.0f, 0.0f, static_cast<float>(size.preferred_width),
                          static_cast<float>(size.preferred_height)});
    host_root.layout_children();

    REQUIRE(native->bounds().width == Catch::Approx(350.0f));
    REQUIRE(native->bounds().height == Catch::Approx(551.0f));
    auto* freeze = beadsy_child(*native, "beadsy-freeze");
    auto* density = beadsy_child(*native, "beadsy-Density");
    REQUIRE(freeze != nullptr);
    REQUIRE(density != nullptr);
    // At 350 px wide the Freeze hit target is centred at (43, 89); at the
    // 201 px fallback it sat at (25, 51), which is what a real right-click
    // on Freeze resolved to in the standalone (it opened the Time menu).
    const auto fb = freeze->bounds();
    REQUIRE(fb.x + fb.width * 0.5f == Catch::Approx(43.0f).margin(1.0f));
    REQUIRE(fb.y + fb.height * 0.5f == Catch::Approx(89.0f).margin(1.0f));
    REQUIRE(host_root.hit_test({43.0f, 89.0f}) == freeze);
    REQUIRE(density->bounds().width == Catch::Approx(91.4f).margin(1.0f));

    pulp::view::route_context_press(host_root, {43.0f, 89.0f});
    REQUIRE(native->modulation_context_target() == beadsy::kFreeze);
}

TEST_CASE("Beadsy toggle faces follow the played modulation and presses step the base") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 350.0f, 551.0f});
    native->layout_children();
    auto* freeze = dynamic_cast<beadsy::FreezeControl*>(beadsy_child(*native, "beadsy-freeze"));
    auto* seed = dynamic_cast<beadsy::SeedModeControl*>(beadsy_child(*native, "beadsy-seed-mode"));
    REQUIRE(freeze != nullptr);
    REQUIRE(seed != nullptr);

    // An internal LFO publishes the value it plays; the shown state follows
    // it while the stored base stays put.
    store.set_display_modulation(beadsy::kFreeze, 1.0f);
    store.set_display_modulation(beadsy::kSeedMode, 2.0f);
    REQUIRE(store.get_value(beadsy::kFreeze) == 0.0f);
    REQUIRE(freeze->displayed_pressed());
    REQUIRE_FALSE(freeze->pressed());
    REQUIRE(seed->displayed_mode() == 2);
    REQUIRE(seed->mode() == 0);

    // A press steps from the stored base, never from the modulated display
    // and never from a stale cached copy on the native view.
    freeze->set_pressed(true);  // stale cache: store still says 0
    native->simulate_click(beadsy_center(*freeze));
    REQUIRE(store.get_value(beadsy::kFreeze) == 1.0f);
    native->simulate_click(beadsy_center(*freeze));
    REQUIRE(store.get_value(beadsy::kFreeze) == 0.0f);
    native->simulate_click(beadsy_center(*seed));
    REQUIRE(store.get_value(beadsy::kSeedMode) == 1.0f);
    store.clear_display_modulation(beadsy::kFreeze);
    store.clear_display_modulation(beadsy::kSeedMode);
    REQUIRE_FALSE(freeze->displayed_pressed());
    REQUIRE(seed->displayed_mode() == 1);
}
