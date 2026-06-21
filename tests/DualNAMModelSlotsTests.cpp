#include "../NeuralAmpModeler/DualNAMModelSlots.h"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
class FakeModel
{
public:
  explicit FakeModel(const int identity, const int latency = 0)
  : mIdentity(identity)
  , mLatency(latency)
  {
  }

  int Identity() const { return mIdentity; }
  int GetLatency() const { return mLatency; }

private:
  int mIdentity;
  int mLatency;
};

void Require(const bool condition, const char* message)
{
  if (condition)
    return;

  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

void TestActivatesSlotsIndependently()
{
  dualnam::ModelSlots<FakeModel> slots;
  slots.Stage(dualnam::ModelSlot::A, std::make_unique<FakeModel>(1));
  slots.Stage(dualnam::ModelSlot::B, std::make_unique<FakeModel>(2));

  const auto changes = slots.ApplyPending();

  Require(changes[0] == dualnam::SlotChange::Activated, "slot A must activate independently");
  Require(changes[1] == dualnam::SlotChange::Activated, "slot B must activate independently");
  Require(slots.Live(dualnam::ModelSlot::A)->Identity() == 1, "slot A must retain its staged model");
  Require(slots.Live(dualnam::ModelSlot::B)->Identity() == 2, "slot B must retain its staged model");
}

void TestClearingOneSlotDoesNotAlterTheOther()
{
  dualnam::ModelSlots<FakeModel> slots;
  slots.Stage(dualnam::ModelSlot::A, std::make_unique<FakeModel>(1));
  slots.Stage(dualnam::ModelSlot::B, std::make_unique<FakeModel>(2));
  slots.ApplyPending();

  slots.RequestRemove(dualnam::ModelSlot::A);
  const auto changes = slots.ApplyPending();

  Require(changes[0] == dualnam::SlotChange::Removed, "slot A must report removal");
  Require(changes[1] == dualnam::SlotChange::None, "slot B must report no change");
  Require(slots.Live(dualnam::ModelSlot::A) == nullptr, "slot A must be empty after removal");
  Require(slots.Live(dualnam::ModelSlot::B)->Identity() == 2, "slot B must survive slot A removal");
}

void TestReportsMaximumBranchLatency()
{
  dualnam::ModelSlots<FakeModel> slots;
  slots.Stage(dualnam::ModelSlot::A, std::make_unique<FakeModel>(1, 12));
  slots.Stage(dualnam::ModelSlot::B, std::make_unique<FakeModel>(2, 37));
  slots.ApplyPending();

  Require(slots.MaximumLatency() == 37, "latency must be the maximum of the two live branches");
}
} // namespace

int main()
{
  TestActivatesSlotsIndependently();
  TestClearingOneSlotDoesNotAlterTheOther();
  TestReportsMaximumBranchLatency();
  std::cout << "DualNAM model-slot tests passed\n";
  return EXIT_SUCCESS;
}
