#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <pulp/format/headless.hpp>
#include "grainfield.hpp"

using Catch::Approx;

TEST_CASE("Grainfield processes audio") {
    pulp::format::HeadlessHost host(grainfield::create_grainfield);
    host.prepare(48000, 512);

    pulp::audio::Buffer<float> in(2, 512), out(2, 512);

    // Fill input with a test signal
    for (std::size_t i = 0; i < 512; ++i) {
        in.channel(0)[i] = 0.5f;
        in.channel(1)[i] = 0.5f;
    }

    const float* in_ptrs[] = {in.channel(0).data(), in.channel(1).data()};
    pulp::audio::BufferView<const float> iv(in_ptrs, 2, 512);
    auto ov = out.view();
    host.process(ov, iv);

    // With Mix at 100%, output should equal processed signal
    REQUIRE(out.channel(0)[0] != 0.0f);
}

TEST_CASE("Grainfield has correct descriptor") {
    pulp::format::HeadlessHost host(grainfield::create_grainfield);
    auto desc = host.descriptor();

    REQUIRE(desc.name == "Grainfield");
    REQUIRE(desc.manufacturer == "Independent Audio");
    REQUIRE(desc.category == pulp::format::PluginCategory::Effect);
}

TEST_CASE("Grainfield state round-trip") {
    pulp::format::HeadlessHost host(grainfield::create_grainfield);
    host.prepare(48000, 512);

    // Change a parameter
    host.state().set_value(grainfield::kMix, 50.0f);

    // Save and restore
    auto data = host.save_state();
    host.state().set_value(grainfield::kMix, 0.0f);
    REQUIRE(host.load_state(data));
    REQUIRE(host.state().get_value(grainfield::kMix) == Approx(50.0f));
}
