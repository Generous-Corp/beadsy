#pragma once

// The band under the divider is the only non-hardware surface on the panel:
// a "?" that opens the help and acknowledgement panel, and the Source chip,
// the software stand-in for a patch cable. Both are Lab-only until the Lab
// configuration becomes the product.

#include <pulp/view/view.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace beadsy {

struct HelpSection {
  const char *title;
  const char *body;
  /// Painted under a rule: the sources and licenses stand apart from the help.
  bool rule_before = false;
};

/// Acknowledgement first, then how the instrument works, in our own words.
inline const std::vector<HelpSection> &help_sections() {
  static const std::vector<HelpSection> sections{
      {"About Beadsy",
       "Beadsy is an independent software instrument inspired by Beads, the "
       "granular texture synthesizer designed by Emilie Gillet for Mutable "
       "Instruments. The signal processing is adapted from the Beads firmware, "
       "which Emilie Gillet released under the MIT license; every adapted file "
       "keeps its original copyright notice. The panel artwork is adapted from the "
       "Beads front panel design by Mutable Instruments, shared under the "
       "Creative Commons BY-SA 3.0 license, with the Mutable Instruments name "
       "and mark removed; Beadsy's adaptation is shared under the same license. "
       "Beadsy is not "
       "affiliated with, sponsored by, or endorsed by Mutable Instruments or "
       "Emilie Gillet. Our sincere thanks for the original work and for sharing "
       "its source."},
      {"What it does",
       "Audio is recorded continuously into a buffer. Each time a grain is "
       "requested, a replay head is placed somewhere on that buffer and plays "
       "it back with its own envelope. Up to thirty heads can fly along the "
       "buffer at once, and there is a reverb. With no audio source selected, "
       "the buffer is filled from eight banks of internal wavetables instead."},
      {"Source",
       "The hardware decides between processing audio and playing its internal "
       "wavetables by sensing a patch cable. Here you choose. Internal synth "
       "plays the wavetables and is played from the keyboard. Main In processes "
       "the plug-in's main audio input (the track it sits on). Side Chain "
       "processes the second input, which the host fills: in Logic, use the "
       "Side Chain pop-up in the plug-in header; in REAPER, send audio to "
       "channels 3 and 4 of this track."},
      {"Playing it",
       "A MIDI note sets the pitch and acts as the hardware's SEED gate. SEED "
       "starts latched, like the module at power-on: lit, breathing, grains "
       "running at the DENSITY rate. A tap toggles the latch off and on, like "
       "FREEZE. Press and hold SEED for a burst: grains play while you hold it "
       "and the latch goes back to how it was. (The module itself needs a "
       "four-second hold, or FREEZE while holding, to latch; both still work.) "
       "Unlatched, grains fire only while a key or the button is held; at the "
       "twelve o'clock DENSITY position exactly one grain per press. Latched, "
       "grains fire continuously at the DENSITY rate: clockwise a randomly "
       "modulated rate, counter-clockwise a steady rate, twelve o'clock none. "
       "Note Gate, in the Source menu, is the amplifier a rack would put after "
       "the module: with it on, the sound fades out over the Release time when "
       "you let go of the keys."},
      {"Grain controls",
       "TIME picks where on the buffer a grain starts, from the newest audio to "
       "the oldest. SIZE sets the grain duration, from 30 ms to 4 s, and plays it "
       "backwards left of eleven o'clock; fully clockwise it becomes a "
       "never-ending grain, which turns the instrument into a delay. SHAPE morphs "
       "the grain envelope from clicky rectangles to slow attacks. PITCH "
       "transposes by up to two octaves either way. All four are read when a "
       "grain starts and stay fixed for that grain, so turning a knob makes a "
       "trail of different grains rather than bending the ones playing."},
      {"The small knobs",
       "The four small knobs under TIME, SIZE, SHAPE and PITCH are the "
       "attenurandomizers. They set how much each parameter is randomized per "
       "grain, with a peaky distribution to the left of centre and a uniform one "
       "to the right. When grains are randomized their stereo position is too."},
      {"Freeze, Quality, Mix",
       "FREEZE stops recording so the heads keep playing what is already in the "
       "buffer. The coloured button under DENSITY selects the recording quality, "
       "from clean 48 kHz to a cassette-like character. FEEDBACK sends output "
       "back into the input (or, with the internal synth, picks the wavetable "
       "bank), DRY/WET balances the untouched signal against the grains, and "
       "REVERB adds the room."},
      {"Modulation",
       "Right-click any knob or button to open its modulation menu: two shared "
       "LFOs with waveform, rate and depth, each routable to any control. The "
       "settings are ordinary host parameters, so they are automatable and "
       "saved with the project."},
      {"Sources, licenses and thanks",
       "The granular engine is adapted from the Beads firmware by Emilie Gillet "
       "(Mutable Instruments), used under the MIT license, together with files "
       "from her stmlib library, also MIT. Source: github.com/pichenettes/eurorack "
       "(Beads at commit f4cfa769, stmlib at commit e3bd7c9c). The license terms "
       "are stated in that repository's README and in the notice at the top of "
       "every adapted file, which Beadsy keeps verbatim: "
       "github.com/pichenettes/eurorack/blob/f4cfa769/README.md. "
       "The help above paraphrases the Beads manual at "
       "mutable-instruments.net/modules/beads. Beadsy is built on the Pulp SDK "
       "(MIT). The panel artwork is adapted from the Beads front panel (Mutable "
       "Instruments, CC BY-SA 3.0, github.com/pichenettes/eurorack, "
       "beads/hardware_design/panel); the name and mark were removed and the "
       "Beadsy lettering added, and the adaptation is CC BY-SA 3.0 as well "
       "(creativecommons.org/licenses/by-sa/3.0). Mutable Instruments and "
       "Emilie Gillet are not "
       "affiliated with Beadsy and do not endorse it. Thank you, Emilie, for the "
       "instrument and for publishing its source.",
       true},
  };
  return sections;
}

/// Full-root overlay: a card with the help text, wheel-scrolled, dismissed
/// with Escape, the close mark, or a press outside the card.
class HelpPanel final : public pulp::view::View {
public:
  HelpPanel() {
    set_id("beadsy-help-panel");
    set_focusable(true);
    set_z_index(2000);
  }
  std::function<void()> on_close;

  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    canvas.set_fill_color(pulp::canvas::Color::rgba8(20, 22, 26, 150));
    canvas.fill_rect(0, 0, b.width, b.height);
    const auto card = card_rect();
    canvas.set_fill_color(pulp::canvas::Color::rgba8(38, 38, 44));
    canvas.fill_rounded_rect(card.x, card.y, card.width, card.height, 8.0f);
    // Close mark.
    const auto close = close_rect();
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(200, 203, 208));
    canvas.set_line_width(1.6f);
    canvas.stroke_line(close.x + 5, close.y + 5, close.x + close.width - 5,
                       close.y + close.height - 5);
    canvas.stroke_line(close.x + close.width - 5, close.y + 5, close.x + 5,
                       close.y + close.height - 5);
    // Text column, clipped to the card below the title row.
    const float pad = 16.0f;
    const float left = card.x + pad;
    const float width = card.width - 2 * pad;
    const float top = card.y + close.height + 8.0f;
    const float bottom = card.y + card.height - pad * 0.5f;
    canvas.save();
    canvas.clip_rect(card.x, top, card.width, bottom - top);
    float y = top - scroll_y_;
    const float body_size = 12.0f, title_size = 14.0f;
    const float body_line = body_size * 1.45f;
    for (const auto &section : help_sections()) {
      if (section.rule_before) {
        y += 6.0f;
        canvas.set_stroke_color(pulp::canvas::Color::rgba8(90, 94, 100));
        canvas.set_line_width(1.0f);
        if (y > top - 2.0f && y < bottom + 2.0f)
          canvas.stroke_line(left, y, left + width, y);
        y += 10.0f;
      }
      canvas.set_font_full("Roboto Mono", title_size, 700, 0, 0.0f);
      canvas.set_text_align(pulp::canvas::TextAlign::left);
      canvas.set_fill_color(pulp::canvas::Color::rgba8(236, 238, 232));
      y += title_size;
      if (y > top - title_size && y < bottom + title_size)
        canvas.fill_text(section.title, left, y);
      y += 8.0f;
      canvas.set_font_full("Roboto Mono", body_size, 400, 0, 0.0f);
      canvas.set_fill_color(pulp::canvas::Color::rgba8(200, 203, 208));
      for (const auto &line : wrap(canvas, section.body, width)) {
        y += body_line;
        if (y > top - body_line && y < bottom + body_line)
          canvas.fill_text(line, left, y);
      }
      y += body_line;
    }
    content_height_ = (y + scroll_y_) - top;
    canvas.restore();
  }

  void on_mouse_event(const pulp::view::MouseEvent &event) override {
    if (event.is_wheel) {
      scroll_by(event.scroll_delta_y);
      return;
    }
    if (!event.isPress())
      return;
    const auto card = card_rect();
    const bool inside = event.position.x >= card.x &&
                        event.position.x <= card.x + card.width &&
                        event.position.y >= card.y &&
                        event.position.y <= card.y + card.height;
    const auto close = close_rect();
    const bool on_close = event.position.x >= close.x &&
                          event.position.x <= close.x + close.width &&
                          event.position.y >= close.y &&
                          event.position.y <= close.y + close.height;
    if (!inside || on_close)
      dismiss();
  }
  bool on_key_event(const pulp::view::KeyEvent &event) override {
    if (!event.is_down)
      return false;
    if (event.key == pulp::view::KeyCode::escape) {
      dismiss();
      return true;
    }
    if (event.key == pulp::view::KeyCode::down) {
      scroll_by(40.0f);
      return true;
    }
    if (event.key == pulp::view::KeyCode::up) {
      scroll_by(-40.0f);
      return true;
    }
    return false;
  }
  bool wants_mouse_input() const override { return true; }
  float scroll_y() const { return scroll_y_; }
  float content_height() const { return content_height_; }
  void dismiss() {
    if (on_close)
      on_close();
  }

  /// Centre of the close mark in panel coordinates, for proofs.
  pulp::view::Point close_mark_centre() const {
    const auto c = close_rect();
    return {c.x + c.width * 0.5f, c.y + c.height * 0.5f};
  }

  static std::vector<std::string> wrap(pulp::canvas::Canvas &canvas,
                                       const std::string &text, float width) {
    std::vector<std::string> lines;
    std::istringstream words(text);
    std::string word, line;
    while (words >> word) {
      const std::string candidate = line.empty() ? word : line + " " + word;
      if (!line.empty() && canvas.measure_text(candidate) > width) {
        lines.push_back(line);
        line = word;
      } else {
        line = candidate;
      }
      // A single token wider than the column (a long URL) breaks at the
      // last character that fits rather than running off the card.
      while (canvas.measure_text(line) > width && line.size() > 1) {
        std::size_t cut = line.size() - 1;
        while (cut > 1 && canvas.measure_text(line.substr(0, cut)) > width)
          --cut;
        lines.push_back(line.substr(0, cut));
        line = line.substr(cut);
      }
    }
    if (!line.empty())
      lines.push_back(line);
    return lines;
  }

private:
  pulp::view::Rect card_rect() const {
    const auto b = local_bounds();
    const float margin = std::min(18.0f, b.width * 0.05f);
    return {margin, margin * 2.0f, b.width - 2 * margin, b.height - margin * 3.0f};
  }
  pulp::view::Rect close_rect() const {
    const auto card = card_rect();
    return {card.x + card.width - 30.0f, card.y + 4.0f, 26.0f, 26.0f};
  }
  void scroll_by(float delta) {
    const auto card = card_rect();
    const float visible = card.height - 40.0f;
    const float max_scroll = std::max(0.0f, content_height_ - visible);
    scroll_y_ = std::clamp(scroll_y_ + delta, 0.0f, max_scroll);
    request_repaint();
  }
  float scroll_y_ = 0.0f;
  float content_height_ = 0.0f;
};

/// The "?" at the left of the band.
class HelpButton final : public pulp::view::View {
public:
  HelpButton() {
    set_id("beadsy-help-button");
    set_access_role(pulp::view::View::AccessRole::button);
    set_access_label("About Beadsy and help");
    set_cursor(pulp::view::View::CursorStyle::pointer);
    set_focusable(true);
  }
  std::function<void()> on_press;
  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    const float r = std::min(b.width, b.height) * 0.5f - 1.0f;
    const float cx = b.width * 0.5f, cy = b.height * 0.5f;
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(120, 124, 128));
    canvas.set_line_width(std::max(1.0f, r * 0.12f));
    canvas.stroke_circle(cx, cy, r);
    canvas.set_font_full("Roboto Mono", r * 1.35f, 700, 0, 0.0f);
    canvas.set_text_align(pulp::canvas::TextAlign::center);
    canvas.set_fill_color(pulp::canvas::Color::rgba8(120, 124, 128));
    canvas.fill_text("?", cx, cy + r * 0.5f);
  }
  void on_mouse_down(pulp::view::Point) override {
    if (on_press)
      on_press();
  }
  bool wants_mouse_input() const override { return true; }
};

/// The Source chip: a pill that names the current source and opens the
/// Source menu on any press, primary or secondary.
class SourceChip final : public pulp::view::View {
public:
  SourceChip() {
    set_id("beadsy-source-chip");
    set_access_role(pulp::view::View::AccessRole::button);
    set_access_label("Source; click to change");
    set_cursor(pulp::view::View::CursorStyle::pointer);
    set_focusable(true);
  }
  std::function<std::string()> label_provider;
  std::function<void(pulp::view::Point)> on_press;
  void paint(pulp::canvas::Canvas &canvas) override {
    const auto b = local_bounds();
    const float radius = b.height * 0.5f;
    canvas.set_fill_color(pulp::canvas::Color::rgba8(255, 255, 255, 180));
    canvas.fill_rounded_rect(0.5f, 0.5f, b.width - 1.0f, b.height - 1.0f, radius);
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(150, 154, 158));
    canvas.set_line_width(1.0f);
    canvas.stroke_rounded_rect(0.5f, 0.5f, b.width - 1.0f, b.height - 1.0f, radius);
    const std::string text = label_provider ? label_provider() : "SOURCE";
    canvas.set_font_full("Roboto Mono", b.height * 0.46f, 700, 0, b.height * 0.06f);
    canvas.set_text_align(pulp::canvas::TextAlign::center);
    canvas.set_fill_color(pulp::canvas::Color::rgba8(80, 84, 88));
    canvas.fill_text(text, b.width * 0.5f - b.height * 0.25f, b.height * 0.68f);
    // Disclosure mark at the right end.
    const float mx = b.width - b.height * 0.75f, my = b.height * 0.5f;
    const float s = b.height * 0.14f;
    canvas.set_stroke_color(pulp::canvas::Color::rgba8(80, 84, 88));
    canvas.set_line_width(1.2f);
    canvas.stroke_line(mx - s, my - s * 0.5f, mx, my + s * 0.5f);
    canvas.stroke_line(mx, my + s * 0.5f, mx + s, my - s * 0.5f);
  }
  void on_mouse_down(pulp::view::Point position) override {
    if (on_press)
      on_press(position);
  }
  bool wants_mouse_input() const override { return true; }
};

} // namespace beadsy
