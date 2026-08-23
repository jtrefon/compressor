#pragma once

#include <compression/FileFormat.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace compression {
namespace events {

/**
 * @brief Categories of events emitted during compression operations.
 *
 * Emitted by the engine layers (CodecRegistry-selected codecs, the parallel
 * decorator, the facades); consumed by adapters (CLI, UI, benchmark).
 */
enum class EventType {
  OperationStarted,
  OperationCompleted,
  CodecSelected,
  BlockStarted,
  BlockCompleted,
  ChunkProgress,
  StageEntered,
  ChecksumVerified,
};

/**
 * @brief Immutable snapshot of a single event.
 */
struct CompressionEvent {
  EventType type = EventType::OperationStarted;
  format::AlgorithmID codec = format::AlgorithmID::UNKNOWN;
  uint64_t bytesIn = 0;
  uint64_t bytesOut = 0;
  uint8_t progressPct = 0;
};

/**
 * @brief Observer of compression events.
 *
 * Implemented by adapters: the CLI prints, the UI marshals to its thread,
 * the benchmark aggregates timings. Never referenced by domain code.
 */
class IEventListener {
public:
  virtual ~IEventListener() = default;
  virtual void onEvent(const CompressionEvent &event) = 0;
};

/**
 * @brief Thread-safe publisher/subscriber bus (Observer pattern).
 *
 * Subscribers hold weak ownership: a destroyed subscriber is dropped
 * automatically on the next publish. There is no mutable global state.
 */
class EventBus {
public:
  void subscribe(std::weak_ptr<IEventListener> listener);

  void publish(const CompressionEvent &event);

  std::size_t subscriberCount() const;

private:
  mutable std::mutex mutex_;
  std::vector<std::weak_ptr<IEventListener>> listeners_;
};

} // namespace events
} // namespace compression
