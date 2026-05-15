#pragma once

#include <memory>

#include <boost/asio.hpp>

namespace accel::server {

class ServerState;

class ClientSession : public std::enable_shared_from_this<ClientSession> 
{
public:
    explicit ClientSession(boost::asio::ip::tcp::socket socket
                        , std::shared_ptr<ServerState> server_state);

    void Start();

private:
    void ReadNextLine();
    void HandleLine(const std::string& line);
    void RegisterRole(const std::string& role);

private:
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf buffer_;

    std::shared_ptr<ServerState> server_state_;
    std::string role_;
};

} // namespace accel::server