#ifndef EventObserverInterface_hpp
#define EventObserverInterface_hpp

#include <stdio.h>

#include <CPBase.hpp>

#include "EventHandler.hpp"

namespace SS {

template <typename T, typename E>
class EventObserverInterface : public EventHandler<T> {
public:
  EventObserverInterface() {
    // Ensure the callback is called
    // when the appropriate event occurs
    EventHandler<T>::template subscribe<E>();
  }
  // Callback to be performed
  virtual auto update(const Subject<T> &s, const T &response) -> void = 0;
};

} // namespace SS

#endif /* EventObserverInterface_hpp */
