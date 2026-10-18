#pragma once

#if !defined(PULP_HEADLESS)

#include "beasy_panel_svg.hpp"
#include "beasy.hpp"

#include <pulp/canvas/canvas.hpp>
#include <pulp/canvas/svg.hpp>
#include <pulp/view/context_menu.hpp>
#include <pulp/view/parameter_binding.hpp>
#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <sstream>
#include <vector>
#include <string_view>

namespace beasy {

namespace detail {

inline pulp::view::View* root_view(pulp::view::View* view) {
    if (!view) return nullptr;
    while (view->parent()) view = view->parent();
    return view;
}

inline pulp::view::Point local_to_root(pulp::view::View* view,
                                       pulp::view::Point point) {
    for (auto* current = view; current; current = current->parent()) {
        const auto bounds = current->bounds();
        point.x += bounds.x;
        point.y += bounds.y;
    }
    return point;
}

class PanelKnob final : public pulp::view::Knob {
public:
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
        set_id("beasy-" + caption_);
    }

    void set_modulation_indicator(std::function<bool()> active) {
        modulation_active_ = std::move(active);
    }
    void set_accent(std::optional<pulp::canvas::Color> accent) {
        accent_ = accent;
    }

    void paint(pulp::canvas::Canvas& canvas) override {
        const auto b = local_bounds();
        const float cx = b.width * 0.5f;
        const float cy = b.height * 0.43f;
        const float radius = paint_radius_for_bounds(b.width, b.height);
        const float angle = pulp::view::Knob::start_angle +
                            (pulp::view::Knob::end_angle - pulp::view::Knob::start_angle) * value();

        // A few auxiliary controls use a colored side strip in the reference
        // panel.  Paint that accent behind the procedural ring so the native
        // knob still owns hit testing and automation.
        if (accent_) {
            canvas.set_fill_color(*accent_);
            canvas.fill_rounded_rect(cx - radius * 0.66f, cy - radius * 2.40f,
                                     radius * 1.32f, radius * 4.80f, radius * 0.58f);
        }

        // The straight-on reference shows a black, physically segmented
        // annulus around a pale inset cap.  Keep this entirely procedural:
        // the reference photo is provenance only and is never embedded.
        const float outer_radius = radius * 1.14f;
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
                static_cast<float>(i) * kTwoPi /
                static_cast<float>(kGrooveCount);
            canvas.stroke_line(
                cx + radius * 0.70f * std::cos(groove_angle),
                cy + radius * 0.70f * std::sin(groove_angle),
                cx + radius * 0.99f * std::cos(groove_angle),
                cy + radius * 0.99f * std::sin(groove_angle));
        }

        // The measured reference cap occupies approximately 53–61% of the
        // outer diameter.  A 0.62-radius inset lands at the middle of that
        // range after the outer shadow is included.
        canvas.set_fill_color(pulp::canvas::Color::rgba8(232, 230, 218));
        canvas.fill_circle(cx - radius * 0.055f, cy - radius * 0.07f,
                           radius * 0.62f);

        // Broad trapezoid pointer: the reference is a white slot rather than
        // the prior 6 px hairline.  It remains centered on the existing Knob
        // value angle, preserving automation and gesture semantics.
        const float ux = std::cos(angle);
        const float uy = std::sin(angle);
        const float px = -uy;
        const float py = ux;
        const float inner = radius * 0.42f;
        const float outer = radius * 0.97f;
        const float inner_half_width = radius * 0.070f;
        const float outer_half_width = radius * 0.040f;
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

class PortRegion final : public pulp::view::View {
public:
    explicit PortRegion(std::string label) : label_(std::move(label)) {
        set_id("beasy-port-" + label_);
        set_access_label(label_ + " (I/O port; non-automatable)");
        set_access_disabled("false");
    }
    void paint(pulp::canvas::Canvas& canvas) override {
        const auto b = local_bounds();
        const float cx = b.width * 0.5f;
        const float cy = b.height * 0.5f;
        const float r = std::min(b.width, b.height) * 0.38f;

        // Procedural straight-on jack treatment. It follows the measured
        // geometry of the reference panel while keeping all upstream photos
        // and artwork outside the product bundle.
        canvas.set_fill_color(pulp::canvas::Color::rgba8(9, 9, 10));
        canvas.fill_circle(cx + r * 0.06f, cy + r * 0.10f, r * 1.08f);
        canvas.set_fill_color(pulp::canvas::Color::rgba8(184, 157, 136));
        canvas.fill_circle(cx, cy, r * 1.02f);
        canvas.set_fill_color(pulp::canvas::Color::rgba8(239, 222, 202));
        canvas.fill_circle(cx - r * 0.04f, cy - r * 0.05f, r * 0.88f);
        canvas.set_fill_color(pulp::canvas::Color::rgba8(32, 31, 31));
        canvas.fill_circle(cx, cy, r * 0.69f);
        canvas.set_fill_color(pulp::canvas::Color::rgba8(7, 7, 8));
        canvas.fill_circle(cx + r * 0.02f, cy + r * 0.03f, r * 0.53f);

        // A short highlight and lower shadow give the metal collar depth at
        // small sizes without introducing a bitmap skin.
        canvas.set_stroke_color(pulp::canvas::Color::rgba8(255, 245, 227));
        canvas.set_line_width(std::max(1.0f, r * 0.08f));
        canvas.set_line_cap(pulp::canvas::LineCap::round);
        canvas.stroke_arc(cx, cy, r * 0.91f, -2.65f, -0.35f);
        canvas.set_stroke_color(pulp::canvas::Color::rgba8(105, 81, 70));
        canvas.set_line_width(std::max(1.0f, r * 0.07f));
        canvas.stroke_arc(cx, cy, r * 0.94f, 0.45f, 2.55f);
    }
    const std::string& label() const { return label_; }
private:
    std::string label_;
};

/// The hardware SEED control is a button with a latched mode indication.  In
/// Beasy the source-backed three-state Seed Mode parameter is the semantic
/// authority, so the straight-on control cycles OFF → ON → LOCKED on click.
/// Keeping this as a native view (rather than a guessed SVG hit region) makes
/// automation/state changes repaint the LED as well as the button.
class SeedModeControl final : public pulp::view::View {
public:
    SeedModeControl() {
        set_id("beasy-seed-mode");
        set_access_role(pulp::view::View::AccessRole::button);
        set_access_label("Seed Mode");
        set_focusable(true);
    }

    void set_mode(int mode) {
        mode_ = std::clamp(mode, 0, 2);
        const char* state = mode_ == 0 ? "off" : mode_ == 1 ? "on" : "locked";
        set_access_label(std::string("Seed Mode (") + state + "); click to cycle");
        request_repaint();
    }
    int mode() const { return mode_; }

    void set_on_press(std::function<void()> callback) {
        on_press_ = std::move(callback);
    }

    void paint(pulp::canvas::Canvas& canvas) override {
        const auto b = local_bounds();
        const float cx = b.width * 0.5f;
        const float button_h = std::max(8.0f, b.height * 0.58f);
        const float button_y = b.height - button_h;
        const auto off = pulp::canvas::Color::rgba8(30, 32, 34);
        const auto on = pulp::canvas::Color::rgba8(66, 72, 73);
        canvas.set_fill_color(mode_ == 0 ? off : on);
        canvas.fill_rounded_rect(0.5f, button_y, std::max(1.0f, b.width - 1.0f),
                                 button_h, button_h * 0.25f);
        canvas.set_stroke_color(pulp::canvas::Color::rgba8(72, 77, 79));
        canvas.set_line_width(0.8f);
        canvas.stroke_rounded_rect(0.5f, button_y, std::max(1.0f, b.width - 1.0f),
                                   button_h, button_h * 0.25f);

        const auto led = mode_ == 0 ? pulp::canvas::Color::rgba8(44, 43, 38) :
            mode_ == 1 ? pulp::canvas::Color::rgba8(211, 164, 74) :
                         pulp::canvas::Color::rgba8(103, 151, 168);
        canvas.set_fill_color(led);
        canvas.fill_circle(cx, std::max(2.0f, button_y * 0.48f),
                           std::max(1.2f, std::min(b.width, b.height) * 0.10f));
        canvas.set_font_full("Roboto Mono", std::max(3.0f, b.width * 0.13f),
                            700, 0, 0.45f);
        canvas.set_text_align(pulp::canvas::TextAlign::center);
        canvas.set_fill_color(pulp::canvas::Color::rgba8(236, 234, 224));
        canvas.fill_text(mode_ == 2 ? "LOCK" : "SEED", cx,
                         button_y + button_h * 0.63f);
    }

    void on_mouse_down(pulp::view::Point) override {
        if (on_press_) on_press_();
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
        set_id("beasy-freeze");
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

    void paint(pulp::canvas::Canvas& canvas) override {
        const auto b = local_bounds();
        const float cx = b.width * 0.5f;
        const float button_h = std::max(8.0f, b.height * 0.67f);
        const float y = b.height - button_h;
        const auto body = pressed_ ? pulp::canvas::Color::rgba8(20, 150, 161)
                                   : pulp::canvas::Color::rgba8(28, 166, 177);
        canvas.set_fill_color(body);
        canvas.fill_rounded_rect(0.5f, y, std::max(1.0f, b.width - 1.0f),
                                 button_h, button_h * 0.38f);
        canvas.set_stroke_color(pulp::canvas::Color::rgba8(7, 104, 112));
        canvas.set_line_width(0.8f);
        canvas.stroke_rounded_rect(0.5f, y, std::max(1.0f, b.width - 1.0f),
                                   button_h, button_h * 0.38f);
        canvas.set_fill_color(pulp::canvas::Color::rgba8(238, 250, 244));
        canvas.set_font_full("Roboto Mono", std::max(3.0f, b.width * 0.13f),
                            700, 0, 0.45f);
        canvas.set_text_align(pulp::canvas::TextAlign::center);
        canvas.fill_text("FREEZE", cx, y + button_h * 0.62f);
    }

    void on_mouse_down(pulp::view::Point) override {
        if (on_press_) on_press_();
    }
    bool wants_mouse_input() const override { return true; }

private:
    bool pressed_ = false;
    std::function<void()> on_press_;
};

class BeasyEditor final : public pulp::view::View {
public:
    BeasyEditor(Beasy& processor, pulp::state::StateStore& store)
        : processor_(processor), store_(store), panel_(pulp::canvas::SvgImage::from_string(kBeasyPanelSvg)) {
        set_id("beasy-editor");
        set_background_color(pulp::canvas::Color::rgba8(22, 21, 19));
        build_controls();
        layout_controls();
        set_continuous_repaint(false);
    }

    void on_resized() override { layout_controls(); }
    void layout_children() override {
        pulp::view::View::layout_children();
        layout_controls();
    }

    void paint(pulp::canvas::Canvas& canvas) override {
        const auto b = local_bounds();
        if (panel_.is_valid()) {
            panel_.render(canvas, 0.0f, 0.0f, b.width, b.height);
        } else {
            canvas.set_fill_color(pulp::canvas::Color::rgba8(22, 21, 19));
            canvas.fill_rect(0, 0, b.width, b.height);
        }
        if (std::string_view(kBeasyPanelSourceStatus) ==
            "independent scaffold; rights-gated artwork probe unavailable") {
            paint_scaffold_labels(canvas, b.width, b.height);
        }
    }

    /// Target selected by the native right-click context interaction. Hosts
    /// can use this seam to present their native modulation menu.
    std::optional<pulp::state::ParamID> modulation_context_target() const {
        return context_target_;
    }
    void open_modulation_context(pulp::state::ParamID id) { context_target_ = id; }
    void clear_modulation_context() { context_target_.reset(); }

    /// Show the same menu used by a knob's real secondary-click handler.
    /// This is public so host/editor characterization tests can exercise the
    /// complete menu lifecycle without synthesizing platform pointer events.
    pulp::view::ContextMenu* show_modulation_context_menu(
        pulp::state::ParamID id, pulp::view::Point position) {
        open_modulation_context(id);
        auto* root = detail::root_view(this);
        if (!root) return nullptr;
        const auto current = processor_.lfo(id);
        const auto* info = store_.info(id);
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
        return pulp::view::ContextMenu::show(
            root, detail::local_to_root(this, position),
            {{0, "LFO → " + target_name, false},
             {1, current.enabled ? "Disable LFO" : "Enable LFO"},
             {3, rate_label() + " −"}, {4, rate_label() + " +"},
             {5, depth_label() + " −"}, {6, depth_label() + " +"},
             {2, "Reset LFO"}},
            [alive, this, id](std::optional<int> command) {
                if (alive.expired()) return;
                if (command) {
                    auto modulation = processor_.lfo(id);
                    if (*command == 1) {
                        modulation.enabled = !modulation.enabled;
                        if (modulation.depth == 0.0f) modulation.depth = 0.1f;
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
                }
                clear_modulation_context();
            });
    }

private:
    struct Placement { pulp::state::ParamID id; const char* name; float x; float y; float r; };

    static void paint_scaffold_labels(pulp::canvas::Canvas& canvas,
                                      float width, float height) {
        // SvgImage intentionally keeps the fallback asset free of runtime
        // font dependencies. Paint its labels through the native canvas so
        // the clean-room scaffold remains legible in screenshots and hosts;
        // rights-gated authoring SVGs are passed through without overlays.
        const float sx = width / 200.693f;
        const float sy = height / 364.252f;
        const float scale = std::min(sx, sy);
        const auto label = [&canvas, sx, sy, scale](const char* value,
                                                     float x, float y,
                                                     float size,
                                                     pulp::canvas::Color color,
                                                     pulp::canvas::TextAlign align = pulp::canvas::TextAlign::center,
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
        label("BEASY", 100.3465f, 17.0f, 8.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("MIDI GRAIN SYNTH", 100.3465f, 25.0f, 3.1f, accent);
        label("GRAIN", 100.3465f, 46.0f, 3.2f, accent, pulp::canvas::TextAlign::center, 700);
        // Top controls retain the original panel's visual hierarchy while
        // remaining Beasy's independently authored labels and typography.
        label("FREEZE", 24.5f, 63.0f, 5.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("SEED", 176.2f, 63.0f, 5.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("DENSITY", 101.4f, 100.0f, 6.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("TIME", 33.8f, 142.0f, 6.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("PITCH", 168.9f, 142.0f, 6.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("SIZE", 69.8f, 201.0f, 6.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("SHAPE", 133.0f, 201.0f, 6.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("MIX / MOTION", 100.3465f, 217.0f, 3.2f, accent,
              pulp::canvas::TextAlign::center, 700);
        label("PARAMETER CONTROLS", 100.3465f, 245.0f, 3.0f, muted);
        label("CV / GATE", 100.3465f, 264.0f, 2.8f, muted, pulp::canvas::TextAlign::center, 700);
        label("IN L", 11.9f, 294.0f, 4.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("IN R", 23.4f, 294.0f, 4.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("FREEZE", 34.9f, 294.0f, 3.7f, ink, pulp::canvas::TextAlign::center, 700);
        label("SEED", 46.3f, 294.0f, 3.7f, ink, pulp::canvas::TextAlign::center, 700);
        label("OUT L", 57.8f, 294.0f, 4.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("OUT R", 69.3f, 294.0f, 4.0f, ink, pulp::canvas::TextAlign::center, 700);
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
            {0, "TIME CV (J1)", 7.115f, 32.0f, 4.4f},
            {0, "SIZE CV (J2)", 18.565f, 32.0f, 4.4f},
            {0, "SHAPE CV (J3)", 30.015f, 32.0f, 4.4f},
            {0, "PITCH CV (J4)", 41.465f, 32.0f, 4.4f},
            {0, "ASSIGN CV (J5)", 52.915f, 32.0f, 4.4f},
            {0, "DENSITY CV (J6)", 64.365f, 32.0f, 4.4f},
            {0, "AUDIO IN L (J7)", 7.115f, 17.4f, 4.4f},
            {0, "AUDIO IN R (J8)", 18.565f, 17.4f, 4.4f},
            {0, "FREEZE GATE (J9)", 30.015f, 17.4f, 4.4f},
            {0, "SEED GATE (J10)", 41.465f, 17.4f, 4.4f},
            {0, "AUDIO OUT L (J11)", 52.915f, 17.4f, 4.4f},
            {0, "AUDIO OUT R (J12)", 64.365f, 17.4f, 4.4f},
        };
        for (const auto& p : placements) {
            const float diameter = p.r * 2.0f;
            const bool is_port = p.r == 4.4f;
            if (is_port) {
                auto port = std::make_unique<PortRegion>(p.name);
                auto* raw_port = port.get();
                add_child(std::move(port));
                ports_.push_back({raw_port, p.x, p.y, p.r});
                continue;
            }
            auto knob = std::make_unique<detail::PanelKnob>(p.name, diameter);
            auto* raw = knob.get();
            if (std::string_view(p.name) == "Feedback")
                raw->set_accent(pulp::canvas::Color::rgba8(24, 165, 175));
            else if (std::string_view(p.name) == "Dry Wet")
                raw->set_accent(pulp::canvas::Color::rgba8(234, 166, 27));
            else if (std::string_view(p.name) == "Reverb")
                raw->set_accent(pulp::canvas::Color::rgba8(211, 59, 97));
            if (p.id != 0) {
                raw->set_modulation_indicator([this, id = p.id] {
                    return processor_.lfo(id).enabled;
                });
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
            if (p.id != 0) bindings_.push_back(pulp::view::bind_parameter(*raw, store_, p.id));
            controls_.push_back({raw, p.x, p.y, p.r});
        }

        // Keep the Seed control after the mapped geometry so existing panel
        // child ordering remains stable for hosts and screenshot probes.
        auto seed = std::make_unique<SeedModeControl>();
        seed_mode_ = seed.get();
        seed_mode_->set_mode(static_cast<int>(std::lround(store_.get_value(kSeedMode))));
        seed_mode_->set_on_press([this] {
            const int next = (seed_mode_->mode() + 1) % 3;
            store_.begin_gesture(kSeedMode);
            store_.set_value(kSeedMode, static_cast<float>(next));
            store_.end_gesture(kSeedMode);
        });
        seed_mode_binding_ = store_.add_listener(
            [this](pulp::state::ParamID changed, float) {
                if (changed == kSeedMode && seed_mode_)
                    seed_mode_->set_mode(static_cast<int>(std::lround(
                        store_.get_value(kSeedMode))));
            }, pulp::state::ListenerThread::Main,
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
            }, pulp::state::ListenerThread::Main,
            pulp::state::ListenerRestoreBehavior::Reconcile);
        add_child(std::move(freeze));
    }

    void layout_controls() {
        const float width = local_bounds().width > 1.0f ? local_bounds().width : 201.0f;
        const float height = local_bounds().height > 1.0f ? local_bounds().height : 364.0f;
        const float sx = width / 70.8f;
        const float sy = height / 128.5f;
        for (const auto& c : controls_) {
            const float diameter = c.radius * 2.0f * std::min(sx, sy);
            const float x = c.x * sx - diameter * 0.5f;
            const float y = (128.5f - c.y) * sy - diameter * 0.5f;
            c.knob->set_position(pulp::view::View::Position::absolute);
            c.knob->set_bounds({x, y, diameter, diameter * 1.25f});
            c.knob->flex().preferred_width = diameter;
            c.knob->flex().preferred_height = diameter * 1.25f;
        }
        for (const auto& p : ports_) {
            const float diameter = p.radius * 2.0f * std::min(sx, sy);
            const float x = p.x * sx - diameter * 0.5f;
            const float y = (128.5f - p.y) * sy - diameter * 0.5f;
            p.view->set_position(pulp::view::View::Position::absolute);
            p.view->set_bounds({x, y, diameter, diameter});
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
    }

    struct Control { detail::PanelKnob* knob; float x; float y; float radius; };
    struct Port { PortRegion* view; float x; float y; float radius; };
    Beasy& processor_;
    pulp::state::StateStore& store_;
    pulp::canvas::SvgImage panel_;
    std::vector<Control> controls_;
    std::vector<Port> ports_;
    std::vector<pulp::view::ParameterBinding> bindings_;
    SeedModeControl* seed_mode_ = nullptr;
    pulp::state::ListenerToken seed_mode_binding_;
    FreezeControl* freeze_mode_ = nullptr;
    pulp::state::ListenerToken freeze_mode_binding_;
    std::optional<pulp::state::ParamID> context_target_;
};

} // namespace beasy

#endif // !PULP_HEADLESS
