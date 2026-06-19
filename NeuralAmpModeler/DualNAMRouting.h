#pragma once

#include <algorithm>
#include <cstddef>

namespace dualnam
{
constexpr std::size_t kStereoChannels = 2;

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
