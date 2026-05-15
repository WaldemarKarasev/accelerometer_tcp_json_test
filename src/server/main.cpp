#include <iostream>
#include <boost/asio.hpp>

#include "server/tcp_server.hpp"

int main() {
    constexpr unsigned short port = 5555;

    std::cout << "[server] started\n";

    boost::asio::io_context io;

    accel::server::TcpServer server(io, port);
    
    if (!server.Start())
    {
        std::cerr << "[server] failed to start" << "\n";
        return EXIT_FAILURE;
    }

    io.run();
    return EXIT_SUCCESS;
}