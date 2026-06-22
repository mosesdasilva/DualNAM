#include "../NeuralAmpModeler/DualNAMState.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
void Require(const bool condition, const char* message)
{
  if (condition)
    return;

  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

void TestDualNAMUsesItsOwnStateHeader()
{
  Require(std::string(dualnam::state::kHeader) == "###DualNAM###", "DualNAM state header must be stable");
  Require(std::string(dualnam::state::kSchemaVersion) == "4",
          "schema version 4 must include independent A/B EQ parameters");
  Require(std::string(dualnam::state::kHeader) != dualnam::state::kLegacyNAMHeader,
          "DualNAM state must not be written with the upstream NAM header");
}

void TestLegacyStateMapsOnlyToSlotA()
{
  const auto paths = dualnam::state::ModelPaths::FromLegacy("/Models/legacy.nam");

  Require(paths.modelA == "/Models/legacy.nam", "legacy NAM path must restore into slot A");
  Require(paths.modelB.empty(), "legacy NAM state must leave slot B empty");
}

void TestDualPathsRemainIndependent()
{
  const dualnam::state::ModelPaths paths{"/Models/a.nam", "/Models/b.nam"};

  Require(paths.modelA == "/Models/a.nam", "slot A path must remain independent");
  Require(paths.modelB == "/Models/b.nam", "slot B path must remain independent");
}

void TestLegacySharedOutputMigratesToBothBranches()
{
  const auto gains = dualnam::state::OutputGains::FromLegacy(-6.0);

  Require(gains.outputA == -6.0, "legacy shared output must migrate to output A");
  Require(gains.outputB == -6.0, "legacy shared output must migrate to output B");
}

void TestLegacySharedEQMigratesToBothBranches()
{
  const auto eq = dualnam::state::EQSettings::FromLegacy(true, 3.0, 4.0, 7.0);

  Require(eq.activeA && eq.activeB, "legacy shared EQ active state must migrate to both branches");
  Require(eq.bassA == 3.0 && eq.bassB == 3.0, "legacy bass must migrate to both branches");
  Require(eq.middleA == 4.0 && eq.middleB == 4.0, "legacy middle must migrate to both branches");
  Require(eq.trebleA == 7.0 && eq.trebleB == 7.0, "legacy treble must migrate to both branches");
}
} // namespace

int main()
{
  TestDualNAMUsesItsOwnStateHeader();
  TestLegacyStateMapsOnlyToSlotA();
  TestDualPathsRemainIndependent();
  TestLegacySharedOutputMigratesToBothBranches();
  TestLegacySharedEQMigratesToBothBranches();
  std::cout << "DualNAM state tests passed\n";
  return EXIT_SUCCESS;
}
