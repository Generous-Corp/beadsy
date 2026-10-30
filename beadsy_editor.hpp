#pragma once

#if !defined(PULP_HEADLESS)

#include "beadsy.hpp"
#include "beadsy_panel_svg.hpp"
#if __has_include("beadsy_panel_png.hpp")
#include "beadsy_panel_png.hpp"
#define BEADSY_HAS_PANEL_PNG 1
#endif

#include <pulp/canvas/canvas.hpp>
#include <pulp/canvas/svg.hpp>
#include <pulp/view/context_menu.hpp>
#include <pulp/view/parameter_binding.hpp>
#include <pulp/view/ui_components.hpp>
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
  // The converted panel artwork is consistently about 1.8 pt left of the
  // DXF-to-view affine fit. Keep that calibration in the editor layout rather
  // than nudging individual controls and losing the relationship between the
  // source map and the native hit regions.
  // Utility controls are overlaid on the authored color strips.  The former
  // 0.64 mm global correction put their black caps visibly left of those
  // strips at Logic's reference scale; keep the shared affine fit nearly
  // neutral and let the source coordinates provide the alignment.
  static constexpr float kSourceXCorrectionMm = 0.10f;
  static constexpr float kSourceYCorrectionMm = 0.24f;
  static constexpr float kUtilityPaintRadiusFraction = 0.50f;

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

  static float utility_paint_radius_for_bounds(float width, float height) {
    // Utility caps cover the source panel's outlined footprint.  Their view
    // bounds remain a little taller so the colored strip and printed icon can
    // continue below the cap without changing the hit target.
    return std::min(width, height * 0.78f) *
           kUtilityPaintRadiusFraction;
  }

  // Utility pots are identified in source panel units, rather than by their
  // rendered pixel radius.  That keeps the black-cap treatment stable when
  // the editor is resized or captured at a different scale.
  static constexpr bool is_utility_diameter(float diameter) {
    return diameter <= 6.0f;
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
  void set_modulation_phase(std::function<float()> phase) {
    modulation_phase_ = std::move(phase);
  }
  void set_accent(std::optional<pulp::canvas::Color> accent) {
    accent_ = accent;
  }

  // Some AUv2 hosts deliver the secondary click to the embedded control but
  // do not run Pulp's root context dispatcher. Handle that case at the knob
  // boundary while preserving the normal Knob gesture path for left clicks.
  void on_mouse_event(const pulp::view::MouseEvent &event) override {
    if (event.button == pulp::view::MouseButton::right && event.isPress()) {
      if (on_context_menu)
        on_context_menu(event.position);
      return;
    }
    pulp::view::Knob::on_mouse_event(event);
  }

  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    const float cx = b.width * 0.5f;
    const bool utility_knob = is_utility_diameter(diameter_);
    // Utility pots sit at the top of a longer painted PCB strip. Keep their
    // cap anchored to the mapped diameter while allowing the strip to extend
    // below it; using the tall bounds' centre here made the strip peek above
    // the black cap at fractional editor scales.
    const float cy = utility_knob ? b.width * 0.5f : b.height * 0.43f;
    const float radius = utility_knob
                             ? utility_paint_radius_for_bounds(b.width, b.height)
                             : paint_radius_for_bounds(b.width, b.height);
    const float angle =
        pulp::view::Knob::start_angle +
        (pulp::view::Knob::end_angle - pulp::view::Knob::start_angle) * value();
    // `radius` is in rendered pixels and therefore changes with editor scale.
    // The source-mapped diameter is the stable distinction between the seven
    // utility pots and the four larger Beads controls.

    // Auxiliary controls use a colored side strip only when the control is a
    // full-size indicator. The seven small utility pots in the reference are
    // entirely black with a single white pointer; allowing an accent behind
    // those caps makes color bleed above or beside the hardware silhouette.
    if (accent_ && !utility_knob) {
      canvas.set_fill_color(*accent_);
      const float top = cy + radius * 0.28f;
      const float bottom = b.height - radius * 0.18f;
      canvas.fill_rounded_rect(cx - radius * 0.66f, top, radius * 1.32f,
                               std::max(1.0f, bottom - top), radius * 0.58f);
    }

    // The seven utility controls are plain black pots with a white pointer.
    // They do not inherit the large Beads pot's pale cap, grooves, or halo.
    if (utility_knob) {
      canvas.set_fill_color(pulp::canvas::Color::rgba8(14, 14, 14));
      canvas.fill_circle(cx, cy, radius * 1.04f);
      const float ux = std::cos(angle);
      const float uy = std::sin(angle);
      const float px = -uy;
      const float py = ux;
      const float inner = radius * 0.26f;
      const float outer = radius * 0.89f;
      const float half_width = radius * 0.13f;
      const pulp::canvas::Canvas::Point2D pointer[] = {
          {cx + ux * inner + px * half_width, cy + uy * inner + py * half_width},
          {cx + ux * inner - px * half_width, cy + uy * inner - py * half_width},
          {cx + ux * outer - px * half_width, cy + uy * outer - py * half_width},
          {cx + ux * outer + px * half_width, cy + uy * outer + py * half_width},
      };
      canvas.set_fill_color(pulp::canvas::Color::rgba8(242, 240, 227));
      canvas.fill_path(pointer, 4);
      return;
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
      const float phase = modulation_phase_ ? modulation_phase_() : 0.0f;
      canvas.set_stroke_color(pulp::canvas::Color::rgba8(216, 155, 69));
      canvas.set_line_width(std::max(1.0f, radius * 0.06f));
      const float start = -2.35f + phase * 6.28318530718f;
      canvas.stroke_arc(cx, cy, radius * 1.18f, start, start + 0.78f);
    }
  }

private:
  std::string caption_;
  float diameter_ = 0.0f;
  std::optional<pulp::canvas::Color> accent_;
  std::function<bool()> modulation_active_;
  std::function<float()> modulation_phase_;
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
    // Match the authored panel's yellow-green lit state instead of changing
    // only the accessibility label when Seed is enabled.
    const auto on = pulp::canvas::Color::rgba8(205, 214, 103);
    canvas.set_fill_color(mode_ == 0 ? off : on);
    canvas.fill_circle(cx, cy, radius);
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(42, 43, 42));
    canvas.set_line_width(0.8f);
    canvas.stroke_circle(cx, cy, radius);

    // The original SEED control is a plain white push button. The black
    // mounting dot above it belongs to the faceplate and is not an LED.
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
    const auto body = pressed_ ? pulp::canvas::Color::rgba8(205, 214, 103)
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

/// The physical lower-left control is a momentary black push button, not a
/// seventh miniature pot. Keep it separate from PanelKnob so it has no
/// parameter pointer, grooves, pale cap, or automation gesture.
class PushButtonControl final : public pulp::view::View {
public:
  PushButtonControl() {
    set_id("beadsy-push-button");
    set_access_role(pulp::view::View::AccessRole::button);
    set_access_label("Quality: 16-bit; click to cycle");
    set_focusable(true);
    set_cursor(pulp::view::View::CursorStyle::pointer);
  }
  void set_quality(int quality) {
    quality_ = std::clamp(quality, 0, 3);
    static constexpr const char *names[] = {"4-bit", "12-bit", "7-bit", "16-bit"};
    set_access_label(std::string("Quality: ") + names[quality_] +
                     "; click to cycle");
    request_repaint();
  }
  int quality() const { return quality_; }
  void set_on_press(std::function<void()> callback) {
    on_press_ = std::move(callback);
  }
  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    const float r = std::max(2.0f, std::min(b.width, b.height) * 0.40f);
    const float cx = b.width * 0.5f;
    const float cy = b.height * 0.5f;
    // This station is one circular push button. Its face carries the selected
    // quality color; there is no second LED that can be mistaken for another
    // control or leave the state ambiguous.
    static constexpr pulp::canvas::Color colors[] = {
        pulp::canvas::Color::rgba8(48, 174, 210),
        pulp::canvas::Color::rgba8(205, 214, 103),
        pulp::canvas::Color::rgba8(234, 166, 27),
        pulp::canvas::Color::rgba8(211, 59, 97),
    };
    canvas.set_fill_color(colors[quality_]);
    canvas.fill_circle(cx, cy, r);
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(52, 52, 52));
    canvas.set_line_width(std::max(0.7f, r * 0.10f));
    canvas.stroke_circle(cx, cy, r);
  }
  void on_mouse_down(pulp::view::Point) override {
    if (on_press_)
      on_press_();
  }
  bool wants_mouse_input() const override { return true; }

private:
  int quality_ = 3;
  std::function<void()> on_press_;
};

/// The reclaimed lower-left M control selects the assignable modulation
/// destination. It remains a real button even though the physical CV jack is
/// absent in software; the state is exposed for automation/UI feedback while
/// the DSP mapping remains deliberately product-local.
class AssignButtonControl final : public pulp::view::View {
public:
  AssignButtonControl() {
    set_id("beadsy-assign-button");
    set_access_role(pulp::view::View::AccessRole::button);
    set_access_label("CV Assign (Feedback); click to cycle");
    set_focusable(true);
    set_cursor(pulp::view::View::CursorStyle::pointer);
  }
  int destination() const { return destination_; }
  void on_mouse_down(pulp::view::Point) override {
    destination_ = (destination_ + 1) % 3;
    static constexpr const char *names[] = {"Feedback", "Dry/Wet", "Reverb"};
    set_access_label(std::string("CV Assign (") + names[destination_] +
                     "); click to cycle");
    request_repaint();
  }
  bool wants_mouse_input() const override { return true; }
  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    const float r = std::max(2.0f, std::min(b.width, b.height) * 0.42f);
    static constexpr pulp::canvas::Color destination_colors[] = {
        pulp::canvas::Color::rgba8(24, 165, 175),
        pulp::canvas::Color::rgba8(234, 166, 27),
        pulp::canvas::Color::rgba8(211, 59, 97),
    };
    // This is a three-state button, not a knob plus a separate LED.  The
    // complete circular face carries the destination colour so its state is
    // visible in the same way as the quality button.
    canvas.set_fill_color(destination_colors[destination_]);
    canvas.fill_circle(b.width * 0.5f, b.height * 0.5f, r);
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(42, 43, 42));
    canvas.set_line_width(std::max(0.7f, r * 0.10f));
    canvas.stroke_circle(b.width * 0.5f, b.height * 0.5f, r);
  }
private:
  int destination_ = 0;
};

/// Two small center indicators present below DENSITY: a pale LED and a black
/// round button. They are visual hardware affordances with no guessed DSP
/// semantics, so they remain non-parameterized until documentation maps them.
class CenterIndicators final : public pulp::view::View {
public:
  static constexpr float kLedXFraction = 0.23f;
  static constexpr float kButtonXFraction = 0.77f;
  static constexpr float kLedRadiusMm = 1.60f;
  static constexpr float kButtonRadiusMm = 3.72f;

  CenterIndicators() {
    set_id("beadsy-center-indicators");
    set_pointer_events(pulp::view::View::PointerEvents::none);
    set_access_role(pulp::view::View::AccessRole::image);
    set_access_label("Status indicators");
  }
  void set_quality(int quality) {
    quality_ = std::clamp(quality, 0, 3);
    request_repaint();
  }
  void paint(pulp::canvas::Canvas &canvas) override {
    // The quality station owns its state now. Keep this compatibility child
    // empty so old layout/accessibility probes remain harmless while no
    // separate LED is painted.
  }
  bool wants_mouse_input() const override { return false; }
private:
  int quality_ = 3;
};

/// Selects the software source path. Hardware Beads uses physical input
/// jacks; the plugin exposes the same choice explicitly for a stereo Main In
/// bus while retaining the internal wavetable fallback.
class ModeControl final : public pulp::view::ComboBox {
public:
  ModeControl() {
    set_id("beadsy-source-mode");
    std::vector<std::string> modes{"MIDI Synth"};
    if (kAvailableSourceModeMax >= 1)
      modes.emplace_back("Main In");
    if (kAvailableSourceModeMax >= 2)
      modes.emplace_back("Sidechain");
    set_items(std::move(modes));
    set_selected_silent(0);
    set_access_label("Source: Synth (MIDI)");
    on_change = [this](int index) {
      mode_ = std::clamp(index, 0, kAvailableSourceModeMax);
      update_access_label();
      if (on_mode_change_)
        on_mode_change_(mode_);
    };
  }

  void set_mode(int mode) {
    // Keep restored/host-written values inside the descriptor's declared
    // topology. AU instrument builds intentionally expose no input bus,
    // while the AU effect variant exposes Main In only.
    mode_ = std::clamp(mode, 0, kAvailableSourceModeMax);
    set_selected_silent(mode_);
    update_access_label();
  }
  int mode() const { return mode_; }
  void set_on_mode_change(std::function<void(int)> callback) {
    on_mode_change_ = std::move(callback);
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
    canvas.set_font_full("Roboto Mono", std::max(3.0f, b.height * 0.40f), 700,
                         0, 0.35f);
    canvas.set_text_align(pulp::canvas::TextAlign::center);
    canvas.set_fill_color(pulp::canvas::Color::rgba8(241, 239, 229));
    canvas.fill_text(mode_ == 0   ? "MIDI"
                     : mode_ == 1 ? "MAIN IN"
                                  : "SIDECHAIN",
                     b.width * 0.5f, b.height * 0.63f);
    canvas.set_fill_color(pulp::canvas::Color::rgba8(241, 239, 229));
    const float arrow_x = b.width - std::max(2.0f, b.height * 0.34f);
    const float arrow_y = b.height * 0.50f;
    const pulp::canvas::Canvas::Point2D arrow[] = {
        {arrow_x - b.height * 0.16f, arrow_y - b.height * 0.10f},
        {arrow_x + b.height * 0.16f, arrow_y - b.height * 0.10f},
        {arrow_x, arrow_y + b.height * 0.14f},
    };
    canvas.fill_path(arrow, 3);
    // ComboBox's inherited event path owns the dropdown. The compact painted
    // field keeps the faceplate treatment while the popup itself gets the
    // shared Pulp checkmark/hover/keyboard behavior.
  }

private:
  void update_access_label() {
    set_access_label(std::string("Source: ") +
                     (mode_ == 0   ? "Synth (MIDI)"
                      : mode_ == 1 ? "Main In (host routed)"
                                   : "Sidechain (host routed)"));
    request_repaint();
  }

  int mode_ = 0;
  std::function<void(int)> on_mode_change_;
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
  explicit AudioRoutePicker(std::size_t bus_index = 0,
                            std::size_t channel_index = 0)
      : bus_index_(bus_index), channel_index_(channel_index) {
    set_id(std::string("beadsy-audio-route-") +
           (channel_index_ == 0 ? "left" : "right"));
    set_access_label(channel_label() + ": host-owned routing menu");
    set_access_disabled("true");
    // Keep the no-catalog state legible inside the compact lower bay.  The
    // accessibility label carries the full host-routing explanation; the
    // visible placeholder must fit without an ellipsis or implying a hidden
    // hardware jack.
    set_items({placeholder_label()});
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
      set_items({placeholder_label()});
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
    const auto connected = [&] {
      if constexpr (requires {
                      catalog_->connected_route_channel(bus_index_,
                                                        channel_index_);
                    }) {
        return catalog_->connected_route_channel(bus_index_, channel_index_);
      } else {
        return catalog_->connected_route(bus_index_);
      }
    }();
    route_ids_.clear();
    std::vector<std::string> labels;
    labels.emplace_back(channel_label());
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
      set_access_label(channel_label() + ": " + selected_text() +
                       " (host-owned)");
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

  void set_on_channel_route_connected(
      std::function<void(std::size_t, std::size_t, std::string_view)> callback) {
    on_channel_route_connected_ = std::move(callback);
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
      set_access_label(channel_label() + ": host-owned routing menu");
    } else if (source_mode_ == 0) {
      set_access_label(
          channel_label() +
          ": MIDI synth selected; host input routing unavailable");
    } else if (!has_routes()) {
      set_access_label(channel_label() + ": no host sources available");
    }
  }

  std::string channel_label() const {
    return channel_index_ == 0 ? "Input L" : "Input R";
  }

  std::string placeholder_label() const {
    // The printed IN L / IN R legends already identify the channel.  A short
    // source token keeps the native ComboBox fully legible at the panel's
    // reference width while the accessibility label remains descriptive.
    // The printed IN L / IN R legends sit immediately above these fields.
    // Keep the live value short enough to remain fully visible at the
    // reference width while retaining an unmistakable channel distinction.
    return channel_index_ == 0 ? "L IN" : "R IN";
  }

  std::size_t bus_index_ = 0;
  std::size_t channel_index_ = 0;
  pulp::host::AudioRouteCatalog *catalog_ = nullptr;
  bool observing_changes_ = false;
  std::vector<std::string> route_ids_;
  std::atomic_bool refresh_pending_{false};
  int source_mode_ = 0;
  std::function<void(std::size_t, std::string_view)> on_route_connected_;
  std::function<void(std::size_t, std::size_t, std::string_view)>
      on_channel_route_connected_;

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
    const bool connected = [&] {
      if constexpr (requires {
                      catalog_->connect_channel(route_id, bus_index_,
                                                channel_index_);
                    }) {
        return catalog_->connect_channel(route_id, bus_index_, channel_index_);
      } else {
        return catalog_->connect(route_id, bus_index_);
      }
    }();
    if (!connected) {
      refresh_routes();
      set_access_label(channel_label() +
                       ": route unavailable; use the host routing menu");
      return;
    }
    if (on_route_connected_)
      on_route_connected_(bus_index_, route_id);
    if (on_channel_route_connected_)
      on_channel_route_connected_(bus_index_, channel_index_, route_id);
    set_access_label(channel_label() + ": " + selected_text() +
                     " (host-owned)");
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
    if (route_picker_left_ || route_picker_right_) {
      if (route_picker_left_)
        route_picker_left_->refresh_if_needed();
      if (route_picker_right_)
        route_picker_right_->refresh_if_needed();
      const auto bus = static_cast<std::size_t>(
          source_mode_ && source_mode_->mode() == 2);
      // Pickers are channel-indexed, while the catalog's legacy
      // connected_route() accessor is bus-indexed.  Prefer the channel-aware
      // accessor so independent L/R selections survive refreshes and so a
      // sidechain refresh cannot overwrite the main-bus identity.  The
      // fallback keeps this compatible with older host catalog SDKs.
      for (std::size_t channel = 0; channel < 2; ++channel) {
        auto *picker = channel == 0 ? route_picker_left_ : route_picker_right_;
        if (!picker || !picker->catalog())
          continue;
        const auto identity = [&] {
          if constexpr (requires {
                          picker->catalog()->connected_route_channel(bus,
                                                                     channel);
                        }) {
            return picker->catalog()->connected_route_channel(bus, channel);
          } else {
            return picker->catalog()->connected_route(bus);
          }
        }();
        processor_.set_route_identity(channel, identity);
      }
    }
#endif
    layout_controls();
  }

  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    // Keep the authored faceplate aspect ratio invariant. Hosts are allowed
    // to hand an editor an arbitrary rectangle (especially while restoring a
    // saved plug-in window); stretching the SVG independently in X/Y makes
    // its small printed symbols drift away from the native controls and can
    // clip the utility artwork at the edge of a narrow resize. The native
    // controls use this same content rect below.
    const float content_scale =
        std::min(b.width / 200.693f, b.height / 364.252f);
    const float content_width = 200.693f * content_scale;
    const float content_height = 364.252f * content_scale;
    const float content_x = (b.width - content_width) * 0.5f;
    const float content_y = (b.height - content_height) * 0.5f;
    // The Illustrator/SVG panel is an opaque white faceplate in the physical
    // module, but the exported scene intentionally carries transparent paper
    // so its vector flourishes remain composable. Paint the substrate before
    // rendering the retained scene; otherwise the source artwork is shown
    // over the editor's dark fallback background and its fine ink disappears.
    canvas.set_fill_color(pulp::canvas::Color::rgba8(248, 248, 246, 255));
    canvas.fill_rect(0.0f, 0.0f, b.width, b.height);
    bool drew_panel = false;
#if defined(BEADSY_HAS_PANEL_PNG)
    if (canvas.supports_image_draw()) {
      drew_panel = canvas.draw_image_from_static_data_rect(
          panel_asset::kPng, panel_asset::kPngSize, 0.0f, 0.0f, 2203.0f,
          4000.0f, content_x, content_y, content_width, content_height);
    }
#endif
    if (!drew_panel && panel_.is_valid()) {
      panel_.render(canvas, content_x, content_y, content_width,
                    content_height);
      drew_panel = true;
    }
    if (!drew_panel) {
      canvas.set_fill_color(pulp::canvas::Color::rgba8(22, 21, 19));
      canvas.fill_rect(0, 0, b.width, b.height);
    }
    // Keep the faceplate's cyan, orange, and magenta utility strips and their
    // printed symbols. They are part of the reviewed straight-on artwork and
    // sit below the three black utility pots; the native caps are painted on
    // top of the actual control footprints, so no color can bleed above a
    // knob. Earlier broad tail masks erased those details and made the panel
    // visibly unlike the source.
    // The lower jack field is a host-routing surface in software. Remove the
    // physical socket and dotted-wire artwork while retaining the original
    // printed IN L / IN R legends and the bottom border. A
    // broad lower-panel wipe used here previously erased those legends and
    // left a conspicuous blank block in the editor.
    // The imported panel is composited over the same warm faceplate substrate
    // painted above.  Using pure white here leaves visible rectangular seams
    // wherever a removed construction line or jack is masked.
    canvas.set_fill_color(pulp::canvas::Color::rgba8(248, 248, 246, 255));
    const float panel_sx = content_width / 70.8f;
    const float panel_sy = content_height / 128.5f;
    const auto panel_rect = [&](float x, float y, float w, float h) {
      canvas.fill_rect(content_x + x * panel_sx,
                       content_y + (128.5f - (y + h)) * panel_sy,
                       w * panel_sx, h * panel_sy);
    };
    // Leave the source artwork beneath the large TIME/PITCH pots intact. The
    // printed rosettes and colored geometry run directly under the hardware
    // caps in the reference panel; clearing circular regions here introduced
    // the visible white padding ring and erased those details.
    // Keep the original dotted runs.  They are printed artwork that visually
    // explains the relationship between each utility control and its former
    // hardware destination; removing the jacks does not remove those guides.
    // A previous broad mask erased the guides and clipped the cyan/yellow/
    // magenta source symbols below them.  The jack silhouettes are cleared by
    // the small, local masks below instead.
    // The physical socket row and the grey output-jack illustration are not
    // functional in a software plug-in. Keep the printed labels above it.
    // Four old hardware socket outlines sit below the printed labels. Clear
    // their silhouettes with small circular masks so the label glyphs remain
    // completely intact.
    for (const float x : {6.5f, 18.0f, 33.4f, 46.0f}) {
      canvas.fill_circle(content_x + x * panel_sx,
                         content_y + (128.5f - 17.2f) * panel_sy,
                        3.4f * std::min(panel_sx, panel_sy));
    }
    // The old output-jack illustration occupies the right-most lower bay;
    // remove that bay while preserving the panel border and printed density
    // label above it.
    panel_rect(47.0f, 0.0f, 23.8f, 31.0f);
    // Clear the complete lower divider. The old mask stopped at the center
    // of the panel, leaving the imported line visibly coloured on its right
    // half in Logic captures.
    panel_rect(4.0f, 16.7f, 62.8f, 2.2f);
    // Replace the partly grey, partly white lower border as one continuous
    // faceplate stroke after masking the old output bay.
    panel_rect(3.5f, 9.0f, 65.3f, 1.1f);
    canvas.set_fill_color(pulp::canvas::Color::rgba8(189, 188, 188));
    panel_rect(3.5f, 9.25f, 65.3f, 0.60f);
    canvas.set_fill_color(pulp::canvas::Color::rgba8(248, 248, 246, 255));
    // FREEZE and SEED were labels for removed hardware jack controls. Keep
    // the stereo IN L / IN R legends above the software route pickers, while
    // removing only these two obsolete legends from the reclaimed row.
    panel_rect(22.0f, 22.0f, 13.0f, 5.0f);
    panel_rect(36.0f, 22.0f, 11.0f, 5.0f);
    if (std::string_view(kBeadsyPanelSourceStatus) ==
        "independent scaffold; authorized artwork unavailable") {
      paint_scaffold_labels(canvas, b.width, b.height);
    }
    // The original lower bay used this space for the two hardware jack
    // legends that software replaces. Keep the new source selector explicit
    // and aligned with the existing IN L / IN R legends.
    canvas.set_font_full("Roboto", std::max(3.0f, 2.8f * std::min(panel_sx, panel_sy)),
                         700, 0, 0.35f);
    canvas.set_text_align(pulp::canvas::TextAlign::center);
    canvas.set_fill_color(pulp::canvas::Color::rgba8(32, 32, 32, 255));
    canvas.fill_text("SOURCE", content_x + 35.4f * panel_sx,
                     content_y + (128.5f - 22.0f) * panel_sy);
    // Reassert the software route legends at the exact centers of their
    // fields. The imported artwork's right legend can drift by a few pixels
    // at narrow host sizes; these labels share the picker geometry below.
    canvas.fill_text("IN L", content_x + 12.0f * panel_sx,
                     content_y + (128.5f - 13.0f) * panel_sy);
    canvas.fill_text("IN R", content_x + 58.8f * panel_sx,
                     content_y + (128.5f - 13.0f) * panel_sy);
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
  PushButtonControl *quality_control() const { return push_button_; }
  AssignButtonControl *assign_control() const { return assign_button_; }
#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
  AudioRoutePicker *audio_route_picker() const { return route_picker_left_; }
  AudioRoutePicker *audio_route_picker_right() const { return route_picker_right_; }
  void set_audio_route_catalog(pulp::host::AudioRouteCatalog *catalog,
                               bool observe_changes = false) {
    if (!route_picker_left_ && !route_picker_right_)
      return;
    if (catalog) {
      const auto bus = static_cast<std::size_t>(
          source_mode_ && source_mode_->mode() == 2);
      for (std::size_t channel = 0; channel < 2; ++channel) {
        const auto saved = processor_.route_identity(channel);
        const auto connected = [&] {
          if constexpr (requires {
                          catalog->connected_route_channel(bus, channel);
                        }) {
            return catalog->connected_route_channel(bus, channel);
          } else {
            return catalog->connected_route(bus);
          }
        }();
        if (!saved.empty() && connected.empty())
          [&] {
            if constexpr (requires {
                            catalog->connect_channel(saved, bus, channel);
                          }) {
              (void)catalog->connect_channel(saved, bus, channel);
            } else {
              (void)catalog->connect(saved, bus);
            }
          }();
        const auto rebound = [&] {
          if constexpr (requires {
                          catalog->connected_route_channel(bus, channel);
                        }) {
            return catalog->connected_route_channel(bus, channel);
          } else {
            return catalog->connected_route(bus);
          }
        }();
        if (!rebound.empty())
          processor_.set_route_identity(channel, rebound);
      }
    }
    if (route_picker_left_)
      route_picker_left_->set_catalog(catalog, observe_changes);
    if (route_picker_right_)
      route_picker_right_->set_catalog(catalog, false);
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
    const auto *info = store_.info(id);
    const std::string target_name = info ? info->name : "Parameter";
    auto alive = import_binding_lifetime_token();
    pulp::view::ContextMenu::ModulationMenuSpec spec;
    spec.target_label = target_name;
    // The shared Spectr-style menu owns the target hierarchy.  Beadsy's
    // modulation lanes may address any of the product's source-backed grain
    // parameters, so expose that catalog instead of silently omitting the
    // TARGETS section from the popup.
    const std::array<std::pair<const char *, pulp::state::ParamID>, 8>
        modulation_targets = {{{"Density", beadsy::kDensity},
                               {"Time", beadsy::kTime},
                               {"Size", beadsy::kSize},
                               {"Shape", beadsy::kShape},
                               {"Pitch", beadsy::kPitch},
                               {"Quality", beadsy::kQuality},
                               {"Freeze", beadsy::kFreeze},
                               {"Source", beadsy::kMode}}};
    for (const auto &[label, target_id] : modulation_targets) {
      spec.target_labels.emplace_back(label);
      spec.all_target_labels.emplace_back(label);
      if (target_id == id)
        spec.target_index = spec.target_labels.size() - 1;
    }
    spec.lane_target_indices = {spec.target_index, spec.target_index};
    spec.on_target_change = [alive, this](std::size_t) {
      if (!alive.expired())
        request_repaint();
    };
    spec.on_target_assignment = [alive, this, id,
                                 modulation_targets](std::size_t lane,
                                                      std::size_t target_index) {
      if (alive.expired() || target_index >= modulation_targets.size())
        return;
      processor_.assign_lfo_target(id, modulation_targets[target_index].second,
                                   lane);
      request_repaint();
    };
    spec.lane_count = 2;
    spec.active_lane_index = 0;
    spec.on_lane_select = [alive, this](std::size_t) {
      if (!alive.expired())
        request_repaint();
    };
    spec.live = true;
    spec.on_live_change = [alive, this](bool) {
      if (!alive.expired())
        request_repaint();
    };
    for (std::size_t lane_index = 0; lane_index < spec.lane_count; ++lane_index) {
      const auto current = processor_.lfo(id, lane_index);
      spec.lanes[lane_index].label =
          std::string("LFO ") + std::to_string(lane_index + 1);
      spec.lanes[lane_index].enabled = current.enabled;
      spec.lanes[lane_index].rate_normalized =
          std::clamp((current.rate_hz - 0.01f) / 19.99f, 0.0f, 1.0f);
      spec.lanes[lane_index].depth_normalized =
          std::clamp(current.depth, 0.0f, 1.0f);
      spec.lanes[lane_index].shapes = {"Sine", "Triangle", "Square", "Saw"};
      spec.lanes[lane_index].shape_index =
          static_cast<std::size_t>(std::min<int>(static_cast<int>(current.shape), 3));
      spec.lanes[lane_index].on_change =
          [alive, this, id, lane_index](const auto &lane) {
            if (alive.expired())
              return;
            auto modulation = processor_.lfo(id, lane_index);
            modulation.enabled = lane.enabled;
            modulation.rate_hz = 0.01f + lane.rate_normalized * 19.99f;
            modulation.depth = lane.enabled && lane.depth_normalized <= 0.0f
                                   ? 0.5f
                                   : lane.depth_normalized;
            modulation.shape = static_cast<beadsy::LfoModulation::Shape>(
                std::min<std::size_t>(lane.shape_index, 3));
            processor_.set_lfo(id, modulation, lane_index);
            request_repaint();
          };
    }
    spec.on_reset = [alive, this, id] {
      if (alive.expired())
        return;
      processor_.clear_lfo(id, 0);
      processor_.clear_lfo(id, 1);
      // Reset is a terminal action. Clear the product target immediately as
      // well as through the SDK dismissal callback so direct/native host
      // dispatch cannot leave a stale modulation target behind.
      clear_modulation_context();
      request_repaint();
    };
    spec.on_dismiss = [alive, this] {
      if (!alive.expired())
        clear_modulation_context();
    };
    return pulp::view::ContextMenu::show_modulation(
        root, detail::local_to_root(this, position), std::move(spec));
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
      // The rendered utility cap is intentionally slightly larger than its
      // mapped DXF diameter. Give the native hit region the same small margin
      // so a click on the cap edge still begins the normal Knob gesture.
      raw->set_hit_slop(std::max(1.0f, diameter * 0.10f));
      if (std::string_view(p.name) == "Feedback")
        raw->set_accent(pulp::canvas::Color::rgba8(24, 165, 175));
      else if (std::string_view(p.name) == "Dry Wet")
        raw->set_accent(pulp::canvas::Color::rgba8(234, 166, 27));
      else if (std::string_view(p.name) == "Reverb")
        raw->set_accent(pulp::canvas::Color::rgba8(211, 59, 97));
      if (p.id != 0) {
        raw->set_modulation_indicator(
            [this, id = p.id] {
              return processor_.lfo(id, 0).enabled || processor_.lfo(id, 1).enabled;
            });
        raw->set_modulation_phase(
            [this, id = p.id] {
              const auto first = processor_.lfo(id, 0);
              const auto second = processor_.lfo(id, 1);
              return first.enabled ? first.phase : second.phase;
            });
        raw->on_context_menu = [this, raw, id = p.id](pulp::view::Point position) {
          // PanelKnob receives the secondary-click point in its own local
          // coordinates. The shared menu factory expects an editor-local
          // anchor, so translate through the knob's authored bounds before
          // converting to the host root.
          const auto knob_bounds = raw->bounds();
          show_modulation_context_menu(
              id, {knob_bounds.x + position.x, knob_bounds.y + position.y});
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
      controls_.push_back({p.id, raw, p.x, p.y, p.r});
    }

    // Some AUv2 bridges deliver a secondary click to the editor container
    // instead of the child knob. Resolve that editor-local point against the
    // same native control bounds and open the identical Spectr-style menu.
    on_context_menu = [this](pulp::view::Point position) {
      for (const auto &control : controls_) {
        if (!control.knob || control.id == 0)
          continue;
        const auto bounds = control.knob->bounds();
        if (position.x >= bounds.x && position.x <= bounds.x + bounds.width &&
            position.y >= bounds.y && position.y <= bounds.y + bounds.height) {
          show_modulation_context_menu(control.id, position);
          return;
        }
      }
    };

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

    auto push = std::make_unique<PushButtonControl>();
    push_button_ = push.get();
    push_button_->set_quality(static_cast<int>(std::lround(
        store_.get_value(kQuality))));
    push_button_->set_on_press([this] {
      const int next = (push_button_->quality() + 1) % 4;
      store_.begin_gesture(kQuality);
      store_.set_value(kQuality, static_cast<float>(next));
      store_.end_gesture(kQuality);
    });
    quality_binding_ = store_.add_listener(
        [this](pulp::state::ParamID changed, float value) {
          if (changed == kQuality) {
            const int quality = static_cast<int>(std::lround(value));
            if (push_button_)
              push_button_->set_quality(quality);
            if (center_indicators_)
              center_indicators_->set_quality(quality);
          }
        },
        pulp::state::ListenerThread::Main,
        pulp::state::ListenerRestoreBehavior::Reconcile);
    add_child(std::move(push));

    auto assign = std::make_unique<AssignButtonControl>();
    assign_button_ = assign.get();
    add_child(std::move(assign));

    auto indicators = std::make_unique<CenterIndicators>();
    center_indicators_ = indicators.get();
    center_indicators_->set_quality(
        static_cast<int>(std::lround(store_.get_value(kQuality))));
    add_child(std::move(indicators));

    auto mode = std::make_unique<ModeControl>();
    source_mode_ = mode.get();
    source_mode_->set_mode(
        static_cast<int>(std::lround(store_.get_value(kMode))));
    source_mode_->set_on_mode_change([this](int next) {
      store_.begin_gesture(kMode);
      store_.set_value(kMode, static_cast<float>(next));
      store_.end_gesture(kMode);
#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
      if (route_picker_left_)
        route_picker_left_->set_source_mode(next);
      if (route_picker_right_)
        route_picker_right_->set_source_mode(next);
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
            if (route_picker_left_)
              route_picker_left_->set_source_mode(
                  static_cast<int>(std::lround(value)));
            if (route_picker_right_)
              route_picker_right_->set_source_mode(
                  static_cast<int>(std::lround(value)));
#endif
          }
        },
        pulp::state::ListenerThread::Main,
        pulp::state::ListenerRestoreBehavior::Reconcile);
    add_child(std::move(mode));

#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
    auto route_picker_left = std::make_unique<AudioRoutePicker>(0, 0);
    route_picker_left_ = route_picker_left.get();
    route_picker_left_->set_access_label("Input L: host-owned routing menu");
    route_picker_left_->set_on_channel_route_connected(
        [this](std::size_t, std::size_t channel, std::string_view stable_id) {
          processor_.set_route_identity(channel, stable_id);
        });
    route_picker_left_->set_source_mode(
        static_cast<int>(std::lround(store_.get_value(kMode))));
    add_child(std::move(route_picker_left));
    auto route_picker_right = std::make_unique<AudioRoutePicker>(0, 1);
    route_picker_right_ = route_picker_right.get();
    route_picker_right_->set_access_label("Input R: host-owned routing menu");
    route_picker_right_->set_on_channel_route_connected(
        [this](std::size_t, std::size_t channel, std::string_view stable_id) {
          processor_.set_route_identity(channel, stable_id);
        });
    route_picker_right_->set_source_mode(
        static_cast<int>(std::lround(store_.get_value(kMode))));
    add_child(std::move(route_picker_right));
#else
    // Older SDKs can still show the complete stereo routing UX while the
    // host catalog capability is being adopted. These selectors are upgraded
    // in-place by the catalog-backed implementation when available.
    auto route_left = std::make_unique<pulp::view::ComboBox>();
    route_picker_left_fallback_ = route_left.get();
    route_picker_left_fallback_->set_id("beadsy-audio-route-left");
    route_picker_left_fallback_->set_access_label("Input L: host routing menu");
    route_picker_left_fallback_->set_items({"L IN"});
    route_picker_left_fallback_->set_selected_silent(0);
    add_child(std::move(route_left));
    auto route_right = std::make_unique<pulp::view::ComboBox>();
    route_picker_right_fallback_ = route_right.get();
    route_picker_right_fallback_->set_id("beadsy-audio-route-right");
    route_picker_right_fallback_->set_access_label("Input R: host routing menu");
    route_picker_right_fallback_->set_items({"R IN"});
    route_picker_right_fallback_->set_selected_silent(0);
    add_child(std::move(route_right));
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
    // Use the same aspect-preserving content rect as paint(). A host resize
    // must never shear the authored panel or separate a symbol from the
    // control it annotates.
    const float unit = std::min(width / 70.8f, height / 128.5f);
    const float content_width = 70.8f * unit;
    const float content_height = 128.5f * unit;
    const float content_x = (width - content_width) * 0.5f;
    const float content_y = (height - content_height) * 0.5f;
    const float sx = unit;
    const float sy = unit;
    // The native host uses Yoga for the final layout pass.  `set_bounds()`
    // updates the cached rectangle, but Yoga positions an absolute child from
    // its CSS edges (left/top), so a bounds-only placement is lost on the
    // first attached frame.  Keep both representations in sync.  This is the
    // root cause of the recurring host-only failure where the panel artwork is
    // upright but native controls collapse at the origin (and then appear
    // missing, flipped, or unresponsive after a resize).
    const auto place_absolute = [](pulp::view::View *view, float x, float y,
                                   float w, float h) {
      if (!view)
        return;
      view->set_position(pulp::view::View::Position::absolute);
      view->set_left(x);
      view->set_top(y);
      view->set_bounds({x, y, w, h});
    };
    for (const auto &c : controls_) {
      const float diameter = c.radius * 2.0f * std::min(sx, sy);
      const float x = content_x +
                      (c.x - detail::PanelKnob::kSourceXCorrectionMm) * sx -
                      diameter * 0.5f;
      const float y = content_y +
                      (128.5f -
                       (c.y - detail::PanelKnob::kSourceYCorrectionMm)) *
                          sy -
                      diameter * 0.5f;
      const float control_height = detail::PanelKnob::is_utility_diameter(diameter)
                                       ? diameter * 2.35f
                                       : diameter * 1.25f;
      place_absolute(c.knob, x, y, diameter, control_height);
      c.knob->flex().preferred_width = diameter;
      c.knob->flex().preferred_height = control_height;
    }
    if (seed_mode_) {
      const float width = 10.0f * std::min(sx, sy);
      const float height = 8.5f * std::min(sx, sy);
      const float x = content_x + 62.0f * sx - width * 0.5f;
      const float y = content_y + (128.5f - 110.5f) * sy - height * 0.5f;
      place_absolute(seed_mode_, x, y, width, height);
    }
    if (freeze_mode_) {
      const float width = 10.0f * std::min(sx, sy);
      const float height = 8.5f * std::min(sx, sy);
      const float x = content_x + 8.7f * sx - width * 0.5f;
      const float y = content_y + (128.5f - 110.5f) * sy - height * 0.5f;
      place_absolute(freeze_mode_, x, y, width, height);
    }
    if (push_button_) {
      const float unit = std::min(sx, sy);
      const float diameter = 7.0f * unit;
      // Recording quality selector [A], directly under DENSITY.
      const float x = content_x + 35.7f * sx - diameter * 0.5f;
      const float y = content_y + (128.5f - 83.24f) * sy - diameter * 0.5f;
      place_absolute(push_button_, x, y, diameter, diameter);
    }
    if (assign_button_) {
      const float unit = std::min(sx, sy);
      const float diameter = 7.0f * unit;
      // The authored dashed guide is centered at 7.1mm; keep the button
      // directly over that guide rather than over the old jack center.
      const float x = content_x + 7.1f * sx - diameter * 0.5f;
      const float y = content_y + (128.5f - 51.7f) * sy - diameter * 0.5f;
      place_absolute(assign_button_, x, y, diameter, diameter);
    }
    if (center_indicators_) {
      const float unit = std::min(sx, sy);
      // These two source markers are independent controls: a small LED at
      // roughly 81 pt and a larger outlined button at roughly 100 pt. Keep
      // their centers and vertical station tied to the panel artwork instead
      // of centering two equal dots in a generic box.
      const float w = 12.0f * unit;
      const float h = 8.0f * unit;
      const float x = content_x + 31.92f * sx - w * 0.5f;
      const float y = content_y + (128.5f - 83.24f) * sy - h * 0.5f;
      place_absolute(center_indicators_, x, y, w, h);
    }
    if (source_mode_) {
      const float unit = std::min(sx, sy);
      const float mode_width = 20.0f * unit;
      const float mode_height = 5.5f * unit;
      // Software source selection occupies the reclaimed lower jack area;
      // keep the original printed header unobstructed.
      const float x = content_x + 35.4f * sx - mode_width * 0.5f;
      const float y = content_y + (128.5f - 17.0f) * sy - mode_height * 0.5f;
      place_absolute(source_mode_, x, y, mode_width, mode_height);
    }
#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
    if (route_picker_left_ || route_picker_right_) {
      const float unit = std::min(sx, sy);
      const float route_width = 20.0f * unit;
      const float route_height = 5.5f * unit;
      const float y = content_y + (128.5f - 17.0f) * sy - route_height * 0.5f;
      if (route_picker_left_) {
        place_absolute(route_picker_left_,
                       content_x + 12.0f * sx - route_width * 0.5f, y, route_width,
                       route_height);
      }
      if (route_picker_right_) {
        place_absolute(route_picker_right_,
                       content_x + 58.8f * sx - route_width * 0.5f, y, route_width,
                       route_height);
      }
    }
#else
    if (route_picker_left_fallback_ || route_picker_right_fallback_) {
      const float unit = std::min(sx, sy);
      const float route_width = 20.0f * unit;
      const float route_height = 5.5f * unit;
      const float y = content_y + (128.5f - 17.0f) * sy - route_height * 0.5f;
      if (route_picker_left_fallback_) {
        place_absolute(route_picker_left_fallback_,
                       content_x + 12.0f * sx - route_width * 0.5f, y, route_width,
                       route_height);
      }
      if (route_picker_right_fallback_) {
        place_absolute(route_picker_right_fallback_,
                       content_x + 58.8f * sx - route_width * 0.5f, y, route_width,
                       route_height);
      }
    }
#endif
    if (resize_grip_) {
      const float grip = std::max(8.0f, 7.0f * std::min(sx, sy));
      place_absolute(resize_grip_, width - grip, height - grip, grip, grip);
    }
  }

  struct Control {
    pulp::state::ParamID id;
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
  PushButtonControl *push_button_ = nullptr;
  AssignButtonControl *assign_button_ = nullptr;
  pulp::state::ListenerToken quality_binding_;
  CenterIndicators *center_indicators_ = nullptr;
  ModeControl *source_mode_ = nullptr;
#if defined(BEADSY_HAS_AUDIO_ROUTE_CATALOG)
  AudioRoutePicker *route_picker_left_ = nullptr;
  AudioRoutePicker *route_picker_right_ = nullptr;
#else
  pulp::view::ComboBox *route_picker_left_fallback_ = nullptr;
  pulp::view::ComboBox *route_picker_right_fallback_ = nullptr;
#endif
  ResizeGrip *resize_grip_ = nullptr;
  pulp::state::ListenerToken source_mode_binding_;
  std::optional<pulp::state::ParamID> context_target_;
};

} // namespace beadsy

#endif // !PULP_HEADLESS
