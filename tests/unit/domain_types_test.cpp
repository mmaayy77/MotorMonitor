#include <gtest/gtest.h>
#include "domain/device_types.h"
#include "domain/command_types.h"
#include "domain/alarm_types.h"
#include "domain/persistence_types.h"

TEST(DeviceIdTest, AcceptsMotorIdAndRejectsMalformedIds) {
    EXPECT_TRUE(motor::isValidDeviceId(QStringLiteral("MOTOR-0001")));
    EXPECT_TRUE(motor::isValidDeviceId(QStringLiteral("MOTOR-0100")));
    EXPECT_FALSE(motor::isValidDeviceId(QStringLiteral("MOTOR-1")));
    EXPECT_FALSE(motor::isValidDeviceId(QStringLiteral("PUMP-0001")));
}

TEST(TelemetryTest, DefaultsToSafeStoppedState) {
    const motor::Telemetry value{};
    EXPECT_EQ(value.operatingState, motor::OperatingState::Stopped);
    EXPECT_EQ(value.healthState, motor::HealthState::Normal);
    EXPECT_EQ(value.speedRpm, 0);
    EXPECT_EQ(value.temperatureC, 20.0F);
}
