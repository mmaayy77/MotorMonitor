#pragma once

#include "domain/device_types.h"
#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>

namespace motor {

class DeviceListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        DeviceIdRole = Qt::UserRole + 1,
        NameRole,
        StatusRole,
        StatusColorRole,
        TemperatureRole,
        SpeedRole,
        CurrentRole
    };

    enum FilterMode {
        FilterAll,
        FilterOnline,
        FilterOffline,
        FilterHasAlarm
    };

    explicit DeviceListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    void updateSnapshot(const DeviceSnapshot& snapshot);
    void removeDevice(DeviceId deviceId);
    void setFilterText(const QString& text);
    void setFilterMode(FilterMode mode);

private:
    struct DeviceItem {
        DeviceId deviceId;
        DeviceSnapshot snapshot;
    };

    QList<DeviceItem> _devices;
    QString _filterText;
    FilterMode _filterMode{FilterAll};
    int indexOf(DeviceId deviceId) const;
    bool matchesFilter(const DeviceItem& item) const;
    void rebuildFilter();
};

}