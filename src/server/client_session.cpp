#include "server/client_session.hpp"

#include <iostream>
#include <istream>
#include <string>

#include "common/json_protocol.hpp"
#include "server/server_state.hpp"

namespace accel::server {

using boost::asio::ip::tcp;

ClientSession::ClientSession(tcp::socket socket, std::shared_ptr<ServerState> server_state)
    : socket_(std::move(socket)) 
    , server_state_{std::move(server_state)} {}

void ClientSession::Start() 
{
    ReadNextLine();
}

void ClientSession::ReadNextLine() 
{
    auto self = shared_from_this();

    boost::asio::async_read_until(socket_
        , buffer_
        , '\n'
        , [this, self](const boost::system::error_code& error, std::size_t) {
            if (error) 
            {
                if (error == boost::asio::error::eof) 
                {
                    std::cout << "[server] client disconnected\n";
                } 
                else 
                {
                    std::cerr << "[server] read error: "
                              << error.message()
                              << '\n';
                }

                return;
            }

            std::istream input(&buffer_);
            std::string line;
            std::getline(input, line);

            HandleLine(line);

            ReadNextLine();
        }
    );
}

void ClientSession::HandleLine(const std::string& line) 
{
    if (line.empty()) 
    {
        std::cerr << "[server] received empty line\n";
        return;
    }

    std::cout << "[server] raw message: "
              << line
              << '\n';

    const auto json_result = accel::protocol::ParseJsonLine(line);

    if (!json_result) 
    {
        std::cerr << "[server] invalid JSON: "
                  << json_result.error
                  << '\n';
        return;
    }

    const auto type_result = accel::protocol::GetMessageType(json_result.value);

    if (!type_result) {
        std::cerr << "[server] protocol error: "
                  << type_result.error
                  << '\n';
        return;
    }

    std::cout << "[server] message type: "
              << type_result.value
              << '\n';

    if (type_result.value == accel::protocol::k_type_hello) 
    {
        const auto role_result = accel::protocol::GetClientRole(json_result.value);

        if (!role_result) {
            std::cerr << "[server] invalid hello message: "
                      << role_result.error
                      << '\n';
            return;
        }

        std::cout << "[server] client role: "
                  << role_result.value
                  << '\n';
    }
}

void ClientSession::RegisterRole(const std::string& role)
{
    if (!server_state_)
    {
        std::cerr << "[server] server state is not available!\n";
        return;
    }

    if (!role_.empty())
    {
        if (role_ == role)
        {
            std::cout << "[server] client already registered as node " << role_ << '\n';
        }
        else
        {
            std::cerr << "[server] client is already registered as node " << role_ 
                    << ", requested node " << role << '\n';
        }

        return;
    }

    if (role == accel::protocol::k_role_A)
    {
        server_state_->SetNodeA(shared_from_this());
        role_ = role;
        
        std::cout << "[server] Node A registered\n";
        return;
    }

    if (role == accel::protocol::k_role_B)
    {
        server_state_->SetNodeB(shared_from_this());
        role_ = role;
        
        std::cout << "[server] Node B registered\n";
        return;
    }

    std::cerr << "[server] unsupported client role: " << role << "\n";
}

} // namespace accel::server