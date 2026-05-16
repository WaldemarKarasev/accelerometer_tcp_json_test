#include "node_a/accelerometer_emulator.hpp"

#include <chrono>
#include <cmath>

namespace accel::node_a {

namespace detail {

    constexpr double k_pi = 3.141592;
    constexpr double k_sample_rate_hz = 50.0;
    constexpr double k_gravity = 9.8;

    int64_t CurrentTimestampMs()
    {
        const auto now = std::chrono::system_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        return ms.count();
    }
    
} // namespace detail

    
AccelPacket AccelerometerEmulator::NextPacket()
{
    const auto timestamp = detail::CurrentTimestampMs();
    const auto time_sec = static_cast<double>(sample_index_) / detail::k_sample_rate_hz;

    AccelPacket packet;
    packet.timestamp = timestamp;
    packet.x = 0.4 * std::sin(2.0 * detail::k_pi * 0.7 * time_sec);
    packet.y = detail::k_gravity + 0.2 * std::sin(2.0 * detail::k_pi * 0.3 * time_sec);
    packet.z = 0.1 * std::cos(2.0 * detail::k_pi * 0.5 * time_sec);

    ++sample_index_;

    return packet;
}

} // namespace accel::node_a
