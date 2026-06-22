#pragma once

#include <string>
#include <utility>

namespace dualnam::state
{
inline constexpr char kHeader[] = "###DualNAM###";
inline constexpr char kLegacyNAMHeader[] = "###NeuralAmpModeler###";
inline constexpr char kSchemaVersion[] = "5";

struct ModelPaths
{
  std::string modelA;
  std::string modelB;

  static ModelPaths FromLegacy(std::string legacyPath) { return {std::move(legacyPath), {}}; }
};

struct OutputGains
{
  double outputA;
  double outputB;
  double globalOutput;

  static OutputGains FromLegacy(const double sharedOutput) { return {sharedOutput, sharedOutput, 0.0}; }
};

struct EQSettings
{
  bool activeA;
  double bassA;
  double middleA;
  double trebleA;
  bool activeB;
  double bassB;
  double middleB;
  double trebleB;

  static EQSettings FromLegacy(const bool active, const double bass, const double middle, const double treble)
  {
    return {active, bass, middle, treble, active, bass, middle, treble};
  }
};
} // namespace dualnam::state
