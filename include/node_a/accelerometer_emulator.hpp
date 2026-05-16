#pragma once

#include <cstdint>

#include "common/accel_packet.hpp"

namespace accel::node_a {

class AccelerometerEmulator
{
public:
    AccelerometerEmulator() = default;

    AccelPacket NextPacket();

private:
    uint64_t sample_index_{0};
};
    
} // namespace accel::node_a
