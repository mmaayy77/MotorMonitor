#include "app_config.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>

namespace motor {

bool AppConfig::loadFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    auto obj = doc.object();
    autoStartSimulator = obj.value(QStringLiteral("autoStartSimulator")).toBool(true);
    simulatorPath = obj.value(QStringLiteral("simulatorPath")).toString();
    defaultPort = static_cast<quint16>(obj.value(QStringLiteral("defaultPort")).toInt(9000));
    defaultHost = obj.value(QStringLiteral("defaultHost")).toString(QStringLiteral("127.0.0.1"));
    logLevel = obj.value(QStringLiteral("logLevel")).toString(QStringLiteral("Info"));
    batchGenerateCount = obj.value(QStringLiteral("batchGenerateCount")).toInt(100);

    devicePresets.clear();
    auto presets = obj.value(QStringLiteral("devicePresets")).toArray();
    for (const auto& val : presets) {
        auto item = val.toObject();
        DeviceConfig cfg;
        cfg.deviceId = item.value(QStringLiteral("deviceId")).toString();
        cfg.displayName = item.value(QStringLiteral("displayName")).toString();
        cfg.host = item.value(QStringLiteral("host")).toString(QStringLiteral("127.0.0.1"));
        cfg.port = static_cast<quint16>(item.value(QStringLiteral("port")).toInt(9000));
        cfg.enabled = item.value(QStringLiteral("enabled")).toBool(true);
        cfg.autoConnect = item.value(QStringLiteral("autoConnect")).toBool(true);
        cfg.ratedCurrentA = static_cast<float>(item.value(QStringLiteral("ratedCurrentA")).toDouble(12.0));
        cfg.protocolType = item.value(QStringLiteral("protocolType")).toInt(0);
        if (!cfg.deviceId.isEmpty()) {
            devicePresets.append(cfg);
        }
    }

    return true;
}

bool AppConfig::saveToFile(const QString& path) const
{
    QJsonObject obj;
    obj[QStringLiteral("autoStartSimulator")] = autoStartSimulator;
    obj[QStringLiteral("simulatorPath")] = simulatorPath;
    obj[QStringLiteral("defaultPort")] = defaultPort;
    obj[QStringLiteral("defaultHost")] = defaultHost;
    obj[QStringLiteral("logLevel")] = logLevel;
    obj[QStringLiteral("batchGenerateCount")] = batchGenerateCount;

    QJsonArray presets;
    for (const auto& cfg : devicePresets) {
        QJsonObject item;
        item[QStringLiteral("deviceId")] = cfg.deviceId;
        item[QStringLiteral("displayName")] = cfg.displayName;
        item[QStringLiteral("host")] = cfg.host;
        item[QStringLiteral("port")] = cfg.port;
        item[QStringLiteral("enabled")] = cfg.enabled;
        item[QStringLiteral("autoConnect")] = cfg.autoConnect;
        item[QStringLiteral("ratedCurrentA")] = static_cast<double>(cfg.ratedCurrentA);
        item[QStringLiteral("protocolType")] = cfg.protocolType;
        presets.append(item);
    }
    obj[QStringLiteral("devicePresets")] = presets;

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

}