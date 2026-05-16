#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include <boost/asio.hpp>

#include <nlohmann/json.hpp>

#include "common/accel_packet.hpp"
#include "common/accel_module.hpp"

namespace accel::server {

class ServerState;

class ClientSession : public std::enable_shared_from_this<ClientSession> 
{
public:
    explicit ClientSession(boost::asio::ip::tcp::socket socket
                        , std::shared_ptr<ServerState> server_state);

    void Start();

    bool SendJson(const nlohmann::json& json);

    
private:
    void ReadNextLine();
    void HandleLine(const std::string& line);
    void RegisterRole(const std::string& role);

    void HandleAccelPacket(const nlohmann::json& json);
    void HandleModulePacket(const nlohmann::json& json);

    void ForwardAccelPacketToNodeB(const AccelPacket& packet);
    void ForwardAccelModuleToNodeA(const AccelModule& module);


    void WriteNextMessage();
    void Close();
    
private:
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf buffer_;

    std::shared_ptr<ServerState> server_state_;
    std::string role_;

    std::deque<std::string> write_queue_;

    bool closed_{false};

    uint64_t received_accel_packets_count_{0};
    uint64_t accepted_accel_packets_count_{0};
    uint64_t dropped_accel_packets_count_{0};
    uint64_t forwared_accel_packets_count_{0};

    uint64_t received_modules_count_{0};
    uint64_t forwared_modules_count_{0};

    bool has_last_accel_packer_{false};
    AccelPacket last_accel_packet_{};

};

} // namespace accel::server