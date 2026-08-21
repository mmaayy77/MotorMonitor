#pragma once

#include "repository.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <memory>

namespace motor {

class SqliteRepository : public Repository {
public:
    explicit SqliteRepository(QString databasePath, QObject* parent = nullptr);
    ~SqliteRepository() override;

    bool initialize() override;
    bool isOpen() const override;

    bool saveTelemetry(const Telemetry& telemetry) override;
    QList<Telemetry> queryTelemetry(const TelemetryQuery& query) override;

    bool saveAlarmEvent(const AlarmEvent& event) override;
    QList<Alarm> queryAlarms(const AlarmQuery& query) override;

    bool saveCommandResult(DeviceId deviceId, const CommandResult& result) override;
    QList<CommandResult> queryCommands(const CommandQuery& query) override;

    bool saveDeviceEvent(const DeviceEvent& event) override;
    QList<DeviceEvent> queryDeviceEvents(const DeviceEventQuery& query) override;

    bool saveDeviceConfig(const DeviceConfig& config) override;
    QList<DeviceConfig> loadAllDeviceConfigs() override;
    bool deleteDeviceConfig(DeviceId deviceId) override;

private:
    QString _path;
    QSqlDatabase _db;
    bool _initialized{false};

    bool createTables();
    bool execSchema(const QString& sql);
    Telemetry readTelemetry(QSqlQuery& query) const;
    Alarm readAlarm(QSqlQuery& query) const;
    CommandResult readCommand(QSqlQuery& query) const;
    DeviceEvent readDeviceEvent(QSqlQuery& query) const;
};

}
