#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>

namespace dualnam
{
enum class ModelSlot : std::size_t
{
  A = 0,
  B = 1
};

enum class SlotChange
{
  None,
  Activated,
  Removed
};

template <typename Model>
class ModelSlots
{
public:
  static constexpr std::size_t kCount = 2;

  ModelSlots()
  {
    for (std::size_t index = 0; index < kCount; ++index)
    {
      mStagedReady[index].store(false);
      mRemoveRequested[index].store(false);
    }
  }

  Model* Live(const ModelSlot slot) const { return mLive[Index(slot)].get(); }
  Model* Staged(const ModelSlot slot) const { return mStaged[Index(slot)].get(); }

  void Stage(const ModelSlot slot, std::unique_ptr<Model> model)
  {
    const auto index = Index(slot);
    mStaged[index] = std::move(model);
    mStagedReady[index].store(true, std::memory_order_release);
  }

  void CancelStaged(const ModelSlot slot)
  {
    const auto index = Index(slot);
    mStagedReady[index].store(false, std::memory_order_release);
    mStaged[index].reset();
  }

  void RequestRemove(const ModelSlot slot)
  {
    mRemoveRequested[Index(slot)].store(true, std::memory_order_release);
  }

  std::array<SlotChange, kCount> ApplyPending()
  {
    std::array<SlotChange, kCount> changes{};
    for (std::size_t index = 0; index < kCount; ++index)
    {
      if (mRemoveRequested[index].exchange(false, std::memory_order_acq_rel))
      {
        mLive[index].reset();
        changes[index] = SlotChange::Removed;
      }

      if (mStagedReady[index].exchange(false, std::memory_order_acq_rel))
      {
        mLive[index] = std::move(mStaged[index]);
        changes[index] = SlotChange::Activated;
      }
    }
    return changes;
  }

  int MaximumLatency() const
  {
    int maximum = 0;
    for (const auto& model : mLive)
      if (model != nullptr)
        maximum = std::max(maximum, model->GetLatency());
    return maximum;
  }

private:
  static constexpr std::size_t Index(const ModelSlot slot) { return static_cast<std::size_t>(slot); }

  std::array<std::unique_ptr<Model>, kCount> mLive;
  std::array<std::unique_ptr<Model>, kCount> mStaged;
  std::array<std::atomic<bool>, kCount> mStagedReady;
  std::array<std::atomic<bool>, kCount> mRemoveRequested;
};
} // namespace dualnam
