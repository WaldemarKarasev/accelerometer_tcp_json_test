#include "server/server_state.hpp"

#include "server/client_session.hpp"

namespace accel::server {

void ServerState::SetNodeA(const std::shared_ptr<ClientSession>& session)
{
    std::lock_guard<std::mutex> lock(mutex_);
    node_a_ = session;
}

void ServerState::SetNodeB(const std::shared_ptr<ClientSession>& session)
{
    std::lock_guard<std::mutex> lock(mutex_);
    node_b_ = session;
}

std::shared_ptr<ClientSession> ServerState::GetNodeA() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return node_a_.lock();
}

std::shared_ptr<ClientSession> ServerState::GetNodeB() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return node_b_.lock();
}

bool ServerState::HasNodeA() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !node_a_.expired();
}

bool ServerState::HasNodeB() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !node_b_.expired();
}

void ServerState::RemoveSession(const ClientSession* session)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const auto node_a = node_a_.lock();

    if (node_a && node_a.get() == session)
    {
        node_a_.reset();
    }

    const auto node_b = node_b_.lock();

    if (node_b && node_b.get() == session)
    {
        node_b_.reset();
    }
}

} // namespace accel::server