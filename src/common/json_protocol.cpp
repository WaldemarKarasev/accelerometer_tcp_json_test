#include "common/json_protocol.hpp"

#include <limits>

namespace accel::protocol
{

namespace detail
{

    Status validateObject(const nlohmann::json &json)
    {
        if (!json.is_object())
        {
            return Status::failure("JSON message must be an object");
        }

        return Status::success();
    }

    Status requireField(const nlohmann::json &json, std::string_view field_name)
    {
        const auto status = validateObject(json);

        if (!status)
        {
            return status;
        }

        const auto field = std::string(field_name);

        if (!json.contains(field))
        {
            return Status::failure("JSON message does not contain required field: " + field);
        }

        return Status::success();
    }

    Result<std::int64_t> getInt64Field(const nlohmann::json &json, std::string_view field_name)
    {
        const auto status = requireField(json, field_name);

        if (!status)
        {
            return Result<std::int64_t>::failure(status.error);
        }

        const auto field = std::string(field_name);
        const auto &value = json[field];

        if (value.is_number_integer())
        {
            return Result<std::int64_t>::success(value.get<std::int64_t>());
        }

        if (value.is_number_unsigned())
        {
            const auto unsigned_value = value.get<std::uint64_t>();

            if (unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            {
                return Result<std::int64_t>::failure("JSON field is too large for int64_t: " + field);
            }

            return Result<std::int64_t>::success(static_cast<std::int64_t>(unsigned_value));
        }

        return Result<std::int64_t>::failure("JSON field must be integer: " + field);
    }

    Result<double> getDoubleField(const nlohmann::json &json, std::string_view fieldName)
    {
        const auto status = requireField(json, fieldName);

        if (!status)
        {
            return Result<double>::failure(status.error);
        }

        const auto field = std::string(fieldName);
        const auto &value = json[field];

        if (!value.is_number())
        {
            return Result<double>::failure("JSON field must be number: " + field);
        }

        return Result<double>::success(value.get<double>());
    }

    Result<std::string> getStringField(const nlohmann::json &json, std::string_view fieldName)
    {
        const auto status = requireField(json, fieldName);

        if (!status)
        {
            return Result<std::string>::failure(status.error);
        }

        const auto field = std::string(fieldName);
        const auto &value = json[field];

        if (!value.is_string())
        {
            return Result<std::string>::failure("JSON field must be string: " + field);
        }

        return Result<std::string>::success(value.get<std::string>());
    }

} // namespace deatail

nlohmann::json MakeHelloMessage(std::string_view role)
{
    return nlohmann::json{
        {"version", k_protocol_version},
        {"type", std::string(k_type_hello)},
        {"role", std::string(role)}};
}

nlohmann::json ToJson(const AccelPacket &packet)
{
    return nlohmann::json{
        {"version", k_protocol_version},
        {"type", std::string(k_type_accel)},
        {"timestamp", packet.timestamp},
        {"x", packet.x},
        {"y", packet.y},
        {"z", packet.z}};
}

nlohmann::json ToJson(const AccelModule &module)
{
    return nlohmann::json{
        {"version", k_protocol_version},
        {"type", std::string(k_type_module)},
        {"timestamp", module.timestamp},
        {"module", module.module}};
}

Result<nlohmann::json> ParseJsonLine(const std::string &line)
{
    auto json = nlohmann::json::parse(line, nullptr, false);

    if (json.is_discarded())
    {
        return Result<nlohmann::json>::failure("Invalid JSON line");
    }

    return Result<nlohmann::json>::success(std::move(json));
}

Result<AccelPacket> ParseAccelPacket(const nlohmann::json &json)
{
    const auto version_status = ValidateProtocolVersion(json);

    if (!version_status)
    {
        return Result<AccelPacket>::failure(version_status.error);
    }

    const auto type_status = ValidateMessageType(json, k_type_accel);

    if (!type_status)
    {
        return Result<AccelPacket>::failure(type_status.error);
    }

    const auto timestamp = detail::getInt64Field(json, "timestamp");

    if (!timestamp)
    {
        return Result<AccelPacket>::failure(timestamp.error);
    }

    const auto x = detail::getDoubleField(json, "x");

    if (!x)
    {
        return Result<AccelPacket>::failure(x.error);
    }

    const auto y = detail::getDoubleField(json, "y");

    if (!y)
    {
        return Result<AccelPacket>::failure(y.error);
    }

    const auto z = detail::getDoubleField(json, "z");

    if (!z)
    {
        return Result<AccelPacket>::failure(z.error);
    }

    AccelPacket packet;
    packet.timestamp = timestamp.value;
    packet.x = x.value;
    packet.y = y.value;
    packet.z = z.value;

    return Result<AccelPacket>::success(packet);
}

Result<AccelModule> ParseAccelModule(const nlohmann::json &json)
{
    const auto version_status = ValidateProtocolVersion(json);

    if (!version_status)
    {
        return Result<AccelModule>::failure(version_status.error);
    }

    const auto type_status = ValidateMessageType(json, k_type_module);

    if (!type_status)
    {
        return Result<AccelModule>::failure(type_status.error);
    }

    const auto timestamp = detail::getInt64Field(json, "timestamp");

    if (!timestamp)
    {
        return Result<AccelModule>::failure(timestamp.error);
    }

    const auto module = detail::getDoubleField(json, "module");

    if (!module)
    {
        return Result<AccelModule>::failure(module.error);
    }

    AccelModule result;
    result.timestamp = timestamp.value;
    result.module = module.value;

    return Result<AccelModule>::success(result);
}

Result<std::string> GetMessageType(const nlohmann::json &json)
{
    return detail::getStringField(json, "type");
}

Result<std::string> GetClientRole(const nlohmann::json &json)
{
    const auto version_status = ValidateProtocolVersion(json);

    if (!version_status)
    {
        return Result<std::string>::failure(version_status.error);
    }

    const auto type_status = ValidateMessageType(json, k_type_hello);

    if (!type_status)
    {
        return Result<std::string>::failure(type_status.error);
    }

    const auto role = detail::getStringField(json, "role");

    if (!role)
    {
        return Result<std::string>::failure(role.error);
    }

    if (role.value != k_role_A && role.value != k_role_B)
    {
        return Result<std::string>::failure("Unsupported client role: " + role.value);
    }

    return Result<std::string>::success(role.value);
}

Status ValidateProtocolVersion(const nlohmann::json &json)
{
    const auto status = detail::requireField(json, "version");

    if (!status)
    {
        return status;
    }

    const auto &version = json["version"];

    if (!version.is_number_integer() && !version.is_number_unsigned())
    {
        return Status::failure("JSON field version must be integer");
    }

    int receivedVersion{};

    if (version.is_number_integer())
    {
        receivedVersion = version.get<int>();
    }
    else
    {
        const auto unsignedVersion = version.get<unsigned int>();
        receivedVersion = static_cast<int>(unsignedVersion);
    }

    if (receivedVersion != k_protocol_version)
    {
        return Status::failure("Unsupported protocol version: " + std::to_string(receivedVersion));
    }

    return Status::success();
}

Status ValidateMessageType(const nlohmann::json &json, std::string_view expected_type)
{
    const auto actual_type = GetMessageType(json);

    if (!actual_type)
    {
        return Status::failure(actual_type.error);
    }

    if (actual_type.value != expected_type)
    {
        return Status::failure("Unexpected message type. Expected: " +
                                std::string(expected_type) +
                                ", actual: " +
                                actual_type.value);
    }

    return Status::success();
}

} // namespace accel::protocol