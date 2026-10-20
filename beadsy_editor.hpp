#pragma once

#if !defined(PULP_HEADLESS)

#include "beadsy.hpp"
#include "beadsy_panel_svg.hpp"

#include <pulp/canvas/canvas.hpp>
#include <pulp/canvas/svg.hpp>
#include <pulp/view/context_menu.hpp>
#include <pulp/view/parameter_binding.hpp>
#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>
#if __has_include(<pulp/host/audio_route_catalog.hpp>)
#include <pulp/host/audio_route_catalog.hpp>
#include <pulp/view/ui_components.hpp>
#define BEADSY_HAS_AUDIO_ROUTE_CATALOG 1
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace beadsy {

namespace detail {

inline pulp::view::View *root_view(pulp::view::View *view) {
  if (!view)
    return nullptr;
  while (view->parent())
    view = view->parent();
  return view;
}

inline pulp::view::Point local_to_root(pulp::view::View *view,
                                       pulp::view::Point point) {
  for (auto *current = view; current; current = current->parent()) {
    const auto bounds = current->bounds();
    point.x += bounds.x;
    point.y += bounds.y;
  }
  return point;
}

class PanelKnob final : public pulp::view::Knob {
public:
  static constexpr float kCapRadiusFraction = 0.62f;
  static constexpr float kOuterRadiusFraction = 1.14f;
  static constexpr float kPointerInnerHalfWidthFraction = 0.10f;
  static constexpr float kPointerOuterHalfWidthFraction = 0.055f;

  static float paint_radius_for_bounds(float width, float height) {
    // The layout width is the DXF-mapped knob diameter. Leave a small
    // margin for the shadow and modulation arc while keeping the ring at
    // the mapped physical diameter.
    return std::min(width, height * 0.78f) * 0.43f;
  }

  PanelKnob(std::string caption, float diameter)
      : caption_(std::move(caption)), diameter_(diameter) {
    set_show_label(false);
    set_render_style(pulp::view::WidgetRenderStyle::standard);
    set_id("beadsy-" + caption_);
  }

  void set_modulation_indicator(std::function<bool()> active) {
    modulation_active_ = std::move(active);
  }
  void set_accent(std::optional<pulp::canvas::Color> accent) {
    accent_ = accent;
  }

  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    const float cx = b.width * 0.5f;
    const float cy = b.height * 0.43f;
    const float radius = paint_radius_for_bounds(b.width, b.height);
    const float angle =
        pulp::view::Knob::start_angle +
        (pulp::view::Knob::end_angle - pulp::view::Knob::start_angle) * value();

    // A few auxiliary controls use a colored side strip in the reference
    // panel.  Paint that accent behind the procedural ring so the native
    // knob still owns hit testing and automation.
    if (accent_) {
      canvas.set_fill_color(*accent_);
      canvas.fill_rounded_rect(cx - radius * 0.66f, cy - radius * 2.40f,
                               radius * 1.32f, radius * 4.80f, radius * 0.58f);
    }

    // The major and medium reference knobs carry a soft gray cast shadow
    // outside the black body. Keep it procedural and limit it to the
    // mapped knob sizes so small utility pots remain compact.
    if (radius > 12.0f) {
      canvas.set_fill_color(pulp::canvas::Color::rgba8(112, 115, 124, 74));
      canvas.fill_circle(cx + radius * 0.02f, cy + radius * 0.03f,
                         radius * 1.23f);
      canvas.set_fill_color(pulp::canvas::Color::rgba8(202, 204, 210, 170));
      canvas.fill_circle(cx, cy, radius * 1.18f);
    }

    // The straight-on reference shows a black, physically segmented
    // annulus around a pale inset cap.  Keep this entirely procedural:
    // the reference photo is provenance only and is never embedded.
    const float outer_radius = radius * kOuterRadiusFraction;
    canvas.set_fill_color(pulp::canvas::Color::rgba8(11, 11, 11));
    canvas.fill_circle(cx, cy, outer_radius);
    canvas.set_fill_color(pulp::canvas::Color::rgba8(24, 24, 24));
    canvas.fill_circle(cx - radius * 0.04f, cy - radius * 0.05f,
                       radius * 1.01f);

    // Recessed radial grooves break up the annulus without relying on
    // image assets. Their spacing is stable at every control size.
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(8, 8, 8));
    canvas.set_line_cap(pulp::canvas::LineCap::butt);
    canvas.set_line_width(std::max(1.0f, radius * 0.045f));
    constexpr int kGrooveCount = 28;
    constexpr float kTwoPi = 6.28318530717958647692f;
    for (int i = 0; i < kGrooveCount; ++i) {
      const float groove_angle =
          static_cast<float>(i) * kTwoPi / static_cast<float>(kGrooveCount);
      canvas.stroke_line(cx + radius * 0.70f * std::cos(groove_angle),
                         cy + radius * 0.70f * std::sin(groove_angle),
                         cx + radius * 0.99f * std::cos(groove_angle),
                         cy + radius * 0.99f * std::sin(groove_angle));
    }

    // The measured reference cap occupies approximately 53–61% of the
    // outer diameter.  A 0.62-radius inset lands at the middle of that
    // range after the outer shadow is included.
    canvas.set_fill_color(pulp::canvas::Color::rgba8(232, 230, 218));
    canvas.fill_circle(cx - radius * 0.055f, cy - radius * 0.07f,
                       radius * kCapRadiusFraction);

    // Broad trapezoid pointer: the reference is a white slot rather than
    // the prior 6 px hairline.  It remains centered on the existing Knob
    // value angle, preserving automation and gesture semantics.
    const float ux = std::cos(angle);
    const float uy = std::sin(angle);
    const float px = -uy;
    const float py = ux;
    const float inner = radius * 0.42f;
    const float outer = radius * 0.97f;
    const float inner_half_width = radius * kPointerInnerHalfWidthFraction;
    const float outer_half_width = radius * kPointerOuterHalfWidthFraction;
    const pulp::canvas::Canvas::Point2D pointer[] = {
        {cx + ux * inner + px * inner_half_width,
         cy + uy * inner + py * inner_half_width},
        {cx + ux * inner - px * inner_half_width,
         cy + uy * inner - py * inner_half_width},
        {cx + ux * outer - px * outer_half_width,
         cy + uy * outer - py * outer_half_width},
        {cx + ux * outer + px * outer_half_width,
         cy + uy * outer + py * outer_half_width},
    };
    canvas.set_fill_color(pulp::canvas::Color::rgba8(244, 242, 230));
    canvas.fill_path(pointer, 4);

    // A restrained amber outer arc makes an enabled product-local LFO
    // visible without changing the original panel's black-and-brass knob.
    if (modulation_active_ && modulation_active_()) {
      canvas.set_stroke_color(pulp::canvas::Color::rgba8(216, 155, 69));
      canvas.set_line_width(std::max(1.0f, radius * 0.06f));
      canvas.stroke_arc(cx, cy, radius * 1.18f, -2.35f, 0.78f);
    }
  }

private:
  std::string caption_;
  float diameter_ = 0.0f;
  std::optional<pulp::canvas::Color> accent_;
  std::function<bool()> modulation_active_;
};

} // namespace detail

/// The hardware SEED control is a button with a latched mode indication.  In
/// Beadsy the source-backed three-state Seed Mode parameter is the semantic
/// authority, so the straight-on control cycles OFF → ON → LOCKED on click.
/// Keeping this as a native view (rather than a guessed SVG hit region) makes
/// automation/state changes repaint the LED as well as the button.
class SeedModeControl final : public pulp::view::View {
public:
  SeedModeControl() {
    set_id("beadsy-seed-mode");
    set_access_role(pulp::view::View::AccessRole::button);
    set_access_label("Seed Mode");
    set_focusable(true);
  }

  void set_mode(int mode) {
    // Seed Mode is a product control with three states (OFF, ON, LOCKED).
    // It is independent of the host audio-bus topology: AU-no-input and LV2
    // variants still expose the same seed-button semantics.
    mode_ = std::clamp(mode, 0, 2);
    const char *state = mode_ == 0 ? "off" : mode_ == 1 ? "on" : "locked";
    set_access_label(std::string("Seed Mode (") + state + "); click to cycle");
    request_repaint();
  }
  int mode() const { return mode_; }

  void set_on_press(std::function<void()> callback) {
    on_press_ = std::move(callback);
  }

  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    const float cx = b.width * 0.5f;
    const float radius = std::max(3.0f, std::min(b.width, b.height) * 0.32f);
    const float cy = b.height * 0.62f;
    const auto off = pulp::canvas::Color::rgba8(226, 225, 217);
    const auto on = pulp::canvas::Color::rgba8(246, 245, 235);
    canvas.set_fill_color(mode_ == 0 ? off : on);
    canvas.fill_circle(cx, cy, radius);
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(42, 43, 42));
    canvas.set_line_width(0.8f);
    canvas.stroke_circle(cx, cy, radius);

    const auto led = mode_ == 0   ? pulp::canvas::Color::rgba8(44, 43, 38)
                     : mode_ == 1 ? pulp::canvas::Color::rgba8(211, 164, 74)
                                  : pulp::canvas::Color::rgba8(103, 151, 168);
    canvas.set_fill_color(led);
    canvas.fill_circle(cx, std::max(2.0f, cy - radius * 1.65f),
                       std::max(1.2f, std::min(b.width, b.height) * 0.10f));
  }

  void on_mouse_down(pulp::view::Point) override {
    if (on_press_)
      on_press_();
  }
  bool wants_mouse_input() const override { return true; }

private:
  int mode_ = 0;
  std::function<void()> on_press_;
};

/// A native, source-backed FREEZE button.  The physical panel uses a large
/// top button; keeping this control procedural preserves the same silhouette
/// at every editor scale while the StateStore remains the automation
/// authority.
class FreezeControl final : public pulp::view::View {
public:
  FreezeControl() {
    set_id("beadsy-freeze");
    set_access_role(pulp::view::View::AccessRole::button);
    set_access_label("Freeze (off); click to toggle");
    set_focusable(true);
  }

  void set_pressed(bool pressed) {
    pressed_ = pressed;
    set_access_label(std::string("Freeze (") + (pressed ? "on" : "off") +
                     "); click to toggle");
    request_repaint();
  }
  bool pressed() const { return pressed_; }

  void set_on_press(std::function<void()> callback) {
    on_press_ = std::move(callback);
  }

  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    const float cx = b.width * 0.5f;
    const float radius = std::max(3.0f, std::min(b.width, b.height) * 0.34f);
    const float cy = b.height * 0.62f;
    const auto body = pressed_ ? pulp::canvas::Color::rgba8(196, 197, 187)
                               : pulp::canvas::Color::rgba8(239, 238, 228);
    canvas.set_fill_color(body);
    canvas.fill_circle(cx, cy, radius);
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(42, 43, 42));
    canvas.set_line_width(0.8f);
    canvas.stroke_circle(cx, cy, radius);
  }

  void on_mouse_down(pulp::view::Point) override {
    if (on_press_)
      on_press_();
  }
  bool wants_mouse_input() const override { return true; }

private:
  bool pressed_ = false;
  std::function<void()> on_press_;
};

/// Selects the software source path. Hardware Beads uses physical input
/// jacks; the plugin exposes the same choice explicitly for a stereo Main In
/// bus while retaining the internal wavetable fallback.
class ModeControl final : public pulp::view::View {
public:
  ModeControl() {
    set_id("beadsy-source-mode");
    set_access_role(pulp::view::View::AccessRole::button);
    set_access_label("Source: Synth (MIDI)");
    set_focusable(true);
  }

  void set_mode(int mode) {
    // Keep restored/host-written values inside the descriptor's declared
    // topology. AU instrument builds intentionally expose no input bus,
    // while the AU effect variant exposes Main In only.
    mode_ = std::clamp(mode, 0, kAvailableSourceModeMax);
    set_access_label(std::string("Source: ") +
                     (mode_ == 0   ? "Synth (MIDI)"
                      : mode_ == 1 ? "Main In (host routed)"
                                   : "Sidechain (host routed)"));
    request_repaint();
  }
  int mode() const { return mode_; }
  void set_on_press(std::function<void()> callback) {
    on_press_ = std::move(callback);
  }

  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    const auto body = mode_ == 0   ? pulp::canvas::Color::rgba8(38, 40, 43)
                      : mode_ == 1 ? pulp::canvas::Color::rgba8(28, 93, 101)
                                   : pulp::canvas::Color::rgba8(74, 71, 103);
    canvas.set_fill_color(body);
    canvas.fill_rounded_rect(0.5f, 0.5f, std::max(1.0f, b.width - 1.0f),
                             std::max(1.0f, b.height - 1.0f),
                             std::max(2.0f, b.height * 0.18f));
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(210, 207, 193));
    canvas.set_line_width(0.8f);
    canvas.stroke_rounded_rect(0.5f, 0.5f, std::max(1.0f, b.width - 1.0f),
                               std::max(1.0f, b.height - 1.0f),
                               std::max(2.0f, b.height * 0.18f));
    canvas.set_font_full("Roboto Mono", std::max(3.0f, b.height * 0.30f), 700,
                         0, 0.35f);
    canvas.set_text_align(pulp::canvas::TextAlign::center);
    canvas.set_fill_color(pulp::canvas::Color::rgba8(241, 239, 229));
    canvas.fill_text(mode_ == 0   ? "SOURCE  MIDI SYNTH"
                     : mode_ == 1 ? "SOURCE  MAIN IN"
                                  : "SOURCE  SIDECHAIN",
                     b.width * 0.5f, b.height * 0.63f);
  }

  void on_mouse_down(pulp::view::Point) override {
    if (on_press_)
      on_press_();
  }
  bool on_key_event(const pulp::view::KeyEvent &event) override {
    if (!event.is_down)
      return false;
    if (event.key == pulp::view::KeyCode::enter ||
        event.key == pulp::view::KeyCode::space) {
      if (on_press_)
        on_press_();
      return true;
    }
    return false;
  }
  bool wants_mouse_input() const override { return true; }

private:
  int mode_ = 0;
  std::function<void()> on_press_;
};

#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)

/// Control-thread picker for sources supplied by a Pulp host.
///
/// The catalog is deliberately injected by the host.  Beadsy never asks an
/// AU/VST3/CLAP adapter to enumerate a DAW project, and an external DAW can
/// leave the picker unattached while retaining ownership of its routing menu.
/// Catalog callbacks only mark the view dirty; the host/editor control thread
/// calls refresh_routes() (or refresh_if_needed()) to touch view state.
class AudioRoutePicker final : public pulp::view::ComboBox {
public:
  explicit AudioRoutePicker(std::size_t bus_index = 0) : bus_index_(bus_index) {
    set_id("beadsy-audio-route");
    set_access_label("Audio route: host-owned routing menu");
    set_access_disabled("true");
    set_items({"Host-owned routing menu"});
    set_selected_silent(0);
    on_change = [this](int index) { on_change_route(index); };
  }

  ~AudioRoutePicker() override { detach_catalog(); }

  /// Attach a host catalog.  The catalog contract has one callback slot;
  /// therefore observation is opt-in and is safe only when this picker owns
  /// a per-editor catalog instance.  Shared catalogs should pass false and
  /// call refresh_routes() from their host control-thread fan-out instead.
  void set_catalog(pulp::host::AudioRouteCatalog *catalog,
                   bool observe_changes = false) {
    detach_catalog();
    catalog_ = catalog;
    if (!catalog_) {
      set_items({"Host-owned routing menu"});
      set_selected_silent(0);
      update_enabled_state();
      return;
    }
    if (observe_changes) {
      observing_changes_ = true;
      catalog_->set_change_callback([this] {
        // AudioRouteCatalog is a control-thread seam.  Do not
        // enumerate or mutate a View from a notification; the next
        // control-thread refresh consumes this flag.
        refresh_pending_.store(true, std::memory_order_release);
      });
    }
    refresh_routes();
  }

  pulp::host::AudioRouteCatalog *catalog() const { return catalog_; }

  void set_bus_index(std::size_t bus_index) {
    if (bus_index_ == bus_index)
      return;
    bus_index_ = bus_index;
    if (catalog_)
      refresh_routes();
  }
  std::size_t bus_index() const { return bus_index_; }

  /// Re-enumerate on the host/editor control thread and preserve the
  /// catalog's connected stable ID when it is still present.
  void refresh_routes() {
    refresh_pending_.store(false, std::memory_order_release);
    if (!catalog_) {
      update_enabled_state();
      return;
    }

    const auto routes = catalog_->enumerate();
    const auto connected = catalog_->connected_route(bus_index_);
    route_ids_.clear();
    std::vector<std::string> labels;
    labels.emplace_back("Select host route…");
    int selected = 0;
    for (const auto &route : routes) {
      route_ids_.push_back(route.stable_id);
      std::string label =
          route.display_name.empty() ? route.stable_id : route.display_name;
      const char *kind = "Internal";
      switch (route.kind) {
      case pulp::host::AudioRouteKind::PhysicalInput:
        kind = "Input";
        break;
      case pulp::host::AudioRouteKind::Track:
        kind = "Track";
        break;
      case pulp::host::AudioRouteKind::Instrument:
        kind = "Instrument";
        break;
      case pulp::host::AudioRouteKind::Bus:
        kind = "Bus";
        break;
      case pulp::host::AudioRouteKind::File:
        kind = "File";
        break;
      case pulp::host::AudioRouteKind::Internal:
        break;
      }
      label = std::string(kind) + ": " + label;
      if (route.channels > 0)
        label += " (" + std::to_string(route.channels) + " ch)";
      if (!route.available)
        label += " — unavailable";
      labels.push_back(std::move(label));
      if (!connected.empty() && route.stable_id == connected)
        selected = static_cast<int>(route_ids_.size());
    }
    set_items(std::move(labels));
    set_selected_silent(selected);
    update_enabled_state();
    if (selected > 0)
      set_access_label("Audio route: " + selected_text() + " (host-owned)");
  }

  void refresh_if_needed() {
    if (refresh_pending_.load(std::memory_order_acquire))
      refresh_routes();
  }

  bool has_routes() const { return !route_ids_.empty(); }

  void set_on_route_connected(
      std::function<void(std::size_t, std::string_view)> callback) {
    on_route_connected_ = std::move(callback);
  }

private:
  void detach_catalog() {
    if (catalog_ && observing_changes_)
      catalog_->set_change_callback({});
    catalog_ = nullptr;
    observing_changes_ = false;
    route_ids_.clear();
    refresh_pending_.store(false, std::memory_order_release);
  }

  void update_enabled_state() {
    const bool enabled =
        catalog_ != nullptr && source_mode_ != 0 && has_routes();
    set_enabled(enabled);
    set_access_disabled(enabled ? "false" : "true");
    if (!catalog_) {
      set_access_label("Audio route: host-owned routing menu");
    } else if (source_mode_ == 0) {
      set_access_label(
          "Audio route: MIDI synth selected; host input routing unavailable");
    } else if (!has_routes()) {
      set_access_label("Audio route: no host sources available");
    }
  }

  std::size_t bus_index_ = 0;
  pulp::host::AudioRouteCatalog *catalog_ = nullptr;
  bool observing_changes_ = false;
  std::vector<std::string> route_ids_;
  std::atomic_bool refresh_pending_{false};
  int source_mode_ = 0;
  std::function<void(std::size_t, std::string_view)> on_route_connected_;

public:
  /// Keep the picker visible but inert while MIDI synth mode is selected.
  void set_source_mode(int mode) {
    source_mode_ = std::clamp(mode, 0, kAvailableSourceModeMax);
    set_bus_index(source_mode_ == 2 ? 1u : 0u);
    update_enabled_state();
  }

  void on_change_route(int index) {
    if (!catalog_ || index <= 0 || index > static_cast<int>(route_ids_.size()))
      return;
    const auto &route_id = route_ids_[static_cast<std::size_t>(index - 1)];
    if (!catalog_->connect(route_id, bus_index_)) {
      refresh_routes();
      set_access_label("Audio route unavailable; use the host routing menu");
      return;
    }
    if (on_route_connected_)
      on_route_connected_(bus_index_, route_id);
    set_access_label("Audio route: " + selected_text() + " (host-owned)");
  }
};

#endif

class InputBusControl final : public pulp::view::View {
public:
  InputBusControl() {
    set_id("beadsy-input-bus");
    set_access_role(pulp::view::View::AccessRole::button);
    set_focusable(true);
    set_source(0);
  }
  void set_source(int source) {
    source_ = source >= 1 ? 1 : 0;
    set_access_label(std::string("Audio bus: ") +
                     (source_ == 0 ? "Main In" : "Sidechain"));
    request_repaint();
  }
  int source() const { return source_; }
  void set_on_press(std::function<void()> callback) {
    on_press_ = std::move(callback);
  }
  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    canvas.set_fill_color(pulp::canvas::Color::rgba8(31, 35, 39));
    canvas.fill_rounded_rect(0.5f, 0.5f, std::max(1.0f, b.width - 1.0f),
                             std::max(1.0f, b.height - 1.0f), b.height * 0.18f);
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(112, 124, 130));
    canvas.set_line_width(0.8f);
    canvas.stroke_rounded_rect(0.5f, 0.5f, std::max(1.0f, b.width - 1.0f),
                               std::max(1.0f, b.height - 1.0f),
                               b.height * 0.18f);
    canvas.set_fill_color(pulp::canvas::Color::rgba8(232, 236, 229));
    canvas.set_font_full("Roboto Mono", std::max(3.0f, b.height * 0.28f), 700,
                         0, 0.25f);
    canvas.set_text_align(pulp::canvas::TextAlign::center);
    canvas.fill_text(source_ == 0 ? "BUS  MAIN IN" : "BUS  SIDECHAIN",
                     b.width * 0.5f, b.height * 0.63f);
  }
  void on_mouse_down(pulp::view::Point) override {
    if (on_press_)
      on_press_();
  }
  bool on_key_event(const pulp::view::KeyEvent &event) override {
    if (!event.is_down)
      return false;
    if (event.key == pulp::view::KeyCode::enter ||
        event.key == pulp::view::KeyCode::space) {
      if (on_press_)
        on_press_();
      return true;
    }
    return false;
  }
  bool wants_mouse_input() const override { return true; }

private:
  int source_ = 0;
  std::function<void()> on_press_;
};

/// Small editor-owned resize affordance. Hosts still decide whether the
/// resize request is accepted; the grip makes that capability discoverable in
/// standalone windows and hosts that expose the Pulp resize callback.
class ResizeGrip final : public pulp::view::View {
public:
  explicit ResizeGrip(Beadsy &processor) : processor_(processor) {
    set_id("beadsy-resize-grip");
    set_access_role(pulp::view::View::AccessRole::button);
    set_access_label("Resize editor");
  }

  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(86, 96, 102, 210));
    canvas.set_line_width(std::max(1.0f, b.width * 0.08f));
    for (int i = 1; i <= 3; ++i) {
      const float inset = b.width * (0.18f + 0.20f * static_cast<float>(i - 1));
      canvas.stroke_line(b.width - inset, b.height - 1.0f, b.width - 1.0f,
                         b.height - inset);
    }
  }

  void on_mouse_event(const pulp::view::MouseEvent &event) override {
    const bool press = event.phase == pulp::view::MousePhase::press ||
                       (event.phase == pulp::view::MousePhase::automatic &&
                        event.is_down && !dragging_);
    const bool release = event.phase == pulp::view::MousePhase::release ||
                         (event.phase == pulp::view::MousePhase::automatic &&
                          event.is_down && dragging_);
    if (press) {
      dragging_ = true;
      pointer_id_ = event.pointer_id;
      start_pointer_ = event.window_position;
      if (auto *root = detail::root_view(this)) {
        start_width_ = root->bounds().width;
        start_height_ = root->bounds().height;
      }
      set_pointer_capture(pointer_id_);
      return;
    }
    if (dragging_ && (event.phase == pulp::view::MousePhase::drag ||
                      event.has_movement_delta)) {
      const float dx = event.has_movement_delta
                           ? event.movement_x
                           : event.window_position.x - start_pointer_.x;
      const float dy = event.has_movement_delta
                           ? event.movement_y
                           : event.window_position.y - start_pointer_.y;
      const float delta = std::abs(dx) >= std::abs(dy) ? dx : dy;
      const float width = std::clamp(start_width_ + delta, 201.0f, 401.0f);
      const float height = width * 364.0f / 201.0f;
      processor_.request_editor_resize(static_cast<uint32_t>(width),
                                       static_cast<uint32_t>(height));
      return;
    }
    if (dragging_ && release) {
      release_pointer_capture(pointer_id_);
      dragging_ = false;
    }
  }
  bool wants_mouse_input() const override { return true; }

private:
  Beadsy &processor_;
  bool dragging_ = false;
  int pointer_id_ = 0;
  pulp::view::Point start_pointer_{};
  float start_width_ = 201.0f;
  float start_height_ = 364.0f;
};

class BeadsyEditor final : public pulp::view::View {
public:
  BeadsyEditor(Beadsy &processor, pulp::state::StateStore &store)
      : processor_(processor), store_(store),
        panel_(pulp::canvas::SvgImage::from_string(kBeadsyPanelSvg)) {
    set_id("beadsy-editor");
    set_background_color(pulp::canvas::Color::rgba8(22, 21, 19));
    build_controls();
    layout_controls();
    set_continuous_repaint(false);
  }

  void on_resized() override { layout_controls(); }
  void layout_children() override {
    pulp::view::View::layout_children();
#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
    if (route_picker_) {
      route_picker_->refresh_if_needed();
      const auto bus = static_cast<std::size_t>(
          source_mode_ && source_mode_->mode() == 2);
      if (route_picker_->catalog())
        processor_.set_route_identity(
            bus, route_picker_->catalog()->connected_route(bus));
    }
#endif
    layout_controls();
  }

  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    // The Illustrator/SVG panel is an opaque white faceplate in the physical
    // module, but the exported scene intentionally carries transparent paper
    // so its vector flourishes remain composable. Paint the substrate before
    // rendering the retained scene; otherwise the source artwork is shown
    // over the editor's dark fallback background and its fine ink disappears.
    canvas.set_fill_color(pulp::canvas::Color::rgba8(248, 248, 246, 255));
    canvas.fill_rect(0.0f, 0.0f, b.width, b.height);
    if (panel_.is_valid()) {
      panel_.render(canvas, 0.0f, 0.0f, b.width, b.height);
    } else {
      canvas.set_fill_color(pulp::canvas::Color::rgba8(22, 21, 19));
      canvas.fill_rect(0, 0, b.width, b.height);
    }
    if (std::string_view(kBeadsyPanelSourceStatus) ==
        "independent scaffold; authorized artwork unavailable") {
      paint_scaffold_labels(canvas, b.width, b.height);
    }
  }

  /// Target selected by the native right-click context interaction. Hosts
  /// can use this seam to present their native modulation menu.
  std::optional<pulp::state::ParamID> modulation_context_target() const {
    return context_target_;
  }
  void open_modulation_context(pulp::state::ParamID id) {
    context_target_ = id;
  }
  void clear_modulation_context() { context_target_.reset(); }
  ModeControl *source_mode_control() const { return source_mode_; }
#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
  AudioRoutePicker *audio_route_picker() const { return route_picker_; }
  void set_audio_route_catalog(pulp::host::AudioRouteCatalog *catalog,
                               bool observe_changes = false) {
    if (!route_picker_)
      return;
    const auto bus =
        static_cast<std::size_t>(source_mode_ && source_mode_->mode() == 2);
    const auto saved = processor_.route_identity(bus);
    if (catalog) {
      const auto connected = catalog->connected_route(bus);
      if (!saved.empty() && connected.empty())
        (void)catalog->connect(saved, bus);
      const auto rebound = catalog->connected_route(bus);
      if (!rebound.empty())
        processor_.set_route_identity(bus, rebound);
    }
    route_picker_->set_catalog(catalog, observe_changes);
  }
#endif

  /// Show the same menu used by a knob's real secondary-click handler.
  /// This is public so host/editor characterization tests can exercise the
  /// complete menu lifecycle without synthesizing platform pointer events.
  pulp::view::ContextMenu *
  show_modulation_context_menu(pulp::state::ParamID id,
                               pulp::view::Point position) {
    open_modulation_context(id);
    auto *root = detail::root_view(this);
    if (!root)
      return nullptr;
    const auto current = processor_.lfo(id);
    const auto *info = store_.info(id);
    const std::string target_name = info ? info->name : "Parameter";
    const auto rate_label = [&current] {
      std::ostringstream text;
      text << "Rate: " << current.rate_hz << " Hz";
      return text.str();
    };
    const auto depth_label = [&current] {
      std::ostringstream text;
      text << "Depth: " << current.depth;
      return text.str();
    };
    auto alive = import_binding_lifetime_token();
#if defined(PULP_CONTEXT_MENU_INLINE_CONTROLS)
    auto set_lfo_if_alive = [alive, this, id](auto update) {
      if (alive.expired())
        return;
      auto modulation = processor_.lfo(id);
      update(modulation);
      processor_.set_lfo(id, modulation);
      request_repaint();
    };
    auto items = std::vector<pulp::view::ContextMenu::Item>{
        pulp::view::ContextMenu::Item::make_toggle(
            1, "LFO Enabled", current.enabled,
            [set_lfo_if_alive](const auto &item) {
              set_lfo_if_alive([&](auto &modulation) {
                modulation.enabled = item.checked;
                if (modulation.enabled && modulation.depth == 0.0f)
                  modulation.depth = 0.1f;
              });
            }),
        pulp::view::ContextMenu::Item::make_slider(
            3, "Rate", (current.rate_hz - 0.01f) / 19.99f, 0.01f,
            [set_lfo_if_alive](const auto &item) {
              set_lfo_if_alive([&](auto &modulation) {
                modulation.rate_hz = 0.01f + item.normalized_value * 19.99f;
              });
            }),
        pulp::view::ContextMenu::Item::make_slider(
            4, "Depth", current.depth, 0.01f,
            [set_lfo_if_alive](const auto &item) {
              set_lfo_if_alive([&](auto &modulation) {
                modulation.depth = item.normalized_value;
              });
            }),
        {2, "Reset LFO"}};
    return pulp::view::ContextMenu::show(
        root, detail::local_to_root(this, position), std::move(items),
        [alive, this, id](std::optional<int> command) {
          if (alive.expired() || !command || *command != 2)
            return;
          processor_.clear_lfo(id);
          request_repaint();
          clear_modulation_context();
        });
#else
    return pulp::view::ContextMenu::show(
        root, detail::local_to_root(this, position),
        {{0, "LFO → " + target_name, false},
         {1, current.enabled ? "LFO Enabled" : "Enable LFO", true,
          current.enabled},
         {3, rate_label() + " ↓"},
         {4, rate_label() + " ↑"},
         {5, depth_label() + " ↓"},
         {6, depth_label() + " ↑"},
         {2, "Reset LFO"}},
        [alive, this, id](std::optional<int> command) {
          if (alive.expired())
            return;
          if (command) {
            auto modulation = processor_.lfo(id);
            if (*command == 1) {
              modulation.enabled = !modulation.enabled;
              if (modulation.depth == 0.0f)
                modulation.depth = 0.1f;
              processor_.set_lfo(id, modulation);
            } else if (*command == 3 || *command == 4) {
              modulation.rate_hz += *command == 3 ? -0.25f : 0.25f;
              modulation.rate_hz = std::clamp(modulation.rate_hz, 0.01f, 20.0f);
              processor_.set_lfo(id, modulation);
            } else if (*command == 5 || *command == 6) {
              modulation.depth += *command == 5 ? -0.05f : 0.05f;
              modulation.depth = std::clamp(modulation.depth, 0.0f, 1.0f);
              processor_.set_lfo(id, modulation);
            } else if (*command == 2) {
              processor_.clear_lfo(id);
            }
            // LFO state is product-local rather than StateStore-backed;
            // invalidate the native tree so the knob's modulation arc
            // reflects attach/detach and rate/depth edits immediately.
            request_repaint();
          }
          clear_modulation_context();
        });
#endif
  }

private:
  struct Placement {
    pulp::state::ParamID id;
    const char *name;
    float x;
    float y;
    float r;
  };

  static void paint_scaffold_labels(pulp::canvas::Canvas &canvas, float width,
                                    float height) {
    // SvgImage intentionally keeps the fallback asset free of runtime
    // font dependencies. Paint its labels through the native canvas so
    // the clean-room scaffold remains legible in screenshots and hosts;
    // rights-gated authoring SVGs are passed through without overlays.
    const float sx = width / 200.693f;
    const float sy = height / 364.252f;
    const float scale = std::min(sx, sy);
    const auto label = [&canvas, sx, sy,
                        scale](const char *value, float x, float y, float size,
                               pulp::canvas::Color color,
                               pulp::canvas::TextAlign align =
                                   pulp::canvas::TextAlign::center,
                               int weight = 500) {
      canvas.set_font_full("Roboto Mono", size * scale, weight, 0,
                           size > 4.0f ? 0.8f * scale : 0.55f * scale);
      canvas.set_text_align(align);
      canvas.set_fill_color(color);
      canvas.fill_text(value, x * sx, y * sy);
    };
    const auto ink = pulp::canvas::Color::rgba8(32, 35, 38);
    const auto accent = pulp::canvas::Color::rgba8(51, 75, 85);
    const auto muted = pulp::canvas::Color::rgba8(101, 112, 120);
    label("BEADSY", 100.3465f, 17.0f, 8.0f, ink,
          pulp::canvas::TextAlign::center, 700);
    label("MIDI GRAIN SYNTH", 100.3465f, 25.0f, 3.1f, accent);
    label("GRAIN", 100.3465f, 46.0f, 3.2f, accent,
          pulp::canvas::TextAlign::center, 700);
    // Top controls retain the original panel's visual hierarchy while
    // remaining Beadsy's independently authored labels and typography.
    label("FREEZE", 24.5f, 63.0f, 5.0f, ink, pulp::canvas::TextAlign::center,
          700);
    label("SEED", 176.2f, 63.0f, 5.0f, ink, pulp::canvas::TextAlign::center,
          700);
    label("DENSITY", 101.4f, 100.0f, 6.0f, ink, pulp::canvas::TextAlign::center,
          700);
    label("TIME", 33.8f, 142.0f, 6.0f, ink, pulp::canvas::TextAlign::center,
          700);
    label("PITCH", 168.9f, 142.0f, 6.0f, ink, pulp::canvas::TextAlign::center,
          700);
    label("SIZE", 69.8f, 201.0f, 6.0f, ink, pulp::canvas::TextAlign::center,
          700);
    label("SHAPE", 133.0f, 201.0f, 6.0f, ink, pulp::canvas::TextAlign::center,
          700);
    label("MIX / MOTION", 100.3465f, 217.0f, 3.2f, accent,
          pulp::canvas::TextAlign::center, 700);
    label("PARAMETER CONTROLS", 100.3465f, 245.0f, 3.0f, muted);
    label("48 KHZ  •  MONO VOICE  •  MIDI", 100.3465f, 344.0f, 3.0f, accent);
    label("INDEPENDENT PRODUCT SCAFFOLD", 100.3465f, 353.0f, 2.5f, muted);
  }

  void build_controls() {
    constexpr Placement placements[] = {
        {kDensity, "Density", 35.735f, 104.4f, 9.25f},
        {kTime, "Time", 11.915f, 90.1f, 9.25f},
        {kPitch, "Pitch", 59.515f, 90.1f, 9.25f},
        {kSize, "Size", 24.615f, 67.9f, 7.2f},
        {kShape, "Shape", 46.865f, 67.9f, 7.2f},
        {kFeedback, "Feedback", 7.115f, 67.9f, 3.0f},
        {kDryWet, "Dry Wet", 64.365f, 67.9f, 3.0f},
        {kReverb, "Reverb", 64.365f, 51.7f, 3.0f},
        {kTimeAmount, "Time CV Amount", 18.565f, 47.9f, 3.0f},
        {kSizeAmount, "Size CV Amount", 30.015f, 47.9f, 3.0f},
        {kShapeAmount, "Shape CV Amount", 41.465f, 47.9f, 3.0f},
        {kPitchAmount, "Pitch CV Amount", 52.915f, 47.9f, 3.0f},
    };
    for (const auto &p : placements) {
      const float diameter = p.r * 2.0f;
      auto knob = std::make_unique<detail::PanelKnob>(p.name, diameter);
      auto *raw = knob.get();
      if (std::string_view(p.name) == "Feedback")
        raw->set_accent(pulp::canvas::Color::rgba8(24, 165, 175));
      else if (std::string_view(p.name) == "Dry Wet")
        raw->set_accent(pulp::canvas::Color::rgba8(234, 166, 27));
      else if (std::string_view(p.name) == "Reverb")
        raw->set_accent(pulp::canvas::Color::rgba8(211, 59, 97));
      if (p.id != 0) {
        raw->set_modulation_indicator(
            [this, id = p.id] { return processor_.lfo(id).enabled; });
        raw->on_context_menu = [this, id = p.id](pulp::view::Point position) {
          show_modulation_context_menu(id, position);
        };
      } else {
        // Geometry-only controls remain visible for layout inspection,
        // but must not look or behave like guessed parameters. Keep
        // them explicitly disabled until an authoritative semantic
        // mapping is available from the source artwork/documentation.
        raw->set_enabled(false);
        // The accessibility bridge consumes the ARIA-compatible
        // boolean token, while the reason belongs in the label.
        raw->set_access_disabled("true");
        raw->set_access_label(std::string(p.name) +
                              " (Provisional control mapping)");
      }
      add_child(std::move(knob));
      if (p.id != 0)
        bindings_.push_back(pulp::view::bind_parameter(*raw, store_, p.id));
      controls_.push_back({raw, p.x, p.y, p.r});
    }

    // Keep the Seed control after the mapped geometry so existing panel
    // child ordering remains stable for hosts and screenshot probes.
    auto seed = std::make_unique<SeedModeControl>();
    seed_mode_ = seed.get();
    seed_mode_->set_mode(
        static_cast<int>(std::lround(store_.get_value(kSeedMode))));
    seed_mode_->set_on_press([this] {
      const int next = (seed_mode_->mode() + 1) % 3;
      store_.begin_gesture(kSeedMode);
      store_.set_value(kSeedMode, static_cast<float>(next));
      store_.end_gesture(kSeedMode);
    });
    seed_mode_binding_ = store_.add_listener(
        [this](pulp::state::ParamID changed, float) {
          if (changed == kSeedMode && seed_mode_)
            seed_mode_->set_mode(
                static_cast<int>(std::lround(store_.get_value(kSeedMode))));
        },
        pulp::state::ListenerThread::Main,
        pulp::state::ListenerRestoreBehavior::Reconcile);
    add_child(std::move(seed));

    auto freeze = std::make_unique<FreezeControl>();
    freeze_mode_ = freeze.get();
    freeze_mode_->set_pressed(store_.get_value(kFreeze) >= 0.5f);
    freeze_mode_->set_on_press([this] {
      store_.begin_gesture(kFreeze);
      store_.set_value(kFreeze, freeze_mode_->pressed() ? 0.0f : 1.0f);
      store_.end_gesture(kFreeze);
    });
    freeze_mode_binding_ = store_.add_listener(
        [this](pulp::state::ParamID changed, float value) {
          if (changed == kFreeze && freeze_mode_)
            freeze_mode_->set_pressed(value >= 0.5f);
        },
        pulp::state::ListenerThread::Main,
        pulp::state::ListenerRestoreBehavior::Reconcile);
    add_child(std::move(freeze));

    auto mode = std::make_unique<ModeControl>();
    source_mode_ = mode.get();
    source_mode_->set_mode(
        static_cast<int>(std::lround(store_.get_value(kMode))));
    source_mode_->set_on_press([this] {
      store_.begin_gesture(kMode);
      const int next =
          (source_mode_->mode() + 1) % (kAvailableSourceModeMax + 1);
      store_.set_value(kMode, static_cast<float>(next));
      store_.end_gesture(kMode);
#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
      if (route_picker_)
        route_picker_->set_source_mode(next);
#endif
      // Keep the legacy bus parameter coherent for old hosts/states;
      // the visible source control is now the single semantic choice.
      if (next >= 1) {
        store_.begin_gesture(kInputSource);
        store_.set_value(kInputSource, next == 2 ? 1.0f : 0.0f);
        store_.end_gesture(kInputSource);
      }
    });
    source_mode_binding_ = store_.add_listener(
        [this](pulp::state::ParamID changed, float value) {
          if (changed == kMode && source_mode_) {
            source_mode_->set_mode(static_cast<int>(std::lround(value)));
#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
            if (route_picker_)
              route_picker_->set_source_mode(
                  static_cast<int>(std::lround(value)));
#endif
          }
        },
        pulp::state::ListenerThread::Main,
        pulp::state::ListenerRestoreBehavior::Reconcile);
    add_child(std::move(mode));

#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
    auto route_picker = std::make_unique<AudioRoutePicker>();
    route_picker_ = route_picker.get();
    route_picker_->set_on_route_connected(
        [this](std::size_t bus, std::string_view stable_id) {
          processor_.set_route_identity(bus, stable_id);
        });
    route_picker_->set_source_mode(
        static_cast<int>(std::lround(store_.get_value(kMode))));
    add_child(std::move(route_picker));
#endif

    auto grip = std::make_unique<ResizeGrip>(processor_);
    resize_grip_ = grip.get();
    add_child(std::move(grip));
  }

  void layout_controls() {
    const float width =
        local_bounds().width > 1.0f ? local_bounds().width : 201.0f;
    const float height =
        local_bounds().height > 1.0f ? local_bounds().height : 364.0f;
    const float sx = width / 70.8f;
    const float sy = height / 128.5f;
    for (const auto &c : controls_) {
      const float diameter = c.radius * 2.0f * std::min(sx, sy);
      const float x = c.x * sx - diameter * 0.5f;
      const float y = (128.5f - c.y) * sy - diameter * 0.5f;
      c.knob->set_position(pulp::view::View::Position::absolute);
      c.knob->set_bounds({x, y, diameter, diameter * 1.25f});
      c.knob->flex().preferred_width = diameter;
      c.knob->flex().preferred_height = diameter * 1.25f;
    }
    if (seed_mode_) {
      const float width = 10.0f * std::min(sx, sy);
      const float height = 8.5f * std::min(sx, sy);
      const float x = 62.0f * sx - width * 0.5f;
      const float y = (128.5f - 110.5f) * sy - height * 0.5f;
      seed_mode_->set_position(pulp::view::View::Position::absolute);
      seed_mode_->set_bounds({x, y, width, height});
    }
    if (freeze_mode_) {
      const float width = 10.0f * std::min(sx, sy);
      const float height = 8.5f * std::min(sx, sy);
      const float x = 8.7f * sx - width * 0.5f;
      const float y = (128.5f - 110.5f) * sy - height * 0.5f;
      freeze_mode_->set_position(pulp::view::View::Position::absolute);
      freeze_mode_->set_bounds({x, y, width, height});
    }
    if (source_mode_) {
      const float unit = std::min(sx, sy);
      const float mode_width = 26.0f * unit;
      const float mode_height = 5.5f * unit;
      // Software source selection occupies the reclaimed lower jack area;
      // keep the original printed header unobstructed.
      const float x = 35.4f * sx - mode_width * 0.5f;
      const float y = (128.5f - 8.0f) * sy - mode_height * 0.5f;
      source_mode_->set_position(pulp::view::View::Position::absolute);
      source_mode_->set_bounds({x, y, mode_width, mode_height});
    }
#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
    if (route_picker_) {
      const float unit = std::min(sx, sy);
      const float route_width = 26.0f * unit;
      const float route_height = 5.5f * unit;
      const float x = 64.0f * sx - route_width * 0.5f;
      const float y = (128.5f - 8.0f) * sy - route_height * 0.5f;
      route_picker_->set_position(pulp::view::View::Position::absolute);
      route_picker_->set_bounds({x, y, route_width, route_height});
    }
#endif
    if (resize_grip_) {
      const float grip = std::max(8.0f, 7.0f * std::min(sx, sy));
      resize_grip_->set_position(pulp::view::View::Position::absolute);
      resize_grip_->set_bounds({width - grip, height - grip, grip, grip});
    }
  }

  struct Control {
    detail::PanelKnob *knob;
    float x;
    float y;
    float radius;
  };
  Beadsy &processor_;
  pulp::state::StateStore &store_;
  pulp::canvas::SvgImage panel_;
  std::vector<Control> controls_;
  std::vector<pulp::view::ParameterBinding> bindings_;
  SeedModeControl *seed_mode_ = nullptr;
  pulp::state::ListenerToken seed_mode_binding_;
  FreezeControl *freeze_mode_ = nullptr;
  pulp::state::ListenerToken freeze_mode_binding_;
  ModeControl *source_mode_ = nullptr;
#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
  AudioRoutePicker *route_picker_ = nullptr;
#endif
  ResizeGrip *resize_grip_ = nullptr;
  pulp::state::ListenerToken source_mode_binding_;
  std::optional<pulp::state::ParamID> context_target_;
};

} // namespace beadsy

#endif // !PULP_HEADLESS
