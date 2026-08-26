#include "storage_worker.h"
#include "persistence/repository.h"
#include "persistence/sqlite_repository.h"
#include <QThread>
#include <QDebug>

namespace motor {

StorageWorker::StorageWorker(const QString& dbPath, QObject* parent)
    : QObject(parent)
    , _dbPath(dbPath)
{
    qDebug() << "[存储线程] ID:" << QThread::currentThreadId();
}

void StorageWorker::initialize()
{
    _repo = std::make_shared<SqliteRepository>(_dbPath);
    bool ok = _repo->initialize();
    emit initialized(ok);
    if (!ok) {
        qWarning() << "StorageWorker: failed to initialize database at" << _dbPath;
    }
}

bool StorageWorker::isOpen() const
{
    return _repo && _repo->isOpen();
}

void StorageWorker::saveTelemetry(const Telemetry& telemetry)
{
    if (_repo) {
        _repo->saveTelemetry(telemetry);
        emit telemetrySaved(telemetry);
    }
}

void StorageWorker::saveAlarmEvent(const AlarmEvent& event)
{
    if (_repo) {
        _repo->saveAlarmEvent(event);
        emit alarmEventSaved(event);
    }
}

void StorageWorker::saveDeviceEvent(const DeviceEvent& event)
{
    if (_repo) _repo->saveDeviceEvent(event);
}

void StorageWorker::saveCommandResult(const DeviceId& deviceId, const CommandResult& result)
{
    if (_repo) _repo->saveCommandResult(deviceId, result);
}

void StorageWorker::saveDeviceConfig(const DeviceConfig& config)
{
    if (_repo) _repo->saveDeviceConfig(config);
}

void StorageWorker::deleteDeviceConfig(const DeviceId& deviceId)
{
    if (_repo) _repo->deleteDeviceConfig(deviceId);
}

QList<DeviceConfig> StorageWorker::loadAllDeviceConfigs()
{
    return _repo ? _repo->loadAllDeviceConfigs() : QList<DeviceConfig>{};
}

QList<Telemetry> StorageWorker::queryTelemetry(const TelemetryQuery& query)
{
    return _repo ? _repo->queryTelemetry(query) : QList<Telemetry>{};
}

QList<Alarm> StorageWorker::queryAlarms(const AlarmQuery& query)
{
    return _repo ? _repo->queryAlarms(query) : QList<Alarm>{};
}

QList<CommandResult> StorageWorker::queryCommands(const CommandQuery& query)
{
    return _repo ? _repo->queryCommands(query) : QList<CommandResult>{};
}

QList<DeviceEvent> StorageWorker::queryDeviceEvents(const DeviceEventQuery& query)
{
    return _repo ? _repo->queryDeviceEvents(query) : QList<DeviceEvent>{};
}

}