#include "standard_rules.h"
#include "domain/device_types.h"

namespace motor {

QList<AlarmRule> createStandardAlarmRules()
{
    QList<AlarmRule> rules;

    AlarmRule highTemp;
    highTemp.code = RuleCode::HighTemperature;
    highTemp.severity = AlarmSeverity::Warning;
    highTemp.name = QStringLiteral("High Temperature");
    highTemp.description = QStringLiteral("Motor temperature exceeds warning threshold");
    highTemp.threshold = 75.0;
    highTemp.requiredConsecutive = 3;
    highTemp.condition = [threshold = highTemp.threshold](const Telemetry& t) -> AlarmCheckResult {
        AlarmCheckResult r;
        r.actualValue = static_cast<double>(t.temperatureC);
        r.threshold = threshold;
        if (t.temperatureC > threshold) {
            r.shouldTrigger = true;
            r.evidence = QString("Temperature %1°C exceeds threshold %2°C")
                             .arg(t.temperatureC, 0, 'f', 1)
                             .arg(threshold, 0, 'f', 1);
        } else if (t.temperatureC < threshold - 5.0) {
            r.shouldClear = true;
        }
        return r;
    };
    rules.append(highTemp);

    AlarmRule criticalOverheat;
    criticalOverheat.code = RuleCode::CriticalOverheat;
    criticalOverheat.severity = AlarmSeverity::Critical;
    criticalOverheat.name = QStringLiteral("Critical Overheat");
    criticalOverheat.description = QStringLiteral("Motor temperature exceeds critical threshold");
    criticalOverheat.threshold = 95.0;
    criticalOverheat.requiredConsecutive = 2;
    criticalOverheat.condition = [threshold = criticalOverheat.threshold](const Telemetry& t) -> AlarmCheckResult {
        AlarmCheckResult r;
        r.actualValue = static_cast<double>(t.temperatureC);
        r.threshold = threshold;
        if (t.temperatureC > threshold) {
            r.shouldTrigger = true;
            r.evidence = QString("Temperature %1°C exceeds critical threshold %2°C")
                             .arg(t.temperatureC, 0, 'f', 1)
                             .arg(threshold, 0, 'f', 1);
        } else if (t.temperatureC < threshold - 10.0) {
            r.shouldClear = true;
        }
        return r;
    };
    rules.append(criticalOverheat);

    AlarmRule overcurrent;
    overcurrent.code = RuleCode::Overcurrent;
    overcurrent.severity = AlarmSeverity::Warning;
    overcurrent.name = QStringLiteral("Overcurrent");
    overcurrent.description = QStringLiteral("Motor current exceeds rated current");
    overcurrent.threshold = 15.0;
    overcurrent.requiredConsecutive = 3;
    overcurrent.condition = [threshold = overcurrent.threshold](const Telemetry& t) -> AlarmCheckResult {
        AlarmCheckResult r;
        r.actualValue = static_cast<double>(t.currentA);
        r.threshold = threshold;
        if (t.currentA > threshold) {
            r.shouldTrigger = true;
            r.evidence = QString("Current %1A exceeds threshold %2A")
                             .arg(t.currentA, 0, 'f', 2)
                             .arg(threshold, 0, 'f', 2);
        } else if (t.currentA < threshold - 2.0) {
            r.shouldClear = true;
        }
        return r;
    };
    rules.append(overcurrent);

    AlarmRule stall;
    stall.code = RuleCode::Stall;
    stall.severity = AlarmSeverity::Critical;
    stall.name = QStringLiteral("Motor Stall");
    stall.description = QStringLiteral("Motor speed is near zero but current is high");
    stall.threshold = 50.0;
    stall.requiredConsecutive = 2;
    stall.condition = [threshold = stall.threshold](const Telemetry& t) -> AlarmCheckResult {
        AlarmCheckResult r;
        r.actualValue = static_cast<double>(t.speedRpm);
        r.threshold = threshold;
        bool stalled = t.speedRpm < threshold && t.currentA > 10.0;
        if (stalled) {
            r.shouldTrigger = true;
            r.evidence = QString("Speed %1 RPM too low with current %2A (stall condition)")
                             .arg(t.speedRpm)
                             .arg(t.currentA, 0, 'f', 2);
        } else if (t.speedRpm > threshold * 2) {
            r.shouldClear = true;
        }
        return r;
    };
    rules.append(stall);

    AlarmRule highVibration;
    highVibration.code = RuleCode::HighVibration;
    highVibration.severity = AlarmSeverity::Warning;
    highVibration.name = QStringLiteral("High Vibration");
    highVibration.description = QStringLiteral("Vibration level exceeds normal range");
    highVibration.threshold = 4.5;
    highVibration.requiredConsecutive = 5;
    highVibration.condition = [threshold = highVibration.threshold](const Telemetry& t) -> AlarmCheckResult {
        AlarmCheckResult r;
        r.actualValue = static_cast<double>(t.vibrationMmPerSec);
        r.threshold = threshold;
        if (t.vibrationMmPerSec > threshold) {
            r.shouldTrigger = true;
            r.evidence = QString("Vibration %1 mm/s exceeds threshold %2 mm/s")
                             .arg(t.vibrationMmPerSec, 0, 'f', 2)
                             .arg(threshold, 0, 'f', 2);
        } else if (t.vibrationMmPerSec < threshold - 1.0) {
            r.shouldClear = true;
        }
        return r;
    };
    rules.append(highVibration);

    AlarmRule abnormalVoltage;
    abnormalVoltage.code = RuleCode::AbnormalVoltage;
    abnormalVoltage.severity = AlarmSeverity::Warning;
    abnormalVoltage.name = QStringLiteral("Abnormal Voltage");
    abnormalVoltage.description = QStringLiteral("Supply voltage out of normal range (200-240V)");
    abnormalVoltage.threshold = 220.0;
    abnormalVoltage.requiredConsecutive = 3;
    abnormalVoltage.condition = [](const Telemetry& t) -> AlarmCheckResult {
        AlarmCheckResult r;
        r.actualValue = static_cast<double>(t.voltageV);
        r.threshold = 220.0;
        const double lowLimit = 200.0;
        const double highLimit = 240.0;
        if (t.voltageV < lowLimit || t.voltageV > highLimit) {
            r.shouldTrigger = true;
            r.evidence = QString("Voltage %1V out of range [%2V, %3V]")
                             .arg(t.voltageV, 0, 'f', 1)
                             .arg(lowLimit, 0, 'f', 1)
                             .arg(highLimit, 0, 'f', 1);
        } else if (t.voltageV >= lowLimit + 5.0 && t.voltageV <= highLimit - 5.0) {
            r.shouldClear = true;
        }
        return r;
    };
    rules.append(abnormalVoltage);

    AlarmRule heartbeatTimeout;
    heartbeatTimeout.code = RuleCode::HeartbeatTimeout;
    heartbeatTimeout.severity = AlarmSeverity::Critical;
    heartbeatTimeout.name = QStringLiteral("Heartbeat Timeout");
    heartbeatTimeout.description = QStringLiteral("No heartbeat received within timeout period");
    heartbeatTimeout.threshold = 5000.0;
    heartbeatTimeout.requiredConsecutive = 1;
    heartbeatTimeout.condition = [](const Telemetry&) -> AlarmCheckResult {
        return AlarmCheckResult{};
    };
    rules.append(heartbeatTimeout);

    AlarmRule telemetryStale;
    telemetryStale.code = RuleCode::TelemetryStale;
    telemetryStale.severity = AlarmSeverity::Warning;
    telemetryStale.name = QStringLiteral("Telemetry Stale");
    telemetryStale.description = QStringLiteral("Telemetry data is outdated");
    telemetryStale.threshold = 3000.0;
    telemetryStale.requiredConsecutive = 1;
    telemetryStale.condition = [](const Telemetry&) -> AlarmCheckResult {
        return AlarmCheckResult{};
    };
    rules.append(telemetryStale);

    return rules;
}

}
