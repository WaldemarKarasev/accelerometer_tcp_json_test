#include "common/tcp_client.hpp"

namespace accel::net {

using boost::asio::ip::tcp;

TcpClient::TcpClient()
    : resolver_(io_context_)
    , socket_(io_context_) {}

bool TcpClient::Connect(const std::string& host, unsigned short port, std::string& error_message)
{
    boost::system::error_code error;

    const auto endpoints = resolver_.resolve(host, std::to_string(port), error);

    if (error) 
    {
        error_message = "resolve failed: " + error.message();
        return false;
    }

    boost::asio::connect(socket_, endpoints, error);

    if (error) 
    {
        error_message = "connect failed: " + error.message();
        return false;
    }

    return true;
}

bool TcpClient::WriteLine(const std::string& line, std::string& error_message) 
{
    if (!IsConnected()) 
    {
        error_message = "socket is not connected";
        return false;
    }

    boost::system::error_code error;

    std::string message = line;

    if (message.empty() || message.back() != '\n') 
    {
        message.push_back('\n');
    }

    boost::asio::write(socket_, boost::asio::buffer(message), error);

    if (error) 
    {
        error_message = "write failed: " + error.message();
        return false;
    }

    return true;
}

bool TcpClient::WriteJson(const nlohmann::json& json, std::string& error_message)
{
    return WriteLine(json.dump(), error_message);
}

bool TcpClient::ReadLine(std::string& line, std::string& error_message)
{
    line.clear();

    if (!IsConnected())
    {
        error_message = "socker is not connected";
        return false;
    }

    boost::system::error_code error;

    boost::asio::read_until(socket_, input_buf_, '\n', error);

    if (error)
    {
        error_message = "read failed: " + error.message();
        return false;
    }

    std::istream input(&input_buf_);
    std::getline(input, line);

    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }

    return true;
}

void TcpClient::Close() 
{
    boost::system::error_code ignored_error;

    if (socket_.is_open()) 
    {
        socket_.shutdown(tcp::socket::shutdown_both, ignored_error);
        socket_.close(ignored_error);
    }
}

bool TcpClient::IsConnected() const 
{
    return socket_.is_open();
}

} // namespace accel::net