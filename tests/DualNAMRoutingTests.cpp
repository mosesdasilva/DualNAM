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

class GainEffect
{
public:
  explicit GainEffect(const double gain)
  : mGain(gain)
  {
  }

  double** Process(double** inputs, const int numChannels, const int numFrames)
  {
    if (numChannels != 1)
      std::exit(EXIT_FAILURE);

    for (int frame = 0; frame < numFrames; ++frame)
      mOutput[frame] = mGain * inputs[0][frame];
    mOutputPointer = mOutput.data();
    return &mOutputPointer;
  }

private:
  double mGain;
  std::array<double, kFrames> mOutput{};
  double* mOutputPointer = nullptr;
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

void TestAppliesIndependentInputGainsBeforeEachModel()
{
  std::array<double, kFrames> leftInput{1.0, 2.0, 3.0, 4.0};
  std::array<double, kFrames> rightInput{10.0, 20.0, 30.0, 40.0};
  std::array<double, kFrames> gainedLeftInput{};
  std::array<double, kFrames> gainedRightInput{};
  std::array<double, kFrames> leftOutput{};
  std::array<double, kFrames> rightOutput{};

  double* inputs[2]{leftInput.data(), rightInput.data()};
  double* gainedInputs[2]{gainedLeftInput.data(), gainedRightInput.data()};
  double* outputs[2]{leftOutput.data(), rightOutput.data()};
  GainModel modelA(1.0);
  GainModel modelB(1.0);
  GainModel* models[2]{&modelA, &modelB};
  const double inputGains[2]{2.0, 0.25};

  dualnam::ApplyStereoInputGains(inputs, gainedInputs, kFrames, inputGains);
  dualnam::ProcessStereoModels(gainedInputs, outputs, kFrames, models);

  for (int frame = 0; frame < kFrames; ++frame)
  {
    RequireNear(leftOutput[frame], 2.0 * leftInput[frame], "input A gain must affect only model A");
    RequireNear(rightOutput[frame], 0.25 * rightInput[frame], "input B gain must affect only model B");
  }
}

void TestConvertsInputGainDecibelsToLinearAmplitude()
{
  RequireNear(dualnam::DecibelsToLinear(20.0), 10.0, "+20 dB must produce ten times amplitude");
  RequireNear(dualnam::DecibelsToLinear(0.0), 1.0, "0 dB must produce unity amplitude");
  RequireNear(dualnam::DecibelsToLinear(-20.0), 0.1, "-20 dB must produce one tenth amplitude");
}

void TestAppliesIndependentOutputGainsAfterProcessing()
{
  std::array<double, kFrames> leftInput{1.0, 2.0, 3.0, 4.0};
  std::array<double, kFrames> rightInput{10.0, 20.0, 30.0, 40.0};
  std::array<double, kFrames> leftOutput{};
  std::array<double, kFrames> rightOutput{};

  double* inputs[2]{leftInput.data(), rightInput.data()};
  double* outputs[2]{leftOutput.data(), rightOutput.data()};
  const double outputGains[2]{0.5, 2.0};

  dualnam::ApplyStereoOutputGains(inputs, outputs, kFrames, outputGains);

  for (int frame = 0; frame < kFrames; ++frame)
  {
    RequireNear(leftOutput[frame], 0.5 * leftInput[frame], "output A gain must affect only the left output");
    RequireNear(rightOutput[frame], 2.0 * rightInput[frame], "output B gain must affect only the right output");
  }
}

void TestSelectsIndependentChannelsForMetering()
{
  std::array<double, kFrames> left{1.0, 2.0, 3.0, 4.0};
  std::array<double, kFrames> right{10.0, 20.0, 30.0, 40.0};
  double* channels[2]{left.data(), right.data()};

  RequireNear(dualnam::StereoChannelPointer(channels, 0)[2], 3.0, "meter A must read only channel A");
  RequireNear(dualnam::StereoChannelPointer(channels, 1)[2], 30.0, "meter B must read only channel B");
}

void TestProcessesStereoBranchesThroughIndependentEffects()
{
  std::array<double, kFrames> leftInput{1.0, 2.0, 3.0, 4.0};
  std::array<double, kFrames> rightInput{10.0, 20.0, 30.0, 40.0};
  double* inputs[2]{leftInput.data(), rightInput.data()};
  double* outputs[2]{};
  GainEffect effectA(2.0);
  GainEffect effectB(0.25);
  GainEffect* effects[2]{&effectA, &effectB};
  const bool enabled[2]{true, true};

  dualnam::RouteStereoEffects(inputs, outputs, kFrames, effects, enabled);

  for (int frame = 0; frame < kFrames; ++frame)
  {
    RequireNear(outputs[0][frame], 2.0 * leftInput[frame], "EQ A must process only channel A");
    RequireNear(outputs[1][frame], 0.25 * rightInput[frame], "EQ B must process only channel B");
  }
}

void TestBypassesEachStereoEffectIndependently()
{
  std::array<double, kFrames> leftInput{1.0, 2.0, 3.0, 4.0};
  std::array<double, kFrames> rightInput{10.0, 20.0, 30.0, 40.0};
  double* inputs[2]{leftInput.data(), rightInput.data()};
  double* outputs[2]{};
  GainEffect effectA(2.0);
  GainEffect effectB(0.25);
  GainEffect* effects[2]{&effectA, &effectB};
  const bool enabled[2]{false, true};

  dualnam::RouteStereoEffects(inputs, outputs, kFrames, effects, enabled);

  for (int frame = 0; frame < kFrames; ++frame)
  {
    RequireNear(outputs[0][frame], leftInput[frame], "bypassed EQ A must leave channel A unchanged");
    RequireNear(outputs[1][frame], 0.25 * rightInput[frame], "enabled EQ B must still process channel B");
  }
}
} // namespace

int main()
{
  TestRoutesStereoChannelsThroughIndependentModels();
  TestSilencesAnUnloadedSlot();
  TestAppliesIndependentInputGainsBeforeEachModel();
  TestConvertsInputGainDecibelsToLinearAmplitude();
  TestAppliesIndependentOutputGainsAfterProcessing();
  TestSelectsIndependentChannelsForMetering();
  TestProcessesStereoBranchesThroughIndependentEffects();
  TestBypassesEachStereoEffectIndependently();
  std::cout << "DualNAM routing tests passed\n";
  return EXIT_SUCCESS;
}
