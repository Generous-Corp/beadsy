#pragma once

#include <pulp/view/context_menu.hpp>
#if __has_include(<pulp/view/native_context_menu.hpp>)
#include <pulp/view/native_context_menu.hpp>
#define BEADSY_HAS_NATIVE_MENUS 1
#endif
#include <cmath>

namespace beadsy::detail {

// Product presentation on the ordinary SDK menu: retain its focus, keyboard,
// scrolling and capture behavior while drawing accurate waveform cells and
// normalising the legacy gesture phases for slider rows.
class ModulationContextMenu final : public pulp::view::ContextMenu {
public:
  static ModulationContextMenu *show(
      pulp::view::View *root, pulp::view::Point position, std::vector<Item> items,
      std::function<void(std::optional<int>)> close) {
#if defined(BEADSY_HAS_NATIVE_MENUS)
    // Trial: the SDK's native presentation (BEADSY_NATIVE_MENUS=1 sets the
    // process default). The platform menu owns its lifetime, so there is no
    // view to return; callers null-check.
    if (ContextMenu::resolve_presentation(std::nullopt) == ContextMenu::Presentation::native &&
        pulp::view::show_native_context_menu(*root, position, std::move(items), std::move(close)))
      return nullptr;
#endif
    for (std::size_t i = root->child_count(); i > 0; --i) {
      if (auto *old = dynamic_cast<ContextMenu *>(root->child_at(i - 1))) {
        old->on_key_event({.key = pulp::view::KeyCode::escape, .is_down = true});
        break;
      }
    }
    auto menu = std::make_unique<ModulationContextMenu>();
    auto *raw = menu.get();
    raw->set_items(std::move(items));
    raw->set_anchor(position);
    raw->set_position(Position::absolute);
    raw->set_left(0);
    raw->set_top(0);
    raw->set_bounds(root->local_bounds());
    raw->flex().preferred_width = root->local_bounds().width;
    raw->flex().preferred_height = root->local_bounds().height;
    // The editor raises its knobs and toggle hit targets above the faceplate
    // with explicit z-indices. The menu must sit above all of them, or the
    // z-sorted hit-test hands a click on a menu row to the control underneath
    // and the knobs paint over the open menu.
    raw->set_z_index(1000);
    raw->on_close = [root, raw, close = std::move(close)](std::optional<int> result) {
      if (close)
        close(result);
      root->remove_child(raw);
    };
    root->add_child(std::move(menu));
    raw->set_overlay_consumes_outside_click(true);
    raw->set_focus(true);
    raw->claim_input_focus();
    return raw;
  }

  void paint(pulp::canvas::Canvas &canvas) override {
    ContextMenu::paint(canvas);
    const auto geometry = layout();
    canvas.save();
    canvas.clip_rect(geometry.box.x, geometry.box.y, geometry.box.width, geometry.box.height);
    for (std::size_t row = 0; row < items().size(); ++row) {
      const auto &item = items()[row];
      if (item.label != "Waveform")
        continue;
      for (std::size_t shape = 0; shape < 4; ++shape) {
        const auto cell = waveform_cell(geometry.rows[row], shape);
        canvas.set_fill_color(shape == item.choice_index
                                 ? pulp::canvas::Color::rgba8(82, 112, 142)
                                 : pulp::canvas::Color::rgba8(38, 38, 44));
        canvas.fill_rounded_rect(cell.x, cell.y, cell.width, cell.height, 2);
        canvas.set_stroke_color(pulp::canvas::Color::rgba8(232, 235, 228));
        canvas.set_line_width(1.1f);
        const auto point = [&](float x, float y) {
          return pulp::view::Point{cell.x + 3 + x * (cell.width - 6),
                                    cell.y + 3 + y * (cell.height - 6)};
        };
        const auto line = [&](float x1, float y1, float x2, float y2) {
          const auto a = point(x1, y1), b = point(x2, y2);
          canvas.stroke_line(a.x, a.y, b.x, b.y);
        };
        if (shape == 0) {
          for (int segment = 0; segment < 16; ++segment) {
            const float x = segment / 16.0f, next = (segment + 1) / 16.0f;
            line(x, 0.5f - 0.45f * std::sin(x * 6.2831853f),
                 next, 0.5f - 0.45f * std::sin(next * 6.2831853f));
          }
        } else if (shape == 1) {
          line(0, 1, 0.5f, 0);
          line(0.5f, 0, 1, 1);
        } else if (shape == 2) {
          line(0, 1, 0.15f, 1);
          line(0.15f, 1, 0.15f, 0);
          line(0.15f, 0, 0.6f, 0);
          line(0.6f, 0, 0.6f, 1);
          line(0.6f, 1, 1, 1);
        } else {
          line(0, 1, 0.85f, 0);
          line(0.85f, 0, 0.85f, 1);
          line(0.85f, 1, 1, 1);
        }
      }
    }
    canvas.restore();
  }

  void on_mouse_event(const pulp::view::MouseEvent &event) override {
    const auto geometry = layout();
    auto mapped = event;
    if (slider_row_ >= 0 && !event.is_wheel) {
      // Honor the SDK's legacy gesture convention as well as native explicit
      // phases. A release carrying movement metadata must still end capture.
      if (event.phase == pulp::view::MousePhase::automatic)
        mapped.phase = event.is_down && !event.has_movement_delta
                           ? pulp::view::MousePhase::release
                           : pulp::view::MousePhase::drag;
      if (event.is_cancelled || mapped.phase == pulp::view::MousePhase::release) {
        mapped.phase = pulp::view::MousePhase::release;
        mapped.is_down = false;
        mapped.has_movement_delta = false;
        slider_row_ = -1;
        ContextMenu::on_mouse_event(mapped);
        return;
      }
      if (event.has_movement_delta)
        mapped.phase = pulp::view::MousePhase::drag;
    }
    const bool inside = event.position.x >= geometry.box.x &&
                        event.position.x <= geometry.box.x + geometry.box.width &&
                        event.position.y >= geometry.box.y &&
                        event.position.y <= geometry.box.y + geometry.box.height;
    if (slider_row_ < 0 && inside && event.isPress() && !event.is_wheel) {
      for (std::size_t row = 0; row < items().size(); ++row) {
        const auto bounds = geometry.rows[row];
        if (event.position.y < bounds.y || event.position.y > bounds.y + bounds.height)
          continue;
        if (items()[row].label == "Waveform") {
          for (std::size_t shape = 0; shape < 4; ++shape) {
            const auto cell = waveform_cell(bounds, shape);
            if (event.position.x < cell.x || event.position.x > cell.x + cell.width)
              continue;
            auto selected = items()[row];
            items()[row].choice_index = shape;
            selected.choice_index = shape;
            if (selected.on_change)
              selected.on_change(selected);
            request_repaint();
            return;
          }
        }
        if (items()[row].control_kind == Item::ControlKind::slider &&
            event.position.x >= bounds.x && event.position.x <= bounds.x + bounds.width)
          slider_row_ = static_cast<int>(row);
      }
    }
    // The SDK maps the pointer against the track it paints (a value label
    // shortens both), so the position passes through unchanged.
    if (event.phase == pulp::view::MousePhase::release)
      slider_row_ = -1;
    ContextMenu::on_mouse_event(mapped);
  }

  static pulp::view::Rect waveform_cell(pulp::view::Rect row, std::size_t shape) {
    return {row.x + 90.0f + static_cast<float>(shape) * 21.0f,
             row.y + (row.height - 18.0f) * 0.5f, 18.0f, 18.0f};
  }

private:
  int slider_row_ = -1;
};

} // namespace beadsy::detail
