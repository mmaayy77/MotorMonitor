#pragma once

#include "device_types.h"

namespace motor {

enum class CommandType {
    Start,
    Stop,
    SetTargetSpeed,
    EmergencyStop,
    ClearRecoverableAlarm,
    QueryStatus
};

enum class CommandStatus {
    Pending,
    Succeeded,
    RejectedState,
    ParameterOutOfRange,
    DeviceFault,
    TimedOut
};

struct CommandRequest {
    quint64 requestId{0};
    DeviceId deviceId;
    CommandType type{CommandType::QueryStatus};
    quint16 targetSpeedRpm{0};
    qint64 sentAtMs{0};
};

struct CommandResult {
    quint64 requestId{0};
    DeviceId deviceId;
    CommandType type{CommandType::QueryStatus};
    CommandStatus status{CommandStatus::Pending};
    QString message;
    qint64 sentAtMs{0};
    qint64 completedAtMs{0};
};

}

Q_DECLARE_METATYPE(motor::CommandResult)
