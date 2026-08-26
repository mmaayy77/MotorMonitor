#pragma once

#include "protocol_adapter.h"

namespace motor::protocol {

class ModbusTcpAdapter : public IProtocolAdapter {
public:
    ModbusTcpAdapter();

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

    ProtocolType type() const override { return ProtocolType::ModbusTCP; }

private:
    quint16 _transactionId{0};

    QByteArray buildMbap(quint8 unitId, const QByteArray& pdu) const;
    QByteArray readHoldingRegisters(quint8 unitId, quint16 startAddr, quint16 count) const;
    QByteArray writeSingleRegister(quint8 unitId, quint16 addr, quint16 value) const;
    QByteArray writeMultipleRegisters(quint8 unitId, quint16 startAddr, const QList<quint16>& values) const;
    QByteArray packFloat32(float value) const;
    float unpackFloat32(const QByteArray& data, int offset) const;
};

}