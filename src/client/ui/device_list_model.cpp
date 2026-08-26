#include "device_list_model.h"

namespace motor {

DeviceListModel::DeviceListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int DeviceListModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    if (_filterText.isEmpty() && _filterMode == FilterAll) {
        return static_cast<int>(_devices.size());
    }
    int count = 0;
    for (const auto& item : _devices) {
        if (matchesFilter(item)) count++;
    }
    return count;
}

QVariant DeviceListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0) return {};

    const DeviceItem* item = nullptr;
    if (_filterText.isEmpty() && _filterMode == FilterAll) {
        if (index.row() >= static_cast<int>(_devices.size())) return {};
        item = &_devices[index.row()];
    } else {
        int visibleRow = 0;
        for (const auto& d : _devices) {
            if (matchesFilter(d)) {
                if (visibleRow == index.row()) {
                    item = &d;
                    break;
                }
                visibleRow++;
            }
        }
        if (!item) return {};
    }

    const auto& snap = item->snapshot;

    switch (role) {
    case Qt::DisplayRole:
        return item->deviceId;
    case DeviceIdRole:
        return item->deviceId;
    case NameRole:
        return item->deviceId;
    case StatusRole: {
        switch (snap.connectionState) {
        case ConnectionState::Online:
            return snap.activeAlarmCount > 0 ? QStringLiteral("告警") : QStringLiteral("在线");
        case ConnectionState::Connecting:
            return QStringLiteral("连接中");
        case ConnectionState::Disconnecting:
            return QStringLiteral("断开中");
        case ConnectionState::Reconnecting:
            return QStringLiteral("重连中");
        case ConnectionState::Registering:
            return QStringLiteral("注册中");
        case ConnectionState::ProtocolError:
            return QStringLiteral("协议错误");
        case ConnectionState::Disabled:
            return QStringLiteral("禁用");
        case ConnectionState::Disconnected:
        default:
            return QStringLiteral("离线");
        }
    }
    case StatusColorRole: {
        if (snap.connectionState == ConnectionState::Online && snap.activeAlarmCount > 0) {
            return QStringLiteral("#e74c3c");
        }
        switch (snap.connectionState) {
        case ConnectionState::Online:
            return QStringLiteral("#2ecc71");
        case ConnectionState::Connecting:
        case ConnectionState::Registering:
        case ConnectionState::Reconnecting:
            return QStringLiteral("#f39c12");
        case ConnectionState::ProtocolError:
            return QStringLiteral("#e74c3c");
        case ConnectionState::Disconnecting:
            return QStringLiteral("#95a5a6");
        case ConnectionState::Disabled:
            return QStringLiteral("#7f8c8d");
        case ConnectionState::Disconnected:
        default:
            return QStringLiteral("#bdc3c7");
        }
    }
    case TemperatureRole:
        return QString::number(snap.telemetry.temperatureC, 'f', 1);
    case SpeedRole:
        return QString::number(snap.telemetry.speedRpm);
    case CurrentRole:
        return QString::number(snap.telemetry.currentA, 'f', 2);
    default:
        return {};
    }
}

QHash<int, QByteArray> DeviceListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[DeviceIdRole] = "deviceId";
    roles[NameRole] = "name";
    roles[StatusRole] = "status";
    roles[StatusColorRole] = "statusColor";
    roles[TemperatureRole] = "temperature";
    roles[SpeedRole] = "speed";
    roles[CurrentRole] = "current";
    return roles;
}

void DeviceListModel::updateSnapshot(const DeviceSnapshot& snapshot)
{
    int idx = indexOf(snapshot.telemetry.deviceId);
    if (idx < 0) {
        int row = static_cast<int>(_devices.size());
        beginInsertRows(QModelIndex(), row, row);
        DeviceItem item;
        item.deviceId = snapshot.telemetry.deviceId;
        item.snapshot = snapshot;
        _devices.append(item);
        endInsertRows();
    } else {
        _devices[idx].snapshot = snapshot;
        auto modelIndex = createIndex(idx, 0);
        emit dataChanged(modelIndex, modelIndex,
            { DeviceIdRole, NameRole, StatusRole, StatusColorRole,
              TemperatureRole, SpeedRole, CurrentRole });
    }
}

void DeviceListModel::updateConnectionState(DeviceId deviceId, ConnectionState state, int alarmCount)
{
    int idx = indexOf(deviceId);
    if (idx < 0) return;
    _devices[idx].snapshot.connectionState = state;
    _devices[idx].snapshot.stale = (state != ConnectionState::Online);
    _devices[idx].snapshot.activeAlarmCount = alarmCount;
    auto modelIndex = createIndex(idx, 0);
    emit dataChanged(modelIndex, modelIndex, { StatusRole, StatusColorRole });
}

void DeviceListModel::removeDevice(DeviceId deviceId)
{
    int idx = indexOf(deviceId);
    if (idx < 0) return;
    beginRemoveRows(QModelIndex(), idx, idx);
    _devices.removeAt(idx);
    endRemoveRows();
}

void DeviceListModel::setFilterText(const QString& text)
{
    _filterText = text;
    rebuildFilter();
}

void DeviceListModel::setFilterMode(FilterMode mode)
{
    _filterMode = mode;
    rebuildFilter();
}

bool DeviceListModel::matchesFilter(const DeviceItem& item) const
{
    if (!_filterText.isEmpty()) {
        if (!item.deviceId.contains(_filterText, Qt::CaseInsensitive)) {
            return false;
        }
    }
    switch (_filterMode) {
    case FilterAll:
        break;
    case FilterOnline:
        if (item.snapshot.connectionState != ConnectionState::Online) return false;
        break;
    case FilterOffline:
        if (item.snapshot.connectionState == ConnectionState::Online) return false;
        break;
    case FilterHasAlarm:
        if (item.snapshot.activeAlarmCount == 0) return false;
        break;
    }
    return true;
}

void DeviceListModel::rebuildFilter()
{
    beginResetModel();
    endResetModel();
}

int DeviceListModel::indexOf(DeviceId deviceId) const
{
    for (int i = 0; i < _devices.size(); ++i) {
        if (_devices[i].deviceId == deviceId) return i;
    }
    return -1;
}

}