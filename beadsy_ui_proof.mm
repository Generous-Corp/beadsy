#include "beadsy_ui_proof.hpp"
#include "beadsy.hpp"
#include "beadsy_editor.hpp"

#import <AppKit/AppKit.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace beadsy {
namespace {

// Authored control centres in editor points at the 350 x 635 default
// (mm * 350 / 70.8, y measured from the top). Scaled to the live view size.
constexpr float kUnit = 350.0f / 70.8f;
constexpr float kFreezeX = 8.7f * kUnit, kFreezeY = (128.5f - 110.5f) * kUnit;
constexpr float kSeedX = 62.0f * kUnit, kSeedY = (128.5f - 110.5f) * kUnit;
constexpr float kQualityX = 35.7f * kUnit, kQualityY = (128.5f - 83.24f) * kUnit;
constexpr float kAssignX = 7.1f * kUnit, kAssignY = (128.5f - 51.7f) * kUnit;
constexpr float kDensityX = 35.316f * kUnit, kDensityY = (128.5f - 104.4f) * kUnit;
constexpr float kGripX = 350.0f - 17.0f, kGripY = 635.0f - 17.0f;
constexpr float kBayX = 175.0f, kBayY = 560.0f; // empty faceplate below the labels

struct Proof {
  pulp::state::StateStore *store = nullptr;
  BeadsyEditor *editor = nullptr;
  std::weak_ptr<const std::uint64_t> editor_alive;
  std::string step;
  NSView *view = nil;
  int event_number = 1;

  bool editor_live() const { return !editor_alive.expired(); }

  void log(const std::string &json) const {
    std::fprintf(stderr, "[beadsy-ui-proof] %s\n", json.c_str());
    std::fflush(stderr);
  }

  NSSize content() const { return view.bounds.size; }

  std::string geometry_json() const {
    const auto vf = view.frame;
    const auto c = content();
    const auto wf = view.window ? view.window.frame : NSZeroRect;
    char buf[320];
    std::snprintf(buf, sizeof buf,
                  "\"view\":[%.1f,%.1f,%.1f,%.1f],\"content\":[%.1f,%.1f],"
                  "\"window\":[%.1f,%.1f,%.1f,%.1f]",
                  vf.origin.x, vf.origin.y, vf.size.width, vf.size.height,
                  c.width, c.height, wf.origin.x, wf.origin.y, wf.size.width,
                  wf.size.height);
    return buf;
  }

  float value(pulp::state::ParamID id) const { return store->get_value(id); }

  std::string state_json() const {
    char buf[320];
    const auto target = editor_live() ? editor->modulation_context_target()
                                      : std::nullopt;
    const int assign = editor_live() && editor->assign_control()
                           ? editor->assign_control()->destination()
                           : -1;
    std::snprintf(buf, sizeof buf,
                  "\"freeze\":%.1f,\"seed_mode\":%.1f,\"quality\":%.1f,"
                  "\"density\":%.3f,\"assign\":%d,\"menu_target\":%d",
                  value(kFreeze), value(kSeedMode), value(kQuality),
                  value(kDensity), assign, target ? static_cast<int>(*target) : -1);
    return buf;
  }

  void report(const char *phase) const {
    log("{\"step\":\"" + step + "\",\"phase\":\"" + phase + "\"," +
        geometry_json() + "," + state_json() + "}");
  }

  // Editor-space point (top-left origin, 350 x 635 design units) to the
  // event's window coordinates through the Pulp NSView, which is not flipped.
  NSPoint to_window(float x, float y) const {
    const auto c = content();
    const NSPoint local{x * (c.width / 350.0f), c.height - y * (c.height / 635.0f)};
    return [view convertPoint:local toView:nil];
  }

  void send(NSEventType type, NSPoint p, int click_count = 1) {
    NSEvent *event = [NSEvent mouseEventWithType:type
                                         location:p
                                    modifierFlags:0
                                        timestamp:[[NSProcessInfo processInfo] systemUptime]
                                     windowNumber:view.window.windowNumber
                                          context:nil
                                      eventNumber:event_number++
                                       clickCount:click_count
                                         pressure:type == NSEventTypeLeftMouseUp ||
                                                          type == NSEventTypeRightMouseUp
                                                      ? 0.0f
                                                      : 1.0f];
    // Dispatch straight to Pulp's NSView: a window that is not key swallows
    // its first click inside NSWindow's own routing, which would make a
    // proof depend on focus.
    switch (type) {
    case NSEventTypeLeftMouseDown: [view mouseDown:event]; break;
    case NSEventTypeLeftMouseUp: [view mouseUp:event]; break;
    case NSEventTypeLeftMouseDragged: [view mouseDragged:event]; break;
    case NSEventTypeRightMouseDown: [view rightMouseDown:event]; break;
    case NSEventTypeRightMouseUp: [view rightMouseUp:event]; break;
    case NSEventTypeMouseMoved: [view mouseMoved:event]; break;
    default: break;
    }
  }

  bool plugin_host() const {
    return [NSStringFromClass([view class]) hasPrefix:@"PulpPluginView"] ||
           [NSStringFromClass([view class]) hasPrefix:@"PulpGpuPluginView"];
  }

  void click(float x, float y, bool right = false) {
    const auto p = to_window(x, y);
    send(NSEventTypeMouseMoved, p);
    send(right ? NSEventTypeRightMouseDown : NSEventTypeLeftMouseDown, p);
    if (right && plugin_host()) {
      // Pulp opens the menu on the secondary press. The plug-in view hands
      // the matching release to the host's responder chain, and REAPER runs
      // a modal tracking loop on a synthetic release that never ends, which
      // froze the proof after the first menu. Leave the release to the host.
      log("{\"note\":\"plugin host: secondary release not synthesized\"}");
      return;
    }
    send(right ? NSEventTypeRightMouseUp : NSEventTypeLeftMouseUp, p);
  }

  void drag(float x0, float y0, float x1, float y1, int steps) {
    send(NSEventTypeMouseMoved, to_window(x0, y0));
    send(NSEventTypeLeftMouseDown, to_window(x0, y0));
    for (int i = 1; i <= steps; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(steps);
      send(NSEventTypeLeftMouseDragged,
           to_window(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t));
    }
    send(NSEventTypeLeftMouseUp, to_window(x1, y1));
  }
};

void after(double seconds, std::function<void()> fn) {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               static_cast<int64_t>(seconds * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{ fn(); });
}

bool is_pulp_view(NSView *view) {
  NSString *name = NSStringFromClass([view class]);
  for (NSString *prefix in @[ @"PulpMetalView", @"PulpView", @"PulpGpuPluginView",
                              @"PulpPluginView" ]) {
    if ([name hasPrefix:prefix]) return true;
  }
  return false;
}

NSView *find_pulp_view(NSView *root) {
  if (!root) return nil;
  if (is_pulp_view(root)) return root;
  for (NSView *child in root.subviews)
    if (auto *found = find_pulp_view(child)) return found;
  return nil;
}

NSView *find_pulp_view() {
  NSView *best = nil;
  for (NSWindow *w in [NSApp windows]) {
    if (auto *found = find_pulp_view(w.contentView)) {
      if (!best || found.bounds.size.height > best.bounds.size.height) best = found;
    }
  }
  return best;
}

using Steps = std::vector<std::function<void(Proof &)>>;

void run_sequence(const std::shared_ptr<Proof> &proof, Steps steps,
                  std::size_t index, double spacing) {
  if (index >= steps.size()) {
    proof->log("{\"step\":\"" + proof->step + "\",\"phase\":\"done\"}");
    return;
  }
  if (!proof->editor_live()) {
    proof->log("{\"step\":\"" + proof->step + "\",\"error\":\"editor closed\"}");
    return;
  }
  steps[index](*proof);
  after(spacing, [proof, steps = std::move(steps), index, spacing] {
    run_sequence(proof, steps, index + 1, spacing);
  });
}

Steps steps_for(const std::string &step) {
  const auto toggle = [](float x, float y, const char *tag, int presses) {
    Steps s;
    for (int i = 1; i <= presses; ++i)
      s.push_back([x, y, tag, i](Proof &p) {
        p.click(x, y);
        p.report((std::string(tag) + "-press-" + std::to_string(i)).c_str());
      });
    return s;
  };
  const auto menu = [](float x, float y, const char *tag) {
    Steps s;
    s.push_back([x, y, tag](Proof &p) {
      p.click(x, y, true);
      p.report((std::string(tag) + "-menu-open").c_str());
    });
    s.push_back([tag](Proof &p) {
      // Dismiss through an outside press on the empty bay, as a user would.
      p.click(kBayX, kBayY);
      p.report((std::string(tag) + "-menu-dismissed").c_str());
    });
    return s;
  };
  const auto grip = [](float dx, float dy, const char *tag) {
    Steps s;
    s.push_back([dx, dy, tag](Proof &p) {
      p.drag(kGripX, kGripY, kGripX + dx, kGripY + dy, 24);
      p.report((std::string(tag) + "-dragged").c_str());
    });
    s.push_back([tag](Proof &p) { p.report((std::string(tag) + "-settled").c_str()); });
    return s;
  };
  const auto join = [](std::initializer_list<Steps> parts) {
    Steps all;
    for (const auto &part : parts) all.insert(all.end(), part.begin(), part.end());
    return all;
  };
  if (step == "layout") return {[](Proof &p) { p.report("after"); }};
  if (step == "click-freeze") return toggle(kFreezeX, kFreezeY, "freeze", 3);
  if (step == "click-seed") return toggle(kSeedX, kSeedY, "seed", 3);
  if (step == "click-quality") return toggle(kQualityX, kQualityY, "quality", 2);
  if (step == "click-assign") return toggle(kAssignX, kAssignY, "assign", 2);
  if (step == "menu-freeze-split")
    return {[](Proof &p) {
              p.send(NSEventTypeRightMouseDown, p.to_window(kFreezeX, kFreezeY));
              p.report("freeze-right-down");
            },
            [](Proof &p) {
              p.send(NSEventTypeRightMouseUp, p.to_window(kFreezeX, kFreezeY));
              p.report("freeze-right-up");
            },
            [](Proof &p) {
              p.click(kBayX, kBayY);
              p.report("freeze-menu-dismissed");
            }};
  if (step == "menu-freeze") return {menu(kFreezeX, kFreezeY, "freeze")[0]};
  if (step == "menu-seed") return {menu(kSeedX, kSeedY, "seed")[0]};
  if (step == "menu-quality") return {menu(kQualityX, kQualityY, "quality")[0]};
  if (step == "menu-assign") return {menu(kAssignX, kAssignY, "assign")[0]};
  if (step == "drag-density")
    return {[](Proof &p) {
      p.drag(kDensityX, kDensityY, kDensityX, kDensityY - 60.0f, 20);
      p.report("after");
    }};
  if (step == "grip-drag") return grip(120.0f, 60.0f, "grip");
  if (step == "grip-drag-vertical") return grip(10.0f, 220.0f, "grip-vertical");
  if (step == "all")
    return join({toggle(kFreezeX, kFreezeY, "freeze", 2),
                 toggle(kSeedX, kSeedY, "seed", 3),
                 toggle(kQualityX, kQualityY, "quality", 2),
                 toggle(kAssignX, kAssignY, "assign", 2),
                 menu(kFreezeX, kFreezeY, "freeze"), menu(kSeedX, kSeedY, "seed"),
                 menu(kQualityX, kQualityY, "quality"),
                 menu(kAssignX, kAssignY, "assign"), grip(120.0f, 60.0f, "grip")});
  return {};
}

void start(const std::shared_ptr<Proof> &proof, int attempts_left) {
  if (!proof->editor_live()) return;
  proof->view = find_pulp_view();
  if (!proof->view || !proof->view.window) {
    if (attempts_left > 0) {
      after(0.3, [proof, attempts_left] { start(proof, attempts_left - 1); });
      return;
    }
    proof->log("{\"step\":\"" + proof->step + "\",\"error\":\"no pulp view\"}");
    return;
  }
  NSString *bundle = [NSBundle bundleForClass:[proof->editor_alive.expired() ? nil : proof->view class]].bundlePath;
  proof->log("{\"step\":\"" + proof->step + "\",\"phase\":\"before\",\"view_class\":\"" +
             std::string(NSStringFromClass([proof->view class]).UTF8String) +
             "\",\"bundle\":\"" + std::string(bundle ? bundle.UTF8String : "") + "\"," +
             proof->geometry_json() + "," + proof->state_json() + "}");
  auto steps = steps_for(proof->step);
  if (steps.empty()) {
    proof->log("{\"step\":\"" + proof->step + "\",\"error\":\"unknown step\"}");
    return;
  }
  run_sequence(proof, std::move(steps), 0, 0.35);
}

} // namespace

void install_ui_proof(pulp::state::StateStore &store, BeadsyEditor &editor) {
  const char *step = std::getenv("BEADSY_UI_PROOF");
  if (!step || !*step) return;
  auto proof = std::make_shared<Proof>();
  proof->store = &store;
  proof->editor = &editor;
  proof->editor_alive = editor.import_binding_lifetime_token();
  proof->step = step;
  after(1.5, [proof] { start(proof, 20); });
}

} // namespace beadsy
