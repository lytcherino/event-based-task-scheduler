#ifndef EventHandler_hpp
#define EventHandler_hpp

#include <algorithm>
#include <iostream>
#include <stdio.h>
#include <typeindex>
#include <unordered_map>

#include "Event.hpp"
#include "Observer.hpp"

namespace SS {

template <typename ResponseType>
class EventHandler : public Observer<ResponseType> {

private:
  std::unordered_map<std::string, std::function<void(Subject<ResponseType> &,
                                                     const ResponseType &)>>
      handlers;

  template <typename Event>
  void registerEventHandler(
      std::function<void(Subject<ResponseType> &, const ResponseType &)>
          handler) {
    handlers[std::string(typeid(ResponseType).name()) +
             std::string(typeid(Event).name())] = handler;
  }

public:
  virtual auto notify(Subject<ResponseType> &subject, const ResponseType &type,
                      const Event &event) -> void override {
    auto find = handlers.find(std::string(std::string(typeid(type).name()) +
                                          std::string(typeid(event).name())));

    if (find != handlers.end()) {
      find->second(subject, type);
    }
  }

  virtual auto update(const Subject<ResponseType> &s,
                      const ResponseType &response) -> void = 0;

  template <typename Event> void subscribe() {
    registerEventHandler<Event>(
        [this](const Subject<ResponseType> &s,
               const ResponseType &response) -> void { update(s, response); });
  }
}; // EventHandler class

} // namespace SS

#endif /* EventHandler_hpp */
