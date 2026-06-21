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
} // namespace

int main()
{
  TestDualNAMUsesItsOwnStateHeader();
  TestLegacyStateMapsOnlyToSlotA();
  TestDualPathsRemainIndependent();
  std::cout << "DualNAM state tests passed\n";
  return EXIT_SUCCESS;
}
