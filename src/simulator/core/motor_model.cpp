#include "motor_model.h"
#include <QDateTime>
#include <cmath>

namespace motor::simulator {

namespace {

constexpr quint16 kMaxRpm = 3000;
constexpr quint16 kStartThreshold = 500;
constexpr int kCacheLimit = 1024;
constexpr float kNoiseAmplitude = 0.01f;
constexpr float kRatedCurrent = 12.0f;

}

MotorModel::MotorModel(DeviceId id, quint32 randomSeed)
    : random_(randomSeed) {
    value_.deviceId = std::move(id);
    value_.operatingState = OperatingState::Stopped;
    value_.healthState = HealthState::Normal;
    value_.temperatureC = 25.0f;
    value_.voltageV = 220.0f;
    value_.currentA = 0.0f;
    value_.speedRpm = 0;
    value_.targetSpeedRpm = 0;
    value_.vibrationMmPerSec = 0.0f;
    value_.runtimeSeconds = 0;
    value_.deviceAlarmBits = 0;
    value_.timestampMs = QDateTime::currentMSecsSinceEpoch();
}

void MotorModel::setFaultMode(FaultMode mode) {
    fault_ = mode;
}

CommandResult MotorModel::execute(const CommandRequest& request) {
    return cachedOrExecute(request);
}

CommandResult MotorModel::cachedOrExecute(const CommandRequest& request) {
    auto it = resultCache_.constFind(request.requestId);
    if (it != resultCache_.cend()) {
        return it.value();
    }
    CommandResult result = doExecute(request);
    if (resultCache_.size() >= kCacheLimit) {
        resultCache_.erase(resultCache_.begin());
    }
    resultCache_.insert(request.requestId, result);
    return result;
}

CommandResult MotorModel::doExecute(const CommandRequest& request) {
    CommandResult result;
    result.requestId = request.requestId;
    result.deviceId = request.deviceId;
    result.type = request.type;
    result.sentAtMs = request.sentAtMs;
    result.completedAtMs = QDateTime::currentMSecsSinceEpoch();

    const bool online = true;

    switch (request.type) {
    case CommandType::Start:
        if (!online) {
            result.status = CommandStatus::RejectedState;
            result.message = QStringLiteral("Device offline");
        } else if (value_.operatingState != OperatingState::Stopped) {
            result.status = CommandStatus::RejectedState;
            result.message = QStringLiteral("Not in Stopped state");
        } else if (value_.healthState == HealthState::Fault) {
            result.status = CommandStatus::DeviceFault;
            result.message = QStringLiteral("Device in fault state");
        } else {
            value_.operatingState = OperatingState::Starting;
            result.status = CommandStatus::Succeeded;
            result.message = QStringLiteral("Started");
        }
        break;

    case CommandType::Stop:
        if (!online) {
            result.status = CommandStatus::RejectedState;
            result.message = QStringLiteral("Device offline");
        } else if (value_.operatingState != OperatingState::Starting &&
                   value_.operatingState != OperatingState::Running) {
            result.status = CommandStatus::RejectedState;
            result.message = QStringLiteral("Not running");
        } else {
            value_.operatingState = OperatingState::Stopping;
            value_.targetSpeedRpm = 0;
            result.status = CommandStatus::Succeeded;
            result.message = QStringLiteral("Stopping");
        }
        break;

    case CommandType::SetTargetSpeed:
        if (!online) {
            result.status = CommandStatus::RejectedState;
            result.message = QStringLiteral("Device offline");
        } else if (value_.operatingState != OperatingState::Starting &&
                   value_.operatingState != OperatingState::Running) {
            result.status = CommandStatus::RejectedState;
            result.message = QStringLiteral("Not running");
        } else if (request.targetSpeedRpm > kMaxRpm) {
            result.status = CommandStatus::ParameterOutOfRange;
            result.message = QStringLiteral("Speed exceeds 3000 RPM");
        } else {
            value_.targetSpeedRpm = request.targetSpeedRpm;
            result.status = CommandStatus::Succeeded;
            result.message = QStringLiteral("Target speed updated");
        }
        break;

    case CommandType::EmergencyStop:
        if (!online) {
            result.status = CommandStatus::RejectedState;
            result.message = QStringLiteral("Device offline");
        } else {
            value_.operatingState = OperatingState::EmergencyStopped;
            value_.targetSpeedRpm = 0;
            value_.speedRpm = 0;
            value_.currentA = 0.0f;
            result.status = CommandStatus::Succeeded;
            result.message = QStringLiteral("Emergency stopped");
        }
        break;

    case CommandType::ClearRecoverableAlarm:
        result.status = CommandStatus::Succeeded;
        result.message = QStringLiteral("Alarm cleared");
        break;

    case CommandType::QueryStatus:
        result.status = CommandStatus::Succeeded;
        result.message = QStringLiteral("OK");
        break;
    }

    return result;
}

Telemetry MotorModel::tick(std::chrono::milliseconds elapsed) {
    const float dt = static_cast<float>(elapsed.count()) / 1000.0f;
    value_.timestampMs = QDateTime::currentMSecsSinceEpoch();

    auto noise = [this](float base) {
        return base + static_cast<float>(random_.bounded(200) - 100) / 100.0f * kNoiseAmplitude * base;
    };

    switch (value_.operatingState) {
    case OperatingState::Starting:
        if (value_.targetSpeedRpm == 0) {
            value_.targetSpeedRpm = 1500;
        }
        if (value_.speedRpm < value_.targetSpeedRpm) {
            value_.speedRpm = std::min<quint16>(value_.targetSpeedRpm,
                                                value_.speedRpm + 120);
            if (value_.speedRpm >= value_.targetSpeedRpm) {
                value_.operatingState = OperatingState::Running;
            }
        }
        break;

    case OperatingState::Running:
        if (value_.speedRpm < value_.targetSpeedRpm) {
            value_.speedRpm = std::min<quint16>(value_.targetSpeedRpm,
                                                value_.speedRpm + 120);
        } else if (value_.speedRpm > value_.targetSpeedRpm) {
            value_.speedRpm = (value_.speedRpm > value_.targetSpeedRpm + 120)
                              ? static_cast<quint16>(value_.speedRpm - 120)
                              : value_.targetSpeedRpm;
        }
        break;

    case OperatingState::Stopping:
        if (value_.speedRpm > 0) {
            value_.speedRpm = (value_.speedRpm > 120)
                              ? static_cast<quint16>(value_.speedRpm - 120)
                              : 0;
            if (value_.speedRpm == 0) {
                value_.operatingState = OperatingState::Stopped;
            }
        }
        break;

    case OperatingState::EmergencyStopped:
    case OperatingState::Stopped:
        break;
    }

    float speedF = static_cast<float>(value_.speedRpm);
    float targetF = static_cast<float>(value_.targetSpeedRpm);

    switch (fault_) {
    case FaultMode::Overload:
        value_.currentA = noise(0.8f + speedF / 220.0f + 6.0f);
        value_.temperatureC = noise(25.0f + speedF / 100.0f + value_.currentA * 1.5f);
        value_.vibrationMmPerSec = noise(0.6f + speedF / 500.0f);
        value_.voltageV = noise(220.0f);
        break;

    case FaultMode::Overheat:
        value_.currentA = noise(0.8f + speedF / 220.0f);
        value_.temperatureC = noise(25.0f + speedF / 100.0f + value_.currentA * 1.5f + 45.0f);
        value_.vibrationMmPerSec = noise(0.6f + speedF / 500.0f);
        value_.voltageV = noise(220.0f);
        break;

    case FaultMode::Stall:
        if (value_.targetSpeedRpm >= kStartThreshold) {
            value_.speedRpm = static_cast<quint16>(targetF * 0.05f);
            speedF = static_cast<float>(value_.speedRpm);
        }
        value_.currentA = noise(0.8f + speedF / 220.0f + 8.0f);
        value_.temperatureC = noise(25.0f + speedF / 100.0f + value_.currentA * 1.5f);
        value_.vibrationMmPerSec = noise(0.6f + speedF / 500.0f);
        value_.voltageV = noise(220.0f);
        break;

    case FaultMode::HighVibration:
        value_.currentA = noise(0.8f + speedF / 220.0f);
        value_.temperatureC = noise(25.0f + speedF / 100.0f + value_.currentA * 1.5f);
        value_.vibrationMmPerSec = noise(0.6f + speedF / 500.0f + 9.0f);
        value_.voltageV = noise(220.0f);
        break;

    case FaultMode::AbnormalVoltage: {
        bool lowSide = random_.bounded(2) == 0;
        value_.voltageV = noise(lowSide ? 190.0f : 250.0f);
        value_.currentA = noise(0.8f + speedF / 220.0f);
        value_.temperatureC = noise(25.0f + speedF / 100.0f + value_.currentA * 1.5f);
        value_.vibrationMmPerSec = noise(0.6f + speedF / 500.0f);
        break;
    }

    case FaultMode::Normal:
        value_.currentA = noise(0.8f + speedF / 220.0f);
        value_.temperatureC = noise(25.0f + speedF / 100.0f + value_.currentA * 1.5f);
        value_.vibrationMmPerSec = noise(0.6f + speedF / 500.0f);
        value_.voltageV = noise(220.0f);
        break;
    }

    if (value_.operatingState == OperatingState::Running) {
        value_.runtimeSeconds += static_cast<quint64>(dt);
    }

    return value_;
}

}
