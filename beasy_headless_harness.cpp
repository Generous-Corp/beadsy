#include "beasy.hpp"

#include <cmath>
#include <iostream>
#include <limits>

#include <pulp/audio/buffer.hpp>
#include <pulp/format/headless.hpp>
#include <pulp/midi/buffer.hpp>

int main() {
    constexpr int kFrames = 512;
    pulp::format::HeadlessHost host(beasy::create_beasy);
    host.prepare(48000.0, kFrames, 0, 2);

    host.state().set_value(beasy::kGain, -6.0f);
    host.state().set_value(beasy::kFreeze, 1.0f);
    const auto state = host.save_state();

    pulp::audio::Buffer<float> output(2, kFrames);
    pulp::audio::BufferView<const float> empty_input(nullptr, 0, 0);
    auto output_view = output.view();
    pulp::midi::MidiBuffer midi_in;
    pulp::midi::MidiBuffer midi_out;
    midi_in.add(pulp::midi::MidiEvent::note_on(0, 60, 100));
    pulp::format::ProcessContext context{.sample_rate = 48000.0,
                                         .num_samples = kFrames};
    host.process(output_view, empty_input, midi_in, midi_out, context);

    double sum_squares = 0.0;
    float peak = 0.0f;
    bool finite = true;
    bool audible = false;
    for (int channel = 0; channel < 2; ++channel) {
        for (float sample : output.channel(channel)) {
            finite &= std::isfinite(sample);
            peak = std::max(peak, std::abs(sample));
            sum_squares += static_cast<double>(sample) * sample;
            audible |= std::abs(sample) > 0.0001f;
        }
    }

    host.state().set_value(beasy::kGain, 0.0f);
    host.state().set_value(beasy::kFreeze, 0.0f);
    const bool restored = host.load_state(state) &&
                          host.state().get_value(beasy::kGain) == -6.0f &&
                          host.state().get_value(beasy::kFreeze) == 1.0f;
    const double rms = std::sqrt(sum_squares / (2.0 * kFrames));

    std::cout << "{\"sample_rate\":48000,\"frames\":" << kFrames
              << ",\"channels\":2,\"finite\":" << (finite ? "true" : "false")
              << ",\"audible\":" << (audible ? "true" : "false")
              << ",\"peak\":" << peak << ",\"rms\":" << rms
              << ",\"state_bytes\":" << state.size()
              << ",\"state_restored\":" << (restored ? "true" : "false")
              << "}\n";
    return finite && audible && restored ? 0 : 1;
}
