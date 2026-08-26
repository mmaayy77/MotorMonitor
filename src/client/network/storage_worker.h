#pragma once

#include "domain/device_types.h"
#include "domain/command_types.h"
#include "domain/alarm_types.h"
#include "domain/persistence_types.h"
#include <QObject>
#include <memory>

namespace motor {

class Repository;

class StorageWorker : public QObject {
    Q_OBJECT
public:
    explicit StorageWorker(const QString& dbPath, QObject* parent = nullptr);

    bool isOpen() const;
    Repository* repository() const { return _repo.get(); }

public slots:
    void initialize();
    void saveTelemetry(const Telemetry& telemetry);
    void saveAlarmEvent(const AlarmEvent& event);
    void saveDeviceEvent(const DeviceEvent& event);
    void saveCommandResult(const DeviceId& deviceId, const CommandResult& result);
    void saveDeviceConfig(const DeviceConfig& config);
    void deleteDeviceConfig(const DeviceId& deviceId);

    QList<DeviceConfig> loadAllDeviceConfigs();
    QList<Telemetry> queryTelemetry(const TelemetryQuery& query);
    QList<Alarm> queryAlarms(const AlarmQuery& query);
    QList<CommandResult> queryCommands(const CommandQuery& query);
    QList<DeviceEvent> queryDeviceEvents(const DeviceEventQuery& query);

signals:
    void initialized(bool ok);
    void telemetrySaved(const Telemetry& telemetry);
    void alarmEventSaved(const AlarmEvent& event);

private:
    QString _dbPath;
    std::shared_ptr<Repository> _repo;
};

}