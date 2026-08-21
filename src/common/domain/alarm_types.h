#pragma once

#include "device_types.h"

namespace motor {

enum class AlarmSeverity {
    Warning,
    Critical
};

enum class RuleCode {
    HighTemperature,
    CriticalOverheat,
    Overcurrent,
    Stall,
    HighVibration,
    AbnormalVoltage,
    HeartbeatTimeout,
    TelemetryStale
};

enum class AlarmState {
    ActiveUnacknowledged,
    ActiveAcknowledged,
    RecoveredUnacknowledged,
    RecoveredAcknowledged,
    Closed
};

struct Alarm {
    QUuid alarmId;
    DeviceId deviceId;
    RuleCode rule{RuleCode::HighTemperature};
    AlarmSeverity severity{AlarmSeverity::Warning};
    AlarmState state{AlarmState::ActiveUnacknowledged};
    double actualValue{0.0};
    double threshold{0.0};
    double peakValue{0.0};
    int consecutiveCount{0};
    qint64 triggeredAtMs{0};
    qint64 acknowledgedAtMs{0};
    qint64 recoveredAtMs{0};
    qint64 closedAtMs{0};
};

struct AlarmEvent {
    Alarm alarm;
    AlarmState previousState;
    qint64 eventTimeMs{0};
    QString evidence;
};

}

Q_DECLARE_METATYPE(motor::AlarmEvent)
