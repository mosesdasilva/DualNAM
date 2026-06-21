#pragma once

#include <string>
#include <utility>

namespace dualnam::state
{
inline constexpr char kHeader[] = "###DualNAM###";
inline constexpr char kLegacyNAMHeader[] = "###NeuralAmpModeler###";
inline constexpr char kSchemaVersion[] = "1";

struct ModelPaths
{
  std::string modelA;
  std::string modelB;

  static ModelPaths FromLegacy(std::string legacyPath) { return {std::move(legacyPath), {}}; }
};
} // namespace dualnam::state
