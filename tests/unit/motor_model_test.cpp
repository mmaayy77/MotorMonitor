#include <gtest/gtest.h>
#include "motor_model.h"
#include "fault_mode.h"
#include <chrono>

using namespace motor::simulator;
using motor::CommandRequest;
using namespace std::chrono_literals;

static CommandRequest makeRequest(motor::CommandType type, quint64 id, quint16 speed = 0) {
    CommandRequest req;
    req.requestId = id;
    req.deviceId = QStringLiteral("MOTOR-0001");
    req.type = type;
    req.targetSpeedRpm = speed;
    req.sentAtMs = 1000;
    return req;
}

TEST(MotorModelTest, StartMovesStoppedToStarting) {
    MotorModel m(QStringLiteral("MOTOR-0001"), 42);
    EXPECT_EQ(m.telemetry().operatingState, motor::OperatingState::Stopped);

    auto result = m.execute(makeRequest(motor::CommandType::Start, 1));
    EXPECT_EQ(result.status, motor::CommandStatus::Succeeded);
    EXPECT_EQ(m.telemetry().operatingState, motor::OperatingState::Starting);
}

TEST(MotorModelTest, SpeedApproachesTarget) {
    MotorModel m(QStringLiteral("MOTOR-0001"), 42);
    m.execute(makeRequest(motor::CommandType::Start, 1));
    m.execute(makeRequest(motor::CommandType::SetTargetSpeed, 2, 1500));

    quint16 lastSpeed = 0;
    for (int i = 0; i < 50; ++i) {
        m.tick(200ms);
        ASSERT_GE(m.telemetry().speedRpm, lastSpeed);
        lastSpeed = m.telemetry().speedRpm;
    }
    EXPECT_EQ(m.telemetry().speedRpm, 1500);
    EXPECT_EQ(m.telemetry().operatingState, motor::OperatingState::Running);
}

TEST(MotorModelTest, StopDeceleratesToZero) {
    MotorModel m(QStringLiteral("MOTOR-0001"), 42);
    m.execute(makeRequest(motor::CommandType::Start, 1));
    m.execute(makeRequest(motor::CommandType::SetTargetSpeed, 2, 1500));
    for (int i = 0; i < 50; ++i) m.tick(200ms);
    ASSERT_EQ(m.telemetry().operatingState, motor::OperatingState::Running);

    auto result = m.execute(makeRequest(motor::CommandType::Stop, 3));
    EXPECT_EQ(result.status, motor::CommandStatus::Succeeded);
    EXPECT_EQ(m.telemetry().operatingState, motor::OperatingState::Stopping);

    for (int i = 0; i < 50; ++i) m.tick(200ms);
    EXPECT_EQ(m.telemetry().speedRpm, 0);
    EXPECT_EQ(m.telemetry().operatingState, motor::OperatingState::Stopped);
}

TEST(MotorModelTest, EmergencyStopIsImmediate) {
    MotorModel m(QStringLiteral("MOTOR-0001"), 42);
    m.execute(makeRequest(motor::CommandType::Start, 1));
    m.execute(makeRequest(motor::CommandType::SetTargetSpeed, 2, 2000));
    for (int i = 0; i < 30; ++i) m.tick(200ms);
    ASSERT_GT(m.telemetry().speedRpm, 0);

    auto result = m.execute(makeRequest(motor::CommandType::EmergencyStop, 4));
    EXPECT_EQ(result.status, motor::CommandStatus::Succeeded);
    EXPECT_EQ(m.telemetry().operatingState, motor::OperatingState::EmergencyStopped);
    EXPECT_EQ(m.telemetry().speedRpm, 0);
    EXPECT_EQ(m.telemetry().targetSpeedRpm, 0);
}

TEST(MotorModelTest, RejectsSpeedAbove3000) {
    MotorModel m(QStringLiteral("MOTOR-0001"), 42);
    m.execute(makeRequest(motor::CommandType::Start, 1));

    auto result = m.execute(makeRequest(motor::CommandType::SetTargetSpeed, 2, 3500));
    EXPECT_EQ(result.status, motor::CommandStatus::ParameterOutOfRange);
}

TEST(MotorModelTest, SameSeedProducesSameTelemetry) {
    MotorModel m1(QStringLiteral("MOTOR-0001"), 42);
    MotorModel m2(QStringLiteral("MOTOR-0001"), 42);

    m1.execute(makeRequest(motor::CommandType::Start, 1));
    m2.execute(makeRequest(motor::CommandType::Start, 1));

    for (int i = 0; i < 10; ++i) {
        auto t1 = m1.tick(200ms);
        auto t2 = m2.tick(200ms);
        EXPECT_EQ(t1.speedRpm, t2.speedRpm);
        EXPECT_FLOAT_EQ(t1.currentA, t2.currentA);
        EXPECT_FLOAT_EQ(t1.temperatureC, t2.temperatureC);
    }
}

TEST(MotorModelTest, DuplicateRequestIdReturnsCachedResult) {
    MotorModel m(QStringLiteral("MOTOR-0001"), 42);
    m.execute(makeRequest(motor::CommandType::Start, 1));
    m.execute(makeRequest(motor::CommandType::SetTargetSpeed, 2, 1500));
    for (int i = 0; i < 50; ++i) m.tick(200ms);

    auto firstResult = m.execute(makeRequest(motor::CommandType::Stop, 100));
    EXPECT_EQ(firstResult.status, motor::CommandStatus::Succeeded);
    EXPECT_EQ(m.telemetry().operatingState, motor::OperatingState::Stopping);

    for (int i = 0; i < 50; ++i) m.tick(200ms);
    EXPECT_EQ(m.telemetry().operatingState, motor::OperatingState::Stopped);

    auto secondResult = m.execute(makeRequest(motor::CommandType::Stop, 100));
    EXPECT_EQ(secondResult.status, firstResult.status);
    EXPECT_EQ(secondResult.completedAtMs, firstResult.completedAtMs);
}

TEST(MotorModelTest, OverloadFaultRaisesCurrent) {
    MotorModel m(QStringLiteral("MOTOR-0001"), 42);
    m.execute(makeRequest(motor::CommandType::Start, 1));
    m.execute(makeRequest(motor::CommandType::SetTargetSpeed, 2, 1500));
    for (int i = 0; i < 50; ++i) m.tick(200ms);
    float normalCurrent = m.telemetry().currentA;

    m.setFaultMode(FaultMode::Overload);
    m.tick(200ms);
    EXPECT_GT(m.telemetry().currentA, normalCurrent + 5.0f);
}

TEST(MotorModelTest, StallFaultClampsSpeed) {
    MotorModel m(QStringLiteral("MOTOR-0001"), 42);
    m.execute(makeRequest(motor::CommandType::Start, 1));
    m.execute(makeRequest(motor::CommandType::SetTargetSpeed, 2, 1000));
    for (int i = 0; i < 50; ++i) m.tick(200ms);
    ASSERT_EQ(m.telemetry().speedRpm, 1000);

    m.setFaultMode(FaultMode::Stall);
    m.tick(200ms);
    EXPECT_LT(m.telemetry().speedRpm, 100);
    EXPECT_GT(m.telemetry().currentA, 8.0f);
}

TEST(MotorModelTest, OverheatFaultRaisesTemperature) {
    MotorModel m(QStringLiteral("MOTOR-0001"), 42);
    m.execute(makeRequest(motor::CommandType::Start, 1));
    m.execute(makeRequest(motor::CommandType::SetTargetSpeed, 2, 1500));
    for (int i = 0; i < 50; ++i) m.tick(200ms);
    float normalTemp = m.telemetry().temperatureC;

    m.setFaultMode(FaultMode::Overheat);
    m.tick(200ms);
    EXPECT_GT(m.telemetry().temperatureC, normalTemp + 40.0f);
}
