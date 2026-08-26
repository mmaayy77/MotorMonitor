#include "modbus_tcp_adapter.h"
#include <QDebug>

namespace motor::protocol {

ModbusTcpAdapter::ModbusTcpAdapter() = default;

QByteArray ModbusTcpAdapter::buildMbap(quint8 unitId, const QByteArray& pdu) const
{
    quint16 tid = qToBigEndian(++const_cast<ModbusTcpAdapter*>(this)->_transactionId);
    quint16 proto = qToBigEndian(static_cast<quint16>(0));
    quint16 len = qToBigEndian(static_cast<quint16>(1 + pdu.size()));

    QByteArray mbap;
    mbap.append(reinterpret_cast<const char*>(&tid), 2);
    mbap.append(reinterpret_cast<const char*>(&proto), 2);
    mbap.append(reinterpret_cast<const char*>(&len), 2);
    mbap.append(static_cast<char>(unitId));
    mbap.append(pdu);
    return mbap;
}

QByteArray ModbusTcpAdapter::readHoldingRegisters(quint8 unitId, quint16 startAddr, quint16 count) const
{
    quint8 func = 0x03;
    quint16 addrBE = qToBigEndian(startAddr);
    quint16 countBE = qToBigEndian(count);

    QByteArray pdu;
    pdu.append(static_cast<char>(func));
    pdu.append(reinterpret_cast<const char*>(&addrBE), 2);
    pdu.append(reinterpret_cast<const char*>(&countBE), 2);
    return buildMbap(unitId, pdu);
}

QByteArray ModbusTcpAdapter::writeSingleRegister(quint8 unitId, quint16 addr, quint16 value) const
{
    quint8 func = 0x06;
    quint16 addrBE = qToBigEndian(addr);
    quint16 valBE = qToBigEndian(value);

    QByteArray pdu;
    pdu.append(static_cast<char>(func));
    pdu.append(reinterpret_cast<const char*>(&addrBE), 2);
    pdu.append(reinterpret_cast<const char*>(&valBE), 2);
    return buildMbap(unitId, pdu);
}

QByteArray ModbusTcpAdapter::writeMultipleRegisters(quint8 unitId, quint16 startAddr, const QList<quint16>& values) const
{
    quint8 func = 0x10;
    quint16 addrBE = qToBigEndian(startAddr);
    quint16 countBE = qToBigEndian(static_cast<quint16>(values.size()));
    quint8 byteCount = static_cast<quint8>(values.size() * 2);

    QByteArray pdu;
    pdu.append(static_cast<char>(func));
    pdu.append(reinterpret_cast<const char*>(&addrBE), 2);
    pdu.append(reinterpret_cast<const char*>(&countBE), 2);
    pdu.append(static_cast<char>(byteCount));
    for (auto v : values) {
        quint16 vBE = qToBigEndian(v);
        pdu.append(reinterpret_cast<const char*>(&vBE), 2);
    }
    return buildMbap(unitId, pdu);
}

QByteArray ModbusTcpAdapter::packFloat32(float value) const
{
    quint32 raw;
    memcpy(&raw, &value, sizeof(raw));
    quint32 be = qToBigEndian(raw);
    QByteArray result;
    result.append(reinterpret_cast<const char*>(&be), 4);
    return result;
}

float ModbusTcpAdapter::unpackFloat32(const QByteArray& data, int offset) const
{
    if (offset + 4 > data.size()) return 0.0f;
    quint32 be;
    memcpy(&be, data.constData() + offset, 4);
    quint32 raw = qFromBigEndian(be);
    float result;
    memcpy(&result, &raw, sizeof(result));
    return result;
}

QByteArray ModbusTcpAdapter::encodeFrame(const Frame& frame)
{
    switch (frame.type) {
    case MessageType::ConnectRequest:
        return encodeConnectRequest(frame.payload);
    case MessageType::HeartbeatRequest:
        return encodeHeartbeat();
    case MessageType::ControlRequest: {
        auto cmd = decodeCommand(frame.payload);
        if (cmd.has_value()) return encodeCommand(cmd.value());
        return {};
    }
    default:
        return {};
    }
}

std::optional<Frame> ModbusTcpAdapter::decodeFrame(QByteArrayView& buffer)
{
    if (buffer.size() < 7) return std::nullopt;

    quint16 lenBE;
    memcpy(&lenBE, buffer.data() + 4, 2);
    quint16 msgLen = qFromBigEndian(lenBE);

    if (buffer.size() < static_cast<qsizetype>(6 + msgLen)) return std::nullopt;

    quint8 funcCode = static_cast<quint8>(buffer[7]);
    QByteArray pdu(buffer.data() + 8, msgLen - 1);

    Frame frame;
    frame.type = MessageType::HeartbeatResponse;

    switch (funcCode) {
    case 0x03: {
        if (pdu.size() >= 3) {
            quint8 byteCount = static_cast<quint8>(pdu[0]);
            QByteArray regData(pdu.data() + 1, byteCount);
            auto telemetry = decodeTelemetry(regData);
            if (telemetry.has_value()) {
                frame.type = MessageType::TelemetryReport;
                frame.payload = encodeTelemetry(telemetry.value());
            }
        }
        break;
    }
    case 0x06: {
        CommandResult result;
        result.status = CommandStatus::Succeeded;
        result.message = QStringLiteral("Modbus register written");
        frame.type = MessageType::ControlResponse;
        frame.payload = encodeCommandResult(result);
        break;
    }
    case 0x10: {
        CommandResult result;
        result.status = CommandStatus::Succeeded;
        result.message = QStringLiteral("Modbus registers written");
        frame.type = MessageType::ControlResponse;
        frame.payload = encodeCommandResult(result);
        break;
    }
    case 0x83: {
        CommandResult result;
        result.status = CommandStatus::DeviceFault;
        result.message = QStringLiteral("Modbus exception: %1").arg(static_cast<quint8>(pdu[0]));
        frame.type = MessageType::ControlResponse;
        frame.payload = encodeCommandResult(result);
        break;
    }
    }

    buffer = buffer.sliced(6 + msgLen);
    return frame;
}

QByteArray ModbusTcpAdapter::encodeConnectRequest(const DeviceId&)
{
    return readHoldingRegisters(1, 0, 12);
}

QByteArray ModbusTcpAdapter::encodeHeartbeat()
{
    return readHoldingRegisters(1, 0, 1);
}

QByteArray ModbusTcpAdapter::encodeTelemetry(const Telemetry& telemetry)
{
    QList<quint16> regs;
    auto tempBytes = packFloat32(telemetry.temperatureC);
    auto speedBytes = packFloat32(static_cast<float>(telemetry.speedRpm));
    auto currentBytes = packFloat32(telemetry.currentA);
    auto voltageBytes = packFloat32(telemetry.voltageV);
    auto vibrBytes = packFloat32(telemetry.vibrationMmPerSec);

    auto toU16 = [](const QByteArray& b, int off) -> quint16 {
        quint16 v;
        memcpy(&v, b.constData() + off, 2);
        return qFromBigEndian(v);
    };
    regs.append(toU16(tempBytes, 0));
    regs.append(toU16(tempBytes, 2));
    regs.append(toU16(speedBytes, 0));
    regs.append(toU16(speedBytes, 2));
    regs.append(toU16(currentBytes, 0));
    regs.append(toU16(currentBytes, 2));
    regs.append(toU16(voltageBytes, 0));
    regs.append(toU16(voltageBytes, 2));
    regs.append(toU16(vibrBytes, 0));
    regs.append(toU16(vibrBytes, 2));
    regs.append(static_cast<quint16>(telemetry.operatingState));
    regs.append(telemetry.targetSpeedRpm);

    return writeMultipleRegisters(1, 0, regs);
}

QByteArray ModbusTcpAdapter::encodeCommand(const CommandRequest& request)
{
    switch (request.type) {
    case CommandType::Start:
        return writeSingleRegister(1, 12, 1);
    case CommandType::Stop:
        return writeSingleRegister(1, 12, 0);
    case CommandType::EmergencyStop:
        return writeSingleRegister(1, 12, 0xFF);
    case CommandType::SetTargetSpeed:
        return writeSingleRegister(1, 11, request.targetSpeedRpm);
    default:
        return {};
    }
}

QByteArray ModbusTcpAdapter::encodeCommandResult(const CommandResult&)
{
    return {};
}

std::optional<DeviceId> ModbusTcpAdapter::decodeConnectRequest(const QByteArray&)
{
    return DeviceId(QStringLiteral("MOTOR-MB-001"));
}

std::optional<Telemetry> ModbusTcpAdapter::decodeTelemetry(const QByteArray& payload)
{
    if (payload.size() < 24) return std::nullopt;

    Telemetry t;
    t.temperatureC = unpackFloat32(payload, 0);
    t.speedRpm = static_cast<quint16>(unpackFloat32(payload, 4));
    t.currentA = unpackFloat32(payload, 8);
    t.voltageV = unpackFloat32(payload, 12);
    t.vibrationMmPerSec = unpackFloat32(payload, 16);

    quint16 stateBE;
    memcpy(&stateBE, payload.constData() + 20, 2);
    t.operatingState = static_cast<OperatingState>(qFromBigEndian(stateBE));

    quint16 targetBE;
    memcpy(&targetBE, payload.constData() + 22, 2);
    t.targetSpeedRpm = qFromBigEndian(targetBE);

    return t;
}

std::optional<CommandRequest> ModbusTcpAdapter::decodeCommand(const QByteArray&)
{
    return std::nullopt;
}

std::optional<CommandResult> ModbusTcpAdapter::decodeCommandResult(const QByteArray&)
{
    return std::nullopt;
}

}