#include "device_server.h"
#include "motor_model.h"
#include <QTcpSocket>
#include <QDebug>

namespace motor::simulator {

DeviceServer::DeviceServer(quint16 port, QObject* parent)
    : QTcpServer(parent)
    , _port(port)
{
}

bool DeviceServer::start()
{
    bool ok = listen(QHostAddress::Any, _port);
    if (ok) {
        qInfo() << "[Server] Listening on port" << _port
                << ", server state:" << isListening();
    } else {
        qWarning() << "[Server] Failed to listen on port" << _port
                   << ", error:" << errorString();
    }
    return ok;
}

void DeviceServer::stop()
{
    for (auto* session : _sessions) {
        session->deleteLater();
    }
    _sessions.clear();
    close();
}

int DeviceServer::sessionCount() const
{
    return static_cast<int>(_sessions.size());
}

void DeviceServer::incomingConnection(qintptr socketDescriptor)
{
    qInfo() << "[Server] incomingConnection socketDescriptor =" << socketDescriptor;

    auto* socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        qWarning() << "[Server] setSocketDescriptor failed:" << socket->errorString();
        socket->deleteLater();
        return;
    }
    qInfo() << "[Server] New connection from" << socket->peerAddress().toString()
            << ":" << socket->peerPort();

    auto model = std::make_unique<MotorModel>(DeviceId(QStringLiteral("MOTOR-SIM-%1").arg(socketDescriptor)), 42);
    auto* session = new DeviceSession(socket, std::move(model), this);

    connect(session, &DeviceSession::disconnected, this, [this, session]() {
        auto id = session->deviceId();
        if (!id.isEmpty()) {
            _sessions.remove(id);
        } else {
            _sessions.remove(QString());
        }
        emit clientDisconnected(id);
        session->deleteLater();
    });

    session->start();
    if (!session->deviceId().isEmpty()) {
        _sessions[session->deviceId()] = session;
    } else {
        _sessions[QString()] = session;
    }
    emit clientConnected(session->deviceId());
}

}
