#include "sqlite_repository.h"
#include <QSqlError>
#include <QSqlRecord>
#include <QFile>
#include <QDir>
#include <QUuid>

namespace motor {

SqliteRepository::SqliteRepository(QString databasePath, QObject* parent)
    : Repository()
{
    Q_UNUSED(parent)
    _path = std::move(databasePath);
}

SqliteRepository::~SqliteRepository()
{
    if (_db.isOpen()) {
        _db.close();
    }
}

bool SqliteRepository::initialize()
{
    if (_initialized) {
        return true;
    }

    QDir dir = QFileInfo(_path).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    _db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("motor_repo_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    _db.setDatabaseName(_path);

    if (!_db.open()) {
        return false;
    }

    if (!createTables()) {
        _db.close();
        return false;
    }

    _initialized = true;
    return true;
}

bool SqliteRepository::isOpen() const
{
    return _db.isOpen();
}

bool SqliteRepository::createTables()
{
    bool ok = true;
    ok &= execSchema(R"(
        CREATE TABLE IF NOT EXISTS telemetry (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            timestamp_ms INTEGER NOT NULL,
            operating_state INTEGER NOT NULL,
            health_state INTEGER NOT NULL,
            temperature_c REAL NOT NULL,
            voltage_v REAL NOT NULL,
            current_a REAL NOT NULL,
            speed_rpm INTEGER NOT NULL,
            target_speed_rpm INTEGER NOT NULL,
            vibration_mm_per_sec REAL NOT NULL,
            runtime_seconds INTEGER NOT NULL,
            device_alarm_bits INTEGER NOT NULL
        )
    )");

    ok &= execSchema(R"(
        CREATE INDEX IF NOT EXISTS idx_telemetry_device_time
        ON telemetry(device_id, timestamp_ms)
    )");

    ok &= execSchema(R"(
        CREATE TABLE IF NOT EXISTS alarms (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            alarm_id TEXT UNIQUE NOT NULL,
            device_id TEXT NOT NULL,
            rule_code INTEGER NOT NULL,
            severity INTEGER NOT NULL,
            state INTEGER NOT NULL,
            actual_value REAL NOT NULL,
            threshold_value REAL NOT NULL,
            peak_value REAL NOT NULL,
            consecutive_count INTEGER NOT NULL,
            triggered_at_ms INTEGER NOT NULL,
            acknowledged_at_ms INTEGER NOT NULL,
            recovered_at_ms INTEGER NOT NULL,
            closed_at_ms INTEGER NOT NULL
        )
    )");

    ok &= execSchema(R"(
        CREATE INDEX IF NOT EXISTS idx_alarms_device_time
        ON alarms(device_id, triggered_at_ms)
    )");

    ok &= execSchema(R"(
        CREATE TABLE IF NOT EXISTS commands (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            request_id INTEGER NOT NULL,
            device_id TEXT NOT NULL,
            command_type INTEGER NOT NULL,
            status INTEGER NOT NULL,
            message TEXT NOT NULL,
            sent_at_ms INTEGER NOT NULL,
            completed_at_ms INTEGER NOT NULL
        )
    )");

    ok &= execSchema(R"(
        CREATE INDEX IF NOT EXISTS idx_commands_device_time
        ON commands(device_id, completed_at_ms)
    )");

    ok &= execSchema(R"(
        CREATE TABLE IF NOT EXISTS device_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            connection_state INTEGER NOT NULL,
            reason TEXT NOT NULL,
            event_time_ms INTEGER NOT NULL
        )
    )");

    ok &= execSchema(R"(
        CREATE INDEX IF NOT EXISTS idx_device_events_time
        ON device_events(device_id, event_time_ms)
    )");

    ok &= execSchema(R"(
        CREATE TABLE IF NOT EXISTS device_config (
            device_id TEXT PRIMARY KEY,
            display_name TEXT NOT NULL,
            host TEXT NOT NULL,
            port INTEGER NOT NULL,
            enabled INTEGER NOT NULL,
            auto_connect INTEGER NOT NULL,
            rated_current_a REAL NOT NULL
        )
    )");

    return ok;
}

bool SqliteRepository::execSchema(const QString& sql)
{
    QSqlQuery query(_db);
    if (!query.exec(sql)) {
        return false;
    }
    return true;
}

bool SqliteRepository::saveTelemetry(const Telemetry& telemetry)
{
    QSqlQuery query(_db);
    query.prepare(R"(
        INSERT INTO telemetry
        (device_id, timestamp_ms, operating_state, health_state,
         temperature_c, voltage_v, current_a, speed_rpm, target_speed_rpm,
         vibration_mm_per_sec, runtime_seconds, device_alarm_bits)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(telemetry.deviceId);
    query.addBindValue(telemetry.timestampMs);
    query.addBindValue(static_cast<int>(telemetry.operatingState));
    query.addBindValue(static_cast<int>(telemetry.healthState));
    query.addBindValue(static_cast<double>(telemetry.temperatureC));
    query.addBindValue(static_cast<double>(telemetry.voltageV));
    query.addBindValue(static_cast<double>(telemetry.currentA));
    query.addBindValue(static_cast<int>(telemetry.speedRpm));
    query.addBindValue(static_cast<int>(telemetry.targetSpeedRpm));
    query.addBindValue(static_cast<double>(telemetry.vibrationMmPerSec));
    query.addBindValue(static_cast<qint64>(telemetry.runtimeSeconds));
    query.addBindValue(static_cast<int>(telemetry.deviceAlarmBits));
    return query.exec();
}

QList<Telemetry> SqliteRepository::queryTelemetry(const TelemetryQuery& query)
{
    QList<Telemetry> result;
    QSqlQuery q(_db);
    QString sql;
    if (query.deviceId.isEmpty()) {
        sql = QStringLiteral(
            "SELECT * FROM telemetry "
            "WHERE timestamp_ms >= ? AND timestamp_ms <= ? "
            "ORDER BY timestamp_ms DESC LIMIT ? OFFSET ?");
        q.prepare(sql);
        q.addBindValue(query.fromMs);
        q.addBindValue(query.toMs);
        q.addBindValue(query.limit);
        q.addBindValue(query.offset);
    } else {
        sql = QStringLiteral(
            "SELECT * FROM telemetry "
            "WHERE device_id = ? AND timestamp_ms >= ? AND timestamp_ms <= ? "
            "ORDER BY timestamp_ms DESC LIMIT ? OFFSET ?");
        q.prepare(sql);
        q.addBindValue(query.deviceId);
        q.addBindValue(query.fromMs);
        q.addBindValue(query.toMs);
        q.addBindValue(query.limit);
        q.addBindValue(query.offset);
    }

    if (!q.exec()) {
        return result;
    }

    while (q.next()) {
        result.append(readTelemetry(q));
    }
    return result;
}

Telemetry SqliteRepository::readTelemetry(QSqlQuery& query) const
{
    Telemetry t;
    t.deviceId = query.value(QStringLiteral("device_id")).toString();
    t.timestampMs = query.value(QStringLiteral("timestamp_ms")).toLongLong();
    t.operatingState = static_cast<OperatingState>(query.value(QStringLiteral("operating_state")).toInt());
    t.healthState = static_cast<HealthState>(query.value(QStringLiteral("health_state")).toInt());
    t.temperatureC = query.value(QStringLiteral("temperature_c")).toFloat();
    t.voltageV = query.value(QStringLiteral("voltage_v")).toFloat();
    t.currentA = query.value(QStringLiteral("current_a")).toFloat();
    t.speedRpm = static_cast<quint16>(query.value(QStringLiteral("speed_rpm")).toUInt());
    t.targetSpeedRpm = static_cast<quint16>(query.value(QStringLiteral("target_speed_rpm")).toUInt());
    t.vibrationMmPerSec = query.value(QStringLiteral("vibration_mm_per_sec")).toFloat();
    t.runtimeSeconds = static_cast<quint64>(query.value(QStringLiteral("runtime_seconds")).toULongLong());
    t.deviceAlarmBits = static_cast<quint32>(query.value(QStringLiteral("device_alarm_bits")).toUInt());
    return t;
}

bool SqliteRepository::saveAlarmEvent(const AlarmEvent& event)
{
    const auto& a = event.alarm;
    QSqlQuery query(_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO alarms
        (alarm_id, device_id, rule_code, severity, state, actual_value,
         threshold_value, peak_value, consecutive_count, triggered_at_ms,
         acknowledged_at_ms, recovered_at_ms, closed_at_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(a.alarmId.toString(QUuid::WithoutBraces));
    query.addBindValue(a.deviceId);
    query.addBindValue(static_cast<int>(a.rule));
    query.addBindValue(static_cast<int>(a.severity));
    query.addBindValue(static_cast<int>(a.state));
    query.addBindValue(a.actualValue);
    query.addBindValue(a.threshold);
    query.addBindValue(a.peakValue);
    query.addBindValue(a.consecutiveCount);
    query.addBindValue(a.triggeredAtMs);
    query.addBindValue(a.acknowledgedAtMs);
    query.addBindValue(a.recoveredAtMs);
    query.addBindValue(a.closedAtMs);
    return query.exec();
}

QList<Alarm> SqliteRepository::queryAlarms(const AlarmQuery& query)
{
    QList<Alarm> result;
    QSqlQuery q(_db);
    QString sql = QStringLiteral("SELECT * FROM alarms WHERE triggered_at_ms >= ? AND triggered_at_ms <= ?");
    if (query.deviceId.has_value()) {
        sql += QStringLiteral(" AND device_id = ?");
    }
    if (query.state.has_value()) {
        sql += QStringLiteral(" AND state = ?");
    }
    sql += QStringLiteral(" ORDER BY triggered_at_ms DESC LIMIT ? OFFSET ?");
    q.prepare(sql);
    q.addBindValue(query.fromMs);
    q.addBindValue(query.toMs);
    if (query.deviceId.has_value()) {
        q.addBindValue(query.deviceId.value());
    }
    if (query.state.has_value()) {
        q.addBindValue(static_cast<int>(query.state.value()));
    }
    q.addBindValue(query.limit);
    q.addBindValue(query.offset);

    if (!q.exec()) {
        return result;
    }

    while (q.next()) {
        result.append(readAlarm(q));
    }
    return result;
}

Alarm SqliteRepository::readAlarm(QSqlQuery& query) const
{
    Alarm a;
    a.alarmId = QUuid(query.value(QStringLiteral("alarm_id")).toString());
    a.deviceId = query.value(QStringLiteral("device_id")).toString();
    a.rule = static_cast<RuleCode>(query.value(QStringLiteral("rule_code")).toInt());
    a.severity = static_cast<AlarmSeverity>(query.value(QStringLiteral("severity")).toInt());
    a.state = static_cast<AlarmState>(query.value(QStringLiteral("state")).toInt());
    a.actualValue = query.value(QStringLiteral("actual_value")).toDouble();
    a.threshold = query.value(QStringLiteral("threshold_value")).toDouble();
    a.peakValue = query.value(QStringLiteral("peak_value")).toDouble();
    a.consecutiveCount = query.value(QStringLiteral("consecutive_count")).toInt();
    a.triggeredAtMs = query.value(QStringLiteral("triggered_at_ms")).toLongLong();
    a.acknowledgedAtMs = query.value(QStringLiteral("acknowledged_at_ms")).toLongLong();
    a.recoveredAtMs = query.value(QStringLiteral("recovered_at_ms")).toLongLong();
    a.closedAtMs = query.value(QStringLiteral("closed_at_ms")).toLongLong();
    return a;
}

bool SqliteRepository::saveCommandResult(DeviceId deviceId, const CommandResult& result)
{
    QSqlQuery query(_db);
    query.prepare(R"(
        INSERT INTO commands
        (request_id, device_id, command_type, status, message, sent_at_ms, completed_at_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(static_cast<qint64>(result.requestId));
    query.addBindValue(deviceId);
    query.addBindValue(static_cast<int>(result.type));
    query.addBindValue(static_cast<int>(result.status));
    query.addBindValue(result.message);
    query.addBindValue(result.sentAtMs);
    query.addBindValue(result.completedAtMs);
    return query.exec();
}

QList<CommandResult> SqliteRepository::queryCommands(const CommandQuery& query)
{
    QList<CommandResult> result;
    QSqlQuery q(_db);
    QString sql = QStringLiteral("SELECT * FROM commands WHERE completed_at_ms >= ? AND completed_at_ms <= ?");
    if (query.deviceId.has_value()) {
        sql += QStringLiteral(" AND device_id = ?");
    }
    sql += QStringLiteral(" ORDER BY completed_at_ms DESC LIMIT ? OFFSET ?");
    q.prepare(sql);
    q.addBindValue(query.fromMs);
    q.addBindValue(query.toMs);
    if (query.deviceId.has_value()) {
        q.addBindValue(query.deviceId.value());
    }
    q.addBindValue(query.limit);
    q.addBindValue(query.offset);

    if (!q.exec()) {
        return result;
    }

    while (q.next()) {
        result.append(readCommand(q));
    }
    return result;
}

CommandResult SqliteRepository::readCommand(QSqlQuery& query) const
{
    CommandResult r;
    r.requestId = static_cast<quint64>(query.value(QStringLiteral("request_id")).toULongLong());
    r.deviceId = query.value(QStringLiteral("device_id")).toString();
    r.type = static_cast<CommandType>(query.value(QStringLiteral("command_type")).toInt());
    r.status = static_cast<CommandStatus>(query.value(QStringLiteral("status")).toInt());
    r.message = query.value(QStringLiteral("message")).toString();
    r.sentAtMs = query.value(QStringLiteral("sent_at_ms")).toLongLong();
    r.completedAtMs = query.value(QStringLiteral("completed_at_ms")).toLongLong();
    return r;
}

bool SqliteRepository::saveDeviceEvent(const DeviceEvent& event)
{
    QSqlQuery query(_db);
    query.prepare(R"(
        INSERT INTO device_events
        (device_id, connection_state, reason, event_time_ms)
        VALUES (?, ?, ?, ?)
    )");
    query.addBindValue(event.deviceId);
    query.addBindValue(static_cast<int>(event.state));
    query.addBindValue(event.reason);
    query.addBindValue(event.eventTimeMs);
    return query.exec();
}

QList<DeviceEvent> SqliteRepository::queryDeviceEvents(const DeviceEventQuery& query)
{
    QList<DeviceEvent> result;
    QSqlQuery q(_db);
    QString sql = QStringLiteral("SELECT * FROM device_events WHERE event_time_ms >= ? AND event_time_ms <= ?");
    if (query.deviceId.has_value()) {
        sql += QStringLiteral(" AND device_id = ?");
    }
    sql += QStringLiteral(" ORDER BY event_time_ms DESC LIMIT ? OFFSET ?");
    q.prepare(sql);
    q.addBindValue(query.fromMs);
    q.addBindValue(query.toMs);
    if (query.deviceId.has_value()) {
        q.addBindValue(query.deviceId.value());
    }
    q.addBindValue(query.limit);
    q.addBindValue(query.offset);

    if (!q.exec()) {
        return result;
    }

    while (q.next()) {
        result.append(readDeviceEvent(q));
    }
    return result;
}

DeviceEvent SqliteRepository::readDeviceEvent(QSqlQuery& query) const
{
    DeviceEvent e;
    e.deviceId = query.value(QStringLiteral("device_id")).toString();
    e.state = static_cast<ConnectionState>(query.value(QStringLiteral("connection_state")).toInt());
    e.reason = query.value(QStringLiteral("reason")).toString();
    e.eventTimeMs = query.value(QStringLiteral("event_time_ms")).toLongLong();
    return e;
}

bool SqliteRepository::saveDeviceConfig(const DeviceConfig& config)
{
    QSqlQuery q(_db);
    q.prepare(R"(
        INSERT OR REPLACE INTO device_config
        (device_id, display_name, host, port, enabled, auto_connect, rated_current_a)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    q.addBindValue(config.deviceId);
    q.addBindValue(config.displayName.isEmpty() ? config.deviceId : config.displayName);
    q.addBindValue(config.host);
    q.addBindValue(static_cast<int>(config.port));
    q.addBindValue(config.enabled ? 1 : 0);
    q.addBindValue(config.autoConnect ? 1 : 0);
    q.addBindValue(static_cast<double>(config.ratedCurrentA));
    return q.exec();
}

QList<DeviceConfig> SqliteRepository::loadAllDeviceConfigs()
{
    QList<DeviceConfig> result;
    QSqlQuery q(_db);
    q.prepare(QStringLiteral("SELECT * FROM device_config"));
    if (!q.exec()) return result;
    while (q.next()) {
        DeviceConfig cfg;
        cfg.deviceId = q.value(QStringLiteral("device_id")).toString();
        cfg.displayName = q.value(QStringLiteral("display_name")).toString();
        cfg.host = q.value(QStringLiteral("host")).toString();
        cfg.port = static_cast<quint16>(q.value(QStringLiteral("port")).toInt());
        cfg.enabled = q.value(QStringLiteral("enabled")).toInt() != 0;
        cfg.autoConnect = q.value(QStringLiteral("auto_connect")).toInt() != 0;
        cfg.ratedCurrentA = static_cast<float>(q.value(QStringLiteral("rated_current_a")).toDouble());
        result.append(cfg);
    }
    return result;
}

bool SqliteRepository::deleteDeviceConfig(DeviceId deviceId)
{
    QSqlQuery q(_db);
    q.prepare(QStringLiteral("DELETE FROM device_config WHERE device_id = ?"));
    q.addBindValue(deviceId);
    return q.exec();
}

}
