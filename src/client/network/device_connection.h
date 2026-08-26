#pragma once

#include "domain/device_types.h"
#include "domain/alarm_types.h"
#include "domain/command_types.h"
#include "protocol/frame_codec.h"
#include "protocol/protocol_adapter.h"
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <memory>

namespace motor {

class DeviceConnection : public QObject {
    Q_OBJECT
public:
    explicit DeviceConnection(const DeviceConfig& config, QObject* parent = nullptr);
    ~DeviceConnection() override;

    bool connectToDevice();
    void disconnectDevice();
    ConnectionState state() const;
    DeviceId deviceId() const;
    const DeviceConfig& config() const;

    bool sendCommand(const CommandRequest& request);
    bool isConnected() const;

signals:
    void connectionStateChanged(DeviceId deviceId, ConnectionState oldState, ConnectionState newState);
    void telemetryReceived(DeviceId deviceId, const Telemetry& telemetry);
    void commandResultReceived(DeviceId deviceId, const CommandResult& result);
    void deviceRegistered(DeviceId deviceId);
    void protocolError();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void sendHeartbeat();

private:
    DeviceConfig _config;
    QTcpSocket* _socket{nullptr};
    protocol::FrameDecoder _decoder;
    protocol::ProtocolAdapterPtr _protocolAdapter;
    ConnectionState _state{ConnectionState::Disconnected};
    QTimer* _heartbeatTimer{nullptr};
    QTimer* _reconnectTimer{nullptr};
    quint32 _sequence{0};
    bool _registered{false};
    int _reconnectAttempt{0};

    void setState(ConnectionState state);
    void sendFrame(protocol::MessageType type, const QByteArray& payload);
    void handleFrame(const protocol::Frame& frame);
    void connectInternal();
    void scheduleReconnect();
};

}
