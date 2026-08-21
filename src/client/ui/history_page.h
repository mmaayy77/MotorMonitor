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
class Repository;

class HistoryPage : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPage(std::shared_ptr<ConnectionManager> connManager,
                         std::shared_ptr<Repository> repo,
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

    std::shared_ptr<ConnectionManager> _connManager;
    std::shared_ptr<Repository> _repo;

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