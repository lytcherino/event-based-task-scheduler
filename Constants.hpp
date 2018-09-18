#ifndef Constants_hpp
#define Constants_hpp

#include <chrono>
#include <stdio.h>
#include <string>

namespace SS {
namespace Constants {

using namespace std::chrono;

// For a value X, 0 <= X <= 1, e.g. 0.52, it represents the ratio between the
// duration after which credentials will be updated, over the total credential
// expiry time.
//
// For 0.52 it means it will be updated after 52 % of the total credential
// expiry time has passed. This value should never be less than 0.5 as there
// would be wasted requests The closer to 0.5 the better, as it provides the
// most amount of time until the credentials expire to retrieve the credentials.
// However, if it is below 0.5 the credentials will expire before they can be
// used, therefore multiple requests would be required.
//
// For example, 0.49 is a terrible value since 1//0.49 = 2, 1 mod 0.49 ~ 0.02
// Therefore 2 requests will be performed, and the last request has only 2 % of
// the total available time to retrieve the credentials, before they expire.
//
// Therefore 0.55 was selected as it is above 0.50 by a large margin (timing
// errors etc.) Yet it provides 45 % of available time to retrieve credentials.
static constexpr double timeToLiveMinRatio{0.55};

} // namespace Constants
} // namespace SS

#endif /* Constants_hpp */
