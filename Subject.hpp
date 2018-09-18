#ifndef Subject_hpp
#define Subject_hpp

#include <memory>
#include <stdio.h>
#include <vector>

#include "Observer.hpp"
namespace SS {

template <typename ResponseType> class Subject {
private:
  std::vector<std::shared_ptr<Observer<ResponseType>>> observers;

public:
  template <typename Event>
  auto notifySubscribers(const ResponseType &type) -> void {
    static_assert(std::is_default_constructible<ResponseType>::value,
                  "ResponseType must be a default constructible type");
    static_assert(std::is_default_constructible<Event>::value,
                  "Event must be a default constructible type");
    for (auto o : observers) {
      o->notify(*this, type, Event{});
    }
  }

  auto registerSubscriber(const std::shared_ptr<Observer<ResponseType>> &o)
      -> void {
    if (std::find(std::begin(observers), std::end(observers), o) ==
        std::end(observers)) {
      observers.push_back(o);
    } else {
      // Already a subscriber
    }
  }

  auto unregisterSubscriber(const std::shared_ptr<Observer<ResponseType>> &o)
      -> void {
    observers.erase(std::remove(std::begin(observers), std::end(observers), o),
                    std::end(observers));
  }

  // Returns true on success
  // Returns false on failure
  virtual auto update() -> bool = 0;
};

} // namespace SS

#endif /* Subject_hpp */
