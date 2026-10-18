#pragma once

#include <pulp/format/processor.hpp>
#include <pulp/midi/buffer.hpp>
#include "beads/dsp/synth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

// Grainfield — an independent Pulp instrument inspired by the published Beads
// architecture. Upstream notices for the adapted DSP live under third_party/.
namespace grainfield {

enum ParamIDs : pulp::state::ParamID {
    kTime = 1,
    kSize,
    kShape,
    kPitch,
    kDensity,
    kFeedback,
    kDryWet,
    kReverb,
    kGain,
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
    pulp::format::PluginDescriptor descriptor() const override { return make_descriptor(); }

    void define_parameters(pulp::state::StateStore& store) override {
        store.add_parameter({.id = kTime, .name = "Time", .unit = "%", .range = {0.0f, 1.0f, 0.5f}});
        store.add_parameter({.id = kSize, .name = "Size", .unit = "%", .range = {0.0f, 1.0f, 0.5f}});
        store.add_parameter({.id = kShape, .name = "Shape", .unit = "%", .range = {0.0f, 1.0f, 0.5f}});
        store.add_parameter({.id = kPitch, .name = "Pitch", .unit = "semitones", .range = {0.0f, 1.0f, 0.5f}});
        store.add_parameter({.id = kDensity, .name = "Density", .unit = "%", .range = {0.0f, 1.0f, 0.5f}});
        store.add_parameter({.id = kFeedback, .name = "Feedback", .unit = "%", .range = {0.0f, 1.0f, 0.0f}});
        store.add_parameter({.id = kDryWet, .name = "Dry/Wet", .unit = "%", .range = {0.0f, 1.0f, 0.5f}});
        store.add_parameter({.id = kReverb, .name = "Reverb", .unit = "%", .range = {0.0f, 1.0f, 0.0f}});
        store.add_parameter({.id = kGain, .name = "Gain", .unit = "dB", .range = {-60.0f, 12.0f, 0.0f}});
    }

    void prepare(const pulp::format::PrepareContext& ctx) override {
        sample_rate_ = ctx.sample_rate;
        max_block_size_ = static_cast<std::size_t>(ctx.max_buffer_size);
        supported_rate_ = std::abs(sample_rate_ - 48000.0) < 0.5;
        if (!supported_rate_) return;
        synth_.Init(reverb_buffer_.data());
        frames_.resize(max_block_size_);
        freeze_.reset(new bool[max_block_size_]);
        std::fill_n(freeze_.get(), max_block_size_, false);
    }

    void process(pulp::audio::BufferView<float>& output,
                 const pulp::audio::BufferView<const float>&,
                 pulp::midi::MidiBuffer& midi_in,
                 pulp::midi::MidiBuffer&,
                 const pulp::format::ProcessContext&) override {
        output.clear();
        if (!supported_rate_ || output.num_channels() < 2 || output.num_samples() > frames_.size()) return;

        bool trigger = false;
        for (const auto& event : midi_in) {
            if (event.is_note_on() && event.velocity() > 0) {
                current_note_ = event.note();
                trigger = true;
            } else if (event.is_note_off() && event.note() == current_note_) {
                gate_ = false;
            }
        }
        if (trigger) gate_ = true;

        beads::Parameters parameters{};
        parameters.time = randomized(kTime);
        parameters.size = randomized(kSize);
        parameters.shape = randomized(kShape);
        parameters.pitch = randomized(kPitch);
        parameters.monophonic_mode = false;

        beads::ModulatedParameter density{state().get_value(kDensity), 0.0f};
        beads::Processor::MixParameters mix{
            state().get_value(kFeedback),
            state().get_value(kDryWet),
            state().get_value(kReverb),
            false,
        };

        const std::size_t n = output.num_samples();
        for (std::size_t i = 0; i < n; ++i) frames_[i] = 0.0f;
        const auto seed = trigger ? beads::SEED_BUTTON_STATE_ON : beads::SEED_BUTTON_STATE_OFF;
        synth_.Render(seed, nullptr, freeze_.data(), beads::Synth::RESOLUTION_16_BIT,
                      density, parameters, mix, frames_.data(), n);

        const float gain = std::pow(10.0f, state().get_value(kGain) / 20.0f);
        for (std::size_t i = 0; i < n; ++i) {
            output.channel(0)[i] = frames_[i].l * gain;
            output.channel(1)[i] = frames_[i].r * gain;
        }
    }

private:
    beads::RandomizedParameter randomized(pulp::state::ParamID id) const {
        return {state().get_value(id), 0.0f, 0.0f, false};
    }

    beads::Synth synth_;
    std::array<std::uint16_t, 32768> reverb_buffer_{};
    std::vector<beads::FloatFrame> frames_;
    std::unique_ptr<bool[]> freeze_;
    double sample_rate_ = 48000.0;
    std::size_t max_block_size_ = 0;
    bool supported_rate_ = false;
    bool gate_ = false;
    std::uint8_t current_note_ = 0;
};

inline std::unique_ptr<pulp::format::Processor> create_grainfield() {
    return std::make_unique<Grainfield>();
}

} // namespace grainfield
