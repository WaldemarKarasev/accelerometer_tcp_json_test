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

bool ClientSession::SendJson(const nlohmann::json& json)
{
    if (closed_ || !socket_.is_open())
    {
        std::cerr << "[server] send failed: socket is not open\n";
        return false;
    }

    std::string message = json.dump();
    message.push_back('\n');

    const auto write_in_progress = !write_queue_.empty();

    write_queue_.push_back(std::move(message));

    if (!write_in_progress)
    {
        WriteNextMessage();
    }

    return true;
}


void ClientSession::ReadNextLine() 
{
    if (closed_)
    {
        return;
    }
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

                Close();
                return;
            }

            std::istream input(&buffer_);
            std::string line;
            std::getline(input, line);

            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            HandleLine(line);

            if (!closed_)
            {
                ReadNextLine();
            }
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

    // std::cout << "[server] raw message: "
    //           << line
    //           << '\n';

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

    // std::cout << "[server] message type: "
    //           << type_result.value
    //           << '\n';

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

        RegisterRole(role_result.value);
        return;
    }

    if (type_result.value == accel::protocol::k_type_accel)
    {
        HandleAccelPacket(json_result.value);
        return;
    }

    if (type_result.value == accel::protocol::k_type_module)
    {
        HandleModulePacket(json_result.value);
        return;
    }

    std::cerr << "[server] unsupported message type: " << type_result.value << "\n";
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
        if (!server_state_->SetNodeA(shared_from_this()))
        {
            std::cerr << "[server] Node A is already connected\n";
            return; 
        }

        role_ = role;
        
        std::cout << "[server] Node A registered\n";
        return;
    }

    if (role == accel::protocol::k_role_B)
    {
        if (!server_state_->SetNodeB(shared_from_this()))
        {
            std::cerr << "[server] Node B is already connected\n";
            return;
        }

        role_ = role;
        
        std::cout << "[server] Node B registered\n";
        return;
    }

    std::cerr << "[server] unsupported client role: " << role << "\n";
}

void ClientSession::HandleAccelPacket(const nlohmann::json& json)
{
    if (role_.empty())
    {
        std::cerr << "[server] accel packet rejected: client is not registered\n";
        return;
    }

    if (role_ != accel::protocol::k_role_A)
    {
        std::cerr << "[server] accel packet rejected: Only Node A can send accel packets\n";
        return;
    }

    const auto packet_result = accel::protocol::ParseAccelPacket(json);

    if (!packet_result)
    {
        std::cerr << "[server] invalid accel packet: " << packet_result.error << std::endl;
        return;
    }

    const auto& packet = packet_result.value;

    ++received_accel_packets_count_;

    if (has_last_accel_packer_ && AccelPacket::IsDuplicate(packet, last_accel_packet_))
    {
        ++dropped_accel_packets_count_;

        if (dropped_accel_packets_count_ % 50 == 0)
        {
            std::cout << "[server] dropped duplicate accel packets: " << dropped_accel_packets_count_ << std::endl;
        }

        return;
    }

    last_accel_packet_ = packet;
    has_last_accel_packer_ = true;
    
    ++accepted_accel_packets_count_;

    ForwardAccelPacketToNodeB(packet);

    if (accepted_accel_packets_count_ % 50 == 0)
    {
        std::cout << "[server] accepted accel packets: " << accepted_accel_packets_count_
            << ", received: " << received_accel_packets_count_
            << ", dropped: " << dropped_accel_packets_count_
            << ", last packet: timestamp=" << packet.timestamp
            << ", x=" << packet.x
            << ", y=" << packet.y
            << ", z=" << packet.z << std::endl;
    }

}

void ClientSession::HandleModulePacket(const nlohmann::json& json)
{
    if (role_.empty())
    {
        std::cerr << "[server] accel module rejected: client is not registered\n";
        return;
    }

    if (role_ != accel::protocol::k_role_B)
    {
        std::cerr << "[server] accel module rejected: only Node B can send accel modules\n";
        return;
    }

    const auto module_result = accel::protocol::ParseAccelModule(json);

    if (!module_result)
    {
        std::cerr << "[server] invalid accel module: " << module_result.error << std::endl;
        return;
    }

    const auto& module = module_result.value;

    ++received_modules_count_;

    ForwardAccelModuleToNodeA(module);

    if (received_modules_count_ % 50 == 0)
    {
        std::cout << "[server] received accel modules: " << received_modules_count_ 
                << ", last module: timestamp=" << module.timestamp
                << ", module=" << module.module << std::endl;
    }
}

void ClientSession::ForwardAccelPacketToNodeB(const AccelPacket& packet)
{
    if (!server_state_)
    {
        std::cerr << "[server] failed to forward accel packet: server state is not available" << std::endl;
        return;
    }

    const auto node_b = server_state_->GetNodeB();

    if (!node_b)
    {
        if (accepted_accel_packets_count_ % 50 == 0)
        {
            std::cerr << "[server] failed to forward accel packet: Node B is not connected" << std::endl;
        }

        return;
    }

    const auto packet_json = accel::protocol::ToJson(packet);

    if (!node_b->SendJson(packet_json))
    {
        std::cerr << "[server] failed to forward accel packet to Node B" << std::endl;
        return;
    }

    ++forwared_accel_packets_count_;
}


void ClientSession::ForwardAccelModuleToNodeA(const AccelModule& module)
{
    if (!server_state_)
    {
        std::cerr << "[server] failed to forward accel module: server state is not availavle" << std::endl;
        return;
    }

    const auto node_a = server_state_->GetNodeA();

    if (!node_a)
    {
        if (received_modules_count_ % 50 == 0)
        {
            std::cerr << "[server] failed to forward accel module: Node A is not connected" << std::endl;
        }
        return;
    }

    const auto module_json = accel::protocol::ToJson(module);

    if (!node_a->SendJson(module_json))
    {
        std::cerr << "[server] failed to forward accel module to Node A\n";
        return;
    }

    ++forwared_modules_count_;
}


void ClientSession::WriteNextMessage()
{
    if (closed_ || write_queue_.empty())
    {
        return;
    }

    auto self = shared_from_this();

    boost::asio::async_write(socket_
                , boost::asio::buffer(write_queue_.front())
                , [this, self](const boost::system::error_code& error, size_t) {
                    if (error)
                    {
                        std::cerr << "[server] write error: "
                                << error.message() << "\n";

                        write_queue_.clear();
                        Close();
                        return;
                    }

                    write_queue_.pop_front();
                    
                    if (!closed_ && !write_queue_.empty())
                    {
                        WriteNextMessage();
                    }
                });               
}

void ClientSession::Close()
{
    if (closed_)
    {
        return;
    }

    closed_ = true;

    if (!role_.empty())
    {
        std::cout << "[server] Node " << role_ << " disconnected\n";
    }

    if (server_state_)
    {
        server_state_->RemoveSession(this);
    }

    boost::system::error_code ignored_error;

    if (socket_.is_open())
    {
        socket_.shutdown(tcp::socket::shutdown_both, ignored_error);
        socket_.close(ignored_error);
    }
}

} // namespace accel::server