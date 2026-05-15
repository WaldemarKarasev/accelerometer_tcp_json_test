#include "server/tcp_server.hpp"

#include <iostream>
#include <memory>

#include "server/client_session.hpp"
#include "server/server_state.hpp"

namespace accel::server {

using boost::asio::ip::tcp;

TcpServer::TcpServer(boost::asio::io_context& io_context, unsigned short port)
    : io_context_(io_context)
    , acceptor_(io_context)
    , port_(port)
    , server_state_{std::make_shared<ServerState>()} {}

bool TcpServer::Start() 
{
    boost::system::error_code error;

    const tcp::endpoint endpoint(tcp::v4(), port_);

    acceptor_.open(endpoint.protocol(), error);
    if (error) 
    {
        std::cerr << "[server] failed to open acceptor: "
                  << error.message()
                  << '\n';
        return false;
    }

    acceptor_.set_option(tcp::acceptor::reuse_address(true), error);
    if (error) 
    {
        std::cerr << "[server] failed to set reuse_address: "
                  << error.message()
                  << '\n';
        return false;
    }

    acceptor_.bind(endpoint, error);
    if (error) 
    {
        std::cerr << "[server] failed to bind port "
                  << port_
                  << ": "
                  << error.message()
                  << '\n';
        return false;
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, error);

    if (error) 
    {
        std::cerr << "[server] failed to listen: "
                  << error.message()
                  << '\n';
        return false;
    }

    std::cout << "[server] listening on 0.0.0.0:"
              << port_
              << '\n';

    AcceptNext();

    return true;
}

void TcpServer::AcceptNext() 
{
    acceptor_.async_accept(
        [this](const boost::system::error_code& error, tcp::socket socket) {
            if (error) {
                std::cerr << "[server] accept error: "
                          << error.message()
                          << '\n';
            } else {
                const auto remote_endpoint = socket.remote_endpoint();

                std::cout << "[server] client connected from "
                          << remote_endpoint.address().to_string()
                          << ":"
                          << remote_endpoint.port()
                          << '\n';

                auto session = std::make_shared<ClientSession>(std::move(socket), server_state_);
                session->Start();
            }

            AcceptNext();
        }
    );
}

} // namespace accel::server