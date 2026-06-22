#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace dualnam
{
constexpr std::size_t kStereoChannels = 2;

inline double DecibelsToLinear(const double decibels) { return std::pow(10.0, decibels / 20.0); }

template <typename Sample>
Sample* StereoChannelPointer(Sample** channels, const std::size_t channel)
{
  return channels[channel];
}

template <typename Sample, typename Gain>
void ApplyStereoInputGains(Sample** inputs, Sample** outputs, const int numFrames,
                           const Gain (&gains)[kStereoChannels])
{
  for (std::size_t channel = 0; channel < kStereoChannels; ++channel)
    for (int frame = 0; frame < numFrames; ++frame)
      outputs[channel][frame] = static_cast<Sample>(gains[channel] * inputs[channel][frame]);
}

template <typename Sample, typename Gain>
void ApplyStereoOutputGains(Sample** inputs, Sample** outputs, const int numFrames,
                            const Gain (&gains)[kStereoChannels], const std::size_t numOutputChannels = kStereoChannels)
{
  for (std::size_t channel = 0; channel < numOutputChannels; ++channel)
  {
    const bool inputAvailable = channel < kStereoChannels;
    for (int frame = 0; frame < numFrames; ++frame)
      outputs[channel][frame] =
        inputAvailable ? static_cast<Sample>(gains[channel] * inputs[channel][frame]) : static_cast<Sample>(0);
  }
}

template <typename Sample, typename Model>
void ProcessStereoModels(Sample** inputs, Sample** outputs, const int numFrames,
                         Model* const (&models)[kStereoChannels])
{
  for (std::size_t channel = 0; channel < kStereoChannels; ++channel)
  {
    if (models[channel] == nullptr)
    {
      std::fill_n(outputs[channel], numFrames, static_cast<Sample>(0));
      continue;
    }

    Sample* modelInput[1]{inputs[channel]};
    Sample* modelOutput[1]{outputs[channel]};
    models[channel]->process(modelInput, modelOutput, numFrames);
  }
}
} // namespace dualnam
