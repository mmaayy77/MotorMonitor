#pragma once

#include "device_connection.h"
#include "domain/device_types.h"
#include <QObject>
#include <QHash>

namespace motor {

class ConnectionManager : public QObject {
    Q_OBJECT
public:
    explicit ConnectionManager(QObject* parent = nullptr);
    ~ConnectionManager() override;

    void addDevice(const DeviceConfig& config);
    void removeDevice(DeviceId deviceId);
    void updateDeviceConfig(const DeviceConfig& config);

    DeviceConnection* connection(DeviceId deviceId);
    QList<DeviceConnection*> allConnections() const;

    void connectAll();
    void disconnectAll();
    int onlineCount() const;

signals:
    void deviceAdded(DeviceId deviceId);
    void deviceRemoved(DeviceId deviceId);
    void connectionStateChanged(DeviceId deviceId, ConnectionState oldState, ConnectionState newState);
    void telemetryReceived(DeviceId deviceId, const Telemetry& telemetry);
    void commandResultReceived(DeviceId deviceId, const CommandResult& result);

private:
    QHash<DeviceId, DeviceConnection*> _connections;
};

}
