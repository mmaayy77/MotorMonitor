#pragma once

#include "motor_model.h"
#include "protocol/frame_codec.h"
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>

namespace motor::simulator {

enum class ProtocolFaultMode {
    None,
    CrcError,
    InvalidLength,
    Truncated
};

class DeviceSession : public QObject {
    Q_OBJECT
public:
    explicit DeviceSession(QTcpSocket* socket, std::unique_ptr<MotorModel> model,
                           QObject* parent = nullptr);

    void start();
    DeviceId deviceId() const;
    void setProtocolFault(ProtocolFaultMode mode);

signals:
    void disconnected();

private slots:
    void onReadyRead();
    void onDisconnected();
    void sendTelemetry();

private:
    QTcpSocket* _socket{nullptr};
    std::unique_ptr<MotorModel> _model;
    protocol::FrameDecoder _decoder;
    DeviceId _deviceId;
    QTimer* _telemetryTimer{nullptr};
    QElapsedTimer _connectedTimer;
    quint32 _sequence{0};
    qint64 _lastHeartbeatMs{0};
    bool _registered{false};
    ProtocolFaultMode _protocolFault{ProtocolFaultMode::None};

    void handleFrame(const protocol::Frame& frame);
    void sendFrame(protocol::MessageType type, const QByteArray& payload);
    void sendDeviceRegister();
    void sendCommandResult(const CommandResult& result);
    void processCommand(const CommandRequest& request);
};

}