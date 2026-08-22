#include <compression/app/EventBus.hpp>

#include <algorithm>

namespace compression {
namespace app {

void EventBus::subscribe(std::weak_ptr<IEventListener> listener) {
  std::lock_guard<std::mutex> lock(mutex_);
  listeners_.push_back(std::move(listener));
}

void EventBus::publish(const CompressionEvent &event) {
  std::lock_guard<std::mutex> lock(mutex_);
  listeners_.erase(
      std::remove_if(listeners_.begin(), listeners_.end(),
                     [](const std::weak_ptr<IEventListener> &listener) {
                       return listener.expired();
                     }),
      listeners_.end());
  for (const auto &listener : listeners_) {
    if (const std::shared_ptr<IEventListener> shared = listener.lock()) {
      shared->onEvent(event);
    }
  }
}

std::size_t EventBus::subscriberCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<std::size_t>(
      std::count_if(listeners_.begin(), listeners_.end(),
                    [](const std::weak_ptr<IEventListener> &listener) {
                      return !listener.expired();
                    }));
}

} // namespace app
} // namespace compression
