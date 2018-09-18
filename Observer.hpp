#ifndef Observer_hpp
#define Observer_hpp

#include <stdio.h>

namespace SS {

// Forward declarations
template <typename C> class Subject;
class Event;

template <typename ResponseType> class Observer {
public:
  virtual ~Observer() {}
  virtual auto notify(Subject<ResponseType> &subject, const ResponseType &type,
                      const Event &event) -> void = 0;
};

} // namespace SS

#endif /* Observer_hpp */
