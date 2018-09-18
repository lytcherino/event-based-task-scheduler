#ifndef Timer_hpp
#define Timer_hpp

#include <CPBase.hpp>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdio.h>

namespace SS {

using namespace std::chrono;

template <typename TimeUnit = milliseconds, typename Clock = system_clock>
class Timer {
public:
  Timer() : m_running{false} {
    m_averageResponseTracker.first = TimeUnit{0};
    m_maxResponseTime = TimeUnit::min();
    m_minResponseTime = TimeUnit::max();
  }

  auto start() -> void {
    if (!m_running) {
      m_startTime = Clock::now();
      m_running = true;
    } else {
      MLoge("CPSS", "Attempted to start timer, whilst active!");
    }
  }

  auto stop(bool success) -> void {
    if (m_running) {
      m_endTime = Clock::now();

      if (success) {
        updateResponseTime(duration());
      }

      m_running = false;
    } else {
      MLoge("CPSS", "Attempted to stop timer, whilst inactive!");
    }
  }

  auto update(Timer &&t) -> void { updateResponseTime(t.duration()); }

  auto duration() -> TimeUnit {
    MLogt("CPSS",
          "Response Time: " +
              std::to_string(
                  duration_cast<TimeUnit>(m_endTime - m_startTime).count()));
    return duration_cast<TimeUnit>(m_endTime - m_startTime);
  }

  auto averageResponseTime() -> TimeUnit {
    if (m_averageResponseTracker.second <= 0) {
      return TimeUnit{m_averageResponseTracker.first.count()};
    }
    auto average =
        duration_cast<TimeUnit>(m_averageResponseTracker.first).count() /
        m_averageResponseTracker.second;

    return TimeUnit{average};
  }

  auto maxResponseTime() -> TimeUnit { return m_maxResponseTime; }

  auto minResponseTime() -> TimeUnit { return m_minResponseTime; }

private:
  auto updateResponseTime(typename Clock::duration &&duration) -> void {
    auto timeUnitDuration = duration_cast<TimeUnit>(duration);

    m_averageResponseTracker.first += timeUnitDuration;
    m_averageResponseTracker.second++;

    if (timeUnitDuration > m_maxResponseTime) {
      m_maxResponseTime = timeUnitDuration;
    }

    if (timeUnitDuration < m_minResponseTime) {
      m_minResponseTime = timeUnitDuration;
    }
    std::ostringstream oss;
    oss << "Current Average Time: " << averageResponseTime().count() << " | "
        << "Current Max Time: " << maxResponseTime().count() << " | "
        << "Current Min Time: " << minResponseTime().count();
    MLogd("CPSS", oss.str());
  }

  typename Clock::time_point m_startTime;
  typename Clock::time_point m_endTime;

  std::pair<TimeUnit, uint64_t> m_averageResponseTracker;
  TimeUnit m_maxResponseTime;
  TimeUnit m_minResponseTime;

  bool m_running;
};

} // namespace SS

#endif /* Timer_hpp */
