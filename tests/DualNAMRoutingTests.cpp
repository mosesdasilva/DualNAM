#include "../NeuralAmpModeler/DualNAMRouting.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
constexpr int kFrames = 4;
constexpr double kTolerance = 1.0e-12;

class GainModel
{
public:
  explicit GainModel(const double gain)
  : mGain(gain)
  {
  }

  void process(double** inputs, double** outputs, const int numFrames)
  {
    for (int frame = 0; frame < numFrames; ++frame)
      outputs[0][frame] = mGain * inputs[0][frame];
  }

private:
  double mGain;
};

void RequireNear(const double actual, const double expected, const char* message)
{
  if (std::abs(actual - expected) <= kTolerance)
    return;

  std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
  std::exit(EXIT_FAILURE);
}

void TestRoutesStereoChannelsThroughIndependentModels()
{
  std::array<double, kFrames> leftInput{1.0, 2.0, 3.0, 4.0};
  std::array<double, kFrames> rightInput{10.0, 20.0, 30.0, 40.0};
  std::array<double, kFrames> leftOutput{};
  std::array<double, kFrames> rightOutput{};

  double* inputs[2]{leftInput.data(), rightInput.data()};
  double* outputs[2]{leftOutput.data(), rightOutput.data()};
  GainModel modelA(2.0);
  GainModel modelB(-0.5);
  GainModel* models[2]{&modelA, &modelB};

  dualnam::ProcessStereoModels(inputs, outputs, kFrames, models);

  for (int frame = 0; frame < kFrames; ++frame)
  {
    RequireNear(leftOutput[frame], 2.0 * leftInput[frame], "model A must process only the left input");
    RequireNear(rightOutput[frame], -0.5 * rightInput[frame], "model B must process only the right input");
  }
}

void TestSilencesAnUnloadedSlot()
{
  std::array<double, kFrames> leftInput{1.0, 2.0, 3.0, 4.0};
  std::array<double, kFrames> rightInput{10.0, 20.0, 30.0, 40.0};
  std::array<double, kFrames> leftOutput{};
  std::array<double, kFrames> rightOutput{9.0, 9.0, 9.0, 9.0};

  double* inputs[2]{leftInput.data(), rightInput.data()};
  double* outputs[2]{leftOutput.data(), rightOutput.data()};
  GainModel modelA(1.0);
  GainModel* models[2]{&modelA, nullptr};

  dualnam::ProcessStereoModels(inputs, outputs, kFrames, models);

  for (int frame = 0; frame < kFrames; ++frame)
  {
    RequireNear(leftOutput[frame], leftInput[frame], "loaded slot A must still process");
    RequireNear(rightOutput[frame], 0.0, "unloaded slot B must be silent");
  }
}
} // namespace

int main()
{
  TestRoutesStereoChannelsThroughIndependentModels();
  TestSilencesAnUnloadedSlot();
  std::cout << "DualNAM routing tests passed\n";
  return EXIT_SUCCESS;
}
