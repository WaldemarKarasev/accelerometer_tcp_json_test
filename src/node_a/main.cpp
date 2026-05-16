#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <filesystem>
#include <fstream>


#include "common/json_protocol.hpp"
#include "common/tcp_client.hpp"
#include "node_a/accelerometer_emulator.hpp"
#include "common/accel_module.hpp"

void ReceiveModules(accel::net::TcpClient& client, std::atomic_bool& is_running)
{
    std::string error_message;
    std::uint64_t received_modules_count = 0;

    std::filesystem::create_directories("accel");
    std::ofstream module_log("accel/module.log", std::ios::app);

    if (!module_log.is_open())
    {
        std::cerr << "[node_a] failed to open accel/module.log" << std::endl;
        is_running = false;
        return;
    }

    std::cout << "[node_a] module log opened: accel/module.log" << std::endl;

    while (is_running)
    {
        std::string line;

        if (!client.ReadLine(line, error_message))
        {
            if (is_running)
            {
                std::cerr << "[node_a] read module error: " << error_message << std::endl;
            }

            is_running = false;
            return;
        }

        const auto json_result = accel::protocol::ParseJsonLine(line);

        if (!json_result)
        {
            std::cerr << "[node_a] invalid json: " << json_result.error << std::endl;
            continue;
        }

        const auto type_result = accel::protocol::GetMessageType(json_result.value);

        if (!type_result)
        {
            std::cerr << "[node_a] protocol error: " << type_result.error << std::endl;
            continue;
        }

        if (type_result.value != accel::protocol::k_type_module)
        {
            std::cerr << "[node_a] unsupported message type: " << type_result.value << std::endl;
            continue;
        }

        const auto module_result = accel::protocol::ParseAccelModule(json_result.value);

        if (!module_result)
        {
            std::cerr << "[node_a] invalid accel module: " << module_result.error << std::endl;
            continue;
        }

        const auto& module = module_result.value;

        // writing to file
        module_log << module.timestamp << " " << module.module << "\n";
        module_log.flush();

        ++received_modules_count;

        if (received_modules_count % 50 == 0)
        {
            std::cout << "[node_a] received accel modules: " << received_modules_count
                      << ", last module: timestamp=" << module.timestamp
                      << ", module=" << module.module << std::endl;
        }
    }
}

int main() 
{
    constexpr const char* server_host = "127.0.0.1";
    constexpr unsigned short server_port = 5555;

    std::cout << "[node_a] started\n";

    accel::net::TcpClient client;

    std::string error_message;

    if (!client.Connect(server_host, server_port, error_message)) {
        std::cerr << "[node_a] connection error: "
                  << error_message
                  << '\n';

        return EXIT_FAILURE;
    }

    std::cout << "[node_a] connected to "
              << server_host
              << ":"
              << server_port
              << '\n';

    const auto hello_message =
        accel::protocol::MakeHelloMessage(accel::protocol::k_role_A);

    if (!client.WriteJson(hello_message, error_message)) {
        std::cerr << "[node_a] send error: "
                  << error_message
                  << '\n';

        return EXIT_FAILURE;
    }

    std::cout << "[node_a] hello message sent\n";

    std::atomic_bool is_runnig{true};

    std::thread receiver_thread([&client, &is_runnig](){
        ReceiveModules(client, is_runnig);
    });

    constexpr auto generation_period = std::chrono::milliseconds(20);

    accel::node_a::AccelerometerEmulator emulator;

    std::uint64_t generated_packets_count = 0;
    auto next_generation_time = std::chrono::steady_clock::now();

    std::cout << "[node_a] accelerometer emulator started, frequency: ~50 Hz\n";

    while (true)
    {
        next_generation_time += generation_period;

        const auto packet = emulator.NextPacket();
        const auto packet_json = accel::protocol::ToJson(packet);

        if (!client.WriteJson(packet_json, error_message))
        {
            std::cerr << "[node_a] send accel packet error: " << error_message << '\n';
            is_runnig = false;
            break;
        }

        ++generated_packets_count;

        if (generated_packets_count % 50 == 0)
        {
            std::cout << "[node_a] sent accel packet: "
                    << packet_json.dump()
                    << '\n';
        }

        std::this_thread::sleep_until(next_generation_time);
    }
    
    client.Close();

    if (receiver_thread.joinable())
    {
        receiver_thread.join();
    }

    return EXIT_SUCCESS;
}