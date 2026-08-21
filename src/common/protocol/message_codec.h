#pragma once

#include <optional>
#include "frame.h"
#include "domain/device_types.h"
#include "domain/command_types.h"

namespace motor::protocol {

QByteArray encodeConnectRequest(const DeviceId& deviceId, quint8 requestedVersion);
std::optional<DeviceId> decodeConnectRequest(QByteArrayView payload);

QByteArray encodeDeviceRegister(const DeviceId& deviceId);
std::optional<DeviceId> decodeDeviceRegister(QByteArrayView payload);

QByteArray encodeTelemetry(const Telemetry& telemetry);
std::optional<Telemetry> decodeTelemetry(QByteArrayView payload);

QByteArray encodeCommand(const CommandRequest& command);
std::optional<CommandRequest> decodeCommand(QByteArrayView payload);

QByteArray encodeCommandResult(const CommandResult& result);
std::optional<CommandResult> decodeCommandResult(QByteArrayView payload);

}
