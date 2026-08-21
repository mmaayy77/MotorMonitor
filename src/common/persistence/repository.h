#pragma once

#include "domain/persistence_types.h"
#include <QList>
#include <QString>

namespace motor {

class Repository {
public:
    virtual ~Repository() = default;

    virtual bool initialize() = 0;
    virtual bool isOpen() const = 0;

    virtual bool saveTelemetry(const Telemetry& telemetry) = 0;
    virtual QList<Telemetry> queryTelemetry(const TelemetryQuery& query) = 0;

    virtual bool saveAlarmEvent(const AlarmEvent& event) = 0;
    virtual QList<Alarm> queryAlarms(const AlarmQuery& query) = 0;

    virtual bool saveCommandResult(DeviceId deviceId, const CommandResult& result) = 0;
    virtual QList<CommandResult> queryCommands(const CommandQuery& query) = 0;

    virtual bool saveDeviceEvent(const DeviceEvent& event) = 0;
    virtual QList<DeviceEvent> queryDeviceEvents(const DeviceEventQuery& query) = 0;

    virtual bool saveDeviceConfig(const DeviceConfig& config) = 0;
    virtual QList<DeviceConfig> loadAllDeviceConfigs() = 0;
    virtual bool deleteDeviceConfig(DeviceId deviceId) = 0;
};

}
