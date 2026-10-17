#include "grainfield.hpp"
#include <pulp/format/standalone.hpp>

int main() {
    pulp::format::StandaloneApp app(grainfield::create_grainfield);
    pulp::format::StandaloneConfig config;
    config.input_channels = 0;
    config.output_channels = 2;
    app.set_config(config);
    return app.run_with_editor(false) ? 0 : 1;
}
