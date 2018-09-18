#ifndef TimedTask_hpp
#define TimedTask_hpp

#include <CPBase.hpp>
#include <chrono>
#include <ctime>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdio.h>

using namespace std::chrono;

namespace SS {

template <typename TimeUnit, typename Clock> class TimedTask {
private:
  std::function<void()> m_callback;
  typename Clock::time_point m_executionTime;
  TimeUnit m_waitTime;

public:
  TimedTask(std::function<void()> &&callback, TimeUnit timeunit)
      : m_executionTime(Clock::now() + timeunit), m_callback(callback),
        m_waitTime(timeunit) {}

  TimedTask(TimedTask &&other) = default;
  TimedTask(const TimedTask &other) = default;
  TimedTask &operator=(TimedTask &&other) = default;
  TimedTask &operator=(const TimedTask &o) = delete;

  TimedTask(std::function<void()> &&callback,
            typename Clock::time_point timepoint)
      : m_executionTime(timepoint), m_callback(callback),
        m_waitTime(duration_cast<TimeUnit>(timepoint - Clock::now())) {

    std::time_t ttp = std::chrono::system_clock::to_time_t(m_executionTime);
    std::time_t now = std::chrono::system_clock::to_time_t(Clock::now());

    std::ostringstream ss;
    ss << "New TimedTask: " << std::ctime(&now)
       << " | Execution: " << std::ctime(&ttp);
    MLogt("CPSS", ss.str());
  }

  typename Clock::time_point executionTime() const { return m_executionTime; }

  std::function<void()> getCallback() const { return m_callback; }

  TimeUnit getWaitTime() const { return m_waitTime; }

  void operator()() const { m_callback(); }
};

template <typename TimeUnit, typename Clock> struct CompareTask {
  bool operator()(const TimedTask<TimeUnit, Clock> &t1,
                  const TimedTask<TimeUnit, Clock> &t2) {
    // Use the greater than operator so the queue is ordered from smallest to
    // largest values
    return t1.executionTime() > t2.executionTime();
  }
};

} // namespace SS

#endif /* TimedTask_hpp */
