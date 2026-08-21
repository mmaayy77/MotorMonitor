#include <gtest/gtest.h>
#include "protocol/message_codec.h"
#include "domain/device_types.h"
#include "domain/command_types.h"

using namespace motor::protocol;

TEST(MessageCodecTest, RoundTripsConnectRequest) {
    auto encoded = encodeConnectRequest(QStringLiteral("MOTOR-0001"), 1);
    auto decoded = decodeConnectRequest(encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, QStringLiteral("MOTOR-0001"));
}

TEST(MessageCodecTest, RejectsDeviceIdOver32Bytes) {
    QString longId = QStringLiteral("MOTOR-") + QString(30, '1');
    auto encoded = encodeConnectRequest(longId, 1);
    EXPECT_TRUE(encoded.isEmpty());
    auto decoded = decodeConnectRequest(encoded);
    EXPECT_FALSE(decoded.has_value());
}

TEST(MessageCodecTest, RoundTripsTelemetry) {
    motor::Telemetry input;
    input.deviceId = QStringLiteral("MOTOR-0001");
    input.operatingState = motor::OperatingState::Running;
    input.healthState = motor::HealthState::Warning;
    input.temperatureC = 87.5f;
    input.voltageV = 225.0f;
    input.currentA = 13.2f;
    input.speedRpm = 1500;
    input.targetSpeedRpm = 1500;
    input.vibrationMmPerSec = 6.8f;
    input.runtimeSeconds = 3600;
    input.deviceAlarmBits = 0x0001;
    input.timestampMs = 1234567890;

    auto encoded = encodeTelemetry(input);
    auto decoded = decodeTelemetry(encoded);
    ASSERT_TRUE(decoded.has_value());

    EXPECT_EQ(decoded->deviceId, input.deviceId);
    EXPECT_EQ(decoded->operatingState, input.operatingState);
    EXPECT_EQ(decoded->healthState, input.healthState);
    EXPECT_FLOAT_EQ(decoded->temperatureC, input.temperatureC);
    EXPECT_FLOAT_EQ(decoded->voltageV, input.voltageV);
    EXPECT_FLOAT_EQ(decoded->currentA, input.currentA);
    EXPECT_EQ(decoded->speedRpm, input.speedRpm);
    EXPECT_EQ(decoded->targetSpeedRpm, input.targetSpeedRpm);
    EXPECT_FLOAT_EQ(decoded->vibrationMmPerSec, input.vibrationMmPerSec);
    EXPECT_EQ(decoded->runtimeSeconds, input.runtimeSeconds);
    EXPECT_EQ(decoded->deviceAlarmBits, input.deviceAlarmBits);
    EXPECT_EQ(decoded->timestampMs, input.timestampMs);
}

TEST(MessageCodecTest, RoundTripsCommandRequest) {
    motor::CommandRequest input;
    input.requestId = 0x1234567890ABCDEF;
    input.deviceId = QStringLiteral("MOTOR-0001");
    input.type = motor::CommandType::SetTargetSpeed;
    input.targetSpeedRpm = 2000;
    input.sentAtMs = 1234567890;

    auto encoded = encodeCommand(input);
    auto decoded = decodeCommand(encoded);
    ASSERT_TRUE(decoded.has_value());

    EXPECT_EQ(decoded->requestId, input.requestId);
    EXPECT_EQ(decoded->deviceId, input.deviceId);
    EXPECT_EQ(decoded->type, input.type);
    EXPECT_EQ(decoded->targetSpeedRpm, input.targetSpeedRpm);
    EXPECT_EQ(decoded->sentAtMs, input.sentAtMs);
}

TEST(MessageCodecTest, RoundTripsCommandResult) {
    motor::CommandResult input;
    input.requestId = 12345;
    input.deviceId = QStringLiteral("MOTOR-0001");
    input.type = motor::CommandType::Start;
    input.status = motor::CommandStatus::Succeeded;
    input.message = QStringLiteral("OK");
    input.sentAtMs = 1000;
    input.completedAtMs = 1010;

    auto encoded = encodeCommandResult(input);
    auto decoded = decodeCommandResult(encoded);
    ASSERT_TRUE(decoded.has_value());

    EXPECT_EQ(decoded->requestId, input.requestId);
    EXPECT_EQ(decoded->deviceId, input.deviceId);
    EXPECT_EQ(decoded->type, input.type);
    EXPECT_EQ(decoded->status, input.status);
    EXPECT_EQ(decoded->message, input.message);
    EXPECT_EQ(decoded->sentAtMs, input.sentAtMs);
    EXPECT_EQ(decoded->completedAtMs, input.completedAtMs);
}

TEST(MessageCodecTest, RejectsMalformedTelemetry) {
    QByteArray shortData;
    shortData.resize(10);
    auto decoded = decodeTelemetry(shortData);
    EXPECT_FALSE(decoded.has_value());
}
