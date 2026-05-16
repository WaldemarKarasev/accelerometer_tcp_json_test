#pragma once

#include <memory>
#include <mutex>

namespace accel::server {

class ClientSession;

class ServerState
{
public:
    bool SetNodeA(const std::shared_ptr<ClientSession>& session);
    bool SetNodeB(const std::shared_ptr<ClientSession>& session);

    std::shared_ptr<ClientSession> GetNodeA() const;
    std::shared_ptr<ClientSession> GetNodeB() const;

    bool HasNodeA() const;
    bool HasNodeB() const;

    void RemoveSession(const ClientSession* session);

private:
    mutable std::mutex mutex_;

    std::weak_ptr<ClientSession> node_a_;
    std::weak_ptr<ClientSession> node_b_;
};

} // namespace accel::server