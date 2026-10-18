#pragma once

#if !defined(PULP_HEADLESS)

#include "grainfield_panel_svg.hpp"
#include "grainfield.hpp"

#include <pulp/canvas/canvas.hpp>
#include <pulp/view/parameter_binding.hpp>
#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace grainfield {

namespace detail {

class PanelKnob final : public pulp::view::Knob {
public:
    PanelKnob(std::string caption, float diameter)
        : caption_(std::move(caption)), diameter_(diameter) {
        set_show_label(false);
        set_render_style(pulp::view::WidgetRenderStyle::standard);
        set_id("grainfield-" + caption_);
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

        canvas.set_fill_color(pulp::canvas::Color::rgba8(228, 220, 192));
        canvas.set_font("Inter", std::max(7.0f, diameter_ * 0.16f));
        canvas.fill_text_anchored(caption_, cx, b.height * 0.92f,
                                 pulp::canvas::Canvas::TextAnchor::GlyphCenter);
    }

private:
    std::string caption_;
    float diameter_ = 0.0f;
};

} // namespace detail

class GrainfieldEditor final : public pulp::view::View {
public:
    explicit GrainfieldEditor(pulp::state::StateStore& store) : store_(store) {
        set_id("grainfield-editor");
        set_background_color(pulp::canvas::Color::rgba8(22, 21, 19));
        build_controls();
        set_continuous_repaint(false);
    }

    void on_resized() override { layout_controls(); }

    void paint(pulp::canvas::Canvas& canvas) override {
        const auto b = local_bounds();
        if (!canvas.draw_svg(kGrainfieldPanelSvg, 0.0f, 0.0f, b.width, b.height)) {
            canvas.set_fill_color(pulp::canvas::Color::rgba8(22, 21, 19));
            canvas.fill_rect(0, 0, b.width, b.height);
        }
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
        };
        for (const auto& p : placements) {
            const float diameter = p.r * 2.0f;
            auto knob = std::make_unique<detail::PanelKnob>(p.name, diameter);
            auto* raw = knob.get();
            add_child(std::move(knob));
            bindings_.push_back(pulp::view::bind_parameter(*raw, store_, p.id));
            controls_.push_back({raw, p.x, p.y, p.r});
        }
    }

    void layout_controls() {
        const float sx = local_bounds().width / 70.8f;
        const float sy = local_bounds().height / 128.5f;
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
    pulp::state::StateStore& store_;
    std::vector<Control> controls_;
    std::vector<pulp::view::ParameterBinding> bindings_;
};

} // namespace grainfield

#endif // !PULP_HEADLESS
