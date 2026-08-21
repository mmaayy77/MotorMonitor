#pragma once

#include "device_session.h"
#include <QTcpServer>
#include <QHash>

namespace motor::simulator {

class DeviceServer : public QTcpServer {
    Q_OBJECT
public:
    explicit DeviceServer(quint16 port, QObject* parent = nullptr);

    bool start();
    void stop();
    int sessionCount() const;

signals:
    void clientConnected(DeviceId deviceId);
    void clientDisconnected(DeviceId deviceId);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    quint16 _port;
    QHash<DeviceId, DeviceSession*> _sessions;
};

}
