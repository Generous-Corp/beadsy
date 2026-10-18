#pragma once

#include <pulp/format/standalone.hpp>

#include <filesystem>
#include <string_view>

namespace grainfield {

using StandaloneStateResult = pulp::format::StandaloneStateFileResult;
using StandaloneStatePolicy = pulp::format::StandaloneStateFilePolicy;

inline StandaloneStateResult save_state(
    const pulp::format::StandaloneApp& app,
    const std::filesystem::path& path,
    StandaloneStatePolicy policy = {}) {
    return pulp::format::save_standalone_state_file(app, path, policy);
}

inline StandaloneStateResult load_state(
    pulp::format::StandaloneApp& app,
    const std::filesystem::path& path,
    StandaloneStatePolicy policy = {}) {
    return pulp::format::load_standalone_state_file(app, path, policy);
}

} // namespace grainfield
