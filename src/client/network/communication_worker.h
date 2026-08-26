#pragma once

#include "domain/device_types.h"
#include "domain/command_types.h"
#include "domain/alarm_types.h"
#include <QObject>
#include <QUuid>
#include <memory>

namespace motor {

class ConnectionManager;
class AlarmEngine;

class CommunicationWorker : public QObject {
    Q_OBJECT
public:
    explicit CommunicationWorker(std::shared_ptr<ConnectionManager> connManager,
                                  std::shared_ptr<AlarmEngine> alarmEngine,
                                  QObject* parent = nullptr);

    ConnectionManager* connectionManager() const { return _connManager.get(); }
    AlarmEngine* alarmEngine() const { return _alarmEngine.get(); }

public slots:
    void addDevice(const DeviceConfig& cfg);
    void removeDevice(const DeviceId& deviceId);
    void connectToDevice(const DeviceId& deviceId);
    void disconnectDevice(const DeviceId& deviceId);
    void sendCommand(const DeviceId& deviceId, const CommandRequest& request);
    void acknowledgeAlarm(const QUuid& alarmId, qint64 timestampMs);
    void closeAlarm(const QUuid& alarmId, qint64 timestampMs);

signals:
    void connectionStateChanged(DeviceId deviceId, ConnectionState oldState, ConnectionState newState);
    void telemetryReceived(DeviceId deviceId, const Telemetry& telemetry);
    void commandResultReceived(DeviceId deviceId, const CommandResult& result);
    void alarmRaised(const AlarmEvent& event);
    void alarmStateChanged(const AlarmEvent& event);

private:
    std::shared_ptr<ConnectionManager> _connManager;
    std::shared_ptr<AlarmEngine> _alarmEngine;
};

}