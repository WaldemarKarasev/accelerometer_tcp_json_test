#pragma once

#include <cstdint>

namespace accel {

struct AccelPacket
{
    int64_t timestamp{};
    double x{};
    double y{};
    double z{};
};

} // namespace accel