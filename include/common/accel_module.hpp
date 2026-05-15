#pragma once

#include <cstdint>

namespace accel {

struct AccelModule 
{
    int64_t timestamp{};
    double module{};
};

} // namespace accel