#include <cstdlib>
#include <iostream>
#include <string>

#include "common/json_protocol.hpp"
#include "common/tcp_client.hpp"

int main() 
{
    constexpr const char* serverHost = "127.0.0.1";
    constexpr unsigned short serverPort = 5555;

    std::cout << "[node_a] started\n";

    accel::net::TcpClient client;

    std::string errorMessage;

    if (!client.Connect(serverHost, serverPort, errorMessage)) {
        std::cerr << "[node_a] connection error: "
                  << errorMessage
                  << '\n';

        return EXIT_FAILURE;
    }

    std::cout << "[node_a] connected to "
              << serverHost
              << ":"
              << serverPort
              << '\n';

    const auto helloMessage =
        accel::protocol::MakeHelloMessage(accel::protocol::k_role_A);

    if (!client.WriteJson(helloMessage, errorMessage)) {
        std::cerr << "[node_a] send error: "
                  << errorMessage
                  << '\n';

        return EXIT_FAILURE;
    }

    std::cout << "[node_a] hello message sent\n";

    client.Close();

    return EXIT_SUCCESS;
}