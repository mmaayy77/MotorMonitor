#pragma once

#include "device_types.h"
#include "command_types.h"
#include "alarm_types.h"

namespace motor {

struct TelemetryQuery {
    DeviceId deviceId;
    qint64 fromMs{0};
    qint64 toMs{0};
    int limit{200};
    int offset{0};
};

struct AlarmQuery {
    std::optional<DeviceId> deviceId;
    qint64 fromMs{0};
    qint64 toMs{0};
    std::optional<AlarmState> state;
    int limit{200};
    int offset{0};
};

struct CommandQuery {
    std::optional<DeviceId> deviceId;
    qint64 fromMs{0};
    qint64 toMs{0};
    int limit{200};
    int offset{0};
};

struct DeviceEvent {
    DeviceId deviceId;
    ConnectionState state;
    QString reason;
    qint64 eventTimeMs{0};
};

struct DeviceEventQuery {
    std::optional<DeviceId> deviceId;
    qint64 fromMs{0};
    qint64 toMs{0};
    int limit{200};
    int offset{0};
};

using PersistRecord = std::variant<Telemetry, AlarmEvent, CommandResult, DeviceEvent>;

}
