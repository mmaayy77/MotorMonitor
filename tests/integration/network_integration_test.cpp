#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QObject>
#include "device_server.h"
#include "device_connection.h"
#include "domain/command_types.h"

using namespace motor;
using namespace motor::simulator;

namespace {

const quint16 TEST_PORT = 19000;
const DeviceId TEST_DEVICE("MOTOR-TEST-01");

}

class NetworkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        _server = std::make_unique<DeviceServer>(TEST_PORT);
        ASSERT_TRUE(_server->start());
    }

    void TearDown() override
    {
        _server->stop();
        _server.reset();
    }

    bool waitForConnection(DeviceConnection* connection, int timeoutMs = 5000)
    {
        QEventLoop loop;
        bool connected = false;
        QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);

        QMetaObject::Connection c = QObject::connect(connection, &DeviceConnection::connectionStateChanged,
            [&connected, &loop](DeviceId, ConnectionState, ConnectionState newState) {
                if (newState == ConnectionState::Online) {
                    connected = true;
                    loop.quit();
                }
            });

        loop.exec();
        QObject::disconnect(c);
        return connected;
    }

    std::unique_ptr<DeviceServer> _server;
};

TEST_F(NetworkIntegrationTest, ConnectAndRegister)
{
    DeviceConfig config;
    config.deviceId = TEST_DEVICE;
    config.host = QStringLiteral("127.0.0.1");
    config.port = TEST_PORT;
    config.enabled = true;
    config.autoConnect = true;

    DeviceConnection conn(config);
    conn.connectToDevice();
    EXPECT_TRUE(waitForConnection(&conn, 3000));
    EXPECT_TRUE(conn.isConnected());
    EXPECT_EQ(conn.state(), ConnectionState::Online);
}

TEST_F(NetworkIntegrationTest, SendCommandAndGetResponse)
{
    DeviceConfig config;
    config.deviceId = TEST_DEVICE;
    config.host = QStringLiteral("127.0.0.1");
    config.port = TEST_PORT;
    config.enabled = true;
    config.autoConnect = true;

    DeviceConnection conn(config);
    conn.connectToDevice();
    EXPECT_TRUE(waitForConnection(&conn, 3000));

    CommandRequest req;
    req.requestId = 12345;
    req.deviceId = TEST_DEVICE;
    req.type = CommandType::Start;
    req.targetSpeedRpm = 1500;
    req.sentAtMs = 1000;

    bool gotResult = false;
    CommandResult result;
    QObject::connect(&conn, &DeviceConnection::commandResultReceived,
        [&gotResult, &result](DeviceId, const CommandResult& r) {
            gotResult = true;
            result = r;
        });

    bool sent = conn.sendCommand(req);
    EXPECT_TRUE(sent);

    QEventLoop loop;
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();

    EXPECT_TRUE(gotResult);
    EXPECT_EQ(result.requestId, 12345);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
