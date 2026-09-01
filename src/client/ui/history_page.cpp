#include "history_page.h"
#include "connection_manager.h"
#include "storage_worker.h"
#include "domain/device_types.h"
#include "domain/command_types.h"
#include "domain/alarm_types.h"
#include "domain/persistence_types.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QDateTime>
#include <QMetaObject>

namespace motor {

HistoryPage::HistoryPage(ConnectionManager* connManager,
                         StorageWorker* storageWorker,
                         QWidget* parent)
    : QWidget(parent)
    , _connManager(connManager)
    , _storageWorker(storageWorker)
{
    setupUi();
}

void HistoryPage::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    auto* filterGroup = new QGroupBox(QStringLiteral("查询条件"));
    auto* filterLayout = new QHBoxLayout(filterGroup);

    filterLayout->addWidget(new QLabel(QStringLiteral("设备:")));
    _deviceCombo = new QComboBox();
    refreshDeviceList();
    filterLayout->addWidget(_deviceCombo);

    filterLayout->addWidget(new QLabel(QStringLiteral("类型:")));
    _typeCombo = new QComboBox();
    _typeCombo->addItem(QStringLiteral("遥测数据"));
    _typeCombo->addItem(QStringLiteral("告警记录"));
    _typeCombo->addItem(QStringLiteral("控制记录"));
    _typeCombo->addItem(QStringLiteral("设备事件"));
    filterLayout->addWidget(_typeCombo);
    connect(_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistoryPage::onTypeChanged);

    filterLayout->addWidget(new QLabel(QStringLiteral("从:")));
    _fromEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-1));
    _fromEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    _fromEdit->setCalendarPopup(true);
    _fromEdit->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _fromEdit->setKeyboardTracking(true);
    filterLayout->addWidget(_fromEdit);

    filterLayout->addWidget(new QLabel(QStringLiteral("到:")));
    _toEdit = new QDateTimeEdit(QDateTime::currentDateTime());
    _toEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    _toEdit->setCalendarPopup(true);
    _toEdit->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _toEdit->setKeyboardTracking(true);
    filterLayout->addWidget(_toEdit);

    _queryBtn = new QPushButton(QStringLiteral("查询"));
    _queryBtn->setStyleSheet(QStringLiteral("background: #3498db; color: white; font-weight: bold; padding: 4px 16px;"));
    connect(_queryBtn, &QPushButton::clicked, this, &HistoryPage::onQueryClicked);
    filterLayout->addWidget(_queryBtn);

    _exportBtn = new QPushButton(QStringLiteral("导出CSV"));
    _exportBtn->setStyleSheet(QStringLiteral("background: #27ae60; color: white; font-weight: bold; padding: 4px 16px;"));
    connect(_exportBtn, &QPushButton::clicked, this, &HistoryPage::onExportClicked);
    filterLayout->addWidget(_exportBtn);

    filterLayout->addStretch();
    mainLayout->addWidget(filterGroup);

    _table = new QTableWidget(0, 1);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table->horizontalHeader()->setStretchLastSection(true);
    _table->setAlternatingRowColors(true);
    mainLayout->addWidget(_table, 1);

    auto* pageRow = new QHBoxLayout();
    _prevBtn = new QPushButton(QStringLiteral("上一页"));
    connect(_prevBtn, &QPushButton::clicked, this, &HistoryPage::onPrevPage);
    _pageLabel = new QLabel(QStringLiteral("第 0 页"));
    _nextBtn = new QPushButton(QStringLiteral("下一页"));
    connect(_nextBtn, &QPushButton::clicked, this, &HistoryPage::onNextPage);
    pageRow->addStretch();
    pageRow->addWidget(_prevBtn);
    pageRow->addWidget(_pageLabel);
    pageRow->addWidget(_nextBtn);
    pageRow->addStretch();
    mainLayout->addLayout(pageRow);

    onTypeChanged(0);
}

void HistoryPage::onTypeChanged(int index)
{
    Q_UNUSED(index)
    updateTableColumns();
    _table->setRowCount(0);
    _currentPage = 0;
    _totalCount = 0;
    _pageLabel->setText(QStringLiteral("第 0 页"));
}

void HistoryPage::updateTableColumns()
{
    _table->clear();
    int type = _typeCombo->currentIndex();
    switch (type) {
    case 0:
        _table->setColumnCount(6);
        _table->setHorizontalHeaderLabels(
            QStringList() << QStringLiteral("时间") << QStringLiteral("设备")
                          << QStringLiteral("温度(°C)") << QStringLiteral("转速(RPM)")
                          << QStringLiteral("电流(A)") << QStringLiteral("运行状态"));
        break;
    case 1:
        _table->setColumnCount(6);
        _table->setHorizontalHeaderLabels(
            QStringList() << QStringLiteral("触发时间") << QStringLiteral("设备")
                          << QStringLiteral("规则") << QStringLiteral("级别")
                          << QStringLiteral("状态") << QStringLiteral("实际值/阈值"));
        break;
    case 2:
        _table->setColumnCount(5);
        _table->setHorizontalHeaderLabels(
            QStringList() << QStringLiteral("时间") << QStringLiteral("设备")
                          << QStringLiteral("指令") << QStringLiteral("结果")
                          << QStringLiteral("消息"));
        break;
    case 3:
        _table->setColumnCount(4);
        _table->setHorizontalHeaderLabels(
            QStringList() << QStringLiteral("时间") << QStringLiteral("设备")
                          << QStringLiteral("状态") << QStringLiteral("原因"));
        break;
    }
}

void HistoryPage::onQueryClicked()
{
    _currentPage = 0;
    int type = _typeCombo->currentIndex();
    switch (type) {
    case 0: queryTelemetryData(); break;
    case 1: queryAlarmData(); break;
    case 2: queryCommandData(); break;
    case 3: queryEventData(); break;
    }
}

template<typename T>
static T invokeStorage(QObject* worker, const char* method, QGenericArgument arg0 = {},
                        QGenericArgument arg1 = {}, QGenericArgument arg2 = {},
                        QGenericArgument arg3 = {}, QGenericArgument arg4 = {})
{
    T result;
    QMetaObject::invokeMethod(worker, method, Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(T, result), arg0, arg1, arg2, arg3, arg4);
    return result;
}

void HistoryPage::queryTelemetryData()
{
    TelemetryQuery q;
    if (_deviceCombo->currentIndex() > 0) {
        q.deviceId = _deviceCombo->currentText();
    }
    q.fromMs = _fromEdit->dateTime().toMSecsSinceEpoch();
    q.toMs = _toEdit->dateTime().toMSecsSinceEpoch();
    q.limit = _pageSize;
    q.offset = _currentPage * _pageSize;

    QList<Telemetry> results;
    QMetaObject::invokeMethod(_storageWorker, "queryTelemetry", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<Telemetry>, results),
                              Q_ARG(TelemetryQuery, q));

    _table->setRowCount(static_cast<int>(results.size()));
    for (int i = 0; i < results.size(); ++i) {
        const auto& t = results[i];
        _table->setItem(i, 0, new QTableWidgetItem(
            QDateTime::fromMSecsSinceEpoch(t.timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        _table->setItem(i, 1, new QTableWidgetItem(t.deviceId));
        _table->setItem(i, 2, new QTableWidgetItem(QString::number(t.temperatureC, 'f', 1)));
        _table->setItem(i, 3, new QTableWidgetItem(QString::number(t.speedRpm)));
        _table->setItem(i, 4, new QTableWidgetItem(QString::number(t.currentA, 'f', 2)));
        QString stateStr;
        switch (t.operatingState) {
        case OperatingState::Stopped: stateStr = QStringLiteral("停止"); break;
        case OperatingState::Starting: stateStr = QStringLiteral("启动中"); break;
        case OperatingState::Running: stateStr = QStringLiteral("运行"); break;
        case OperatingState::Stopping: stateStr = QStringLiteral("停止中"); break;
        case OperatingState::EmergencyStopped: stateStr = QStringLiteral("急停"); break;
        }
        _table->setItem(i, 5, new QTableWidgetItem(stateStr));
    }
    _totalCount = static_cast<int>(results.size());
    _pageLabel->setText(QStringLiteral("第 %1 页 (%2 条)").arg(_currentPage + 1).arg(results.size()));
    _prevBtn->setEnabled(_currentPage > 0);
    _nextBtn->setEnabled(results.size() >= _pageSize);
    if (results.isEmpty()) {
        _pageLabel->setText(QStringLiteral("无数据 — 请先上线并运行设备"));
    }
}

void HistoryPage::queryAlarmData()
{
    AlarmQuery q;
    if (_deviceCombo->currentIndex() > 0) {
        q.deviceId = _deviceCombo->currentText();
    }
    q.fromMs = _fromEdit->dateTime().toMSecsSinceEpoch();
    q.toMs = _toEdit->dateTime().toMSecsSinceEpoch();
    q.limit = _pageSize;
    q.offset = _currentPage * _pageSize;

    QList<Alarm> results;
    QMetaObject::invokeMethod(_storageWorker, "queryAlarms", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<Alarm>, results),
                              Q_ARG(AlarmQuery, q));

    _table->setRowCount(static_cast<int>(results.size()));
    for (int i = 0; i < results.size(); ++i) {
        const auto& a = results[i];
        _table->setItem(i, 0, new QTableWidgetItem(
            QDateTime::fromMSecsSinceEpoch(a.triggeredAtMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        _table->setItem(i, 1, new QTableWidgetItem(a.deviceId));
        _table->setItem(i, 2, new QTableWidgetItem(ruleName(static_cast<int>(a.rule))));
        _table->setItem(i, 3, new QTableWidgetItem(
            a.severity == AlarmSeverity::Critical ? QStringLiteral("严重") : QStringLiteral("警告")));
        QString stateStr;
        switch (a.state) {
        case AlarmState::ActiveUnacknowledged: stateStr = QStringLiteral("激活"); break;
        case AlarmState::ActiveAcknowledged: stateStr = QStringLiteral("已确认"); break;
        case AlarmState::RecoveredUnacknowledged: stateStr = QStringLiteral("已恢复"); break;
        case AlarmState::RecoveredAcknowledged: stateStr = QStringLiteral("已确认/恢复"); break;
        case AlarmState::Closed: stateStr = QStringLiteral("已关闭"); break;
        }
        _table->setItem(i, 4, new QTableWidgetItem(stateStr));
        _table->setItem(i, 5, new QTableWidgetItem(
            QStringLiteral("%1 / %2").arg(a.actualValue, 0, 'f', 1).arg(a.threshold, 0, 'f', 1)));
    }
    _totalCount = static_cast<int>(results.size());
    _pageLabel->setText(QStringLiteral("第 %1 页 (%2 条)").arg(_currentPage + 1).arg(results.size()));
    _prevBtn->setEnabled(_currentPage > 0);
    _nextBtn->setEnabled(results.size() >= _pageSize);
}

void HistoryPage::queryCommandData()
{
    CommandQuery q;
    if (_deviceCombo->currentIndex() > 0) {
        q.deviceId = _deviceCombo->currentText();
    }
    q.fromMs = _fromEdit->dateTime().toMSecsSinceEpoch();
    q.toMs = _toEdit->dateTime().toMSecsSinceEpoch();
    q.limit = _pageSize;
    q.offset = _currentPage * _pageSize;

    QList<CommandResult> results;
    QMetaObject::invokeMethod(_storageWorker, "queryCommands", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<CommandResult>, results),
                              Q_ARG(CommandQuery, q));

    _table->setRowCount(static_cast<int>(results.size()));
    for (int i = 0; i < results.size(); ++i) {
        const auto& c = results[i];
        _table->setItem(i, 0, new QTableWidgetItem(
            QDateTime::fromMSecsSinceEpoch(c.completedAtMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        _table->setItem(i, 1, new QTableWidgetItem(c.deviceId));
        QString typeStr;
        switch (c.type) {
        case CommandType::Start: typeStr = QStringLiteral("启动"); break;
        case CommandType::Stop: typeStr = QStringLiteral("停止"); break;
        case CommandType::SetTargetSpeed: typeStr = QStringLiteral("调速"); break;
        case CommandType::EmergencyStop: typeStr = QStringLiteral("急停"); break;
        default: typeStr = QStringLiteral("其他"); break;
        }
        _table->setItem(i, 2, new QTableWidgetItem(typeStr));
        QString resultStr;
        switch (c.status) {
        case CommandStatus::Pending: resultStr = QStringLiteral("等待中"); break;
        case CommandStatus::Succeeded: resultStr = QStringLiteral("成功"); break;
        case CommandStatus::DeviceFault: resultStr = QStringLiteral("设备故障"); break;
        case CommandStatus::TimedOut: resultStr = QStringLiteral("超时"); break;
        case CommandStatus::RejectedState: resultStr = QStringLiteral("状态拒绝"); break;
        case CommandStatus::ParameterOutOfRange: resultStr = QStringLiteral("参数越界"); break;
        default: resultStr = QStringLiteral("未知"); break;
        }
        _table->setItem(i, 3, new QTableWidgetItem(resultStr));
        _table->setItem(i, 4, new QTableWidgetItem(c.message));
    }
    _totalCount = static_cast<int>(results.size());
    _pageLabel->setText(QStringLiteral("第 %1 页 (%2 条)").arg(_currentPage + 1).arg(results.size()));
    _prevBtn->setEnabled(_currentPage > 0);
    _nextBtn->setEnabled(results.size() >= _pageSize);
}

void HistoryPage::queryEventData()
{
    DeviceEventQuery q;
    if (_deviceCombo->currentIndex() > 0) {
        q.deviceId = _deviceCombo->currentText();
    }
    q.fromMs = _fromEdit->dateTime().toMSecsSinceEpoch();
    q.toMs = _toEdit->dateTime().toMSecsSinceEpoch();
    q.limit = _pageSize;
    q.offset = _currentPage * _pageSize;

    QList<DeviceEvent> results;
    QMetaObject::invokeMethod(_storageWorker, "queryDeviceEvents", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<DeviceEvent>, results),
                              Q_ARG(DeviceEventQuery, q));

    _table->setRowCount(static_cast<int>(results.size()));
    for (int i = 0; i < results.size(); ++i) {
        const auto& e = results[i];
        _table->setItem(i, 0, new QTableWidgetItem(
            QDateTime::fromMSecsSinceEpoch(e.eventTimeMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        _table->setItem(i, 1, new QTableWidgetItem(e.deviceId));
        QString stateStr;
        switch (e.state) {
        case ConnectionState::Online: stateStr = QStringLiteral("在线"); break;
        case ConnectionState::Disconnected: stateStr = QStringLiteral("离线"); break;
        case ConnectionState::Connecting: stateStr = QStringLiteral("连接中"); break;
        case ConnectionState::Reconnecting: stateStr = QStringLiteral("重连中"); break;
        case ConnectionState::ProtocolError: stateStr = QStringLiteral("协议错误"); break;
        default: stateStr = QStringLiteral("其他"); break;
        }
        _table->setItem(i, 2, new QTableWidgetItem(stateStr));
        _table->setItem(i, 3, new QTableWidgetItem(e.reason));
    }
    _totalCount = static_cast<int>(results.size());
    _pageLabel->setText(QStringLiteral("第 %1 页 (%2 条)").arg(_currentPage + 1).arg(results.size()));
    _prevBtn->setEnabled(_currentPage > 0);
    _nextBtn->setEnabled(results.size() >= _pageSize);
}

void HistoryPage::onPrevPage()
{
    if (_currentPage > 0) {
        _currentPage--;
        onQueryClicked();
    }
}

void HistoryPage::onNextPage()
{
    _currentPage++;
    onQueryClicked();
}

void HistoryPage::onExportClicked()
{
    int cols = _table->columnCount();
    int rows = _table->rowCount();
    if (rows == 0) return;

    QStringList headers;
    for (int c = 0; c < cols; ++c) {
        auto* item = _table->horizontalHeaderItem(c);
        headers << (item ? item->text() : QString());
    }

    QList<QStringList> data;
    for (int r = 0; r < rows; ++r) {
        QStringList row;
        for (int c = 0; c < cols; ++c) {
            auto* item = _table->item(r, c);
            row << (item ? item->text() : QString());
        }
        data.append(row);
    }

    exportToCsv(headers, data);
}

void HistoryPage::exportToCsv(const QStringList& headers, const QList<QStringList>& rows)
{
    QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("导出CSV"),
        QStringLiteral("export_%1.csv").arg(
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入文件: %1").arg(file.errorString()));
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    stream << QStringLiteral("\uFEFF");

    for (int i = 0; i < headers.size(); ++i) {
        if (i > 0) stream << ',';
        stream << '"' << headers[i] << '"';
    }
    stream << '\n';

    for (const auto& row : rows) {
        for (int i = 0; i < row.size(); ++i) {
            if (i > 0) stream << ',';
            stream << '"' << row[i] << '"';
        }
        stream << '\n';
    }

    file.close();
    QMessageBox::information(this, QStringLiteral("导出成功"),
                             QStringLiteral("已导出 %1 条记录到:\n%2").arg(rows.size()).arg(fileName));
}

void HistoryPage::refreshDeviceList()
{
    _deviceCombo->blockSignals(true);
    _deviceCombo->clear();
    _deviceCombo->addItem(QStringLiteral("全部设备"));
    for (auto* conn : _connManager->allConnections()) {
        _deviceCombo->addItem(conn->deviceId());
    }
    _deviceCombo->blockSignals(false);
}

QString HistoryPage::ruleName(int code) const
{
    switch (code) {
    case 0: return QStringLiteral("高温预警");
    case 1: return QStringLiteral("严重过热");
    case 2: return QStringLiteral("过流");
    case 3: return QStringLiteral("堵转");
    case 4: return QStringLiteral("振动异常");
    case 5: return QStringLiteral("电压异常");
    case 6: return QStringLiteral("心跳超时");
    case 7: return QStringLiteral("遥测停更");
    default: return QStringLiteral("未知");
    }
}

}
