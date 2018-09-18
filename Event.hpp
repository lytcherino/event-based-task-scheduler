#ifndef Event_hpp
#define Event_hpp

#include <stdio.h>

namespace SS {

class Event {
public:
  virtual ~Event() = default;
};

class CredentialExpire : public Event {};

} // namespace SS

#endif /* Event_hpp */
