#pragma once

#include <QWidget>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QList>
#include <memory>

namespace motor {

class ConnectionManager;
class StorageWorker;

class HistoryPage : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPage(ConnectionManager* connManager,
                         StorageWorker* storageWorker,
                         QWidget* parent = nullptr);

    void refreshDeviceList();

private slots:
    void onQueryClicked();
    void onExportClicked();
    void onPrevPage();
    void onNextPage();
    void onTypeChanged(int index);

private:
    void setupUi();
    void updateTableColumns();
    void queryTelemetryData();
    void queryAlarmData();
    void queryCommandData();
    void queryEventData();
    void exportToCsv(const QStringList& headers, const QList<QStringList>& rows);
    QString ruleName(int code) const;

    ConnectionManager* _connManager{nullptr};
    StorageWorker* _storageWorker{nullptr};

    QComboBox* _deviceCombo{nullptr};
    QComboBox* _typeCombo{nullptr};
    QDateTimeEdit* _fromEdit{nullptr};
    QDateTimeEdit* _toEdit{nullptr};
    QPushButton* _queryBtn{nullptr};
    QPushButton* _exportBtn{nullptr};
    QTableWidget* _table{nullptr};
    QLabel* _pageLabel{nullptr};
    QPushButton* _prevBtn{nullptr};
    QPushButton* _nextBtn{nullptr};

    int _currentPage{0};
    int _pageSize{200};
    int _totalCount{0};
};

}