#pragma once

#include <QByteArray>
#include <QVector>
#include <QElapsedTimer>
#include "domain/device_types.h"

namespace motor::protocol {

constexpr quint16 kMagic = 0x4D54;
constexpr quint8 kVersion = 1;
constexpr qsizetype kMinFrameBytes = 33;
constexpr qsizetype kMaxFrameBytes = 65'536;
constexpr qsizetype kMaxReceiveBufferBytes = 131'072;

enum class MessageType : quint16 {
    ConnectRequest = 1,
    DeviceRegister = 2,
    HeartbeatRequest = 3,
    HeartbeatResponse = 4,
    TelemetryReport = 5,
    ControlRequest = 6,
    ControlResponse = 7,
    StatusQuery = 8,
    StatusResponse = 9,
    ProtocolError = 10
};

struct Frame {
    MessageType type;
    quint32 sequence;
    quint64 requestId;
    qint64 timestampMs;
    QByteArray payload;
};

enum class DecodeError {
    BadMagic,
    InvalidLength,
    BadCrc,
    UnknownMessage,
    BufferOverflow
};

struct DecodeBatch {
    QVector<Frame> frames;
    QVector<DecodeError> errors;
    qsizetype discardedBytes{0};
    bool mustDisconnect{false};
};

}
