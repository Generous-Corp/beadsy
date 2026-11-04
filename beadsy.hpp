#pragma once

#include "beads/dsp/synth.h"
#include <pulp/format/processor.hpp>
#include <pulp/midi/buffer.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <vector>

// Beadsy — an independent Pulp instrument inspired by the published Beads
// architecture. Upstream notices for the adapted DSP live under third_party/.
namespace beadsy {

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
  kQuality,
  kFreeze,
  kMode,
  kInputSource,
  kTimeAmount,
  kSizeAmount,
  kShapeAmount,
  kPitchAmount,
  // Seed is a discrete button-mode contract rather than an attenuverter.
  // Keep it after the existing IDs so current automation and state payloads
  // retain their meanings when this source-backed mode is introduced.
  kSeedMode,
};

/// Stable product-level source identity. The legacy kInputSource parameter is
/// retained for state/automation compatibility; new UI code should write this
/// one discrete selector and leave the descriptor bus topology unchanged.
enum class SourceMode : int {
  MidiSynth = 0,
  MainInput = 1,
  Sidechain = 2,
};

#if defined(BEADSY_AU_NO_INPUT) || defined(BEADSY_LV2)
inline constexpr int kAvailableSourceModeMax = 0;
#elif defined(BEADSY_AU_MAIN_INPUT_ONLY)
inline constexpr int kAvailableSourceModeMax = 1;
#else
inline constexpr int kAvailableSourceModeMax = 2;
#endif

template <typename Bus> void mark_sidechain_role_if_supported(Bus &bus) {
  // Keep Beadsy source-compatible with older installed Pulp SDKs. Newer
  // SDKs expose BusInfo::role; the numeric value is intentionally the
  // appended Sidechain enum member (Main=0, Aux=1, Sidechain=2).
  if constexpr (requires(Bus &candidate) { candidate.role; })
    bus.role = static_cast<decltype(bus.role)>(2);
}

// This mapping mirrors the published Beads TriggerGenerator contract.  The
// hardware UI exposes OFF, ON (the seed button held), and LOCKED (the seed
// input is used as a clock/divider source).  Hosts can automate arbitrary
// floating-point values, so round and clamp at the product boundary before
// passing the value to the adapted DSP.  No additional hardware combinations
// are inferred here.
inline beads::SeedButtonState seed_button_state_from_value(float value) {
  if (!std::isfinite(value))
    return beads::SEED_BUTTON_STATE_OFF;
  // Clamp before converting to an integer so arbitrarily large finite host
  // values cannot overflow the intermediate conversion.
  if (value <= 0.0f)
    return beads::SEED_BUTTON_STATE_OFF;
  if (value >= 2.0f)
    return beads::SEED_BUTTON_STATE_LOCKED;
  const int mode = std::clamp(static_cast<int>(std::lround(value)), 0, 2);
  switch (mode) {
  case 1:
    return beads::SEED_BUTTON_STATE_ON;
  case 2:
    return beads::SEED_BUTTON_STATE_LOCKED;
  default:
    return beads::SEED_BUTTON_STATE_OFF;
  }
}

// Parameter IDs are intentionally 1-based.  Keep the LFO table sized by the
// declared parameter range rather than by an unrelated historical count so a
// newly added control cannot silently become non-modulatable.
inline constexpr pulp::state::ParamID kFirstModulatableParam = kTime;
inline constexpr pulp::state::ParamID kLastModulatableParam = kSeedMode;
inline constexpr std::size_t kModulatableParamCount = static_cast<std::size_t>(
    kLastModulatableParam - kFirstModulatableParam + 1);

/// Product-local LFO modulation.  The base StateStore value remains the host
/// automation value; the offset is applied through StateStore's modulation API.
struct LfoModulation {
  enum class Shape : std::uint8_t { Sine = 0, Triangle = 1, Square = 2, Saw = 3 };
  bool enabled = false;
  float rate_hz = 1.0f;
  // In target-facing lfo()/set_lfo(), this is that destination's normalized
  // depth. Generator snapshots retain it for older callers/state migration;
  // shared generator depth does not scale routes in the processing path.
  float depth = 0.0f;
  float phase = 0.0f;
  Shape shape = Shape::Sine;
};

inline pulp::format::PluginDescriptor make_descriptor() {
  auto descriptor = pulp::format::PluginDescriptor{
      .name =
#if defined(BEADSY_EFFECT_VARIANT)
          "Beadsy FX",
#else
          "Beadsy",
#endif
      .manufacturer = "Independent Audio",
      .bundle_id =
#if defined(BEADSY_EFFECT_VARIANT)
          "com.independent_audio.beadsy-fx",
#else
          "com.independent_audio.beadsy",
#endif
      .version = "1.0.8",
      .category =
#if defined(BEADSY_EFFECT_VARIANT)
          pulp::format::PluginCategory::Effect,
#else
          pulp::format::PluginCategory::Instrument,
#endif
      .input_buses =
#if defined(BEADSY_AU_NO_INPUT) || defined(BEADSY_LV2)
          {},
#elif defined(BEADSY_AU_MAIN_INPUT_ONLY)
          {{"Main In", 2, false}},
#elif defined(BEADSY_EFFECT_VARIANT)
          // The FX companion must have an active main input by default.  A
          // host may leave the auxiliary sidechain disconnected, but marking
          // Main In optional makes VST3/AU advertise zero default inputs and
          // prevents ordinary track/bus routing from reaching the processor.
          {{"Main In", 2, false}, {"Sidechain", 2, true}},
#else
              {{"Main In", 2, true}, {"Sidechain", 2, true}},
#endif
      .output_buses = {{"Main Out", 2, false}},
      .accepts_midi = true,
  };
  if (descriptor.input_buses.size() > 1)
    mark_sidechain_role_if_supported(descriptor.input_buses[1]);
  return descriptor;
}

class Beadsy : public pulp::format::Processor {
public:
  pulp::format::PluginDescriptor descriptor() const override {
    return make_descriptor();
  }

  pulp::format::PrepareResourceUsage estimate_prepare_resources(
      const pulp::format::PrepareContext &ctx) const override {
    const auto block =
        static_cast<std::size_t>(std::max(ctx.max_buffer_size, 0));
    const auto frame_storage = block * sizeof(beads::FloatFrame) * 2;
    const auto synth_storage =
        block * (sizeof(beads::TriggerGenerator::Output) +
                 sizeof(stmlib::GateFlags) * 2 + sizeof(bool));
    const auto effect_storage =
        block * (sizeof(beads::TriggerGenerator::Output) +
                 sizeof(beads::DelayController::Output) + sizeof(bool) +
                 sizeof(beads::FloatFrame) + sizeof(stmlib::GateFlags));
    const auto velocity_storage = block * sizeof(float) * 2;
    return {
        .persistent_bytes = kDelayStorageBytes + sizeof(reverb_buffer_) +
                            sizeof(effect_reverb_buffer_) + frame_storage +
                            synth_storage + effect_storage + velocity_storage,
        .block_scratch_bytes = 0,
        .block_size = ctx.max_buffer_size,
        .input_channels = 2,
        .output_channels = 2,
    };
  }

#if defined(BEADSY_NATIVE_EDITOR) && !defined(PULP_HEADLESS)
  pulp::format::ViewSize view_size() const override {
    // The modulation popup needs enough vertical room for its readable
    // Spectr-style rows before it has to scroll. Keep the authored aspect
    // ratio while raising the host-provided minimum size.
    // The authored panel is 240:434.  Use the wider review size as the
    // default while retaining that exact aspect ratio; the minimum and
    // maximum bounds stay proportional so AUv2 hosts cannot introduce edge
    // padding or a sheared panel during resize.
    constexpr uint32_t kDefaultWidth = 350;
    constexpr uint32_t kDefaultHeight = 633; // round(350 * 434 / 240)
    return {kDefaultWidth, kDefaultHeight, kDefaultWidth, kDefaultHeight,
            kDefaultWidth * 2, kDefaultHeight * 2};
  }
  std::unique_ptr<pulp::view::View> create_view() override;
#endif

  void define_parameters(pulp::state::StateStore &store) override {
    modulation_store_ = &store;
    store.add_parameter({.id = kTime,
                         .name = "Time",
                         .unit = "%",
                         .range = {0.0f, 1.0f, 0.5f}});
    store.add_parameter({.id = kSize,
                         .name = "Size",
                         .unit = "%",
                         .range = {0.0f, 1.0f, 0.5f}});
    store.add_parameter({.id = kShape,
                         .name = "Shape",
                         .unit = "%",
                         .range = {0.0f, 1.0f, 0.5f}});
    store.add_parameter({.id = kPitch,
                         .name = "Pitch",
                         .unit = "semitones",
                         .range = {-24.0f, 24.0f, 0.0f}});
    store.add_parameter({.id = kDensity,
                         .name = "Density",
                         .unit = "%",
                         .range = {0.0f, 1.0f, 0.5f}});
    store.add_parameter({.id = kFeedback,
                         .name = "Feedback",
                         .unit = "%",
                         .range = {0.0f, 1.0f, 0.0f}});
    store.add_parameter({.id = kDryWet,
                         .name = "Dry/Wet",
                         .unit = "%",
                         .range = {0.0f, 1.0f, 0.5f}});
    store.add_parameter({.id = kReverb,
                         .name = "Reverb",
                         .unit = "%",
                         .range = {0.0f, 1.0f, 0.0f}});
    store.add_parameter({.id = kGain,
                         .name = "Gain",
                         .unit = "dB",
                         .range = {-60.0f, 12.0f, 0.0f}});
    // The four values map directly to Beads' synth Resolution enum:
    // 4-bit, 16/6-bit, 7-bit, and full 16-bit output paths.
    store.add_parameter(
        {.id = kQuality,
         .name = "Quality",
         .unit = "mode",
         .range = pulp::state::ParamRange::linear(0.0f, 3.0f, 3.0f, 1.0f)});
    store.add_parameter(
        {.id = kFreeze,
         .name = "Freeze",
         .unit = "toggle",
         .range = pulp::state::ParamRange::linear(0.0f, 1.0f, 0.0f, 1.0f)});
    store.add_parameter({.id = kMode,
                         .name = "Source",
                         .unit = "mode",
                         .range = pulp::state::ParamRange::linear(
                             0.0f, static_cast<float>(kAvailableSourceModeMax),
#if defined(BEADSY_EFFECT_VARIANT)
                             1.0f,
#else
                             0.0f,
#endif
                             1.0f)});
    store.add_parameter(
        {.id = kInputSource,
         .name = "Audio Source",
         .unit = "mode",
         .range = pulp::state::ParamRange::linear(0.0f, 1.0f, 0.0f, 1.0f)});
    store.add_parameter(
        {.id = kSeedMode,
         .name = "Seed Mode",
         .unit = "mode",
         .range = pulp::state::ParamRange::linear(0.0f, 2.0f, 0.0f, 1.0f)});
    // Source-backed Beads attenurandomizer amounts. These are separate
    // host parameters so automation remains explicit and stable. The
    // hardware LAW_ATTENUVERTER is bipolar: its physical midpoint is
    // zero, the positive half selects direct internal randomization, and
    // the negative half selects the peaked/random-walk behavior. Keep
    // that centered law in the host contract instead of exposing only
    // the positive half as a 0..1 amount.
    store.add_parameter({.id = kTimeAmount,
                         .name = "Time CV Amount",
                         .unit = "%",
                         .range = {-1.0f, 1.0f, 0.0f}});
    store.add_parameter({.id = kSizeAmount,
                         .name = "Size CV Amount",
                         .unit = "%",
                         .range = {-1.0f, 1.0f, 0.0f}});
    store.add_parameter({.id = kShapeAmount,
                         .name = "Shape CV Amount",
                         .unit = "%",
                         .range = {-1.0f, 1.0f, 0.0f}});
    store.add_parameter({.id = kPitchAmount,
                         .name = "Pitch CV Amount",
                         .unit = "%",
                         .range = {-1.0f, 1.0f, 0.0f}});
  }

  void set_lfo(pulp::state::ParamID id, LfoModulation lfo,
               std::size_t lane = 0) {
    if (!is_modulatable(id) || lane >= lfo_generators_.size())
      return;
    // Retain this convenience for callers that enable a target and configure
    // its generator together. Removing a target never stops other routes.
    if (lfo.enabled) {
      set_lfo_generator(lane, lfo);
      set_lfo_target_depth(id, lane, lfo.depth);
    }
    set_lfo_target_enabled(id, lane, lfo.enabled);
  }
  LfoModulation lfo(pulp::state::ParamID id, std::size_t lane = 0) const {
    if (!is_modulatable(id) || lane >= lfo_generators_.size())
      return {};
    auto result = lfo_generator(lane);
    result.enabled = result.enabled && lfo_target_enabled(id, lane);
    result.depth = lfo_target_depth(id, lane);
    return result;
  }
  LfoModulation lfo_generator(std::size_t lane) const {
    return lane < lfo_generators_.size() ? lfo_generators_[lane].load()
                                        : LfoModulation{};
  }
  void set_lfo_generator(std::size_t lane, LfoModulation lfo) {
    if (lane >= lfo_generators_.size())
      return;
    lfo = sanitize_lfo(lfo);
    lfo_generators_[lane].store(lfo);
    if (!lfo.enabled)
      for (pulp::state::ParamID id = kFirstModulatableParam;
           id <= kLastModulatableParam; ++id)
        clear_inactive_modulation(id);
  }
  bool lfo_target_enabled(pulp::state::ParamID id, std::size_t lane) const {
    return is_modulatable(id) && lane < lfo_targets_.size() &&
           lfo_targets_[lane][id].enabled.load(std::memory_order_relaxed);
  }
  float lfo_target_depth(pulp::state::ParamID id, std::size_t lane) const {
    return is_modulatable(id) && lane < lfo_targets_.size()
               ? lfo_targets_[lane][id].depth.load(std::memory_order_relaxed)
               : 0.0f;
  }
  void set_lfo_target_depth(pulp::state::ParamID id, std::size_t lane,
                            float depth) {
    if (!is_modulatable(id) || lane >= lfo_targets_.size())
      return;
    depth = std::isfinite(depth) ? std::clamp(depth, 0.0f, 1.0f) : 0.0f;
    lfo_targets_[lane][id].depth.store(depth, std::memory_order_relaxed);
    if (depth == 0.0f)
      clear_inactive_modulation(id);
  }
  void set_lfo_target_enabled(pulp::state::ParamID id, std::size_t lane,
                              bool enabled) {
    if (!is_modulatable(id) || lane >= lfo_targets_.size())
      return;
    lfo_targets_[lane][id].enabled.store(enabled, std::memory_order_relaxed);
    if (!enabled)
      clear_inactive_modulation(id);
  }
  void clear_lfo(pulp::state::ParamID id, std::size_t lane = 0) {
    set_lfo_target_enabled(id, lane, false);
  }

  /// Host-owned route identity captured by the editor for state restore.
  /// The host still owns the graph connection; these stable IDs are only a
  /// state hint that can be rebound when the catalog is injected again.
  void set_route_identity(std::size_t bus_index, std::string_view stable_id) {
    if (bus_index < route_ids_.size())
      route_ids_[bus_index] = std::string(stable_id);
  }
  std::string_view route_identity(std::size_t bus_index) const {
    return bus_index < route_ids_.size()
               ? std::string_view(route_ids_[bus_index])
               : std::string_view{};
  }

#if defined(BEADSY_TESTING)
  // Test-only seam: expose the four source-backed Beads attenurandomizer
  // values without widening the shipping processor API.  This keeps the
  // control-to-DSP contract directly verifiable while the runtime remains
  // allocation-free and private.
  std::array<beads::RandomizedParameter, 4>
  debug_randomized_parameters() const {
    return {randomized(kTime), randomized(kSize), randomized(kShape),
            randomized(kPitch)};
  }
#endif

  std::vector<std::uint8_t> serialize_plugin_state() const override {
    // Version 4 stores two shared generators independently from target routes.
    // This keeps enabled/settings state even when no parameter is assigned.
    std::vector<std::uint8_t> out{'G', 'F', 'M', 'D', 4, 0};
    const auto append_float = [&out](float value) {
      const auto *bytes = reinterpret_cast<const std::uint8_t *>(&value);
      out.insert(out.end(), bytes, bytes + sizeof(float));
    };
    for (std::size_t lane = 0; lane < kLfoLaneCount; ++lane) {
      const auto l = lfo_generator(lane);
      out.push_back(static_cast<std::uint8_t>(l.enabled));
      out.push_back(static_cast<std::uint8_t>(l.shape));
      append_float(l.rate_hz);
      append_float(l.depth);
      append_float(l.phase);
      for (pulp::state::ParamID id = kFirstModulatableParam;
           id <= kLastModulatableParam; ++id) {
        out.push_back(static_cast<std::uint8_t>(lfo_target_enabled(id, lane)));
        append_float(lfo_target_depth(id, lane));
      }
    }
    for (const auto &route_id : route_ids_) {
      const auto length = std::min<std::size_t>(route_id.size(), 1024);
      out.push_back(static_cast<std::uint8_t>(length & 0xffu));
      out.push_back(static_cast<std::uint8_t>((length >> 8u) & 0xffu));
      out.insert(out.end(), route_id.begin(), route_id.begin() + length);
    }
    return out;
  }

  bool deserialize_plugin_state(std::span<const std::uint8_t> data) override {
    std::array<LfoModulation, kLfoLaneCount> generators{};
    struct StoredRoute {
      bool enabled = false;
      float depth = 0.5f;
    };
    std::array<std::array<StoredRoute, kLastModulatableParam + 1>, kLfoLaneCount>
        targets{};
    std::array<std::string, 2> route_ids{};
    if (!data.empty()) {
      if (data.size() < 6 || data[0] != 'G' || data[1] != 'F' ||
          data[2] != 'M' || data[3] != 'D')
        return false;
      constexpr std::size_t old_param_count = 17;
      constexpr std::size_t old_stride = 1 + sizeof(float) * 3;
      constexpr std::size_t shape_stride = 2 + sizeof(float) * 3;
      const auto version = data[4];
      const bool legacy = (version == 1 || version == 2) &&
                          data.size() == 6 + old_stride * 10;
      if (version < 1 || version > 4)
        return false;
      std::size_t offset = 6;
      const auto read_lfo = [&](LfoModulation &l, bool has_shape) {
        const auto bytes = has_shape ? shape_stride : old_stride;
        if (offset + bytes > data.size())
          return false;
        l.enabled = data[offset++] != 0;
        if (has_shape)
          l.shape = static_cast<LfoModulation::Shape>(data[offset++]);
        const auto read_float = [&]() {
          float value;
          std::memcpy(&value, data.data() + offset, sizeof(float));
          offset += sizeof(float);
          return value;
        };
        l.rate_hz = read_float();
        l.depth = read_float();
        l.phase = read_float();
        l = sanitize_lfo(l);
        return true;
      };
      if (version == 4) {
        for (std::size_t lane = 0; lane < kLfoLaneCount; ++lane) {
          if (!read_lfo(generators[lane], true) ||
              offset + kModulatableParamCount * (1 + sizeof(float)) > data.size())
            return false;
          for (pulp::state::ParamID id = kFirstModulatableParam;
               id <= kLastModulatableParam; ++id) {
            auto &route = targets[lane][id];
            route.enabled = data[offset++] != 0;
            std::memcpy(&route.depth, data.data() + offset, sizeof(float));
            offset += sizeof(float);
            route.depth = std::isfinite(route.depth)
                              ? std::clamp(route.depth, 0.0f, 1.0f) : 0.0f;
          }
        }
      } else {
        // Older presets had a generator per target. Keep every enabled route;
        // the lowest enabled parameter ID supplies the shared generator for
        // each lane. A disabled lane retains its first stored configuration.
        // This deterministic conversion intentionally unifies differing old
        // target rates/shapes into the shared LFO model used by the editor.
        const auto lanes = version == 3 ? kLfoLaneCount : std::size_t{1};
        const auto slots = legacy ? std::size_t{10} : old_param_count;
        for (std::size_t lane = 0; lane < lanes; ++lane) {
          bool selected = false;
          for (std::size_t slot = 0; slot < slots; ++slot) {
            LfoModulation l;
            if (!read_lfo(l, version == 3))
              return false;
            const auto id = static_cast<pulp::state::ParamID>(
                legacy ? slot : slot + kFirstModulatableParam);
            if (!is_modulatable(id))
              continue;
            // Old depth was a plain-unit offset; route depth is normalized.
            // Preserve each destination's excursion while sharing timing.
            const float span = modulation_target_span(id);
            targets[lane][id] = {
                l.enabled, span > 0.0f ? std::clamp(l.depth / span, 0.0f, 1.0f)
                                      : 0.0f};
            if (id == kFirstModulatableParam)
              generators[lane] = l;
            if (l.enabled && !selected) {
              generators[lane] = l;
              selected = true;
            }
          }
        }
      }
      if (version >= 2 && !legacy) {
        for (auto &route_id : route_ids) {
          if (offset + 2 > data.size())
            return false;
          const auto length = static_cast<std::size_t>(data[offset]) |
                              (static_cast<std::size_t>(data[offset + 1]) << 8u);
          offset += 2;
          if (length > 1024 || offset + length > data.size())
            return false;
          route_id.assign(reinterpret_cast<const char *>(data.data() + offset),
                          length);
          offset += length;
        }
      }
      if (offset != data.size())
        return false;
    }
    // Do not partially alter a running processor on a malformed payload.
    for (std::size_t lane = 0; lane < kLfoLaneCount; ++lane) {
      set_lfo_generator(lane, generators[lane]);
      for (pulp::state::ParamID id = kFirstModulatableParam;
           id <= kLastModulatableParam; ++id) {
        lfo_targets_[lane][id].enabled.store(targets[lane][id].enabled,
                                             std::memory_order_relaxed);
        lfo_targets_[lane][id].depth.store(targets[lane][id].depth,
                                           std::memory_order_relaxed);
      }
    }
    route_ids_ = std::move(route_ids);
    for (pulp::state::ParamID id = kFirstModulatableParam;
         id <= kLastModulatableParam; ++id)
      clear_inactive_modulation(id);
    return true;
  }

  void prepare(const pulp::format::PrepareContext &ctx) override {
    sample_rate_ = ctx.sample_rate;
    max_block_size_ = static_cast<std::size_t>(ctx.max_buffer_size);
    supported_rate_ = select_effect_media(sample_rate_, effect_media_);
    active_notes_.fill(false);
    sustained_notes_.fill(false);
    note_velocity_gain_.fill(1.0f);
    sustain_down_ = false;
    gate_ = false;
    current_note_ = 60;
    velocity_gain_ = 1.0f;
    pitch_bend_semitones_ = 0.0f;
    has_note_activity_ = false;
    resources_ok_ = check_prepare_resource_limits(ctx) ==
                    pulp::format::PrepareResourceLimit::None;
    if (!supported_rate_ || !resources_ok_)
      return;
    synth_.Init(reverb_buffer_.data());
    delay_buffers_[0] = delay_storage_.data();
    delay_buffers_[1] = delay_storage_.data() + kDelayBankBytes;
    delay_buffers_[2] = delay_storage_.data() + 2 * kDelayBankBytes;
    effect_.Init(delay_buffers_.data(), effect_reverb_buffer_.data());
    effect_.Configure(effect_media_, false);
    effect_scratch_.Prepare(max_block_size_);
    frames_.resize(max_block_size_);
    input_frames_.resize(max_block_size_);
    triggers_.resize(max_block_size_);
    seed_gate_flags_.resize(max_block_size_);
    merged_gate_flags_.resize(max_block_size_);
    velocity_gain_events_.resize(max_block_size_);
    velocity_gain_stream_.resize(max_block_size_);
    freeze_.reset(new bool[max_block_size_]);
    std::fill_n(freeze_.get(), max_block_size_, false);
  }

  void process(pulp::format::ProcessBuffers &audio,
               pulp::midi::MidiBuffer &midi_in,
               pulp::midi::MidiBuffer &midi_out,
               const pulp::format::ProcessContext &context) override {
    auto *output = audio.main_output();
    if (!output)
      return;
    const auto *input = audio.main_input();
    const auto *sidechain = audio.sidechain_input();
    const int mode =
        std::clamp(static_cast<int>(std::lround(state().get_modulated(kMode))),
                   0, kAvailableSourceModeMax);
    // kInputSource remains a migration path for older states that used
    // kMode=1 plus a separate Main/Sidechain selector.
    const bool use_sidechain =
        mode == static_cast<int>(SourceMode::Sidechain) ||
        (mode == static_cast<int>(SourceMode::MainInput) &&
         state().get_modulated(kInputSource) >= 0.5f);
    const auto *source = mode == static_cast<int>(SourceMode::MidiSynth)
                             ? nullptr
                             : (use_sidechain ? sidechain : input);
    pulp::audio::BufferView<const float> empty_input;
    process(*output, source ? *source : empty_input, midi_in, midi_out,
            context);
  }

  void process(pulp::audio::BufferView<float> &output,
               const pulp::audio::BufferView<const float> &input,
               pulp::midi::MidiBuffer &midi_in, pulp::midi::MidiBuffer &,
               const pulp::format::ProcessContext &) override {
    output.clear();
    last_gate_size_ = 0;
    last_velocity_gain_size_ = 0;
    if (!supported_rate_ || !resources_ok_ || output.num_channels() < 2 ||
        output.num_samples() > frames_.size())
      return;

    update_modulation(output.num_samples());

    std::fill(seed_gate_flags_.begin(),
              seed_gate_flags_.begin() + output.num_samples(),
              stmlib::GATE_FLAG_LOW);
    const std::size_t n = output.num_samples();
    // Keep the gain that was active at the block boundary separate from
    // the final note state. MIDI velocity events can arrive in the middle
    // of a block, so applying the final gain to every output sample would
    // retroactively change audio before the event. This prepared timeline
    // is filled while walking the ordered event stream below, then read
    // without allocation in the output loop.
    const float velocity_gain_at_block_start = velocity_gain_;
    const float no_velocity_event = std::numeric_limits<float>::quiet_NaN();
    std::fill(velocity_gain_events_.begin(), velocity_gain_events_.begin() + n,
              no_velocity_event);
    const bool gate_at_block_start = gate_;
    bool gate = gate_at_block_start;
    for (const auto &event : midi_in) {
      const auto offset = std::clamp<std::size_t>(
          event.sample_offset < 0
              ? 0
              : static_cast<std::size_t>(event.sample_offset),
          0, output.num_samples() == 0 ? 0 : output.num_samples() - 1);
      if (event.is_note_on() && event.velocity() > 0) {
        has_note_activity_ = true;
        active_notes_[event.note()] = true;
        sustained_notes_[event.note()] = false;
        note_velocity_gain_[event.note()] =
            static_cast<float>(event.velocity()) / 127.0f;
        current_note_ = event.note();
        velocity_gain_ = note_velocity_gain_[event.note()];
        velocity_gain_events_[offset] = velocity_gain_;
        gate = true;
        seed_gate_flags_[offset] = static_cast<stmlib::GateFlags>(
            seed_gate_flags_[offset] | stmlib::GATE_FLAG_RISING);
      } else if (event.is_note_off()) {
        active_notes_[event.note()] = false;
        if (sustain_down_) {
          sustained_notes_[event.note()] = true;
        } else if (event.note() == current_note_) {
          bool replacement = false;
          for (int note = 127; note >= 0; --note) {
            if (active_notes_[static_cast<std::size_t>(note)]) {
              current_note_ = static_cast<std::uint8_t>(note);
              velocity_gain_ =
                  note_velocity_gain_[static_cast<std::size_t>(note)];
              velocity_gain_events_[offset] = velocity_gain_;
              replacement = true;
              break;
            }
          }
          if (!replacement) {
            gate = false;
            seed_gate_flags_[offset] = static_cast<stmlib::GateFlags>(
                seed_gate_flags_[offset] | stmlib::GATE_FLAG_FALLING);
          }
        }
      } else if (event.is_cc() && event.cc_number() == 64) {
        const bool down = event.cc_value() >= 64;
        if (sustain_down_ && !down) {
          sustained_notes_.fill(false);
          if (!active_notes_[current_note_]) {
            bool replacement = false;
            for (int note = 127; note >= 0; --note) {
              if (active_notes_[static_cast<std::size_t>(note)]) {
                current_note_ = static_cast<std::uint8_t>(note);
                velocity_gain_ =
                    note_velocity_gain_[static_cast<std::size_t>(note)];
                velocity_gain_events_[offset] = velocity_gain_;
                replacement = true;
                break;
              }
            }
            if (!replacement) {
              gate = false;
              seed_gate_flags_[offset] = static_cast<stmlib::GateFlags>(
                  seed_gate_flags_[offset] | stmlib::GATE_FLAG_FALLING);
            }
          }
        }
        sustain_down_ = down;
      } else if (event.is_pitch_bend()) {
        const float normalized =
            (static_cast<float>(event.message.getPitchWheelValue()) - 8192.0f) /
            8192.0f;
        pitch_bend_semitones_ = std::clamp(2.0f * normalized, -2.0f, 2.0f);
      }
    }
    gate_ = gate;

    // Expand the global MIDI gate into a sample-accurate external gate
    // stream. The original Beads trigger generator consumes rising/falling
    // gate flags, so this preserves event offsets without allocating in
    // the audio callback.
    gate = gate_at_block_start;
    for (std::size_t i = 0; i < output.num_samples(); ++i) {
      auto &flags = seed_gate_flags_[i];
      if (flags & stmlib::GATE_FLAG_FALLING)
        gate = false;
      if (flags & stmlib::GATE_FLAG_RISING)
        gate = true;
      if (gate)
        flags = static_cast<stmlib::GateFlags>(flags | stmlib::GATE_FLAG_HIGH);
    }
    last_gate_size_ = output.num_samples();

    float velocity_gain = velocity_gain_at_block_start;
    for (std::size_t i = 0; i < n; ++i) {
      const float event_gain = velocity_gain_events_[i];
      if (!std::isnan(event_gain))
        velocity_gain = event_gain;
      velocity_gain_stream_[i] = velocity_gain;
    }
    last_velocity_gain_size_ = n;

    beads::Parameters parameters{};
    parameters.time = randomized(kTime);
    parameters.size = randomized(kSize);
    parameters.shape = randomized(kShape);
    // Beads' pitch control is a ±24-semitone offset. MIDI note 60 is the
    // neutral reference for the software instrument, so note changes add
    // a musical transposition without overwriting host automation.
    parameters.pitch = randomized(kPitch, static_cast<float>(current_note_) -
                                              60.0f + pitch_bend_semitones_);
    parameters.monophonic_mode = false;

    beads::ModulatedParameter density{state().get_modulated(kDensity), 0.0f};
    beads::Processor::MixParameters mix{
        state().get_modulated(kFeedback),
        state().get_modulated(kDryWet),
        state().get_modulated(kReverb),
        false,
    };

    for (std::size_t i = 0; i < n; ++i)
      frames_[i] = 0.0f;
    const bool frozen = state().get_modulated(kFreeze) >= 0.5f;
    std::fill_n(freeze_.get(), n, frozen);
    const bool live_input = state().get_modulated(kMode) >= 0.5f;
    const auto seed =
        seed_button_state_from_value(state().get_modulated(kSeedMode));
    last_seed_button_state_ = seed;
    if (live_input) {
      if (input.num_channels() < 2 || input.num_samples() < n)
        return;
      for (std::size_t i = 0; i < n; ++i) {
        input_frames_[i].l = input.channel(0)[i];
        input_frames_[i].r = input.channel(1)[i];
      }
      effect_.Process(seed, seed_gate_flags_.data(), freeze_.get(), density,
                      parameters, mix, input_frames_.data(), frames_.data(), n,
                      effect_scratch_);
    } else {
      // A software instrument should be quiet when it is first opened. The
      // adapted Beads wavetable path remains available as soon as the host
      // sends a MIDI note, while avoiding an unsolicited tone during editor
      // and DAW instantiation.
      if (!has_note_activity_)
        return;
      const auto quality_index = std::clamp(
          static_cast<int>(std::lround(state().get_modulated(kQuality))), 0, 3);
      const auto quality = static_cast<beads::Synth::Resolution>(quality_index);
      synth_.Render(seed, seed_gate_flags_.data(), freeze_.get(), quality,
                    density, parameters, mix, triggers_, merged_gate_flags_,
                    frames_.data(), n);
    }

    const float gain = std::pow(10.0f, state().get_modulated(kGain) / 20.0f);
    for (std::size_t i = 0; i < n; ++i) {
      output.channel(0)[i] = frames_[i].l * gain * velocity_gain_stream_[i];
      output.channel(1)[i] = frames_[i].r * gain * velocity_gain_stream_[i];
    }
  }

private:
  float modulation_target_span(pulp::state::ParamID id) const {
    if (modulation_store_)
      if (const auto *info = modulation_store_->info(id))
        return info->range.max - info->range.min;
    // State can be restored before parameter registration. These are the
    // declared non-unit ranges; all other destinations have a unit range.
    switch (id) {
    case kPitch: return 48.0f;
    case kGain: return 72.0f;
    case kQuality: return 3.0f;
    case kMode: return static_cast<float>(kAvailableSourceModeMax);
    case kTimeAmount:
    case kSizeAmount:
    case kShapeAmount:
    case kPitchAmount:
    case kSeedMode: return 2.0f;
    default: return 1.0f;
    }
  }

  void clear_inactive_modulation(pulp::state::ParamID id) {
    if (!modulation_store_)
      return;
    for (std::size_t lane = 0; lane < kLfoLaneCount; ++lane) {
      const auto config = lfo(id, lane);
      if (config.enabled && config.depth > 0.0f)
        return;
    }
    modulation_store_->set_mod_offset(id, 0.0f);
    modulation_store_->clear_display_modulation(id);
  }

  void update_modulation(std::size_t samples) {
    if (!sample_rate_)
      return;
    std::array<float, kLfoLaneCount> waves{};
    std::array<bool, kLfoLaneCount> active_lanes{};
    for (std::size_t lane = 0; lane < kLfoLaneCount; ++lane) {
      const auto l = lfo_generator(lane);
      if (!l.enabled)
        continue;
      active_lanes[lane] = true;
      const float phase = l.phase;
      float value = 0.0f;
      switch (l.shape) {
      case LfoModulation::Shape::Triangle:
        value = 1.0f - 4.0f * std::fabs(phase - 0.5f);
        break;
      case LfoModulation::Shape::Square:
        value = phase < 0.5f ? 1.0f : -1.0f;
        break;
      case LfoModulation::Shape::Saw:
        value = 2.0f * phase - 1.0f;
        break;
      case LfoModulation::Shape::Sine:
      default:
        value = std::sin(phase * 6.28318530718f);
        break;
      }
      waves[lane] = value;
      const float next_phase = std::fmod(
          phase + l.rate_hz * static_cast<float>(samples) /
                      static_cast<float>(sample_rate_),
          1.0f);
      // A generator advances once per block even with no assigned target.
      // UI changes and audio phase publication use lock-free atomic fields.
      auto observed_phase = phase;
      lfo_generators_[lane].phase.compare_exchange_strong(
          observed_phase, next_phase, std::memory_order_relaxed);
    }
    for (pulp::state::ParamID id = kFirstModulatableParam;
         id <= kLastModulatableParam; ++id) {
      float offset = 0.0f;
      bool active = false;
      for (std::size_t lane = 0; lane < kLfoLaneCount; ++lane) {
        if (!active_lanes[lane] || !lfo_target_enabled(id, lane))
          continue;
        const auto depth = lfo_target_depth(id, lane);
        active |= depth > 0.0f;
        offset += waves[lane] * depth;
      }
      const float span = modulation_target_span(id);
      const float bounded_offset = std::clamp(offset, -1.0f, 1.0f) * span;
      state().set_mod_offset(id, bounded_offset);
      if (active)
        state().set_display_modulation(id, state().get_modulated(id));
      else
        state().clear_display_modulation(id);
    }
  }

  beads::RandomizedParameter randomized(pulp::state::ParamID id,
                                        float offset = 0.0f) const {
    // Source-backed Beads attenurandomizer amounts are passed through as
    // the centered DSP attenuverter value. No CV input is synthesized
    // here; a future host/CV bridge can provide it without changing this
    // bipolar mapping.
    pulp::state::ParamID amount_id = 0;
    switch (id) {
    case kTime:
      amount_id = kTimeAmount;
      break;
    case kSize:
      amount_id = kSizeAmount;
      break;
    case kShape:
      amount_id = kShapeAmount;
      break;
    case kPitch:
      amount_id = kPitchAmount;
      break;
    default:
      break;
    }
    const float amount = amount_id ? state().get_modulated(amount_id) : 0.0f;
    return {state().get_modulated(id) + offset, amount, 0.0f, false};
  }

  beads::Synth synth_;
  static constexpr std::size_t kDelayBankBytes = 262144;
  static constexpr std::size_t kDelayStorageBytes = kDelayBankBytes * 3;
  alignas(16) std::array<std::uint8_t, kDelayStorageBytes> delay_storage_{};
  std::array<std::uint8_t *, 3> delay_buffers_{};
  std::array<std::uint16_t, 32768> reverb_buffer_{};
  std::array<std::uint16_t, 32768> effect_reverb_buffer_{};
  std::vector<beads::FloatFrame> frames_;
  std::vector<beads::FloatFrame> input_frames_;
  std::vector<beads::TriggerGenerator::Output> triggers_;
  std::vector<stmlib::GateFlags> seed_gate_flags_;
  std::vector<stmlib::GateFlags> merged_gate_flags_;
  std::unique_ptr<bool[]> freeze_;
  std::vector<float> velocity_gain_events_;
  std::vector<float> velocity_gain_stream_;
  beads::Processor effect_;
  beads::Processor::Scratch effect_scratch_;
  double sample_rate_ = 48000.0;
  std::size_t max_block_size_ = 0;
  bool supported_rate_ = false;
  beads::Processor::Media effect_media_ = beads::Processor::MEDIA_DIGITAL;
  bool resources_ok_ = false;
  beads::SeedButtonState last_seed_button_state_ = beads::SEED_BUTTON_STATE_OFF;
  bool gate_ = false;
  std::uint8_t current_note_ = 60;
  std::array<bool, 128> active_notes_{};
  std::array<bool, 128> sustained_notes_{};
  std::array<float, 128> note_velocity_gain_{};
  bool sustain_down_ = false;
  bool has_note_activity_ = false;
  float velocity_gain_ = 1.0f;
  float pitch_bend_semitones_ = 0.0f;
  std::size_t last_gate_size_ = 0;
  std::size_t last_velocity_gain_size_ = 0;
#if defined(BEADSY_TESTING)
public:
  beads::SeedButtonState debug_seed_button_state() const {
    return last_seed_button_state_;
  }
  std::span<const stmlib::GateFlags> debug_seed_gate_flags() const {
    return {seed_gate_flags_.data(), last_gate_size_};
  }
  std::span<const float> debug_velocity_gain_stream() const {
    return {velocity_gain_stream_.data(), last_velocity_gain_size_};
  }

private:
#endif
  static constexpr bool is_modulatable(pulp::state::ParamID id) {
    return id >= kFirstModulatableParam && id <= kLastModulatableParam;
  }

  // Slot zero is deliberately unused because Pulp parameter IDs are
  // 1-based.  This makes indexing safe for every declared parameter ID.
  static constexpr std::size_t kLfoLaneCount = 2;
  static LfoModulation sanitize_lfo(LfoModulation lfo) {
    lfo.rate_hz = std::isfinite(lfo.rate_hz)
                      ? std::clamp(lfo.rate_hz, 0.01f, 20.0f) : 1.0f;
    lfo.depth = std::isfinite(lfo.depth)
                    ? std::clamp(lfo.depth, 0.0f, 1.0f) : 0.0f;
    lfo.phase = std::isfinite(lfo.phase)
                    ? lfo.phase - std::floor(lfo.phase) : 0.0f;
    lfo.shape = static_cast<LfoModulation::Shape>(
        std::min<std::uint8_t>(static_cast<std::uint8_t>(lfo.shape), 3));
    return lfo;
  }

  struct SharedLfo {
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
    std::atomic<bool> enabled{false};
    std::atomic<float> rate_hz{1.0f};
    std::atomic<float> depth{0.0f};
    std::atomic<float> phase{0.0f};
    std::atomic<std::uint8_t> shape{0};
    LfoModulation load() const {
      return {.enabled = enabled.load(std::memory_order_relaxed),
              .rate_hz = rate_hz.load(std::memory_order_relaxed),
              .depth = depth.load(std::memory_order_relaxed),
              .phase = phase.load(std::memory_order_relaxed),
              .shape = static_cast<LfoModulation::Shape>(
                  shape.load(std::memory_order_relaxed))};
    }
    void store(const LfoModulation &lfo) {
      rate_hz.store(lfo.rate_hz, std::memory_order_relaxed);
      depth.store(lfo.depth, std::memory_order_relaxed);
      phase.store(lfo.phase, std::memory_order_relaxed);
      shape.store(static_cast<std::uint8_t>(lfo.shape), std::memory_order_relaxed);
      enabled.store(lfo.enabled, std::memory_order_relaxed);
    }
  };
  struct LfoRoute {
    std::atomic<bool> enabled{false};
    std::atomic<float> depth{0.5f};
  };
  using LfoTargets = std::array<LfoRoute,
                               static_cast<std::size_t>(kLastModulatableParam) + 1>;
  std::array<SharedLfo, kLfoLaneCount> lfo_generators_{};
  std::array<LfoTargets, kLfoLaneCount> lfo_targets_{};
  pulp::state::StateStore *modulation_store_ = nullptr;
  std::array<std::string, 2> route_ids_{};

  static bool select_effect_media(double sample_rate,
                                  beads::Processor::Media &media) {
    // Beads::Synth::Init() and Beads::Processor::Init() hard-code their
    // internal timing/filter constants to 48 kHz. The processor's
    // hardware-oriented 24/32 kHz media modes do not resample the synth
    // path, so accepting those host rates would produce incorrect timing.
    if (std::abs(sample_rate - 48000.0) < 0.5) {
      media = beads::Processor::MEDIA_DIGITAL;
      return true;
    }
    return false;
  }
};

inline std::unique_ptr<pulp::format::Processor> create_beadsy() {
  return std::make_unique<Beadsy>();
}

} // namespace beadsy

#if defined(BEADSY_NATIVE_EDITOR) && !defined(PULP_HEADLESS)
#include "beadsy_editor.hpp"
inline std::unique_ptr<pulp::view::View> beadsy::Beadsy::create_view() {
  return std::make_unique<beadsy::BeadsyEditor>(*this, state());
}
#endif
