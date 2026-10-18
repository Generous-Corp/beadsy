#include "grainfield.hpp"
#include "grainfield_standalone_state.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

TEST_CASE("Grainfield standalone command bridge round trips state") {
    const auto root = std::filesystem::temp_directory_path() /
                      "grainfield-standalone-state-integration";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);

    pulp::format::StandaloneApp app(grainfield::create_grainfield);
    pulp::format::StandaloneConfig config;
    config.input_channels = 0;
    config.output_channels = 2;
    config.screenshot_path = (root / "startup.png").string();
    config.screenshot_frame_delay = 1;
    app.set_config(config);
    REQUIRE(app.start());

    app.state().set_value(grainfield::kGain, -9.0f);
    app.state().set_value(grainfield::kFreeze, 1.0f);
    const auto path = root / "preset.pulpstate";
    REQUIRE(grainfield::save_state(app, path).ok);

    app.state().set_value(grainfield::kGain, 0.0f);
    app.state().set_value(grainfield::kFreeze, 0.0f);
    const auto loaded = grainfield::load_state(app, path);
    REQUIRE(loaded.ok);
    REQUIRE(app.state().get_value(grainfield::kGain) == -9.0f);
    REQUIRE(app.state().get_value(grainfield::kFreeze) == 1.0f);

    app.stop();
    std::filesystem::remove_all(root, ec);
}
