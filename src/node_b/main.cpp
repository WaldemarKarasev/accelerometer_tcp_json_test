#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "common/accel_module.hpp"
#include "common/json_protocol.hpp"
#include "common/tcp_client.hpp"

int main() {
    constexpr const char* server_host = "127.0.0.1";
    constexpr unsigned short server_port = 5555;

    std::cout << "[node_b] started\n";

    accel::net::TcpClient client;

    std::string error_message;

    if (!client.Connect(server_host, server_port, error_message)) {
        std::cerr << "[node_b] connection error: "
                  << error_message
                  << '\n';

        return EXIT_FAILURE;
    }

    std::cout << "[node_b] connected to "
              << server_host
              << ":"
              << server_port
              << '\n';

    const auto hello_message = accel::protocol::MakeHelloMessage(accel::protocol::k_role_B);

    if (!client.WriteJson(hello_message, error_message)) 
    {
        std::cerr << "[node_b] send error: "
                  << error_message
                  << '\n';

        return EXIT_FAILURE;
    }

    std::cout << "[node_b] hello message sent\n";
    std::cout << "[node_b] waiting for accel packets\n";

    uint64_t received_packets_count = 0;
    uint64_t send_modules_count = 0;

    while (true)
    {
        std::string line;

        if (!client.ReadLine(line, error_message))
        {
            std::cerr << "[node_b] read error: " << error_message << std::endl;
            client.Close();
            return EXIT_FAILURE;
        }

        const auto json_result = accel::protocol::ParseJsonLine(line);

        if (!json_result)
        {
            std::cerr << "[node_b] invalid json: " << json_result.error << std::endl;
            continue;
        }

        const auto type_result = accel::protocol::GetMessageType(json_result.value);

        if (!type_result)
        {
            std::cout << "[node_b] protocol error: " << type_result.error << std::endl;
            continue;
        }

        if (type_result.value != accel::protocol::k_type_accel)
        {
            std::cerr << "[node_b] unsupported message type: " << type_result.value << std::endl;
            continue;
        }

        const auto packet_result = accel::protocol::ParseAccelPacket(json_result.value);

        if (!packet_result)
        {
            std::cerr << "[node_b] invalid accel packet: " << packet_result.error << std::endl;
            continue;
        }

        const auto& packet = packet_result.value;

        const auto module = std::sqrt(packet.x * packet.x 
                                    + packet.y * packet.y
                                    + packet.z * packet.z);
        
        accel::AccelModule accel_module;
        accel_module.timestamp = packet.timestamp;
        accel_module.module = module;

        const auto module_json = accel::protocol::ToJson(accel_module);

        if (!client.WriteJson(module_json, error_message))
        {
            std::cerr << "[node_b] send module error: " << error_message << std::endl;

            client.Close();
            return EXIT_FAILURE;
        }

        ++received_packets_count;
        ++send_modules_count;

        if (received_packets_count % 50 == 0)
        {
            std::cout << "[node_b] received accel packets:" << received_packets_count
                    << ", send modules: " << send_modules_count
                    << ", last packet: timestamp=" << packet.timestamp
                    << ", x=" << packet.x
                    << ", y=" << packet.y
                    << ", z=" << packet.z 
                    << ", module=" << module << std::endl;
        }
    }

    return EXIT_SUCCESS;
}