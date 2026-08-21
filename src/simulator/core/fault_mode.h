#pragma once

namespace motor::simulator {

enum class FaultMode {
    Normal,
    Overload,
    Overheat,
    Stall,
    HighVibration,
    AbnormalVoltage
};

}
