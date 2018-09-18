#ifndef SubscriptionServiceInterface_hpp
#define SubscriptionServiceInterface_hpp

#include <atomic>
#include <chrono>
#include <future>
#include <stdio.h>
#include <utility>

#include "Constants.hpp"
#include "Event.hpp"
#include "NetworkException.hpp"
#include "Subject.hpp"
#include "Timer.hpp"

namespace SS {

using namespace std::chrono;

template <typename ResponseType, typename TimeUnitType = milliseconds,
          typename ClockType = system_clock>
class SubscriptionServiceInterface : public Subject<ResponseType> {
public:
  // For base classes
  using TimeUnit = TimeUnitType;
  using Clock = ClockType;
  using Response = ResponseType;

  SubscriptionServiceInterface() {}

  virtual auto timeToLive() -> TimeUnit = 0;

protected:
  virtual auto getResponse() -> ResponseType = 0;

  auto cacheResponse(ResponseType response) const -> void {
    m_cachedResponse = response;
  }

  auto getCachedResponse() const -> ResponseType { return m_cachedResponse; }

  auto startTimer() -> void { responseTimer.start(); }

  auto endTimer(bool success) -> void { responseTimer.stop(success); }

  auto averageResponseTime() const -> TimeUnit {
    return responseTimer.averageResponseTime();
  }

  auto maxResponseTime() const -> TimeUnit {
    return responseTimer.maxResponseTime();
  }

  auto minResponseTime() const -> TimeUnit {
    return responseTimer.minResponseTime();
  }

  auto updateResponseTimer(Timer<TimeUnit, Clock> &&timer) -> void {
    responseTimer.update(std::move(timer));
  }

private:
  mutable ResponseType m_cachedResponse;
  mutable Timer<TimeUnit, Clock> responseTimer;
};

} // namespace SS

#endif /* SubscriptionServiceInterface_hpp */
