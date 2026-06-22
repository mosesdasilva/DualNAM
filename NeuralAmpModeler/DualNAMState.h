#pragma once

#include <string>
#include <utility>

namespace dualnam::state
{
inline constexpr char kHeader[] = "###DualNAM###";
inline constexpr char kLegacyNAMHeader[] = "###NeuralAmpModeler###";
inline constexpr char kSchemaVersion[] = "3";

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

  static OutputGains FromLegacy(const double sharedOutput) { return {sharedOutput, sharedOutput}; }
};
} // namespace dualnam::state
