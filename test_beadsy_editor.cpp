#include <catch2/catch_test_macros.hpp>
#include <pulp/state/store.hpp>
#include <pulp/view/screenshot.hpp>
#include <pulp/view/pointer_dispatch.hpp>
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
    REQUIRE(b.y > 115.0f);
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
    // Targets are progressively disclosed only after a lane is enabled.
    REQUIRE(menu->items().size() == 18);

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
    REQUIRE(menu->items().size() == 18);
    REQUIRE(menu->items()[3].control_kind ==
            pulp::view::ContextMenu::Item::ControlKind::toggle);
    REQUIRE(menu->items()[15].control_kind ==
            pulp::view::ContextMenu::Item::ControlKind::choice);
    REQUIRE(menu->items()[16].control_kind ==
            pulp::view::ContextMenu::Item::ControlKind::slider);
    REQUIRE(menu->items()[17].control_kind ==
            pulp::view::ContextMenu::Item::ControlKind::slider);
    REQUIRE(menu->items()[4].label == "Reset to Live");
    REQUIRE(std::none_of(menu->items().begin(), menu->items().end(),
                         [](const auto &item) { return item.label == "TARGETS"; }));

    REQUIRE(menu->items()[7].on_change);
    auto toggle = menu->items()[7];
    toggle.checked = true;
    menu->items()[7].on_change(toggle);
    REQUIRE(processor.lfo(beadsy::kDensity).enabled);
    REQUIRE(processor.lfo(beadsy::kDensity).depth > 0.0f);
    REQUIRE(menu->items()[16].normalized_value >= 0.0f);
    REQUIRE(menu->items()[16].normalized_value <= 1.0f);
    REQUIRE(menu->items()[17].normalized_value >= 0.0f);
    REQUIRE(menu->items()[17].normalized_value <= 1.0f);
    REQUIRE(std::any_of(menu->items().begin(), menu->items().end(),
                        [](const auto &item) { return item.label == "TARGETS"; }));

    // LFO 2 is a separate lane with its own toggle and controls.
    REQUIRE(menu->items()[8].control_kind ==
            pulp::view::ContextMenu::Item::ControlKind::toggle);
    auto toggle2 = menu->items()[8];
    toggle2.checked = true;
    menu->items()[8].on_change(toggle2);
    REQUIRE(processor.lfo(beadsy::kDensity, 1).enabled);

    menu->on_close(1);
    REQUIRE_FALSE(processor.lfo(beadsy::kDensity).enabled);
    REQUIRE_FALSE(processor.lfo(beadsy::kDensity, 1).enabled);
    REQUIRE(processor.lfo(beadsy::kDensity).depth == 0.0f);
    REQUIRE_FALSE(native->modulation_context_target().has_value());
}

TEST_CASE("Beadsy modulation menu is reachable through root secondary dispatch") {
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
    const pulp::view::Point center{b.x + b.width * 0.5f,
                                   b.y + b.height * 0.5f};
    REQUIRE(pulp::view::dispatch_context_menu(*native, center));

    // The root-dispatched path must reach the same canonical factory used by
    // a host right-click, including both LFO lanes and the reset command.
    auto* menu = native->child_at(native->child_count() - 1);
    auto* context = dynamic_cast<pulp::view::ContextMenu*>(menu);
    REQUIRE(context != nullptr);
    REQUIRE(context->items().size() == 18);
    REQUIRE(context->items()[7].label == "LFO 1");
    REQUIRE(context->items()[8].label == "LFO 2");
}

TEST_CASE("Beadsy modulation shape choices expose names and stepped waveform cues") {
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
    const auto choice = std::find_if(
        menu->items().begin(), menu->items().end(), [](const auto& item) {
          return item.control_kind ==
                 pulp::view::ContextMenu::Item::ControlKind::choice;
        });
    REQUIRE(choice != menu->items().end());
    REQUIRE(choice->choices.size() == 4);
    REQUIRE(choice->choices[0].find("Sine") != std::string::npos);
    REQUIRE(choice->choices[1].find("Triangle") != std::string::npos);
    REQUIRE(choice->choices[2].find("Square") != std::string::npos);
    REQUIRE(choice->choices[2].find("_|‾|") != std::string::npos);
    REQUIRE(choice->choices[3].find("Saw") != std::string::npos);
    REQUIRE(choice->on_change);

    auto selected = *choice;
    selected.set_choice_index(2);
    REQUIRE(processor.lfo(beadsy::kDensity).shape ==
            beadsy::LfoModulation::Shape::Square);
}

TEST_CASE("Beadsy modulation toggles and lane tabs work through root dispatch") {
    beadsy::Beadsy processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    auto* menu = native->show_modulation_context_menu(
        beadsy::kDensity, pulp::view::Point{12.0f, 12.0f});
    REQUIRE(menu != nullptr);
    const auto click_row = [&](std::string_view label) {
        const auto& items = menu->items();
        const auto it = std::find_if(items.begin(), items.end(),
                                     [&](const auto& item) { return item.label == label; });
        REQUIRE(it != items.end());
        const auto index = static_cast<std::size_t>(std::distance(items.begin(), it));
        const auto row = menu->layout().rows[index];
        native->simulate_click({row.x + row.width * 0.5f,
                                 row.y + row.height * 0.5f});
    };

    click_row("LFO 1");
    REQUIRE(processor.lfo(beadsy::kDensity, 0).enabled);
    REQUIRE(processor.lfo(beadsy::kDensity, 0).depth > 0.0f);

    // Target rows are below the compact viewport; exercise the same callback
    // after locating the row (the core scroll/pointer path is covered by the
    // shared ContextMenu tests).
    auto target_it = std::find_if(menu->items().begin(), menu->items().end(),
                                  [](const auto &item) {
                                      return item.label == "LFO 1 -> Freeze";
                                  });
    REQUIRE(target_it != menu->items().end());
    auto target_item = *target_it;
    target_item.checked = true;
    target_it->on_change(target_item);
    REQUIRE_FALSE(processor.lfo(beadsy::kDensity, 0).enabled);
    REQUIRE(processor.lfo(beadsy::kFreeze, 0).enabled);

    click_row("EDIT LFO 2");
    const auto& after_tab = menu->items();
    REQUIRE(std::find_if(after_tab.begin(), after_tab.end(), [](const auto& item) {
        return item.label == "LFO 2 PARAMETERS";
    }) != after_tab.end());
    REQUIRE(std::find_if(after_tab.begin(), after_tab.end(), [](const auto& item) {
        return item.label == "LFO 1 PARAMETERS";
    }) == after_tab.end());

    click_row("LFO 2");
    REQUIRE(processor.lfo(beadsy::kDensity, 1).enabled);
    REQUIRE(processor.lfo(beadsy::kDensity, 1).depth > 0.0f);
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

    auto editor = processor.create_view();
    auto* native = dynamic_cast<beadsy::BeadsyEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->set_bounds({0.0f, 0.0f, 201.0f, 364.0f});
    native->layout_children();

    beadsy::ResizeGrip* grip = nullptr;
    for (std::size_t i = 0; i < native->child_count(); ++i) {
        if (!grip)
            grip = dynamic_cast<beadsy::ResizeGrip*>(native->child_at(i));
    }
    REQUIRE(grip != nullptr);

    pulp::view::MouseEvent press;
    press.phase = pulp::view::MousePhase::press;
    press.is_down = true;
    press.pointer_id = 1;
    press.window_position = {195.0f, 358.0f};
    grip->on_mouse_event(press);

    pulp::view::MouseEvent drag;
    drag.phase = pulp::view::MousePhase::drag;
    drag.is_down = true;
    drag.pointer_id = 1;
    drag.has_movement_delta = true;
    drag.movement_x = 40.0f;
    drag.movement_y = 4.0f;
    drag.window_position = {235.0f, 362.0f};
    grip->on_mouse_event(drag);
    REQUIRE(requested_width == 241);
    REQUIRE(requested_height == 435);

    // AUv2 hosts commonly report each pointer update as a delta. The second
    // update must remain anchored to the original press size rather than
    // jumping back to the first frame's width.
    drag.movement_x = 10.0f;
    drag.movement_y = 0.0f;
    drag.window_position = {245.0f, 362.0f};
    grip->on_mouse_event(drag);
    REQUIRE(requested_width == 251);
    REQUIRE(requested_height == 453);

    pulp::view::MouseEvent release;
    release.phase = pulp::view::MousePhase::release;
    release.pointer_id = 1;
    grip->on_mouse_event(release);
    processor.set_editor_resize_handler({});
}
