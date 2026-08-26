#include "message_codec.h"
#include <QtEndian>

namespace motor::protocol {

namespace {

constexpr int kMaxDeviceIdBytes = 32;

}

QByteArray encodeConnectRequest(const DeviceId& deviceId, quint8 requestedVersion) {
    QByteArray result;
    QByteArray utf8Id = deviceId.toUtf8();
    if (utf8Id.size() > kMaxDeviceIdBytes) {
        return result;
    }
    quint8 len = static_cast<quint8>(utf8Id.size());
    result.append(reinterpret_cast<const char*>(&len), 1);
    result.append(requestedVersion);
    result.append(utf8Id);
    return result;
}

std::optional<DeviceId> decodeConnectRequest(QByteArrayView payload) {
    if (payload.isEmpty() || payload.size() < 2) return std::nullopt;
    quint8 len = static_cast<quint8>(payload[0]);
    if (len > kMaxDeviceIdBytes || len > payload.size() - 2) return std::nullopt;
    return QString::fromUtf8(payload.data() + 2, len);
}

QByteArray encodeDeviceRegister(const DeviceId& deviceId) {
    return encodeConnectRequest(deviceId, kVersion);
}

std::optional<DeviceId> decodeDeviceRegister(QByteArrayView payload) {
    return decodeConnectRequest(payload);
}

QByteArray encodeTelemetry(const Telemetry& t) {
    QByteArray result;
    QByteArray idBytes = t.deviceId.toUtf8();
    quint8 idLen = static_cast<quint8>(idBytes.size());
    result.append(reinterpret_cast<const char*>(&idLen), 1);
    result.append(idBytes);

    quint8 opState = static_cast<quint8>(t.operatingState);
    quint8 health = static_cast<quint8>(t.healthState);
    result.append(opState);
    result.append(health);

    float tempBE = qToBigEndian(t.temperatureC);
    float voltBE = qToBigEndian(t.voltageV);
    float currBE = qToBigEndian(t.currentA);
    float vibBE = qToBigEndian(t.vibrationMmPerSec);
    quint16 speedBE = qToBigEndian(t.speedRpm);
    quint16 targetBE = qToBigEndian(t.targetSpeedRpm);
    quint64 runtimeBE = qToBigEndian(t.runtimeSeconds);
    quint32 bitsBE = qToBigEndian(t.deviceAlarmBits);
    qint64 tsBE = qToBigEndian(t.timestampMs);

    result.append(reinterpret_cast<const char*>(&tempBE), sizeof(tempBE));
    result.append(reinterpret_cast<const char*>(&voltBE), sizeof(voltBE));
    result.append(reinterpret_cast<const char*>(&currBE), sizeof(currBE));
    result.append(reinterpret_cast<const char*>(&vibBE), sizeof(vibBE));
    result.append(reinterpret_cast<const char*>(&speedBE), sizeof(speedBE));
    result.append(reinterpret_cast<const char*>(&targetBE), sizeof(targetBE));
    result.append(reinterpret_cast<const char*>(&runtimeBE), sizeof(runtimeBE));
    result.append(reinterpret_cast<const char*>(&bitsBE), sizeof(bitsBE));
    result.append(reinterpret_cast<const char*>(&tsBE), sizeof(tsBE));

    return result;
}

std::optional<Telemetry> decodeTelemetry(QByteArrayView payload) {
    if (payload.size() < 1 + 1 + 2 + 4 + 4 + 4 + 4 + 2 + 2 + 8 + 4 + 8) {
        return std::nullopt;
    }
    int offset = 0;
    quint8 idLen = static_cast<quint8>(payload[offset++]);
    if (offset + idLen > payload.size()) return std::nullopt;

    Telemetry t;
    t.deviceId = QString::fromUtf8(payload.data() + offset, idLen);
    offset += idLen;

    t.operatingState = static_cast<OperatingState>(static_cast<quint8>(payload[offset++]));
    t.healthState = static_cast<HealthState>(static_cast<quint8>(payload[offset++]));

    t.temperatureC = qFromBigEndian<float>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 4;
    t.voltageV = qFromBigEndian<float>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 4;
    t.currentA = qFromBigEndian<float>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 4;
    t.vibrationMmPerSec = qFromBigEndian<float>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 4;
    t.speedRpm = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 2;
    t.targetSpeedRpm = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 2;
    t.runtimeSeconds = qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 8;
    t.deviceAlarmBits = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 4;
    t.timestampMs = qFromBigEndian<qint64>(reinterpret_cast<const uchar*>(payload.data() + offset));

    return t;
}

QByteArray encodeCommand(const CommandRequest& cmd) {
    QByteArray result;
    quint64 reqIdBE = qToBigEndian(cmd.requestId);
    quint16 typeBE = qToBigEndian(static_cast<quint16>(cmd.type));
    quint16 targetBE = qToBigEndian(cmd.targetSpeedRpm);
    qint64 sentBE = qToBigEndian(cmd.sentAtMs);
    QByteArray idBytes = cmd.deviceId.toUtf8();
    quint8 idLen = static_cast<quint8>(idBytes.size());

    result.append(reinterpret_cast<const char*>(&reqIdBE), sizeof(reqIdBE));
    result.append(reinterpret_cast<const char*>(&typeBE), sizeof(typeBE));
    result.append(reinterpret_cast<const char*>(&targetBE), sizeof(targetBE));
    result.append(reinterpret_cast<const char*>(&sentBE), sizeof(sentBE));
    result.append(reinterpret_cast<const char*>(&cmd.protocolFaultMode), 1);
    result.append(reinterpret_cast<const char*>(&idLen), 1);
    result.append(idBytes);

    return result;
}

std::optional<CommandRequest> decodeCommand(QByteArrayView payload) {
    if (payload.size() < 8 + 2 + 2 + 8 + 1 + 1) return std::nullopt;
    int offset = 0;

    CommandRequest cmd;
    cmd.requestId = qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 8;

    quint16 typeRaw = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(payload.data() + offset));
    cmd.type = static_cast<CommandType>(typeRaw);
    offset += 2;

    cmd.targetSpeedRpm = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 2;
    cmd.sentAtMs = qFromBigEndian<qint64>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 8;

    cmd.protocolFaultMode = static_cast<quint8>(payload[offset++]);

    quint8 idLen = static_cast<quint8>(payload[offset++]);
    if (offset + idLen != payload.size()) return std::nullopt;
    cmd.deviceId = QString::fromUtf8(payload.data() + offset, idLen);

    return cmd;
}

QByteArray encodeCommandResult(const CommandResult& res) {
    QByteArray result;
    quint64 reqIdBE = qToBigEndian(res.requestId);
    quint16 typeBE = qToBigEndian(static_cast<quint16>(res.type));
    quint8 status = static_cast<quint8>(res.status);
    qint64 sentBE = qToBigEndian(res.sentAtMs);
    qint64 doneBE = qToBigEndian(res.completedAtMs);
    QByteArray idBytes = res.deviceId.toUtf8();
    quint8 idLen = static_cast<quint8>(idBytes.size());
    QByteArray msgBytes = res.message.toUtf8();
    quint16 msgLen = qToBigEndian(static_cast<quint16>(msgBytes.size()));

    result.append(reinterpret_cast<const char*>(&reqIdBE), sizeof(reqIdBE));
    result.append(reinterpret_cast<const char*>(&typeBE), sizeof(typeBE));
    result.append(status);
    result.append(reinterpret_cast<const char*>(&sentBE), sizeof(sentBE));
    result.append(reinterpret_cast<const char*>(&doneBE), sizeof(doneBE));
    result.append(reinterpret_cast<const char*>(&idLen), 1);
    result.append(idBytes);
    result.append(reinterpret_cast<const char*>(&msgLen), sizeof(msgLen));
    result.append(msgBytes);

    return result;
}

std::optional<CommandResult> decodeCommandResult(QByteArrayView payload) {
    if (payload.size() < 8 + 2 + 1 + 8 + 8 + 1 + 2) return std::nullopt;
    int offset = 0;

    CommandResult res;
    res.requestId = qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 8;

    quint16 typeRaw = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(payload.data() + offset));
    res.type = static_cast<CommandType>(typeRaw);
    offset += 2;

    res.status = static_cast<CommandStatus>(static_cast<quint8>(payload[offset++]));
    res.sentAtMs = qFromBigEndian<qint64>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 8;
    res.completedAtMs = qFromBigEndian<qint64>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 8;

    quint8 idLen = static_cast<quint8>(payload[offset++]);
    if (offset + idLen + 2 > payload.size()) return std::nullopt;
    res.deviceId = QString::fromUtf8(payload.data() + offset, idLen);
    offset += idLen;

    quint16 msgLen = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(payload.data() + offset));
    offset += 2;
    if (offset + msgLen != payload.size()) return std::nullopt;
    res.message = QString::fromUtf8(payload.data() + offset, msgLen);

    return res;
}

}
