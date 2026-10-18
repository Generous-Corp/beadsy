#include "beadsy.hpp"
#include "beadsy_standalone_state.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

TEST_CASE("Beadsy standalone command bridge round trips state") {
    const auto root = std::filesystem::temp_directory_path() /
                      "beadsy-standalone-state-integration";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);

    pulp::format::StandaloneApp app(beadsy::create_beadsy);
    pulp::format::StandaloneConfig config;
    config.input_channels = 0;
    config.output_channels = 2;
    config.screenshot_path = (root / "startup.png").string();
    config.screenshot_frame_delay = 1;
    app.set_config(config);
    REQUIRE(app.start());

    app.state().set_value(beadsy::kGain, -9.0f);
    app.state().set_value(beadsy::kFreeze, 1.0f);
    const auto path = root / "preset.pulpstate";
    REQUIRE(beadsy::save_state(app, path).ok);

    app.state().set_value(beadsy::kGain, 0.0f);
    app.state().set_value(beadsy::kFreeze, 0.0f);
    const auto loaded = beadsy::load_state(app, path);
    REQUIRE(loaded.ok);
    REQUIRE(app.state().get_value(beadsy::kGain) == -9.0f);
    REQUIRE(app.state().get_value(beadsy::kFreeze) == 1.0f);

    app.stop();
    std::filesystem::remove_all(root, ec);
}
