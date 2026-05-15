#pragma once

#include <string>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

namespace accel::net {

class TcpClient 
{
public:
    TcpClient();

    bool Connect(const std::string& host
                , unsigned short port
                , std::string& error_message);

    bool WriteLine(const std::string& line
                  , std::string& error_message);

    bool WriteJson(const nlohmann::json& json
                    , std::string& error_message);

    bool ReadLine(std::string& line
                , std::string& error_message);

    void Close();

    bool IsConnected() const;

private:
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf input_buf_;
};

} // namespace accel::net