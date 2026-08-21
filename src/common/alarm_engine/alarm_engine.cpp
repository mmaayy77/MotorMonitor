#include "alarm_engine.h"

namespace motor {

AlarmEngine::AlarmEngine(QObject* parent)
    : QObject(parent)
{
}

void AlarmEngine::registerRule(const AlarmRule& rule)
{
    _rules.append(rule);
}

QList<AlarmRule> AlarmEngine::allRules() const
{
    return _rules;
}

QList<AlarmEvent> AlarmEngine::processTelemetry(DeviceId deviceId,
                                                const Telemetry& telemetry,
                                                qint64 timestampMs)
{
    QList<AlarmEvent> events;
    auto& state = _deviceStates[deviceId];

    for (const auto& rule : _rules) {
        auto result = evaluateRule(rule, telemetry);

        if (result.shouldTrigger) {
            state.consecutiveCounts[rule.code]++;
            if (qAbs(result.actualValue) > qAbs(state.runningPeaks[rule.code])) {
                state.runningPeaks[rule.code] = result.actualValue;
            }
        } else {
            state.consecutiveCounts[rule.code] = 0;
            state.runningPeaks[rule.code] = 0.0;
        }

        int count = state.consecutiveCounts[rule.code];
        bool isActive = state.activeAlarms.contains(rule.code);

        if (!isActive && result.shouldTrigger && count >= rule.requiredConsecutive) {
            auto peak = state.runningPeaks[rule.code];
            auto event = triggerAlarm(deviceId, rule, result, timestampMs, peak);
            events.append(event);
            emit alarmRaised(event);
            emit alarmStateChanged(event);
        }

        if (isActive && result.shouldClear) {
            auto event = recoverAlarm(deviceId, rule.code, timestampMs);
            events.append(event);
            emit alarmStateChanged(event);
            state.runningPeaks[rule.code] = 0.0;
        }

        if (isActive) {
            auto& alarm = state.activeAlarms[rule.code];
            if (result.shouldTrigger && qAbs(result.actualValue) > qAbs(alarm.peakValue)) {
                alarm.peakValue = result.actualValue;
            }
            alarm.consecutiveCount = state.consecutiveCounts[rule.code];
        }
    }

    return events;
}

QList<Alarm> AlarmEngine::activeAlarms(DeviceId deviceId) const
{
    if (!_deviceStates.contains(deviceId)) {
        return {};
    }
    return _deviceStates[deviceId].activeAlarms.values();
}

QList<Alarm> AlarmEngine::allAlarms(DeviceId deviceId) const
{
    if (!_deviceStates.contains(deviceId)) {
        return {};
    }
    return _deviceStates[deviceId].alarmHistory;
}

AlarmEvent AlarmEngine::acknowledgeAlarm(QUuid alarmId, qint64 timestampMs)
{
    AlarmEvent event;
    event.eventTimeMs = timestampMs;
    event.previousState = AlarmState::ActiveUnacknowledged;

    for (auto& state : _deviceStates) {
        for (auto& alarm : state.activeAlarms) {
            if (alarm.alarmId == alarmId) {
                event.previousState = alarm.state;
                if (alarm.state == AlarmState::ActiveUnacknowledged) {
                    alarm.state = AlarmState::ActiveAcknowledged;
                } else if (alarm.state == AlarmState::RecoveredUnacknowledged) {
                    alarm.state = AlarmState::RecoveredAcknowledged;
                }
                alarm.acknowledgedAtMs = timestampMs;
                event.alarm = alarm;
                emit alarmStateChanged(event);
                return event;
            }
        }
    }

    for (auto& state : _deviceStates) {
        for (auto& alarm : state.alarmHistory) {
            if (alarm.alarmId == alarmId) {
                event.previousState = alarm.state;
                event.alarm = alarm;
                return event;
            }
        }
    }

    return event;
}

AlarmEvent AlarmEngine::closeAlarm(QUuid alarmId, qint64 timestampMs)
{
    AlarmEvent event;
    event.eventTimeMs = timestampMs;

    for (auto& state : _deviceStates) {
        for (auto it = state.activeAlarms.begin(); it != state.activeAlarms.end(); ++it) {
            if (it.value().alarmId == alarmId) {
                event.previousState = it.value().state;
                it.value().state = AlarmState::Closed;
                it.value().closedAtMs = timestampMs;
                event.alarm = it.value();
                state.alarmHistory.append(it.value());
                state.activeAlarms.erase(it);
                emit alarmStateChanged(event);
                return event;
            }
        }
    }

    for (auto& state : _deviceStates) {
        for (auto& alarm : state.alarmHistory) {
            if (alarm.alarmId == alarmId) {
                event.previousState = alarm.state;
                event.alarm = alarm;
                return event;
            }
        }
    }

    return event;
}

int AlarmEngine::totalAlarmCount() const
{
    int total = 0;
    for (const auto& state : _deviceStates) {
        total += static_cast<int>(state.activeAlarms.size() + state.alarmHistory.size());
    }
    return total;
}

AlarmCheckResult AlarmEngine::evaluateRule(const AlarmRule& rule, const Telemetry& telemetry)
{
    return rule.condition(telemetry);
}

AlarmEvent AlarmEngine::triggerAlarm(DeviceId deviceId, const AlarmRule& rule,
                                     const AlarmCheckResult& result, qint64 timestampMs,
                                     double peakValue)
{
    auto alarm = createAlarm(deviceId, rule, result, timestampMs, peakValue);
    _deviceStates[deviceId].activeAlarms[rule.code] = alarm;

    AlarmEvent event;
    event.alarm = alarm;
    event.previousState = AlarmState::Closed;
    event.eventTimeMs = timestampMs;
    event.evidence = result.evidence;

    return event;
}

AlarmEvent AlarmEngine::recoverAlarm(DeviceId deviceId, RuleCode code, qint64 timestampMs)
{
    auto& state = _deviceStates[deviceId];
    auto alarm = state.activeAlarms[code];

    AlarmEvent event;
    event.alarm = alarm;
    event.previousState = alarm.state;
    event.eventTimeMs = timestampMs;

    if (alarm.state == AlarmState::ActiveUnacknowledged) {
        event.alarm.state = AlarmState::RecoveredUnacknowledged;
    } else if (alarm.state == AlarmState::ActiveAcknowledged) {
        event.alarm.state = AlarmState::RecoveredAcknowledged;
    }
    event.alarm.recoveredAtMs = timestampMs;

    state.activeAlarms[code] = event.alarm;

    return event;
}

Alarm AlarmEngine::createAlarm(DeviceId deviceId, const AlarmRule& rule,
                               const AlarmCheckResult& result, qint64 timestampMs,
                               double peakValue) const
{
    Alarm alarm;
    alarm.alarmId = QUuid::createUuid();
    alarm.deviceId = deviceId;
    alarm.rule = rule.code;
    alarm.severity = rule.severity;
    alarm.state = AlarmState::ActiveUnacknowledged;
    alarm.actualValue = result.actualValue;
    alarm.threshold = result.threshold;
    alarm.peakValue = peakValue;
    alarm.consecutiveCount = rule.requiredConsecutive;
    alarm.triggeredAtMs = timestampMs;
    return alarm;
}

}
