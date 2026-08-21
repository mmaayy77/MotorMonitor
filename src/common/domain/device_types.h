#pragma once

#include <QString>
#include <QStringView>
#include <QRegularExpression>
#include <QUuid>
#include <optional>
#include <variant>

namespace motor {

using DeviceId = QString;

enum class ConnectionState {
    Disabled,
    Disconnected,
    Connecting,
    Registering,
    Online,
    Reconnecting,
    Disconnecting,
    ProtocolError
};

enum class OperatingState {
    Stopped,
    Starting,
    Running,
    Stopping,
    EmergencyStopped
};

enum class HealthState {
    Normal,
    Warning,
    Fault
};

struct Telemetry {
    DeviceId deviceId;
    OperatingState operatingState{OperatingState::Stopped};
    HealthState healthState{HealthState::Normal};
    float temperatureC{20.0F};
    float voltageV{220.0F};
    float currentA{0.0F};
    quint16 speedRpm{0};
    quint16 targetSpeedRpm{0};
    float vibrationMmPerSec{0.0F};
    quint64 runtimeSeconds{0};
    quint32 deviceAlarmBits{0};
    qint64 timestampMs{0};
};

struct DeviceConfig {
    DeviceId deviceId;
    QString displayName;
    QString host;
    quint16 port{9000};
    bool enabled{true};
    bool autoConnect{true};
    float ratedCurrentA{12.0F};
    struct Thresholds {
        float temperatureWarningC{85.0F};
        float temperatureWarningRecoveryC{80.0F};
        float temperatureCriticalC{100.0F};
        float temperatureCriticalRecoveryC{90.0F};
        float vibrationWarningMmPerSec{7.1F};
        float vibrationRecoveryMmPerSec{6.0F};
        float voltageLowV{200.0F};
        float voltageHighV{240.0F};
        float voltageRecoveryLowV{205.0F};
        float voltageRecoveryHighV{235.0F};
    } thresholds;
};

struct DeviceSnapshot {
    Telemetry telemetry;
    ConnectionState connectionState{ConnectionState::Disconnected};
    int activeAlarmCount{0};
    qint64 receivedAtMs{0};
    bool stale{true};
};

inline bool isValidDeviceId(QStringView id) {
    static const QRegularExpression pattern(QStringLiteral("^MOTOR-[0-9]{4}$"));
    return pattern.matchView(id).hasMatch();
}

}

Q_DECLARE_METATYPE(motor::Telemetry)
Q_DECLARE_METATYPE(motor::DeviceSnapshot)
