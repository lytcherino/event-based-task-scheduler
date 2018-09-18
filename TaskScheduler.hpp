#ifndef TaskScheduler_hpp
#define TaskScheduler_hpp

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdio.h>
#include <thread>

#include "Constants.hpp"
#include "SubscriptionServiceInterface.hpp"
#include "ThreadPool.hpp"
#include "TimedTask.hpp"

namespace SS {

using namespace std::chrono;

template <typename TimeUnit = milliseconds, typename Clock = system_clock,
          typename PriorityComparator = CompareTask<TimeUnit, Clock>>
class TaskScheduler {
private:
  // Queue structure where tasks to be done at some time in the future
  // are organized from first to be done, to last
  std::priority_queue<TimedTask<TimeUnit, Clock>,
                      std::vector<TimedTask<TimeUnit, Clock>>,
                      PriorityComparator>
      m_queue;

  // Thread pool that executes the tasks
  //
  // The major reason for a thread pool is to avoid the massive overhead of
  // creating a thread in comparison to using an existing thread to perform the
  // callback (which is tens to hundreds of times faster)
  ThreadPool<std::function<void()>> m_threadPool;

  // Status of the queue worker loop
  std::atomic<bool> m_running;

  // If true, the TaskScheduler::run() thread has finished
  // This might occur if the scheduler is paused
  std::atomic<bool> m_threadDone;

  // Pointer to the main worker thread
  std::unique_ptr<std::thread> m_thread;

  // Used to keep track of queue status
  std::condition_variable m_condition;

  // Exclusive access to queue
  std::mutex m_mutex;

public:
  TaskScheduler()
      : m_running{false}, m_threadDone{true},
        m_threadPool(Constants::numberOfInitialWorkers) {
    Logsd("SS") << "ENTRY: " << __FUNCTION__;
    start();
    Logd("SS", std::string("EXIT: ") + std::string(__FUNCTION__));
  }

  ~TaskScheduler() {
    // Set the thread running run() to stop
    m_running = false;

    // Notify all the threads
    m_condition.notify_all();

    // Join all the threads
    if (m_thread &&
        m_thread->joinable()) { // Short-circuit avoids nullptr function call
      m_thread->join();
    }
  }

  template <typename T>
  void scheduleRepeat(std::shared_ptr<SubscriptionServiceInterface<T>> ss) {
    Logd("SS", std::string("ENTRY: ") + std::string(__FUNCTION__));

    auto task = ([this, ss]() {
      try {
        // Attempt to get the expiry time
        // If it fails, retry again later
        Logd("SS", "::scheduleRepeat - will attempt to get ttl");
        auto ttl = ss->timeToLive();
        Logd("SS", "::scheduleRepeat - after attempt to get ttl");
        scheduleEvery(ttl, [ss]() -> bool { return ss->update(); });

        // Possible to catch different exception types to decide when to repeat
        // or not
      } catch (std::exception &e) {
        Logse("SS") << "Failed to add task to scheduler: " << e.what()
                       << ". "
                       << "Attempting to add again in "
                       << duration_cast<milliseconds>(
                              Constants::defaultRetryTimeAdding)
                              .count()
                       << " ms";
        scheduleOnceAt(Constants::defaultRetryTimeAdding + Clock::now(),
                       [this, ss]() { scheduleRepeat(ss); });
      }
    });
    Logd("SS", "::scheduleRepeat() - adding task to thread pool");

    // Task will be executed by a thread in the thread pool
    m_threadPool.addTask(std::move(task));

    Logd("SS", std::string("EXIT: ") + std::string(__FUNCTION__));
  }

  void scheduleOnceAt(const typename Clock::time_point &time,
                      std::function<void()> callback) {
    Logd("SS", std::string("ENTRY: ") + std::string(__FUNCTION__));
    // std::function<void()> threadCallback = [callback]() {
    //    std::thread t(callback);
    //    t.detach();
    //};
    std::unique_lock<std::mutex> lock(m_mutex);
    m_queue.push(TimedTask<TimeUnit, Clock>(std::move(callback), time));
    lock.unlock();
    m_condition.notify_all();
    Logd("SS", std::string("EXIT: ") + std::string(__FUNCTION__));
  }

  void start() {
    Logd("SS", std::string("ENTRY: ") + std::string(__FUNCTION__));
    // If thread is not running and not done, simply return to
    // running mode such that the thread may continue working
    if (!m_running && !m_threadDone) {
      m_running = true;
      m_condition.notify_all();

      // If the thread is not registered as running and the running
      // thread is done, create new thread
    } else if (!m_running && m_threadDone) {
      m_running = true;
      m_threadDone = false;
      if (!m_thread) {
        // Use make_unique if supported, for exception safety
        m_thread = std::unique_ptr<std::thread>(new std::thread([this]() {
          run();
          m_threadDone = true;
        }));
      } else {
        m_thread->join();
        // Use make_unique if supported instead, for exception safety
        m_thread.reset(new std::thread([this]() {
          run();
          m_threadDone = true;
        }));
      }
    }
    Logd("SS", std::string("EXIT: ") + std::string(__FUNCTION__));
  }

  /**
   * Stops any more tasks from executing
   *
   * If a task is currently executing when pause() is executed,
   * the task will finish and then cease to trigger again.
   *
   * However note that a task will still be considered overdue (time is not
   * paused -- of course). If pause is used for a period such that a task misses
   * its execution time. Upon resuming the scheduler, with resume(), these
   * overdue task(s) will be immediately executed
   *
   */
  void pause() {
    if (m_running) {
      m_running = false;
      // NB: The thread pool should not be stopped
      m_condition.notify_all();
    }
  }

  /**
   * Removes all the tasks from the queue
   */
  void clear() {
    pause();
    // No erase mechanism for priority queue, must set to empty queue
    m_queue = {};
    // Also remove the tasks for the thread pool
    m_threadPool.clearTasks();
    // Start scheduler again
    resume();
  }

  /**
   * Resumes a paused state triggered with pause()
   *
   * Note that any overdue tasks will be immediately executed
   * upon resuming.
   */
  void resume() {
    if (!m_running) {
      start(); // Re-start if the working thread finished during the pause
      m_condition.notify_all();
    }
  }

private:
  void scheduleEvery(TimeUnit interval, std::function<bool()> callback) {
    Logd("SS", std::string("ENTRY: ") + std::string(__FUNCTION__));
    scheduleRepeater(interval, Constants::defaultRetryTimeRequest, callback);
    Logd("SS", std::string("EXIT: ") + std::string(__FUNCTION__));
  }

  void schedule(const typename Clock::time_point &time,
                std::function<void()> callback) {
    Logd("SS", std::string("ENTRY: ") + std::string(__FUNCTION__));

    std::unique_lock<std::mutex> lock(m_mutex);
    m_queue.push(TimedTask<TimeUnit, Clock>(std::move(callback), time));
    Logd("SS", "Added to queue (now): " + std::to_string(m_queue.size()));
    lock.unlock();
    m_condition.notify_all();

    Logd("SS", std::string("EXIT: ") + std::string(__FUNCTION__));
  }

  void scheduleRepeater(const TimeUnit &defaultInterval,
                        const TimeUnit &intervalFailure,
                        std::function<bool()> callback) {
    Logd("SS", std::string("ENTRY: ") + std::string(__FUNCTION__));
    scheduleRepeaterHelper(defaultInterval, defaultInterval, intervalFailure,
                           callback, true);
    Logd("SS", std::string("EXIT: ") + std::string(__FUNCTION__));
  }

  void scheduleRepeaterHelper(const TimeUnit &defaultInterval,
                              const TimeUnit &intervalSuccess,
                              const TimeUnit &intervalFailure,
                              std::function<bool()> callback, bool success) {
    Logd("SS", std::string("ENTRY: ") + std::string(__FUNCTION__));

    std::function<void()> repeaterThread = [this, defaultInterval,
                                            intervalFailure, callback]() {
      auto start = Clock::now();
      auto success = callback(); // Perform callback
      auto end = Clock::now();

      auto duration = duration_cast<TimeUnit>(end - start);
      auto intervalSuccess =
          defaultInterval -
          duration; // Schedule less far into the future to
                    // take into account execution time of callback

      // Callback duration is longer than provided ttl!
      if (intervalSuccess.count() <= 0) {
        if (success) {
          Logse("SS") << "Warning: Callback duration, "
                         << duration_cast<milliseconds>(duration).count()
                         << "ms, is longer than the "
                         << "ttl interval, "
                         << duration_cast<milliseconds>(defaultInterval).count()
                         << "ms";
          intervalSuccess = TimeUnit{0}; // Execute as soon as possible
        } else {
          Logse("SS") << "Error: Callback timed out after "
                         << duration_cast<milliseconds>(duration).count()
                         << "ms.";
        }
      }

      //            std::cout << "intervalSuccess=" << intervalSuccess.count()
      //            << "\n"; std::cout << "duration=" << duration.count() <<
      //            "\n"; std::cout << "defaultInterval=" <<
      //            defaultInterval.count() << "\n"; std::cout << "success=" <<
      //            success << "\n";
      scheduleRepeaterHelper(defaultInterval, intervalSuccess, intervalFailure,
                             callback, success);
    };
    Logsd("SS") << "Scheduling with interval="
                   << (success
                           ? duration_cast<TimeUnit>(intervalSuccess).count()
                           : duration_cast<TimeUnit>(intervalFailure).count())
                   << " ms";

    schedule(Clock::now() + (success ? intervalSuccess : intervalFailure),
             std::move(repeaterThread));
    Logd("SS", std::string("EXIT: ") + std::string(__FUNCTION__));
  }

  void run() {

    Logd("SS", std::string("ENTRY: ") + std::string(__FUNCTION__));
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_running) {

      m_condition.wait(lock,
                       [this]() { return !m_queue.empty() || !m_running; });

      auto currentTime = Clock::now();
      while (!m_queue.empty() && m_queue.top().executionTime() <= currentTime) {

        if (!m_queue.empty() &&
            currentTime > m_queue.top().executionTime() + milliseconds{10}) {
          std::ostringstream ss;
          ss << "Warning: Task was due "
             << duration_cast<milliseconds>(currentTime -
                                            m_queue.top().executionTime())
                    .count()
             << " ms ago!";
          Loge("SS", ss.str());
        }

        auto task = std::move(m_queue.top()); // Why not move constructed???
        m_queue.pop();
        Logd("SS",
              "Removed from queue (now): " + std::to_string(m_queue.size()));
        lock.unlock();

        std::time_t now = std::chrono::system_clock::to_time_t(Clock::now());
        Logd("SS", "Executing Task: " + std::string(std::ctime(&now)));

        // Task will be executed by a thread in the thread pool
        //
        // NOTE: If Constants::addWorkersIfNeeded = true, this addTask(...) may
        // hold up this thread for at most Constants::minimumThreadPoolTaskDelay
        // units of time
        //
        // Set Constants::asynchAddTaskToThreadPool = true to avoid this
        // (See Constants.hpp for a longer discussion of the benefits/drawbacks
        // if flexible number of workers is desired)
        m_threadPool.addTask(std::move(task));

        lock.lock();
      }

      if (!m_queue.empty() && m_queue.top().executionTime() > currentTime) {

        // Let thread wait until any of the tasks need to be executed or queue
        // is updated
        Logt("SS",
              "Waiting at most " +
                  std::to_string(
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                          m_queue.top().executionTime() - currentTime)
                          .count()) +
                  " ms");

        // This wait condition allows the queue to update when a new item is
        // added avoiding the problem of sleeping for too long, if a new task
        // with the (now) shortest wait time is added (shorter than the shortest
        // wait time previously in the queue).
        //
        // This is a problem given a frequently occuring task and a very
        // rarely occuring task. The (random) order in which they are added to
        // the queue could cause a very long sleep duration, causing the other
        // to miss its first deadlines.
        auto currentSize = m_queue.size();
        if (m_condition.wait_until(
                lock,
                Clock::now() + (m_queue.top().executionTime() - currentTime),
                [this, currentSize] {
                  return currentSize != m_queue.size() || !m_running;
                })) {
          Logt("SS",
                "Exited wait state early because queue status was updated");
        } else {
          Logt("SS", "Wait state timed out -- waiting time for task at head "
                        "of queue ended");
        }
      }

    } // end of while(running)
    Logd("SS", std::string("EXIT: ") + std::string(__FUNCTION__));
  } // end of run()

}; // class TaskScheduler

} // namespace SS

#endif /* TaskScheduler_hpp */
