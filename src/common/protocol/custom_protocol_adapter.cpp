#include "custom_protocol_adapter.h"
#include "modbus_tcp_adapter.h"
#include "frame_codec.h"
#include "message_codec.h"

namespace motor::protocol {

CustomProtocolAdapter::CustomProtocolAdapter() = default;

QByteArray CustomProtocolAdapter::encodeFrame(const Frame& frame)
{
    return protocol::encodeFrame(frame);
}

std::optional<Frame> CustomProtocolAdapter::decodeFrame(QByteArrayView& buffer)
{
    auto batch = _decoder.append(buffer);
    if (batch.frames.isEmpty()) return std::nullopt;
    return batch.frames.front();
}

QByteArray CustomProtocolAdapter::encodeConnectRequest(const DeviceId& deviceId)
{
    return protocol::encodeConnectRequest(deviceId, 1);
}

QByteArray CustomProtocolAdapter::encodeHeartbeat()
{
    return {};
}

QByteArray CustomProtocolAdapter::encodeTelemetry(const Telemetry& telemetry)
{
    return protocol::encodeTelemetry(telemetry);
}

QByteArray CustomProtocolAdapter::encodeCommand(const CommandRequest& request)
{
    return protocol::encodeCommand(request);
}

QByteArray CustomProtocolAdapter::encodeCommandResult(const CommandResult& result)
{
    return protocol::encodeCommandResult(result);
}

std::optional<DeviceId> CustomProtocolAdapter::decodeConnectRequest(const QByteArray& payload)
{
    return protocol::decodeConnectRequest(payload);
}

std::optional<Telemetry> CustomProtocolAdapter::decodeTelemetry(const QByteArray& payload)
{
    return protocol::decodeTelemetry(payload);
}

std::optional<CommandRequest> CustomProtocolAdapter::decodeCommand(const QByteArray& payload)
{
    return protocol::decodeCommand(payload);
}

std::optional<CommandResult> CustomProtocolAdapter::decodeCommandResult(const QByteArray& payload)
{
    return protocol::decodeCommandResult(payload);
}

ProtocolAdapterPtr createAdapter(ProtocolType type)
{
    switch (type) {
    case ProtocolType::Custom:
        return std::make_unique<CustomProtocolAdapter>();
    case ProtocolType::ModbusTCP:
        return std::make_unique<ModbusTcpAdapter>();
    case ProtocolType::ModbusRTU:
        return nullptr;
    }
    return nullptr;
}

}