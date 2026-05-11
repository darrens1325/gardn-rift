#pragma once

// Bundle "unity" prologue.
//
// In the BUNDLE build, each .cc from Client/ Server/ Shared/ is compiled
// inside a per-side C++ namespace (gardn::client or gardn::server) so that
// duplicate definitions and the CLIENTSIDE/SERVERSIDE layout split don't
// collide at link time. This file collects every <system> header used
// anywhere in the project tree so they are pulled in at GLOBAL scope
// before the namespace opens — otherwise an in-namespace `#include <vector>`
// would put std::vector under `gardn::client::std::vector` and break ODR
// across files that share template instantiations.

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <emscripten.h>
#include <format>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdint.h>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <zlib.h>
