#pragma once

#if !defined(PULP_HEADLESS)

#include "grainfield_panel_svg.hpp"
#include "grainfield.hpp"

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

namespace grainfield {

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
        set_id("grainfield-" + caption_);
    }

    void set_modulation_indicator(std::function<bool()> active) {
        modulation_active_ = std::move(active);
    }

    void paint(pulp::canvas::Canvas& canvas) override {
        const auto b = local_bounds();
        const float cx = b.width * 0.5f;
        const float cy = b.height * 0.43f;
        const float radius = paint_radius_for_bounds(b.width, b.height);
        const float angle = pulp::view::Knob::start_angle +
                            (pulp::view::Knob::end_angle - pulp::view::Knob::start_angle) * value();

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
    std::function<bool()> modulation_active_;
};

} // namespace detail

class PortRegion final : public pulp::view::View {
public:
    explicit PortRegion(std::string label) : label_(std::move(label)) {
        set_id("grainfield-port-" + label_);
        set_access_label(label_ + " (I/O port; non-automatable)");
        set_access_disabled("false");
    }
    void paint(pulp::canvas::Canvas& canvas) override {
        const auto b = local_bounds();
        const float r = std::min(b.width, b.height) * 0.34f;
        canvas.set_stroke_color(pulp::canvas::Color::rgba8(181, 171, 151));
        canvas.set_line_width(std::max(1.0f, r * 0.12f));
        canvas.stroke_circle(b.width * 0.5f, b.height * 0.5f, r);
    }
    const std::string& label() const { return label_; }
private:
    std::string label_;
};

class GrainfieldEditor final : public pulp::view::View {
public:
    GrainfieldEditor(Grainfield& processor, pulp::state::StateStore& store)
        : processor_(processor), store_(store), panel_(pulp::canvas::SvgImage::from_string(kGrainfieldPanelSvg)) {
        set_id("grainfield-editor");
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
        if (std::string_view(kGrainfieldPanelSourceStatus) ==
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
        const auto ink = pulp::canvas::Color::rgba8(238, 233, 220);
        const auto accent = pulp::canvas::Color::rgba8(217, 194, 158);
        const auto muted = pulp::canvas::Color::rgba8(185, 192, 201);
        label("GRAINFIELD", 100.3465f, 17.0f, 8.0f, ink, pulp::canvas::TextAlign::center, 700);
        label("MIDI GRAIN SYNTH", 100.3465f, 25.0f, 3.1f, accent);
        label("GRAIN", 100.3465f, 46.0f, 3.2f, accent, pulp::canvas::TextAlign::center, 700);
        label("DENSITY", 101.4f, 97.0f, 3.8f, ink, pulp::canvas::TextAlign::center, 700);
        label("TIME", 33.8f, 139.0f, 3.8f, ink, pulp::canvas::TextAlign::center, 700);
        label("PITCH", 168.9f, 139.0f, 3.8f, ink, pulp::canvas::TextAlign::center, 700);
        label("SIZE", 69.8f, 198.0f, 3.8f, ink, pulp::canvas::TextAlign::center, 700);
        label("SHAPE", 133.0f, 198.0f, 3.8f, ink, pulp::canvas::TextAlign::center, 700);
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
    }

    struct Control { detail::PanelKnob* knob; float x; float y; float radius; };
    struct Port { PortRegion* view; float x; float y; float radius; };
    Grainfield& processor_;
    pulp::state::StateStore& store_;
    pulp::canvas::SvgImage panel_;
    std::vector<Control> controls_;
    std::vector<Port> ports_;
    std::vector<pulp::view::ParameterBinding> bindings_;
    std::optional<pulp::state::ParamID> context_target_;
};

} // namespace grainfield

#endif // !PULP_HEADLESS
