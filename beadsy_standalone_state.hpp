#pragma once

#include <pulp/format/standalone.hpp>
#include <pulp/format/plugin_state_io.hpp>

#include <filesystem>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <vector>
#include <string_view>
#include <string>

namespace beadsy {

struct StandaloneStateResult {
    bool ok = false;
    std::string message;
};

inline StandaloneStateResult save_state(
    pulp::format::StandaloneApp& app,
    const std::filesystem::path& path,
    int = 0) {
    if (!app.processor()) return {false, "standalone processor unavailable"};
    const auto bytes = pulp::format::plugin_state_io::serialize(
        app.state(), *app.processor());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return {false, "cannot open state file for writing"};
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file) return {false, "cannot write state file"};
    return {true, {}};
}

inline StandaloneStateResult load_state(
    pulp::format::StandaloneApp& app,
    const std::filesystem::path& path,
    int = 0) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {false, "cannot open state file for reading"};
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    if (!app.processor() || !pulp::format::plugin_state_io::deserialize(
                                bytes, app.state(), *app.processor())) {
        return {false, "state payload rejected"};
    }
    return {true, {}};
}

} // namespace beadsy
