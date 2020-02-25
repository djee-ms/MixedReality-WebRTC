// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"

#include <atomic>
#include <thread>

using namespace Microsoft::MixedReality::WebRTC;

namespace {

class GlobalFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Ensure global factory is not initialized before the test
    ASSERT_EQ(GlobalFactory::InstancePtrIfExist().get(), nullptr);
  }
  void TearDown() override {
    // Ensure global factory is not intialized after the test
    ASSERT_EQ(GlobalFactory::InstancePtrIfExist().get(), nullptr);
  }
};

/// Helper to busy-wait on multiple threads being ready and release them all at
/// once, to improve the concurrency rate when testing thread safety.
struct BusyWaitSynchronizer {
  BusyWaitSynchronizer(int num_threads) : num_threads_(num_threads) {}
  inline void sync() {
    int n = num_threads_.fetch_sub(1, std::memory_order_acquire) - 1;
    while (n > 0) {
      n = num_threads_.load(std::memory_order_acquire);
    }
  }
  std::atomic_int32_t num_threads_{0};
};

}  // namespace

TEST_F(GlobalFactoryTest, BasicInitShutdown) {
  {
    RefPtr<GlobalFactory> factory(GlobalFactory::InstancePtr());
    ASSERT_NE(factory.get(), nullptr);
    ASSERT_EQ(0u, factory->ReportLiveObjects());
    ASSERT_EQ(0u, GlobalFactory::StaticReportLiveObjects());
    ASSERT_FALSE(GlobalFactory::TryShutdown());
  }
  ASSERT_EQ(0u, GlobalFactory::StaticReportLiveObjects());
  ASSERT_TRUE(GlobalFactory::TryShutdown());
}

TEST_F(GlobalFactoryTest, BasicNotExist) {
  RefPtr<GlobalFactory> factory(GlobalFactory::InstancePtrIfExist());
  ASSERT_EQ(factory.get(), nullptr);
  ASSERT_EQ(0u, GlobalFactory::StaticReportLiveObjects());
  ASSERT_TRUE(GlobalFactory::TryShutdown());
}

TEST_F(GlobalFactoryTest, ConcurrentGrab) {
  constexpr int kNumThreads = 10;
  std::thread* threads[kNumThreads];
  BusyWaitSynchronizer synchronizer(kNumThreads);
  for (auto&& t : threads) {
    t = new std::thread([&]() {
      // Wait for all other threads
      synchronizer.sync();
      // Grab factory
      RefPtr<GlobalFactory> factory(GlobalFactory::InstancePtr());
      ASSERT_NE(factory.get(), nullptr);
    });
  }
  for (auto&& t : threads) {
    t->join();
  }
}

TEST_F(GlobalFactoryTest, ConcurrentGrabAndRelease) {
  constexpr int kNumAcquire = 5;
  constexpr int kNumThreads = kNumAcquire * 2;
  std::thread* threads[kNumThreads];
  BusyWaitSynchronizer synchronizer(kNumThreads);
  // First group acquires first, then release when signaled
  for (int i = 0; i < kNumAcquire; ++i) {
    auto& t = threads[i];
    t = new std::thread([&]() {
      // Grab factory
      RefPtr<GlobalFactory> factory(GlobalFactory::InstancePtr());
      ASSERT_NE(factory.get(), nullptr);
      // Wait for all other threads
      synchronizer.sync();
      // Release factory
      factory.reset();
    });
  }
  // Second group acquire when signaled
  for (int i = kNumAcquire; i < kNumThreads; ++i) {
    auto& t = threads[i];
    t = new std::thread([&]() {
      // Wait for all other threads
      synchronizer.sync();
      // Grab factory
      RefPtr<GlobalFactory> factory(GlobalFactory::InstancePtr());
      ASSERT_NE(factory.get(), nullptr);
    });
  }
  for (auto&& t : threads) {
    t->join();
  }
}
