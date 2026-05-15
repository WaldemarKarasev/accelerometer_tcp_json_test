#include <cstdlib>
#include <iostream>
#include <string>

#include "common/json_protocol.hpp"
#include "common/tcp_client.hpp"

int main() {
    constexpr const char* serverHost = "127.0.0.1";
    constexpr unsigned short serverPort = 5555;

    std::cout << "[node_b] started\n";

    accel::net::TcpClient client;

    std::string errorMessage;

    if (!client.Connect(serverHost, serverPort, errorMessage)) {
        std::cerr << "[node_b] connection error: "
                  << errorMessage
                  << '\n';

        return EXIT_FAILURE;
    }

    std::cout << "[node_b] connected to "
              << serverHost
              << ":"
              << serverPort
              << '\n';

    const auto helloMessage =
        accel::protocol::MakeHelloMessage(accel::protocol::k_role_B);

    if (!client.WriteJson(helloMessage, errorMessage)) 
    {
        std::cerr << "[node_b] send error: "
                  << errorMessage
                  << '\n';

        return EXIT_FAILURE;
    }

    std::cout << "[node_b] hello message sent\n";

    client.Close();

    return EXIT_SUCCESS;
}