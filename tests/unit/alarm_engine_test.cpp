#include <gtest/gtest.h>
#include "alarm_engine/alarm_engine.h"
#include "alarm_engine/standard_rules.h"
#include "domain/device_types.h"
#include <QCoreApplication>
#include <QSignalSpy>

using namespace motor;

namespace {

DeviceId testId("MOTOR-TEST-001");

Telemetry makeTelemetry(double temp, double current, double speed,
                         double vibration = 2.0, double voltage = 220.0)
{
    Telemetry t;
    t.timestampMs = 0;
    t.speedRpm = static_cast<quint16>(speed);
    t.currentA = static_cast<float>(current);
    t.temperatureC = static_cast<float>(temp);
    t.vibrationMmPerSec = static_cast<float>(vibration);
    t.voltageV = static_cast<float>(voltage);
    t.targetSpeedRpm = 1500;
    t.runtimeSeconds = 1000;
    return t;
}

}

class AlarmEngineTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = createStandardAlarmRules();
        for (const auto& rule : rules) {
            engine.registerRule(rule);
        }
    }

    AlarmEngine engine;
};

TEST_F(AlarmEngineTest, HasAllEightRules)
{
    auto rules = engine.allRules();
    EXPECT_EQ(rules.size(), 8);
}

TEST_F(AlarmEngineTest, HighTemperatureTriggersAfterThreeConsecutive)
{
    auto normal = makeTelemetry(60.0, 10.0, 1500.0);
    auto hot = makeTelemetry(80.0, 10.0, 1500.0);

    auto events1 = engine.processTelemetry(testId, hot, 1000);
    EXPECT_EQ(events1.size(), 0);
    EXPECT_EQ(engine.activeAlarms(testId).size(), 0);

    auto events2 = engine.processTelemetry(testId, hot, 2000);
    EXPECT_EQ(events2.size(), 0);

    auto events3 = engine.processTelemetry(testId, hot, 3000);
    EXPECT_EQ(events3.size(), 1);
    EXPECT_EQ(events3[0].alarm.rule, RuleCode::HighTemperature);
    EXPECT_EQ(events3[0].alarm.severity, AlarmSeverity::Warning);
    EXPECT_EQ(engine.activeAlarms(testId).size(), 1);
}

TEST_F(AlarmEngineTest, HighTemperatureRecoversWhenCooledDown)
{
    auto hot = makeTelemetry(80.0, 10.0, 1500.0);
    auto cool = makeTelemetry(65.0, 10.0, 1500.0);

    for (int i = 0; i < 3; ++i) {
        engine.processTelemetry(testId, hot, (i + 1) * 1000);
    }
    ASSERT_EQ(engine.activeAlarms(testId).size(), 1);

    auto recoverEvents = engine.processTelemetry(testId, cool, 5000);
    EXPECT_EQ(recoverEvents.size(), 1);
    EXPECT_EQ(recoverEvents[0].alarm.state, AlarmState::RecoveredUnacknowledged);
}

TEST_F(AlarmEngineTest, CriticalOverheatTriggersFaster)
{
    auto veryHot = makeTelemetry(100.0, 10.0, 1500.0);

    auto events1 = engine.processTelemetry(testId, veryHot, 1000);
    EXPECT_EQ(events1.size(), 0);

    auto events2 = engine.processTelemetry(testId, veryHot, 2000);
    ASSERT_GE(events2.size(), 1);

    bool hasCritical = false;
    for (const auto& e : events2) {
        if (e.alarm.rule == RuleCode::CriticalOverheat) {
            hasCritical = true;
            EXPECT_EQ(e.alarm.severity, AlarmSeverity::Critical);
        }
    }
    EXPECT_TRUE(hasCritical);
}

TEST_F(AlarmEngineTest, OvercurrentTrigger)
{
    auto highCurrent = makeTelemetry(50.0, 18.0, 1500.0);

    for (int i = 0; i < 2; ++i) {
        engine.processTelemetry(testId, highCurrent, (i + 1) * 1000);
    }
    EXPECT_EQ(engine.activeAlarms(testId).size(), 0);

    engine.processTelemetry(testId, highCurrent, 3000);
    auto active = engine.activeAlarms(testId);
    bool hasOvercurrent = false;
    for (const auto& a : active) {
        if (a.rule == RuleCode::Overcurrent) hasOvercurrent = true;
    }
    EXPECT_TRUE(hasOvercurrent);
}

TEST_F(AlarmEngineTest, StallConditionTriggers)
{
    auto stalled = makeTelemetry(50.0, 20.0, 20.0);

    auto events1 = engine.processTelemetry(testId, stalled, 1000);
    EXPECT_EQ(events1.size(), 0);

    auto events2 = engine.processTelemetry(testId, stalled, 2000);
    bool hasStall = false;
    for (const auto& e : events2) {
        if (e.alarm.rule == RuleCode::Stall) hasStall = true;
    }
    EXPECT_TRUE(hasStall);
}

TEST_F(AlarmEngineTest, HighVibrationNeedsFiveConsecutive)
{
    auto vib = makeTelemetry(50.0, 10.0, 1500.0, 5.5);

    for (int i = 0; i < 4; ++i) {
        engine.processTelemetry(testId, vib, (i + 1) * 1000);
    }
    auto active = engine.activeAlarms(testId);
    for (const auto& a : active) {
        EXPECT_NE(a.rule, RuleCode::HighVibration);
    }

    engine.processTelemetry(testId, vib, 5000);
    active = engine.activeAlarms(testId);
    bool hasVib = false;
    for (const auto& a : active) {
        if (a.rule == RuleCode::HighVibration) hasVib = true;
    }
    EXPECT_TRUE(hasVib);
}

TEST_F(AlarmEngineTest, AbnormalVoltageLow)
{
    auto lowVoltage = makeTelemetry(50.0, 10.0, 1500.0, 2.0, 190.0);

    for (int i = 0; i < 3; ++i) {
        engine.processTelemetry(testId, lowVoltage, (i + 1) * 1000);
    }
    auto active = engine.activeAlarms(testId);
    bool hasVolt = false;
    for (const auto& a : active) {
        if (a.rule == RuleCode::AbnormalVoltage) hasVolt = true;
    }
    EXPECT_TRUE(hasVolt);
}

TEST_F(AlarmEngineTest, AbnormalVoltageHigh)
{
    auto highVoltage = makeTelemetry(50.0, 10.0, 1500.0, 2.0, 250.0);

    for (int i = 0; i < 3; ++i) {
        engine.processTelemetry(testId, highVoltage, (i + 1) * 1000);
    }
    auto active = engine.activeAlarms(testId);
    bool hasVolt = false;
    for (const auto& a : active) {
        if (a.rule == RuleCode::AbnormalVoltage) hasVolt = true;
    }
    EXPECT_TRUE(hasVolt);
}

TEST_F(AlarmEngineTest, AcknowledgeAlarm)
{
    auto hot = makeTelemetry(80.0, 10.0, 1500.0);
    for (int i = 0; i < 3; ++i) {
        engine.processTelemetry(testId, hot, (i + 1) * 1000);
    }

    auto active = engine.activeAlarms(testId);
    ASSERT_FALSE(active.isEmpty());
    auto alarmId = active.first().alarmId;

    auto event = engine.acknowledgeAlarm(alarmId, 10000);
    EXPECT_EQ(event.previousState, AlarmState::ActiveUnacknowledged);
    EXPECT_EQ(event.alarm.state, AlarmState::ActiveAcknowledged);
    EXPECT_GT(event.alarm.acknowledgedAtMs, 0);
}

TEST_F(AlarmEngineTest, CloseAlarmMovesToHistory)
{
    auto hot = makeTelemetry(80.0, 10.0, 1500.0);
    for (int i = 0; i < 3; ++i) {
        engine.processTelemetry(testId, hot, (i + 1) * 1000);
    }

    auto active = engine.activeAlarms(testId);
    ASSERT_FALSE(active.isEmpty());
    auto alarmId = active.first().alarmId;

    int beforeCount = engine.totalAlarmCount();
    auto event = engine.closeAlarm(alarmId, 20000);
    EXPECT_EQ(event.alarm.state, AlarmState::Closed);
    EXPECT_EQ(engine.activeAlarms(testId).size(), 0);
    EXPECT_EQ(engine.totalAlarmCount(), beforeCount);
}

TEST_F(AlarmEngineTest, PeakValueTracked)
{
    auto hot1 = makeTelemetry(80.0, 10.0, 1500.0);
    auto hot2 = makeTelemetry(85.0, 10.0, 1500.0);
    auto hot3 = makeTelemetry(82.0, 10.0, 1500.0);

    engine.processTelemetry(testId, hot1, 1000);
    engine.processTelemetry(testId, hot2, 2000);
    engine.processTelemetry(testId, hot3, 3000);

    auto active = engine.activeAlarms(testId);
    ASSERT_FALSE(active.isEmpty());
    EXPECT_EQ(active.first().peakValue, 85.0);
}

TEST_F(AlarmEngineTest, AlarmSignalEmitted)
{
    QSignalSpy spy(&engine, &AlarmEngine::alarmRaised);

    auto hot = makeTelemetry(80.0, 10.0, 1500.0);
    for (int i = 0; i < 3; ++i) {
        engine.processTelemetry(testId, hot, (i + 1) * 1000);
    }

    EXPECT_EQ(spy.count(), 1);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
