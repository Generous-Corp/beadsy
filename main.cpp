#include "beadsy.hpp"
#include "beadsy_standalone_state.hpp"

#include <pulp/format/standalone.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace {
struct StateCommand {
    std::optional<std::filesystem::path> load;
    std::optional<std::filesystem::path> save;
};

std::optional<StateCommand> parse_state_command(int argc, char** argv) {
    StateCommand command;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--load-state" || arg == "--save-state") {
            if (++i >= argc) return std::nullopt;
            auto path = std::filesystem::path(argv[i]);
            if (arg == "--load-state") command.load = std::move(path);
            else command.save = std::move(path);
        } else {
            return std::nullopt;
        }
    }
    return command;
}
} // namespace

int main(int argc, char** argv) {
    const auto command = parse_state_command(argc, argv);
    if (!command) {
        std::cerr << "usage: Beadsy [--load-state FILE] [--save-state FILE]\n";
        return 2;
    }

    pulp::format::StandaloneApp app(beadsy::create_beadsy);
    pulp::format::StandaloneConfig config;
    config.input_channels = 0;
    config.output_channels = 2;

    // A state command is a bounded control-thread operation. Use the existing
    // screenshot-only startup path so it never opens a hardware device, then
    // apply exactly one load/save request and exit. Ordinary launches continue
    // through the normal editor/audio path below.
    if (command->load || command->save) {
        const auto temp_capture = std::filesystem::temp_directory_path() /
                                  "beadsy-state-command.png";
        config.screenshot_path = temp_capture.string();
        config.screenshot_frame_delay = 1;
        app.set_config(config);
        if (!app.start()) return 1;
        if (command->load) {
            const auto result = beadsy::load_state(app, *command->load);
            if (!result.ok) {
                std::cerr << "Beadsy state load failed: " << result.message << "\n";
                app.stop();
                return 1;
            }
        }
        if (command->save) {
            const auto result = beadsy::save_state(app, *command->save);
            if (!result.ok) {
                std::cerr << "Beadsy state save failed: " << result.message << "\n";
                app.stop();
                return 1;
            }
        }
        app.stop();
        std::error_code ec;
        std::filesystem::remove(temp_capture, ec);
        return 0;
    }

    app.set_config(config);
    return app.run_with_editor(false) ? 0 : 1;
}
