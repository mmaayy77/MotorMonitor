#pragma once

#include "protocol_adapter.h"
#include "frame_codec.h"

namespace motor::protocol {

class CustomProtocolAdapter : public IProtocolAdapter {
public:
    CustomProtocolAdapter();

    QByteArray encodeFrame(const Frame& frame) override;
    std::optional<Frame> decodeFrame(QByteArrayView& buffer) override;

    QByteArray encodeConnectRequest(const DeviceId& deviceId) override;
    QByteArray encodeHeartbeat() override;
    QByteArray encodeTelemetry(const Telemetry& telemetry) override;
    QByteArray encodeCommand(const CommandRequest& request) override;
    QByteArray encodeCommandResult(const CommandResult& result) override;

    std::optional<DeviceId> decodeConnectRequest(const QByteArray& payload) override;
    std::optional<Telemetry> decodeTelemetry(const QByteArray& payload) override;
    std::optional<CommandRequest> decodeCommand(const QByteArray& payload) override;
    std::optional<CommandResult> decodeCommandResult(const QByteArray& payload) override;

    ProtocolType type() const override { return ProtocolType::Custom; }

private:
    FrameDecoder _decoder;
};

}