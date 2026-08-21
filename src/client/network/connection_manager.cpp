#include "connection_manager.h"

namespace motor {

ConnectionManager::ConnectionManager(QObject* parent)
    : QObject(parent)
{
}

ConnectionManager::~ConnectionManager()
{
    disconnectAll();
    qDeleteAll(_connections);
    _connections.clear();
}

void ConnectionManager::addDevice(const DeviceConfig& config)
{
    if (_connections.contains(config.deviceId)) {
        return;
    }
    auto* conn = new DeviceConnection(config, this);

    connect(conn, &DeviceConnection::connectionStateChanged,
            this, &ConnectionManager::connectionStateChanged);
    connect(conn, &DeviceConnection::telemetryReceived,
            this, &ConnectionManager::telemetryReceived);
    connect(conn, &DeviceConnection::commandResultReceived,
            this, &ConnectionManager::commandResultReceived);

    _connections[config.deviceId] = conn;
    emit deviceAdded(config.deviceId);
}

void ConnectionManager::removeDevice(DeviceId deviceId)
{
    if (!_connections.contains(deviceId)) return;
    auto* conn = _connections.take(deviceId);
    conn->disconnectDevice();
    delete conn;
    emit deviceRemoved(deviceId);
}

void ConnectionManager::updateDeviceConfig(const DeviceConfig& config)
{
    auto it = _connections.find(config.deviceId);
    if (it == _connections.end()) {
        addDevice(config);
        return;
    }
    bool wasConnected = it.value()->isConnected();
    it.value()->disconnectDevice();
    delete it.value();

    auto* conn = new DeviceConnection(config, this);
    connect(conn, &DeviceConnection::connectionStateChanged,
            this, &ConnectionManager::connectionStateChanged);
    connect(conn, &DeviceConnection::telemetryReceived,
            this, &ConnectionManager::telemetryReceived);
    connect(conn, &DeviceConnection::commandResultReceived,
            this, &ConnectionManager::commandResultReceived);
    _connections[config.deviceId] = conn;

    if (wasConnected) {
        conn->connectToDevice();
    }
}

DeviceConnection* ConnectionManager::connection(DeviceId deviceId)
{
    if (!_connections.contains(deviceId)) return nullptr;
    return _connections[deviceId];
}

QList<DeviceConnection*> ConnectionManager::allConnections() const
{
    return _connections.values();
}

void ConnectionManager::connectAll()
{
    for (auto* conn : _connections) {
        if (conn->config().enabled) {
            conn->connectToDevice();
        }
    }
}

void ConnectionManager::disconnectAll()
{
    for (auto* conn : _connections) {
        conn->disconnectDevice();
    }
}

int ConnectionManager::onlineCount() const
{
    int count = 0;
    for (const auto* conn : _connections) {
        if (conn->isConnected()) count++;
    }
    return count;
}

}
