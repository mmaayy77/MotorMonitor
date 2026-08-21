#pragma once

#include "alarm_rule.h"
#include "domain/alarm_types.h"
#include "domain/device_types.h"
#include <QObject>
#include <QHash>
#include <QList>
#include <memory>

namespace motor {

class AlarmEngine : public QObject {
    Q_OBJECT
public:
    explicit AlarmEngine(QObject* parent = nullptr);

    void registerRule(const AlarmRule& rule);
    QList<AlarmRule> allRules() const;

    QList<AlarmEvent> processTelemetry(DeviceId deviceId, const Telemetry& telemetry,
                                       qint64 timestampMs);

    QList<Alarm> activeAlarms(DeviceId deviceId) const;
    QList<Alarm> allAlarms(DeviceId deviceId) const;

    AlarmEvent acknowledgeAlarm(QUuid alarmId, qint64 timestampMs);
    AlarmEvent closeAlarm(QUuid alarmId, qint64 timestampMs);

    int totalAlarmCount() const;

signals:
    void alarmRaised(const AlarmEvent& event);
    void alarmStateChanged(const AlarmEvent& event);

private:
    struct DeviceAlarmState {
        QHash<RuleCode, Alarm> activeAlarms;
        QList<Alarm> alarmHistory;
        QHash<RuleCode, int> consecutiveCounts;
        QHash<RuleCode, double> runningPeaks;
    };

    QList<AlarmRule> _rules;
    QHash<DeviceId, DeviceAlarmState> _deviceStates;

    AlarmCheckResult evaluateRule(const AlarmRule& rule, const Telemetry& telemetry);
    AlarmEvent triggerAlarm(DeviceId deviceId, const AlarmRule& rule,
                            const AlarmCheckResult& result, qint64 timestampMs,
                            double peakValue);
    AlarmEvent recoverAlarm(DeviceId deviceId, RuleCode code, qint64 timestampMs);
    Alarm createAlarm(DeviceId deviceId, const AlarmRule& rule,
                      const AlarmCheckResult& result, qint64 timestampMs,
                      double peakValue) const;
};

}
