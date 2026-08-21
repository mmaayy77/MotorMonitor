#pragma once

#include "domain/alarm_types.h"
#include "domain/device_types.h"
#include <functional>

namespace motor {

class Telemetry;

struct AlarmCheckResult {
    bool shouldTrigger{false};
    bool shouldClear{false};
    double actualValue{0.0};
    double threshold{0.0};
    QString evidence;
};

using AlarmCondition = std::function<AlarmCheckResult(const Telemetry&)>;

struct AlarmRule {
    RuleCode code;
    AlarmSeverity severity;
    QString name;
    QString description;
    double threshold;
    int requiredConsecutive;
    AlarmCondition condition;
};

}
