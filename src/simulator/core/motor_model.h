#pragma once

#include "fault_mode.h"
#include "domain/device_types.h"
#include "domain/command_types.h"
#include <QHash>
#include <QRandomGenerator>
#include <chrono>

namespace motor::simulator {

class MotorModel {
public:
    MotorModel(DeviceId id, quint32 randomSeed);

    CommandResult execute(const CommandRequest& request);
    Telemetry tick(std::chrono::milliseconds elapsed);
    void setFaultMode(FaultMode mode);

    const Telemetry& telemetry() const { return value_; }
    const DeviceId& deviceId() const { return value_.deviceId; }

private:
    CommandResult cachedOrExecute(const CommandRequest& request);
    CommandResult doExecute(const CommandRequest& request);

    Telemetry value_;
    FaultMode fault_{FaultMode::Normal};
    QRandomGenerator random_;

    QHash<quint64, CommandResult> resultCache_;
};

}
