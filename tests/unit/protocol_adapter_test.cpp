#include <gtest/gtest.h>
#include <QtEndian>
#include "protocol/protocol_adapter.h"
#include "protocol/custom_protocol_adapter.h"
#include "protocol/modbus_tcp_adapter.h"
#include "protocol/frame_codec.h"
#include "protocol/message_codec.h"
#include "domain/device_types.h"
#include "domain/command_types.h"
#include "domain/alarm_types.h"

using namespace motor::protocol;
using namespace motor;

class CustomProtocolAdapterTest : public ::testing::Test {
protected:
    void SetUp() override { _adapter = std::make_unique<CustomProtocolAdapter>(); }
    ProtocolAdapterPtr _adapter;
};

TEST_F(CustomProtocolAdapterTest, TypeIsCustom)
{
    EXPECT_EQ(_adapter->type(), ProtocolType::Custom);
}

TEST_F(CustomProtocolAdapterTest, EncodeDecodeFrameRoundTrip)
{
    Frame frame;
    frame.type = MessageType::TelemetryReport;
    frame.sequence = 42;
    frame.requestId = 100;
    frame.timestampMs = 12345;
    frame.payload = QByteArray("test");

    auto encoded = _adapter->encodeFrame(frame);
    QByteArrayView view(encoded);
    auto decoded = _adapter->decodeFrame(view);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->type, MessageType::TelemetryReport);
    EXPECT_EQ(decoded->sequence, 42);
    EXPECT_EQ(decoded->requestId, 100);
    EXPECT_EQ(decoded->timestampMs, 12345);
    EXPECT_EQ(decoded->payload, QByteArray("test"));
}

TEST_F(CustomProtocolAdapterTest, EncodeConnectRequest)
{
    auto encoded = _adapter->encodeConnectRequest(QStringLiteral("MOTOR-001"));
    EXPECT_GT(encoded.size(), 0);
}

TEST_F(CustomProtocolAdapterTest, EncodeHeartbeatReturnsEmpty)
{
    auto encoded = _adapter->encodeHeartbeat();
    EXPECT_TRUE(encoded.isEmpty());
}

TEST_F(CustomProtocolAdapterTest, TelemetryRoundTrip)
{
    Telemetry t;
    t.deviceId = QStringLiteral("MOTOR-001");
    t.temperatureC = 42.5f;
    t.speedRpm = 1500;
    t.currentA = 12.3f;
    t.voltageV = 220.0f;
    t.vibrationMmPerSec = 3.5f;
    t.operatingState = OperatingState::Running;
    t.targetSpeedRpm = 1500;

    auto encoded = _adapter->encodeTelemetry(t);
    auto decoded = _adapter->decodeTelemetry(encoded);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_FLOAT_EQ(decoded->temperatureC, 42.5f);
    EXPECT_EQ(decoded->speedRpm, 1500);
    EXPECT_FLOAT_EQ(decoded->currentA, 12.3f);
    EXPECT_FLOAT_EQ(decoded->voltageV, 220.0f);
    EXPECT_FLOAT_EQ(decoded->vibrationMmPerSec, 3.5f);
    EXPECT_EQ(decoded->operatingState, OperatingState::Running);
}

TEST_F(CustomProtocolAdapterTest, CommandRoundTrip)
{
    CommandRequest cmd;
    cmd.requestId = 1;
    cmd.deviceId = QStringLiteral("MOTOR-001");
    cmd.type = CommandType::SetTargetSpeed;
    cmd.targetSpeedRpm = 2000;
    cmd.protocolFaultMode = 0;

    auto encoded = _adapter->encodeCommand(cmd);
    auto decoded = _adapter->decodeCommand(encoded);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->type, CommandType::SetTargetSpeed);
    EXPECT_EQ(decoded->targetSpeedRpm, 2000);
}

TEST_F(CustomProtocolAdapterTest, CommandResultRoundTrip)
{
    CommandResult result;
    result.requestId = 1;
    result.deviceId = QStringLiteral("MOTOR-001");
    result.type = CommandType::Start;
    result.status = CommandStatus::Succeeded;
    result.message = QStringLiteral("OK");

    auto encoded = _adapter->encodeCommandResult(result);
    auto decoded = _adapter->decodeCommandResult(encoded);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->status, CommandStatus::Succeeded);
    EXPECT_EQ(decoded->message, QStringLiteral("OK"));
}

class ModbusTcpAdapterTest : public ::testing::Test {
protected:
    void SetUp() override { _adapter = std::make_unique<ModbusTcpAdapter>(); }
    ProtocolAdapterPtr _adapter;
};

TEST_F(ModbusTcpAdapterTest, TypeIsModbusTCP)
{
    EXPECT_EQ(_adapter->type(), ProtocolType::ModbusTCP);
}

TEST_F(ModbusTcpAdapterTest, EncodeConnectRequestProducesMBAP)
{
    auto encoded = _adapter->encodeConnectRequest(QStringLiteral("MOTOR-001"));
    ASSERT_GE(encoded.size(), 7);

    quint16 protoId;
    memcpy(&protoId, encoded.constData() + 2, 2);
    EXPECT_EQ(qFromBigEndian(protoId), 0);

    quint8 unitId = static_cast<quint8>(encoded[6]);
    EXPECT_EQ(unitId, 1);

    quint8 funcCode = static_cast<quint8>(encoded[7]);
    EXPECT_EQ(funcCode, 0x03);
}

TEST_F(ModbusTcpAdapterTest, EncodeHeartbeatProducesMBAP)
{
    auto encoded = _adapter->encodeHeartbeat();
    ASSERT_GE(encoded.size(), 7);

    quint8 funcCode = static_cast<quint8>(encoded[7]);
    EXPECT_EQ(funcCode, 0x03);
}

TEST_F(ModbusTcpAdapterTest, EncodeTelemetryProducesWriteMultipleRegisters)
{
    Telemetry t;
    t.temperatureC = 25.0f;
    t.speedRpm = 1500;
    t.currentA = 5.0f;
    t.voltageV = 220.0f;
    t.vibrationMmPerSec = 2.0f;
    t.operatingState = OperatingState::Running;
    t.targetSpeedRpm = 1500;

    auto encoded = _adapter->encodeTelemetry(t);
    ASSERT_GE(encoded.size(), 7);

    quint8 funcCode = static_cast<quint8>(encoded[7]);
    EXPECT_EQ(funcCode, 0x10);
}

TEST_F(ModbusTcpAdapterTest, EncodeStartCommandProducesWriteSingleRegister)
{
    CommandRequest cmd;
    cmd.type = CommandType::Start;

    auto encoded = _adapter->encodeCommand(cmd);
    ASSERT_GE(encoded.size(), 7);

    quint8 funcCode = static_cast<quint8>(encoded[7]);
    EXPECT_EQ(funcCode, 0x06);
}

TEST_F(ModbusTcpAdapterTest, EncodeStopCommand)
{
    CommandRequest cmd;
    cmd.type = CommandType::Stop;

    auto encoded = _adapter->encodeCommand(cmd);
    ASSERT_GE(encoded.size(), 7);

    quint8 funcCode = static_cast<quint8>(encoded[7]);
    EXPECT_EQ(funcCode, 0x06);
}

TEST_F(ModbusTcpAdapterTest, EncodeEmergencyStopCommand)
{
    CommandRequest cmd;
    cmd.type = CommandType::EmergencyStop;

    auto encoded = _adapter->encodeCommand(cmd);
    ASSERT_GE(encoded.size(), 7);

    quint8 funcCode = static_cast<quint8>(encoded[7]);
    EXPECT_EQ(funcCode, 0x06);
}

TEST_F(ModbusTcpAdapterTest, DecodeTelemetryResponse)
{
    QByteArray payload;
    auto appendFloat = [&](float v) {
        quint32 raw;
        memcpy(&raw, &v, sizeof(raw));
        quint32 be = qToBigEndian(raw);
        payload.append(reinterpret_cast<const char*>(&be), 4);
    };
    appendFloat(30.0f);
    appendFloat(1200.0f);
    appendFloat(8.0f);
    appendFloat(225.0f);
    appendFloat(4.0f);

    quint16 state = qToBigEndian(static_cast<quint16>(OperatingState::Running));
    payload.append(reinterpret_cast<const char*>(&state), 2);

    quint16 target = qToBigEndian(static_cast<quint16>(1200));
    payload.append(reinterpret_cast<const char*>(&target), 2);

    auto decoded = _adapter->decodeTelemetry(payload);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_FLOAT_EQ(decoded->temperatureC, 30.0f);
    EXPECT_EQ(decoded->speedRpm, 1200);
    EXPECT_FLOAT_EQ(decoded->currentA, 8.0f);
    EXPECT_FLOAT_EQ(decoded->voltageV, 225.0f);
    EXPECT_FLOAT_EQ(decoded->vibrationMmPerSec, 4.0f);
    EXPECT_EQ(decoded->operatingState, OperatingState::Running);
}

TEST_F(ModbusTcpAdapterTest, DecodeFrameReadHoldingRegisters)
{
    QByteArray buf;
    quint16 tid = qToBigEndian(static_cast<quint16>(1));
    quint16 proto = qToBigEndian(static_cast<quint16>(0));
    buf.append(reinterpret_cast<const char*>(&tid), 2);
    buf.append(reinterpret_cast<const char*>(&proto), 2);

    QByteArray pdu;
    pdu.append(static_cast<char>(0x03));
    pdu.append(static_cast<char>(24));

    auto appendFloat = [&](float v) {
        quint32 raw;
        memcpy(&raw, &v, sizeof(raw));
        quint32 be = qToBigEndian(raw);
        pdu.append(reinterpret_cast<const char*>(&be), 4);
    };
    appendFloat(30.0f);
    appendFloat(1200.0f);
    appendFloat(8.0f);
    appendFloat(225.0f);
    appendFloat(4.0f);

    quint16 state = qToBigEndian(static_cast<quint16>(OperatingState::Running));
    pdu.append(reinterpret_cast<const char*>(&state), 2);
    quint16 target = qToBigEndian(static_cast<quint16>(1200));
    pdu.append(reinterpret_cast<const char*>(&target), 2);

    quint16 len = qToBigEndian(static_cast<quint16>(1 + pdu.size()));
    buf.append(reinterpret_cast<const char*>(&len), 2);
    buf.append(static_cast<char>(1));
    buf.append(pdu);

    QByteArrayView view(buf);
    auto frame = _adapter->decodeFrame(view);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->type, MessageType::TelemetryReport);
}

TEST_F(ModbusTcpAdapterTest, DecodeFrameWriteSingleRegister)
{
    QByteArray buf;
    quint16 tid = qToBigEndian(static_cast<quint16>(1));
    quint16 proto = qToBigEndian(static_cast<quint16>(0));
    quint16 len = qToBigEndian(static_cast<quint16>(6));
    buf.append(reinterpret_cast<const char*>(&tid), 2);
    buf.append(reinterpret_cast<const char*>(&proto), 2);
    buf.append(reinterpret_cast<const char*>(&len), 2);
    buf.append(static_cast<char>(1));
    buf.append(static_cast<char>(0x06));
    quint16 addr = qToBigEndian(static_cast<quint16>(12));
    quint16 val = qToBigEndian(static_cast<quint16>(1));
    buf.append(reinterpret_cast<const char*>(&addr), 2);
    buf.append(reinterpret_cast<const char*>(&val), 2);

    QByteArrayView view(buf);
    auto frame = _adapter->decodeFrame(view);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->type, MessageType::ControlResponse);
}

TEST_F(ModbusTcpAdapterTest, DecodeFrameModbusException)
{
    QByteArray buf;
    quint16 tid = qToBigEndian(static_cast<quint16>(1));
    quint16 proto = qToBigEndian(static_cast<quint16>(0));
    quint16 len = qToBigEndian(static_cast<quint16>(3));
    buf.append(reinterpret_cast<const char*>(&tid), 2);
    buf.append(reinterpret_cast<const char*>(&proto), 2);
    buf.append(reinterpret_cast<const char*>(&len), 2);
    buf.append(static_cast<char>(1));
    buf.append(static_cast<char>(0x83));
    buf.append(static_cast<char>(0x02));

    QByteArrayView view(buf);
    auto frame = _adapter->decodeFrame(view);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->type, MessageType::ControlResponse);
}

TEST_F(ModbusTcpAdapterTest, DecodeFrameIncompleteBuffer)
{
    QByteArray buf;
    buf.append(static_cast<char>(0x00));
    buf.append(static_cast<char>(0x01));
    buf.append(static_cast<char>(0x00));
    buf.append(static_cast<char>(0x00));

    QByteArrayView view(buf);
    auto frame = _adapter->decodeFrame(view);
    EXPECT_FALSE(frame.has_value());
}

TEST_F(ModbusTcpAdapterTest, FactoryCreatesModbusTcpAdapter)
{
    auto adapter = createAdapter(ProtocolType::ModbusTCP);
    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->type(), ProtocolType::ModbusTCP);
}

TEST_F(ModbusTcpAdapterTest, FactoryCreatesCustomAdapter)
{
    auto adapter = createAdapter(ProtocolType::Custom);
    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->type(), ProtocolType::Custom);
}

TEST_F(ModbusTcpAdapterTest, EncodeSetSpeedCommand)
{
    CommandRequest cmd;
    cmd.type = CommandType::SetTargetSpeed;
    cmd.targetSpeedRpm = 2500;

    auto encoded = _adapter->encodeCommand(cmd);
    ASSERT_GE(encoded.size(), 7);

    quint8 funcCode = static_cast<quint8>(encoded[7]);
    EXPECT_EQ(funcCode, 0x06);
}
