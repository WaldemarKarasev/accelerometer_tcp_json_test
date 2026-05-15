#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "common/accel_packet.hpp"
#include "common/accel_module.hpp"

namespace accel::protocol {

inline constexpr int k_protocol_version = 1;

inline constexpr std::string_view k_type_hello = "hello";
inline constexpr std::string_view k_type_accel = "accel";
inline constexpr std::string_view k_type_module = "module";

inline constexpr std::string_view k_role_A = "A";
inline constexpr std::string_view k_role_B = "B";

struct Status 
{
    bool ok{};
    std::string error;

    explicit operator bool() const noexcept 
    {
        return ok;
    }

    static Status success() 
    {
        return Status{true, ""};
    }

    static Status failure(std::string message) 
    {
        return Status{false, std::move(message)};
    }
};

template <typename T>
struct Result 
{
    bool ok{};
    T value{};
    std::string error;

    explicit operator bool() const noexcept 
    {
        return ok;
    }

    static Result<T> success(T value) 
    {
        return Result<T>{true, std::move(value), ""};
    }

    static Result<T> failure(std::string message) 
    {
        return Result<T>{false, T{}, std::move(message)};
    }
};

nlohmann::json MakeHelloMessage(std::string_view role);

nlohmann::json ToJson(const AccelPacket& packet);
nlohmann::json ToJson(const AccelModule& module);

Result<nlohmann::json> ParseJsonLine(const std::string& line);

Result<AccelPacket> ParseAccelPacket(const nlohmann::json& json);
Result<AccelModule> ParseAccelModule(const nlohmann::json& json);

Result<std::string> GetMessageType(const nlohmann::json& json);
Result<std::string> GetClientRole(const nlohmann::json& json);

Status ValidateProtocolVersion(const nlohmann::json& json);
Status ValidateMessageType(const nlohmann::json& json, std::string_view expected_type);

} // namespace accel::protocol