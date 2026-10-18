#pragma once

#include <pulp/format/processor.hpp>
#include <pulp/midi/buffer.hpp>
#include "beads/dsp/synth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
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

/// Product-local LFO modulation.  The base StateStore value remains the host
/// automation value; the offset is applied through StateStore's modulation API.
struct LfoModulation {
    bool enabled = false;
    float rate_hz = 1.0f;
    float depth = 0.0f;
    float phase = 0.0f;
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

#if defined(GRAINFIELD_NATIVE_EDITOR) && !defined(PULP_HEADLESS)
    pulp::format::ViewSize view_size() const override { return {201, 364, 201, 364, 401, 729}; }
    std::unique_ptr<pulp::view::View> create_view() override;
#endif

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

    void set_lfo(pulp::state::ParamID id, LfoModulation lfo) {
        if (id < lfo_.size()) lfo_[id] = lfo;
    }
    LfoModulation lfo(pulp::state::ParamID id) const {
        return id < lfo_.size() ? lfo_[id] : LfoModulation{};
    }
    void clear_lfo(pulp::state::ParamID id) {
        if (id < lfo_.size()) lfo_[id] = {};
        state().set_mod_offset(id, 0.0f);
    }

    std::vector<std::uint8_t> serialize_plugin_state() const override {
        std::vector<std::uint8_t> out{'G', 'F', 'M', 'D', 1, 0};
        for (std::size_t id = 0; id < lfo_.size(); ++id) {
            const auto& l = lfo_[id];
            out.push_back(static_cast<std::uint8_t>(l.enabled));
            const auto append_float = [&out](float value) {
                const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
                out.insert(out.end(), bytes, bytes + sizeof(float));
            };
            append_float(l.rate_hz);
            append_float(l.depth);
            append_float(l.phase);
        }
        return out;
    }

    bool deserialize_plugin_state(std::span<const std::uint8_t> data) override {
        if (data.empty()) { lfo_.fill({}); return true; }
        constexpr std::size_t stride = 1 + sizeof(float) * 3;
        if (data.size() != 6 + stride * lfo_.size() || data[0] != 'G' ||
            data[1] != 'F' || data[2] != 'M' || data[3] != 'D' || data[4] != 1)
            return false;
        std::size_t offset = 6;
        for (auto& l : lfo_) {
            l.enabled = data[offset++] != 0;
            const auto read_float = [&data, &offset]() {
                float value = 0.0f;
                std::memcpy(&value, data.data() + offset, sizeof(float));
                offset += sizeof(float);
                return value;
            };
            l.rate_hz = read_float(); l.depth = read_float(); l.phase = read_float();
        }
        return true;
    }

    void prepare(const pulp::format::PrepareContext& ctx) override {
        sample_rate_ = ctx.sample_rate;
        max_block_size_ = static_cast<std::size_t>(ctx.max_buffer_size);
        supported_rate_ = std::abs(sample_rate_ - 48000.0) < 0.5;
        if (!supported_rate_) return;
        synth_.Init(reverb_buffer_.data());
        frames_.resize(max_block_size_);
        triggers_.resize(max_block_size_);
        merged_gate_flags_.resize(max_block_size_);
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

        update_modulation(output.num_samples());

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

        beads::ModulatedParameter density{state().get_modulated(kDensity), 0.0f};
        beads::Processor::MixParameters mix{
            state().get_modulated(kFeedback),
            state().get_modulated(kDryWet),
            state().get_modulated(kReverb),
            false,
        };

        const std::size_t n = output.num_samples();
        for (std::size_t i = 0; i < n; ++i) frames_[i] = 0.0f;
        const auto seed = trigger ? beads::SEED_BUTTON_STATE_ON : beads::SEED_BUTTON_STATE_OFF;
        synth_.Render(seed, nullptr, freeze_.get(), beads::Synth::RESOLUTION_16_BIT,
                      density, parameters, mix, triggers_, merged_gate_flags_,
                      frames_.data(), n);

        const float gain = std::pow(10.0f, state().get_modulated(kGain) / 20.0f);
        for (std::size_t i = 0; i < n; ++i) {
            output.channel(0)[i] = frames_[i].l * gain;
            output.channel(1)[i] = frames_[i].r * gain;
        }
    }

private:
    void update_modulation(std::size_t samples) {
        if (!sample_rate_) return;
        for (std::size_t id = 0; id < lfo_.size(); ++id) {
            auto& l = lfo_[id];
            if (!l.enabled || l.depth == 0.0f) {
                state().set_mod_offset(static_cast<pulp::state::ParamID>(id), 0.0f);
                continue;
            }
            state().set_mod_offset(static_cast<pulp::state::ParamID>(id),
                                   std::sin(l.phase * 6.28318530718f) * l.depth);
            l.phase = std::fmod(l.phase + l.rate_hz * static_cast<float>(samples) /
                                             static_cast<float>(sample_rate_), 1.0f);
            if (l.phase < 0.0f) l.phase += 1.0f;
        }
    }

    beads::RandomizedParameter randomized(pulp::state::ParamID id) const {
        return {state().get_modulated(id), 0.0f, 0.0f, false};
    }

    beads::Synth synth_;
    std::array<std::uint16_t, 32768> reverb_buffer_{};
    std::vector<beads::FloatFrame> frames_;
    std::vector<beads::TriggerGenerator::Output> triggers_;
    std::vector<stmlib::GateFlags> merged_gate_flags_;
    std::unique_ptr<bool[]> freeze_;
    double sample_rate_ = 48000.0;
    std::size_t max_block_size_ = 0;
    bool supported_rate_ = false;
    bool gate_ = false;
    std::uint8_t current_note_ = 0;
    std::array<LfoModulation, 10> lfo_{};
};

inline std::unique_ptr<pulp::format::Processor> create_grainfield() {
    return std::make_unique<Grainfield>();
}

} // namespace grainfield

#if defined(GRAINFIELD_NATIVE_EDITOR) && !defined(PULP_HEADLESS)
#include "grainfield_editor.hpp"
inline std::unique_ptr<pulp::view::View> grainfield::Grainfield::create_view() {
    return std::make_unique<grainfield::GrainfieldEditor>(state());
}
#endif
