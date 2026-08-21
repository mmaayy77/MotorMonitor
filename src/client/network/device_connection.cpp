#include "device_connection.h"
#include "protocol/message_codec.h"
#include <QHostAddress>
#include <QElapsedTimer>
#include <QDebug>

namespace motor {

DeviceConnection::DeviceConnection(const DeviceConfig& config, QObject* parent)
    : QObject(parent)
    , _config(config)
{
    _socket = new QTcpSocket(this);
    _heartbeatTimer = new QTimer(this);
    _reconnectTimer = new QTimer(this);
    _reconnectTimer->setSingleShot(true);

    connect(_socket, &QTcpSocket::connected, this, &DeviceConnection::onConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &DeviceConnection::onDisconnected);
    connect(_socket, &QTcpSocket::readyRead, this, &DeviceConnection::onReadyRead);
    connect(_socket, &QTcpSocket::errorOccurred, this, &DeviceConnection::onSocketError);
    connect(_heartbeatTimer, &QTimer::timeout, this, &DeviceConnection::sendHeartbeat);
    connect(_reconnectTimer, &QTimer::timeout, this, &DeviceConnection::connectInternal);
}

DeviceConnection::~DeviceConnection()
{
    if (_socket->isOpen()) {
        _socket->disconnectFromHost();
    }
}

bool DeviceConnection::connectToDevice()
{
    if (!_config.enabled) {
        return false;
    }
    if (_socket->isOpen()) {
        return false;
    }
    setState(ConnectionState::Connecting);
    connectInternal();
    return true;
}

void DeviceConnection::connectInternal()
{
    setState(ConnectionState::Connecting);
    _socket->connectToHost(_config.host, _config.port);
}

void DeviceConnection::disconnectDevice()
{
    _reconnectTimer->stop();
    _heartbeatTimer->stop();
    if (_socket->isOpen()) {
        setState(ConnectionState::Disconnecting);
        _socket->disconnectFromHost();
    } else {
        setState(ConnectionState::Disconnected);
    }
    _registered = false;
    _reconnectAttempt = 0;
}

ConnectionState DeviceConnection::state() const
{
    return _state;
}

DeviceId DeviceConnection::deviceId() const
{
    return _config.deviceId;
}

const DeviceConfig& DeviceConnection::config() const
{
    return _config;
}

bool DeviceConnection::isConnected() const
{
    return _state == ConnectionState::Online && _registered;
}

bool DeviceConnection::sendCommand(const CommandRequest& request)
{
    if (!isConnected()) {
        return false;
    }
    auto payload = protocol::encodeCommand(request);
    sendFrame(protocol::MessageType::ControlRequest, payload);
    return true;
}

void DeviceConnection::setState(ConnectionState state)
{
    if (_state == state) return;
    auto oldState = _state;
    _state = state;
    emit connectionStateChanged(_config.deviceId, oldState, state);
}

void DeviceConnection::sendFrame(protocol::MessageType type, const QByteArray& payload)
{
    if (_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "[Client]" << _config.deviceId
                   << "sendFrame failed: socket not connected, state="
                   << _socket->state();
        return;
    }
    protocol::Frame frame;
    frame.type = type;
    frame.sequence = _sequence++;
    frame.requestId = 0;
    frame.timestampMs = 0;
    frame.payload = payload;
    _socket->write(protocol::encodeFrame(frame));
}

void DeviceConnection::onConnected()
{
    setState(ConnectionState::Registering);
    _reconnectAttempt = 0;
    _registered = false;
    auto payload = protocol::encodeConnectRequest(_config.deviceId, 1);
    sendFrame(protocol::MessageType::ConnectRequest, payload);
}

void DeviceConnection::onDisconnected()
{
    _heartbeatTimer->stop();
    _registered = false;
    auto oldState = _state;
    if (oldState != ConnectionState::Disconnecting) {
        setState(ConnectionState::Disconnected);
        scheduleReconnect();
    } else {
        setState(ConnectionState::Disconnected);
    }
}

void DeviceConnection::onSocketError(QAbstractSocket::SocketError error)
{
    qWarning() << "[Client]" << _config.deviceId << "socket error:"
               << error << _socket->errorString();
    if (_state == ConnectionState::Connecting) {
        setState(ConnectionState::Disconnected);
        scheduleReconnect();
    }
}

void DeviceConnection::onReadyRead()
{
    auto data = _socket->readAll();
    auto batch = _decoder.append(data);

    if (batch.mustDisconnect) {
        qWarning() << "[Client]" << _config.deviceId
                   << "mustDisconnect, errors=" << batch.errors.size()
                   << "discarded=" << batch.discardedBytes;
        emit protocolError();
        setState(ConnectionState::ProtocolError);
        _socket->disconnectFromHost();
        return;
    }

    for (const auto& frame : batch.frames) {
        handleFrame(frame);
    }
}

void DeviceConnection::handleFrame(const protocol::Frame& frame)
{
    using namespace protocol;

    switch (frame.type) {
    case MessageType::DeviceRegister: {
        auto id = decodeDeviceRegister(frame.payload);
        if (id.has_value()) {
            _registered = true;
            setState(ConnectionState::Online);
            _heartbeatTimer->start(3000);
            emit deviceRegistered(_config.deviceId);
        } else {
            qWarning() << "[Client]" << _config.deviceId
                       << "decodeDeviceRegister FAILED";
        }
        break;
    }
    case MessageType::TelemetryReport: {
        auto t = decodeTelemetry(frame.payload);
        if (t.has_value()) {
            t.value().deviceId = _config.deviceId;
            emit telemetryReceived(_config.deviceId, t.value());
        }
        break;
    }
    case MessageType::ControlResponse: {
        auto r = decodeCommandResult(frame.payload);
        if (r.has_value()) {
            r.value().deviceId = _config.deviceId;
            emit commandResultReceived(_config.deviceId, r.value());
        }
        break;
    }
    case MessageType::HeartbeatResponse:
        break;
    case MessageType::StatusResponse: {
        auto t = decodeTelemetry(frame.payload);
        if (t.has_value()) {
            t.value().deviceId = _config.deviceId;
            emit telemetryReceived(_config.deviceId, t.value());
        }
        break;
    }
    default:
        break;
    }
}

void DeviceConnection::sendHeartbeat()
{
    if (!_socket->isOpen()) return;
    sendFrame(protocol::MessageType::HeartbeatRequest, QByteArray());
}

void DeviceConnection::scheduleReconnect()
{
    if (!_config.autoConnect || _state == ConnectionState::Disabled) return;
    _reconnectAttempt++;
    int delay = qMin(30000, 1000 * (1 << qMin(_reconnectAttempt, 5)));
    _reconnectTimer->start(delay);
}

}
