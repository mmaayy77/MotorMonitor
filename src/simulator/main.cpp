#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include "network/device_server.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Motor Simulator"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Industrial Motor Network Simulator"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(QStringList() << QStringLiteral("p") << QStringLiteral("port"),
        QStringLiteral("Listen port"), QStringLiteral("port"), QStringLiteral("9000"));
    parser.addOption(portOption);
    parser.process(app);

    quint16 port = static_cast<quint16>(parser.value(portOption).toUShort());

    motor::simulator::DeviceServer server(port);
    if (!server.start()) {
        qCritical() << "Failed to start server on port" << port;
        return 1;
    }

    qInfo() << "Motor Simulator started, listening on port" << port;

    QObject::connect(&server, &motor::simulator::DeviceServer::clientConnected,
        [](motor::DeviceId id) { qInfo() << "Client connected:" << id; });
    QObject::connect(&server, &motor::simulator::DeviceServer::clientDisconnected,
        [](motor::DeviceId id) { qInfo() << "Client disconnected:" << id; });

    return app.exec();
}
