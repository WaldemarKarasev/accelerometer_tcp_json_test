#pragma once

#include "server_state.hpp"
#include <memory>

#include <boost/asio.hpp>

namespace accel::server {

class ServerState;

class TcpServer 
{
public:
    TcpServer(boost::asio::io_context& io_context, unsigned short port);

    bool Start();

private:
    void AcceptNext();

private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    unsigned short port_{};

    std::shared_ptr<ServerState> server_state_;
};

} // namespace accel::server