#include "beads/dsp/synth.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kFrames = 4096;

void render_case(const std::string& prefix, std::size_t block_size, int quality,
                 int state, int seed_scenario) {
  std::uint16_t reverb[32768] = {};
  beads::Synth synth;
  synth.Init(reverb);

  beads::Parameters parameters{};
  parameters.time.base = state == 1 ? 0.25f : 0.5f;
  parameters.time.attenuverter = 0.0f;
  parameters.time.has_cv = false;
  parameters.size.base = state == 2 ? 0.8f : 0.5f;
  parameters.size.attenuverter = 0.0f;
  parameters.size.has_cv = false;
  parameters.shape.base = state == 3 ? 0.2f : 0.5f;
  parameters.shape.attenuverter = 0.0f;
  parameters.shape.has_cv = false;
  parameters.pitch.base = state == 1 ? 7.0f : 0.0f;
  parameters.pitch.attenuverter = 0.0f;
  parameters.pitch.has_cv = false;
  parameters.monophonic_mode = false;

  beads::Processor::MixParameters mix{
      state == 2 ? 0.3f : 0.0f,
      state == 0 ? 1.0f : 0.75f,
      state == 3 ? 0.2f : 0.0f,
      false};
  beads::ModulatedParameter density{};
  density.base = state == 2 ? 0.8f : 0.5f;
  density.cv = 0.0f;
  std::unique_ptr<bool[]> freeze(new bool[block_size]);
  std::fill_n(freeze.get(), block_size, state == 3);
  std::vector<beads::FloatFrame> output(block_size);
  std::vector<stmlib::GateFlags> external_gate(block_size,
                                                stmlib::GATE_FLAG_LOW);
#ifdef BEASY_PORT_API
  std::vector<beads::TriggerGenerator::Output> triggers(block_size);
  std::vector<stmlib::GateFlags> merged_gate_flags(block_size);
#endif

  std::vector<float> samples;
  samples.reserve(kFrames * 2);
  for (std::size_t frame = 0; frame < kFrames; frame += block_size) {
    const auto seed = seed_scenario == 1
                          ? beads::SEED_BUTTON_STATE_LOCKED
                          : (seed_scenario == 2
                                 ? beads::SEED_BUTTON_STATE_OFF
                                 : (frame == 0 ? beads::SEED_BUTTON_STATE_ON
                                               : beads::SEED_BUTTON_STATE_OFF));
    const bool has_external = seed_scenario != 0;
    for (std::size_t i = 0; i < block_size; ++i) {
      const std::size_t absolute = frame + i;
      const bool previous = absolute == 0 ? false : ((absolute - 1) % 256) < 64;
      const bool current = (absolute % 256) < 64;
      external_gate[i] = stmlib::ExtractGateFlags(previous, current);
    }
#ifdef BEASY_PORT_API
    synth.Render(seed, has_external ? external_gate.data() : nullptr,
                 freeze.get(),
                 static_cast<beads::Synth::Resolution>(quality), density,
                 parameters, mix, triggers, merged_gate_flags, output.data(),
                 block_size);
#else
    synth.Render(seed, has_external ? external_gate.data() : nullptr,
                 freeze.get(),
                 static_cast<beads::Synth::Resolution>(quality), density,
                 parameters, mix, output.data(), block_size);
#endif
    for (const auto& frame_output : output) {
      samples.push_back(frame_output.l);
      samples.push_back(frame_output.r);
    }
  }
  const auto path = prefix + "-b" + std::to_string(block_size) + "-q" +
                    std::to_string(quality) + "-c" + std::to_string(state) +
                    "-s" + std::to_string(seed_scenario) + ".bin";
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(samples.data()),
             static_cast<std::streamsize>(samples.size() * sizeof(float)));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string prefix = argv[1];
  for (const auto block_size : {std::size_t{32}, std::size_t{64},
                                std::size_t{128}}) {
    for (int quality = 0; quality < 4; ++quality) {
      for (int state = 0; state < 4; ++state) {
        for (int seed_scenario = 0; seed_scenario < 3; ++seed_scenario) {
          render_case(prefix, block_size, quality, state, seed_scenario);
        }
      }
    }
  }
  return 0;
}
