#pragma once

#include <QString>
#include <QList>
#include "domain/device_types.h"

namespace motor {

struct AppConfig {
    bool autoStartSimulator{true};
    QString simulatorPath;
    quint16 defaultPort{9000};
    QString defaultHost{QStringLiteral("127.0.0.1")};
    QString logLevel{QStringLiteral("Info")};
    int batchGenerateCount{100};

    QList<DeviceConfig> devicePresets;

    bool loadFromFile(const QString& path);
    bool saveToFile(const QString& path) const;
};

}