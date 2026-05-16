#pragma once

#include <cmath>
#include <cstdint>

namespace accel {

struct AccelPacket
{
public:
    int64_t timestamp{};
    double x{};
    double y{};
    double z{};

public:
    static bool IsDuplicate(const AccelPacket& lhs, const AccelPacket& rhs)
    {
        return RoundAxisValue(lhs.x) == RoundAxisValue(rhs.x)
            && RoundAxisValue(lhs.y) == RoundAxisValue(rhs.y)
            && RoundAxisValue(lhs.z) == RoundAxisValue(rhs.z);
    }

private:
    static int64_t RoundAxisValue(double value)
    {
        return static_cast<int64_t>(std::llround(value * 1000.0));
    }

};

} // namespace accel