#pragma once

#include "domain/device_types.h"
#include "domain/command_types.h"
#include "domain/alarm_types.h"
#include "frame.h"
#include <QByteArray>
#include <optional>
#include <memory>

namespace motor::protocol {

enum class ProtocolType {
    Custom,
    ModbusRTU,
    ModbusTCP
};

class IProtocolAdapter {
public:
    virtual ~IProtocolAdapter() = default;

    virtual QByteArray encodeFrame(const Frame& frame) = 0;
    virtual std::optional<Frame> decodeFrame(QByteArrayView& buffer) = 0;

    virtual QByteArray encodeConnectRequest(const DeviceId& deviceId) = 0;
    virtual QByteArray encodeHeartbeat() = 0;
    virtual QByteArray encodeTelemetry(const Telemetry& telemetry) = 0;
    virtual QByteArray encodeCommand(const CommandRequest& request) = 0;
    virtual QByteArray encodeCommandResult(const CommandResult& result) = 0;

    virtual std::optional<DeviceId> decodeConnectRequest(const QByteArray& payload) = 0;
    virtual std::optional<Telemetry> decodeTelemetry(const QByteArray& payload) = 0;
    virtual std::optional<CommandRequest> decodeCommand(const QByteArray& payload) = 0;
    virtual std::optional<CommandResult> decodeCommandResult(const QByteArray& payload) = 0;

    virtual ProtocolType type() const = 0;
};

using ProtocolAdapterPtr = std::unique_ptr<IProtocolAdapter>;

ProtocolAdapterPtr createAdapter(ProtocolType type);

}