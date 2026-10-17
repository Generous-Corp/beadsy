#pragma once

#include <pulp/format/processor.hpp>
#include <pulp/signal/oscillator.hpp>
#include <pulp/signal/adsr.hpp>
#include <cmath>

// Grainfield — A Pulp audio instrument

namespace grainfield {

enum ParamIDs : pulp::state::ParamID {
    kGain = 1,
};

inline pulp::format::PluginDescriptor make_descriptor() {
    return {
        .name = "Grainfield",
        .manufacturer = "Independent Audio",
        .bundle_id = "com.independent_audio.grainfield",
        .version = "1.0.0",
        .category = pulp::format::PluginCategory::Instrument,
        .input_buses = {},
        .output_buses = {{"Main Out", 2, false}},
        .accepts_midi = true,
    };
}

class Grainfield : public pulp::format::Processor {
public:
    pulp::format::PluginDescriptor descriptor() const override {
        return make_descriptor();
    }

    void define_parameters(pulp::state::StateStore& store) override {
        store.add_parameter({
            .id = kGain,
            .name = "Gain",
            .unit = "dB",
            .range = {-60.0f, 12.0f, 0.0f},
        });
    }

    void prepare(const pulp::format::PrepareContext& ctx) override {
        osc_.set_sample_rate(static_cast<float>(ctx.sample_rate));
        env_.set_sample_rate(static_cast<float>(ctx.sample_rate));
        env_.set_params({0.01f, 0.1f, 0.7f, 0.3f});
    }

    void process(
        pulp::audio::BufferView<float>& output,
        const pulp::audio::BufferView<const float>& /*input*/,
        pulp::midi::MidiBuffer& midi_in,
        pulp::midi::MidiBuffer& /*midi_out*/,
        const pulp::format::ProcessContext& /*ctx*/) override
    {
        float gain_db = state().get_value(kGain);
        float gain = std::pow(10.0f, gain_db / 20.0f);

        for (const auto& event : midi_in) {
            if (event.is_note_on()) {
                float freq = 440.0f * std::pow(2.0f, (event.note() - 69) / 12.0f);
                osc_.set_frequency(freq);
                env_.note_on();
            } else if (event.is_note_off()) {
                env_.note_off();
            }
        }

        output.clear();
        for (std::size_t i = 0; i < output.num_samples(); ++i) {
            float sample = osc_.next() * env_.next() * gain;
            for (std::size_t ch = 0; ch < output.num_channels(); ++ch)
                output.channel(ch)[i] = sample;
        }
    }

private:
    pulp::signal::Oscillator osc_;
    pulp::signal::Adsr env_;
};

inline std::unique_ptr<pulp::format::Processor> create_grainfield() {
    return std::make_unique<Grainfield>();
}

} // namespace grainfield
