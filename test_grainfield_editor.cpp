#include <catch2/catch_test_macros.hpp>
#include <pulp/state/store.hpp>
#include <pulp/view/screenshot.hpp>
#include "grainfield.hpp"

#include <cstdlib>
#include <fstream>

TEST_CASE("Grainfield native editor captures a non-empty faithful panel frame") {
    grainfield::Grainfield processor;
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
    if (const char* path = std::getenv("GRAINFIELD_EDITOR_CAPTURE")) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(capture.png.data()),
                  static_cast<std::streamsize>(capture.png.size()));
        REQUIRE(static_cast<bool>(out));
    }
}

TEST_CASE("Grainfield editor exposes a right-click modulation target seam") {
    grainfield::Grainfield processor;
    pulp::state::StateStore store;
    processor.set_state_store(&store);
    processor.define_parameters(store);
    auto editor = processor.create_view();
    auto* native = dynamic_cast<grainfield::GrainfieldEditor*>(editor.get());
    REQUIRE(native != nullptr);
    REQUIRE_FALSE(native->modulation_context_target().has_value());
    native->open_modulation_context(grainfield::kDensity);
    REQUIRE(native->modulation_context_target() == grainfield::kDensity);
    native->clear_modulation_context();
    REQUIRE_FALSE(native->modulation_context_target().has_value());
}
