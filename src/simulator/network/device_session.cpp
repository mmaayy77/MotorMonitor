#include "device_session.h"
#include "protocol/message_codec.h"
#include <QHostAddress>
#include <QDebug>

namespace motor::simulator {

DeviceSession::DeviceSession(QTcpSocket* socket, std::unique_ptr<MotorModel> model,
                             QObject* parent)
    : QObject(parent)
    , _socket(socket)
    , _model(std::move(model))
{
    _socket->setParent(this);
    _telemetryTimer = new QTimer(this);
    connect(_telemetryTimer, &QTimer::timeout, this, &DeviceSession::sendTelemetry);
    connect(_socket, &QTcpSocket::readyRead, this, &DeviceSession::onReadyRead);
    connect(_socket, &QTcpSocket::disconnected, this, &DeviceSession::onDisconnected);
    connect(_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError err) {
        qWarning() << "DeviceSession socket error:" << err << _socket->errorString();
    });
}

void DeviceSession::start()
{
    _connectedTimer.start();
}

DeviceId DeviceSession::deviceId() const
{
    return _deviceId;
}

void DeviceSession::onReadyRead()
{
    auto data = _socket->readAll();
    auto batch = _decoder.append(data);

    if (batch.mustDisconnect) {
        qWarning() << "DeviceSession must disconnect, errors:" << batch.errors.size();
        _socket->disconnectFromHost();
        return;
    }

    for (const auto& frame : batch.frames) {
        handleFrame(frame);
    }
}

void DeviceSession::handleFrame(const protocol::Frame& frame)
{
    using namespace protocol;

    switch (frame.type) {
    case MessageType::ConnectRequest: {
        auto id = protocol::decodeConnectRequest(frame.payload);
        if (!id.has_value()) {
            qWarning() << "[Session] decodeConnectRequest FAILED, disconnecting";
            sendFrame(MessageType::ProtocolError, QByteArray());
            _socket->disconnectFromHost();
            return;
        }
        _deviceId = id.value();
        _registered = true;
        sendDeviceRegister();
        _telemetryTimer->start(1000);
        break;
    }
    case MessageType::HeartbeatRequest: {
        _lastHeartbeatMs = frame.timestampMs;
        sendFrame(MessageType::HeartbeatResponse, QByteArray());
        break;
    }
    case MessageType::ControlRequest: {
        auto cmd = protocol::decodeCommand(frame.payload);
        if (cmd.has_value()) {
            processCommand(cmd.value());
        }
        break;
    }
    case MessageType::StatusQuery: {
        auto telemetry = _model->telemetry();
        sendFrame(MessageType::StatusResponse, protocol::encodeTelemetry(telemetry));
        break;
    }
    default:
        break;
    }
}

void DeviceSession::sendFrame(protocol::MessageType type, const QByteArray& payload)
{
    protocol::Frame frame;
    frame.type = type;
    frame.sequence = _sequence++;
    frame.requestId = 0;
    frame.timestampMs = _connectedTimer.elapsed();
    frame.payload = payload;
    _socket->write(protocol::encodeFrame(frame));
}

void DeviceSession::sendDeviceRegister()
{
    sendFrame(protocol::MessageType::DeviceRegister, protocol::encodeDeviceRegister(_deviceId));
}

void DeviceSession::sendCommandResult(const CommandResult& result)
{
    sendFrame(protocol::MessageType::ControlResponse, protocol::encodeCommandResult(result));
}

void DeviceSession::processCommand(const CommandRequest& request)
{
    auto result = _model->execute(request);
    sendCommandResult(result);
}

void DeviceSession::sendTelemetry()
{
    if (!_registered || !_socket->isOpen()) {
        return;
    }
    auto telemetry = _model->tick(std::chrono::milliseconds(1000));
    telemetry.deviceId = _deviceId;
    sendFrame(protocol::MessageType::TelemetryReport, protocol::encodeTelemetry(telemetry));
}

void DeviceSession::onDisconnected()
{
    _telemetryTimer->stop();
    emit disconnected();
}

}
