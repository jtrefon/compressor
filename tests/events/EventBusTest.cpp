#include <compression/events/EventBus.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

using namespace compression::events;

namespace {

class RecordingListener final : public IEventListener {
public:
  void onEvent(const CompressionEvent &event) override {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(event);
  }

  std::vector<CompressionEvent> events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
  }

  std::size_t count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.size();
  }

private:
  mutable std::mutex mutex_;
  std::vector<CompressionEvent> events_;
};

CompressionEvent makeEvent(EventType type, uint8_t pct = 0) {
  return CompressionEvent{type, compression::format::AlgorithmID::BWT_COMPRESSOR,
                          0, 0, pct};
}

} // namespace

TEST(EventBusTest, DeliversToSubscriber) {
  EventBus bus;
  auto listener = std::make_shared<RecordingListener>();
  bus.subscribe(listener);
  bus.publish(makeEvent(EventType::OperationStarted, 0));
  ASSERT_EQ(listener->count(), 1u);
  EXPECT_EQ(listener->events()[0].type, EventType::OperationStarted);
}

TEST(EventBusTest, DeliversToMultipleSubscribers) {
  EventBus bus;
  auto a = std::make_shared<RecordingListener>();
  auto b = std::make_shared<RecordingListener>();
  bus.subscribe(a);
  bus.subscribe(b);
  bus.publish(makeEvent(EventType::CodecSelected));
  EXPECT_EQ(a->count(), 1u);
  EXPECT_EQ(b->count(), 1u);
}

TEST(EventBusTest, ExpiredSubscriberIsDropped) {
  EventBus bus;
  {
    auto listener = std::make_shared<RecordingListener>();
    bus.subscribe(listener);
    EXPECT_EQ(bus.subscriberCount(), 1u);
  }
  // Listener destroyed; publishing must not crash and must drop it.
  bus.publish(makeEvent(EventType::OperationStarted));
  EXPECT_EQ(bus.subscriberCount(), 0u);
}

TEST(EventBusTest, CarriesEventFields) {
  EventBus bus;
  auto listener = std::make_shared<RecordingListener>();
  bus.subscribe(listener);
  CompressionEvent event;
  event.type = EventType::ChunkProgress;
  event.codec = compression::format::AlgorithmID::LZ77_COMPRESSOR;
  event.bytesIn = 100;
  event.bytesOut = 40;
  event.progressPct = 42;
  bus.publish(event);
  const auto received = listener->events()[0];
  EXPECT_EQ(received.bytesIn, 100u);
  EXPECT_EQ(received.bytesOut, 40u);
  EXPECT_EQ(received.progressPct, 42u);
  EXPECT_EQ(received.codec, compression::format::AlgorithmID::LZ77_COMPRESSOR);
}

TEST(EventBusTest, ThreadSafePublish) {
  EventBus bus;
  auto listener = std::make_shared<RecordingListener>();
  bus.subscribe(listener);

  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&bus]() {
      for (int i = 0; i < 100; ++i) {
        bus.publish(makeEvent(EventType::OperationCompleted, 100));
      }
    });
  }
  for (auto &t : threads) {
    t.join();
  }
  EXPECT_EQ(listener->count(), 400u);
}
