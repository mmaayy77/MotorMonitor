#include "communication_worker.h"
#include "connection_manager.h"
#include "device_connection.h"
#include "alarm_engine/alarm_engine.h"
#include <QDateTime>
#include <QThread>
#include <QDebug>

namespace motor {

CommunicationWorker::CommunicationWorker(std::shared_ptr<ConnectionManager> connManager,
                                          std::shared_ptr<AlarmEngine> alarmEngine,
                                          QObject* parent)
    : QObject(parent)
    , _connManager(std::move(connManager))
    , _alarmEngine(std::move(alarmEngine))
{
    qDebug() << "[通信线程] ID:" << QThread::currentThreadId();
    connect(_connManager.get(), &ConnectionManager::connectionStateChanged,
            this, &CommunicationWorker::connectionStateChanged);
    connect(_connManager.get(), &ConnectionManager::telemetryReceived,
            this, [this](DeviceId deviceId, const Telemetry& telemetry) {
                _alarmEngine->processTelemetry(deviceId, telemetry,
                    QDateTime::currentMSecsSinceEpoch());
                emit telemetryReceived(deviceId, telemetry);
            });
    connect(_connManager.get(), &ConnectionManager::commandResultReceived,
            this, &CommunicationWorker::commandResultReceived);
    connect(_alarmEngine.get(), &AlarmEngine::alarmRaised,
            this, &CommunicationWorker::alarmRaised);
    connect(_alarmEngine.get(), &AlarmEngine::alarmStateChanged,
            this, &CommunicationWorker::alarmStateChanged);
}

void CommunicationWorker::addDevice(const DeviceConfig& cfg)
{
    _connManager->addDevice(cfg);
}

void CommunicationWorker::removeDevice(const DeviceId& deviceId)
{
    _connManager->removeDevice(deviceId);
}

void CommunicationWorker::connectToDevice(const DeviceId& deviceId)
{
    auto* conn = _connManager->connection(deviceId);
    if (conn) conn->connectToDevice();
}

void CommunicationWorker::disconnectDevice(const DeviceId& deviceId)
{
    auto* conn = _connManager->connection(deviceId);
    if (conn) conn->disconnectDevice();
}

void CommunicationWorker::sendCommand(const DeviceId& deviceId, const CommandRequest& request)
{
    auto* conn = _connManager->connection(deviceId);
    if (conn) conn->sendCommand(request);
}

void CommunicationWorker::acknowledgeAlarm(const QUuid& alarmId, qint64 timestampMs)
{
    _alarmEngine->acknowledgeAlarm(alarmId, timestampMs);
}

void CommunicationWorker::closeAlarm(const QUuid& alarmId, qint64 timestampMs)
{
    _alarmEngine->closeAlarm(alarmId, timestampMs);
}

}