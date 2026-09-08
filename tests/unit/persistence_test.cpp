#include <gtest/gtest.h>
#include "persistence/sqlite_repository.h"
#include "domain/device_types.h"
#include "domain/alarm_types.h"
#include "domain/command_types.h"
#include "domain/persistence_types.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryFile>
#include <QUuid>

using namespace motor;

namespace {

DeviceId testId("MOTOR-0001");

Telemetry makeTelemetry(float temp, float current, quint16 speed, qint64 ts)
{
    Telemetry t;
    t.deviceId = testId;
    t.timestampMs = ts;
    t.operatingState = OperatingState::Running;
    t.healthState = HealthState::Normal;
    t.temperatureC = temp;
    t.voltageV = 220.0F;
    t.currentA = current;
    t.speedRpm = speed;
    t.targetSpeedRpm = 1500;
    t.vibrationMmPerSec = 2.0F;
    t.runtimeSeconds = 1000;
    t.deviceAlarmBits = 0;
    return t;
}

AlarmEvent makeAlarmEvent(RuleCode code, double value, qint64 ts)
{
    Alarm a;
    a.alarmId = QUuid::createUuid();
    a.deviceId = testId;
    a.rule = code;
    a.severity = AlarmSeverity::Warning;
    a.state = AlarmState::ActiveUnacknowledged;
    a.actualValue = value;
    a.threshold = 75.0;
    a.peakValue = value;
    a.consecutiveCount = 3;
    a.triggeredAtMs = ts;

    AlarmEvent e;
    e.alarm = a;
    e.previousState = AlarmState::Closed;
    e.eventTimeMs = ts;
    e.evidence = QStringLiteral("test evidence");
    return e;
}

CommandResult makeCommandResult(quint64 requestId, CommandStatus status, qint64 ts)
{
    CommandResult r;
    r.requestId = requestId;
    r.deviceId = testId;
    r.type = CommandType::Start;
    r.status = status;
    r.message = QStringLiteral("ok");
    r.sentAtMs = ts - 100;
    r.completedAtMs = ts;
    return r;
}

DeviceEvent makeDeviceEvent(ConnectionState state, qint64 ts)
{
    DeviceEvent e;
    e.deviceId = testId;
    e.state = state;
    e.reason = QStringLiteral("test reason");
    e.eventTimeMs = ts;
    return e;
}

}

class PersistenceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        _tempFile = new QTemporaryFile();
        _tempFile->open();
        _path = _tempFile->fileName();
        _tempFile->close();

        repo = std::make_unique<SqliteRepository>(_path);
        ASSERT_TRUE(repo->initialize());
        ASSERT_TRUE(repo->isOpen());
    }

    void TearDown() override
    {
        repo.reset();
        delete _tempFile;
    }

    QTemporaryFile* _tempFile{nullptr};
    QString _path;
    std::unique_ptr<SqliteRepository> repo;
};

TEST_F(PersistenceTest, SaveAndQueryTelemetry)
{
    Telemetry t1 = makeTelemetry(60.0F, 10.0F, 1500, 1000);
    Telemetry t2 = makeTelemetry(65.0F, 11.0F, 1450, 2000);

    EXPECT_TRUE(repo->saveTelemetry(t1));
    EXPECT_TRUE(repo->saveTelemetry(t2));

    TelemetryQuery q;
    q.deviceId = testId;
    q.fromMs = 0;
    q.toMs = 3000;
    q.limit = 10;

    auto result = repo->queryTelemetry(q);
    ASSERT_EQ(result.size(), 2);
    EXPECT_FLOAT_EQ(result[0].temperatureC, 65.0F);
    EXPECT_FLOAT_EQ(result[1].temperatureC, 60.0F);
}

TEST_F(PersistenceTest, EnablesWriteAheadLogging)
{
    const QString connectionName = QStringLiteral("wal_verify_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(_path);
    ASSERT_TRUE(db.open());

    {
        QSqlQuery query(db);
        ASSERT_TRUE(query.exec(QStringLiteral("PRAGMA journal_mode")));
        ASSERT_TRUE(query.next());
        EXPECT_EQ(query.value(0).toString().toLower(), QStringLiteral("wal"));
    }

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

TEST_F(PersistenceTest, QueryTelemetryWithTimeRange)
{
    repo->saveTelemetry(makeTelemetry(60.0F, 10.0F, 1500, 1000));
    repo->saveTelemetry(makeTelemetry(70.0F, 12.0F, 1500, 2000));
    repo->saveTelemetry(makeTelemetry(80.0F, 14.0F, 1500, 3000));

    TelemetryQuery q;
    q.deviceId = testId;
    q.fromMs = 1500;
    q.toMs = 2500;
    q.limit = 10;

    auto result = repo->queryTelemetry(q);
    ASSERT_EQ(result.size(), 1);
    EXPECT_FLOAT_EQ(result[0].temperatureC, 70.0F);
}

TEST_F(PersistenceTest, SaveAndQueryAlarms)
{
    auto event = makeAlarmEvent(RuleCode::HighTemperature, 82.0, 1000);
    EXPECT_TRUE(repo->saveAlarmEvent(event));

    AlarmQuery q;
    q.deviceId = testId;
    q.fromMs = 0;
    q.toMs = 2000;
    q.limit = 10;

    auto result = repo->queryAlarms(q);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].rule, RuleCode::HighTemperature);
    EXPECT_DOUBLE_EQ(result[0].actualValue, 82.0);
    EXPECT_EQ(result[0].state, AlarmState::ActiveUnacknowledged);
}

TEST_F(PersistenceTest, UpdateAlarmState)
{
    auto event = makeAlarmEvent(RuleCode::Overcurrent, 18.0, 1000);
    repo->saveAlarmEvent(event);

    event.alarm.state = AlarmState::ActiveAcknowledged;
    event.alarm.acknowledgedAtMs = 2000;
    repo->saveAlarmEvent(event);

    AlarmQuery q;
    q.deviceId = testId;
    q.fromMs = 0;
    q.toMs = 3000;
    q.limit = 10;

    auto result = repo->queryAlarms(q);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].state, AlarmState::ActiveAcknowledged);
    EXPECT_EQ(result[0].acknowledgedAtMs, 2000);
}

TEST_F(PersistenceTest, SaveAndQueryCommands)
{
    auto r1 = makeCommandResult(1, CommandStatus::Succeeded, 1000);
    auto r2 = makeCommandResult(2, CommandStatus::TimedOut, 2000);

    EXPECT_TRUE(repo->saveCommandResult(testId, r1));
    EXPECT_TRUE(repo->saveCommandResult(testId, r2));

    CommandQuery q;
    q.deviceId = testId;
    q.fromMs = 0;
    q.toMs = 3000;
    q.limit = 10;

    auto result = repo->queryCommands(q);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].requestId, 2);
    EXPECT_EQ(result[0].status, CommandStatus::TimedOut);
}

TEST_F(PersistenceTest, SaveAndQueryDeviceEvents)
{
    auto e1 = makeDeviceEvent(ConnectionState::Online, 1000);
    auto e2 = makeDeviceEvent(ConnectionState::Disconnected, 2000);

    EXPECT_TRUE(repo->saveDeviceEvent(e1));
    EXPECT_TRUE(repo->saveDeviceEvent(e2));

    DeviceEventQuery q;
    q.deviceId = testId;
    q.fromMs = 0;
    q.toMs = 3000;
    q.limit = 10;

    auto result = repo->queryDeviceEvents(q);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].state, ConnectionState::Disconnected);
}

TEST_F(PersistenceTest, QueryWithLimitAndOffset)
{
    for (int i = 0; i < 5; ++i) {
        repo->saveTelemetry(makeTelemetry(60.0F + static_cast<float>(i), 10.0F, 1500, (i + 1) * 1000));
    }

    TelemetryQuery q;
    q.deviceId = testId;
    q.fromMs = 0;
    q.toMs = 6000;
    q.limit = 2;
    q.offset = 1;

    auto result = repo->queryTelemetry(q);
    ASSERT_EQ(result.size(), 2);
    EXPECT_FLOAT_EQ(result[0].temperatureC, 63.0F);
    EXPECT_FLOAT_EQ(result[1].temperatureC, 62.0F);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
