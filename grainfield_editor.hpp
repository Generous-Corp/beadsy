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
        const float radius = std::min(b.width, b.height * 0.78f) * 0.34f;
        const float angle = pulp::view::Knob::start_angle +
                            (pulp::view::Knob::end_angle - pulp::view::Knob::start_angle) * value();

        canvas.set_fill_color(pulp::canvas::Color::rgba8(20, 20, 20));
        canvas.fill_circle(cx, cy, radius * 1.14f);
        canvas.set_fill_color(pulp::canvas::Color::rgba8(43, 42, 39));
        canvas.fill_circle(cx - radius * 0.05f, cy - radius * 0.07f, radius);
        canvas.set_stroke_color(pulp::canvas::Color::rgba8(164, 150, 113));
        canvas.set_line_width(std::max(1.0f, radius * 0.045f));
        canvas.stroke_circle(cx, cy, radius);

        canvas.set_stroke_color(pulp::canvas::Color::rgba8(224, 209, 169));
        canvas.set_line_cap(pulp::canvas::LineCap::round);
        canvas.set_line_width(std::max(1.2f, radius * 0.075f));
        canvas.stroke_line(cx + radius * 0.18f * std::cos(angle),
                           cy + radius * 0.18f * std::sin(angle),
                           cx + radius * 0.76f * std::cos(angle),
                           cy + radius * 0.76f * std::sin(angle));

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

    void build_controls() {
        constexpr Placement placements[] = {
            {kDensity, "Density", 35.735f, 104.4f, 9.25f},
            {kTime, "Time", 11.915f, 90.1f, 9.25f},
            {kPitch, "Pitch", 59.515f, 90.1f, 9.25f},
            {kSize, "Size", 24.615f, 67.9f, 7.2f},
            {kShape, "Shape", 46.865f, 67.9f, 7.2f},
            {0, "Aux 01", 7.115f, 67.9f, 3.0f},
            {0, "Aux 02", 64.365f, 67.9f, 3.0f},
            {0, "Aux 03", 64.365f, 51.7f, 3.0f},
            {0, "Aux 04", 18.565f, 47.9f, 3.0f},
            {0, "Aux 05", 30.015f, 47.9f, 3.0f},
            {0, "Aux 06", 41.465f, 47.9f, 3.0f},
            {0, "Aux 07", 52.915f, 47.9f, 3.0f},
            {0, "Small 01", 7.115f, 32.0f, 4.4f},
            {0, "Small 02", 18.565f, 32.0f, 4.4f},
            {0, "Small 03", 30.015f, 32.0f, 4.4f},
            {0, "Small 04", 41.465f, 32.0f, 4.4f},
            {0, "Small 05", 52.915f, 32.0f, 4.4f},
            {0, "Small 06", 64.365f, 32.0f, 4.4f},
            {0, "Small 07", 7.115f, 17.4f, 4.4f},
            {0, "Small 08", 18.565f, 17.4f, 4.4f},
            {0, "Small 09", 30.015f, 17.4f, 4.4f},
            {0, "Small 10", 41.465f, 17.4f, 4.4f},
            {0, "Small 11", 52.915f, 17.4f, 4.4f},
            {0, "Small 12", 64.365f, 17.4f, 4.4f},
        };
        for (const auto& p : placements) {
            const float diameter = p.r * 2.0f;
            auto knob = std::make_unique<detail::PanelKnob>(p.name, diameter);
            auto* raw = knob.get();
            if (p.id != 0) {
                raw->set_modulation_indicator([this, id = p.id] {
                    return processor_.lfo(id).enabled;
                });
                raw->on_context_menu = [this, id = p.id](pulp::view::Point position) {
                    show_modulation_context_menu(id, position);
                };
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
    }

    struct Control { detail::PanelKnob* knob; float x; float y; float radius; };
    Grainfield& processor_;
    pulp::state::StateStore& store_;
    pulp::canvas::SvgImage panel_;
    std::vector<Control> controls_;
    std::vector<pulp::view::ParameterBinding> bindings_;
    std::optional<pulp::state::ParamID> context_target_;
};

} // namespace grainfield

#endif // !PULP_HEADLESS
