#include "main_window.h"
#include "device_list_model.h"
#include "realtime_chart.h"
#include "history_page.h"
#include "log_page.h"
#include "device_config_dialog.h"
#include "connection_manager.h"
#include "alarm_engine/alarm_engine.h"
#include "persistence/sqlite_repository.h"
#include <QSplitter>
#include <QListView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QTextEdit>
#include <QDateTime>
#include <QStatusBar>
#include <QTabWidget>

namespace motor {

MainWindow::MainWindow(std::shared_ptr<ConnectionManager> connManager,
                       std::shared_ptr<AlarmEngine> alarmEngine,
                       std::shared_ptr<Repository> repo,
                       QWidget* parent)
    : QMainWindow(parent)
    , _connManager(std::move(connManager))
    , _alarmEngine(std::move(alarmEngine))
    , _repo(std::move(repo))
{
    _statusTimer = new QTimer(this);
    connect(_statusTimer, &QTimer::timeout, this, &MainWindow::onStatusBarRefresh);
    _statusTimer->start(1000);
    _refreshTimer = new QTimer(this);
    connect(_refreshTimer, &QTimer::timeout, this, &MainWindow::onRefreshTick);
    _refreshTimer->start(200);
    setupUi();
    loadDefaultDevices();
    _historyPage->refreshDeviceList();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("工业电机监控系统"));
    resize(1280, 800);

    _mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(_mainSplitter);

    setupDeviceList();
    setupDetailPanel();
    setupAlarmPanel();
    setupTrendPanel();

    _monitorPanel = new QWidget();
    auto* monitorLayout = new QVBoxLayout(_monitorPanel);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    monitorLayout->addWidget(_detailPanel, 0);
    monitorLayout->addWidget(_trendPanel, 1);
    monitorLayout->addWidget(_alarmPanel, 1);

    _historyPage = new HistoryPage(_connManager, _repo);

    _logPage = new LogPage();

    _tabWidget = new QTabWidget();
    _tabWidget->addTab(_monitorPanel, QStringLiteral("实时监控"));
    _tabWidget->addTab(_historyPage, QStringLiteral("历史查询"));
    _tabWidget->addTab(_logPage, QStringLiteral("运行日志"));

    _mainSplitter->addWidget(_listPanel);
    _mainSplitter->addWidget(_tabWidget);
    _mainSplitter->setStretchFactor(0, 1);
    _mainSplitter->setStretchFactor(1, 4);
}

void MainWindow::setupDeviceList()
{
    _listPanel = new QWidget();
    auto* layout = new QVBoxLayout(_listPanel);
    layout->setContentsMargins(5, 5, 5, 5);

    auto* title = new QLabel(QStringLiteral("设备列表"), _listPanel);
    auto f = title->font();
    f.setBold(true);
    f.setPointSize(11);
    title->setFont(f);
    layout->addWidget(title);

    _searchEdit = new QLineEdit(_listPanel);
    _searchEdit->setPlaceholderText(QStringLiteral("搜索设备ID..."));
    connect(_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onFilterTextChanged);
    layout->addWidget(_searchEdit);

    _filterCombo = new QComboBox(_listPanel);
    _filterCombo->addItem(QStringLiteral("全部状态"));
    _filterCombo->addItem(QStringLiteral("在线"));
    _filterCombo->addItem(QStringLiteral("离线"));
    _filterCombo->addItem(QStringLiteral("告警"));
    connect(_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFilterStateChanged);
    layout->addWidget(_filterCombo);

    _deviceList = new QListView(_listPanel);
    _deviceModel = new DeviceListModel(this);
    _deviceList->setModel(_deviceModel);
    connect(_deviceList->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onDeviceSelected);
    layout->addWidget(_deviceList, 1);

    auto* btnRow = new QHBoxLayout();
    _addDeviceBtn = new QPushButton(QStringLiteral("+添加"), _listPanel);
    _addDeviceBtn->setStyleSheet(QStringLiteral("background: #27ae60; color: white; font-weight: bold;"));
    connect(_addDeviceBtn, &QPushButton::clicked, this, &MainWindow::onAddDeviceClicked);
    _editDeviceBtn = new QPushButton(QStringLiteral("编辑"), _listPanel);
    connect(_editDeviceBtn, &QPushButton::clicked, this, &MainWindow::onEditDeviceClicked);
    _deleteDeviceBtn = new QPushButton(QStringLiteral("删除"), _listPanel);
    _deleteDeviceBtn->setStyleSheet(QStringLiteral("color: #e74c3c;"));
    connect(_deleteDeviceBtn, &QPushButton::clicked, this, &MainWindow::onDeleteDeviceClicked);
    btnRow->addWidget(_addDeviceBtn);
    btnRow->addWidget(_editDeviceBtn);
    btnRow->addWidget(_deleteDeviceBtn);

    auto* batchBtn = new QPushButton(QStringLiteral("批量生成"), _listPanel);
    batchBtn->setStyleSheet(QStringLiteral("color: #3498db;"));
    connect(batchBtn, &QPushButton::clicked, this, &MainWindow::onBatchGenerateClicked);
    btnRow->addWidget(batchBtn);

    layout->addLayout(btnRow);

    _listStatusLabel = new QLabel(QStringLiteral("在线: 0 / 0"), _listPanel);
    layout->addWidget(_listStatusLabel);
}

void MainWindow::setupDetailPanel()
{
    _detailPanel = new QGroupBox(QStringLiteral("设备详情"));
    auto* layout = new QVBoxLayout(_detailPanel);

    auto* infoRow = new QHBoxLayout();
    auto* stateLabel = new QLabel(QStringLiteral("连接状态:"), _detailPanel);
    _statusLabel = new QLabel(QStringLiteral("离线"), _detailPanel);
    _statusLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; border-radius: 4px; font-weight: bold; color: white; background: #bdc3c7;"));
    auto* opStateLabel = new QLabel(QStringLiteral("运行状态:"), _detailPanel);
    _operatingStateLabel = new QLabel(QStringLiteral("未知"), _detailPanel);
    _operatingStateLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; border-radius: 4px; font-weight: bold; color: white; background: #95a5a6;"));
    infoRow->addWidget(stateLabel);
    infoRow->addWidget(_statusLabel);
    infoRow->addWidget(opStateLabel);
    infoRow->addWidget(_operatingStateLabel);
    infoRow->addStretch();
    layout->addLayout(infoRow);

    auto* dataGrid = new QGridLayout();

    auto makeValueLabel = [&](const QString& unit) {
        auto* lbl = new QLabel(QStringLiteral("-- %1").arg(unit), _detailPanel);
        lbl->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
        return lbl;
    };

    dataGrid->addWidget(new QLabel(QStringLiteral("温度"), _detailPanel), 0, 0);
    _tempLabel = makeValueLabel(QStringLiteral("°C"));
    dataGrid->addWidget(_tempLabel, 1, 0);

    dataGrid->addWidget(new QLabel(QStringLiteral("转速"), _detailPanel), 0, 1);
    _speedLabel = makeValueLabel(QStringLiteral("RPM"));
    dataGrid->addWidget(_speedLabel, 1, 1);

    dataGrid->addWidget(new QLabel(QStringLiteral("电流"), _detailPanel), 0, 2);
    _currentLabel = makeValueLabel(QStringLiteral("A"));
    dataGrid->addWidget(_currentLabel, 1, 2);

    dataGrid->addWidget(new QLabel(QStringLiteral("电压"), _detailPanel), 2, 0);
    _voltageLabel = makeValueLabel(QStringLiteral("V"));
    dataGrid->addWidget(_voltageLabel, 3, 0);

    dataGrid->addWidget(new QLabel(QStringLiteral("振动"), _detailPanel), 2, 1);
    _vibrationLabel = makeValueLabel(QStringLiteral("mm/s"));
    dataGrid->addWidget(_vibrationLabel, 3, 1);

    layout->addLayout(dataGrid);

    auto* connRow = new QHBoxLayout();
    _connectBtn = new QPushButton(QStringLiteral("上线"), _detailPanel);
    _connectBtn->setStyleSheet(QStringLiteral("background: #27ae60; color: white; font-weight: bold;"));
    _disconnectBtn = new QPushButton(QStringLiteral("下线"), _detailPanel);
    _disconnectBtn->setStyleSheet(QStringLiteral("background: #7f8c8d; color: white; font-weight: bold;"));
    connRow->addWidget(_connectBtn);
    connRow->addWidget(_disconnectBtn);
    connRow->addStretch();
    layout->addLayout(connRow);

    auto* ctrlRow = new QHBoxLayout();
    _startBtn = new QPushButton(QStringLiteral("启动"), _detailPanel);
    _stopBtn = new QPushButton(QStringLiteral("停止"), _detailPanel);
    _emergencyBtn = new QPushButton(QStringLiteral("急停"), _detailPanel);
    _emergencyBtn->setStyleSheet(QStringLiteral("background: #e74c3c; color: white; font-weight: bold;"));
    ctrlRow->addWidget(_startBtn);
    ctrlRow->addWidget(_stopBtn);
    ctrlRow->addWidget(_emergencyBtn);
    ctrlRow->addStretch();
    layout->addLayout(ctrlRow);

    auto* speedRow = new QHBoxLayout();
    speedRow->addWidget(new QLabel(QStringLiteral("目标转速:"), _detailPanel));
    _speedSpin = new QSpinBox(_detailPanel);
    _speedSpin->setRange(0, 3000);
    _speedSpin->setValue(1500);
    _setSpeedBtn = new QPushButton(QStringLiteral("设置"), _detailPanel);
    speedRow->addWidget(_speedSpin);
    speedRow->addWidget(_setSpeedBtn);
    speedRow->addStretch();
    layout->addLayout(speedRow);

    connect(_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(_emergencyBtn, &QPushButton::clicked, this, &MainWindow::onEmergencyStopClicked);
    connect(_setSpeedBtn, &QPushButton::clicked, this, &MainWindow::onSetSpeedClicked);
}

void MainWindow::setupAlarmPanel()
{
    _alarmPanel = new QGroupBox(QStringLiteral("告警记录"));
    auto* layout = new QVBoxLayout(_alarmPanel);

    _alarmTable = new QTableWidget(0, 5, _alarmPanel);
    _alarmTable->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("时间") << QStringLiteral("设备")
                      << QStringLiteral("规则") << QStringLiteral("级别")
                      << QStringLiteral("状态"));
    _alarmTable->horizontalHeader()->setStretchLastSection(true);
    _alarmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _alarmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(_alarmTable, 1);

    auto* btnRow = new QHBoxLayout();
    auto* ackBtn = new QPushButton(QStringLiteral("确认告警"), _alarmPanel);
    auto* clearBtn = new QPushButton(QStringLiteral("清除告警"), _alarmPanel);
    btnRow->addWidget(ackBtn);
    btnRow->addWidget(clearBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(ackBtn, &QPushButton::clicked, this, &MainWindow::onAckAlarmClicked);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearAlarmClicked);

    _logEdit = new QTextEdit(_alarmPanel);
    _logEdit->setReadOnly(true);
    _logEdit->setMaximumHeight(100);
    layout->addWidget(_logEdit);
}

void MainWindow::setupTrendPanel()
{
    _trendPanel = new QGroupBox(QStringLiteral("实时曲线 (60秒)"));
    auto* layout = new QVBoxLayout(_trendPanel);

    _chart = new RealtimeChart(_trendPanel);
    layout->addWidget(_chart);
}

void MainWindow::loadDefaultDevices()
{
    QList<DeviceConfig> configs;
    if (_repo && _repo->isOpen()) {
        configs = _repo->loadAllDeviceConfigs();
    }

    if (configs.isEmpty()) {
        DeviceConfig cfg;
        cfg.deviceId = QStringLiteral("MOTOR-0001");
        cfg.displayName = QStringLiteral("1号电机");
        cfg.host = QStringLiteral("127.0.0.1");
        cfg.port = 9000;
        cfg.enabled = true;
        cfg.autoConnect = true;
        configs.append(cfg);
        if (_repo && _repo->isOpen()) {
            _repo->saveDeviceConfig(cfg);
        }
    }

    for (const auto& cfg : configs) {
        _connManager->addDevice(cfg);
        DeviceSnapshot snap;
        snap.telemetry.deviceId = cfg.deviceId;
        snap.connectionState = ConnectionState::Disconnected;
        snap.receivedAtMs = 0;
        snap.stale = true;
        snap.activeAlarmCount = 0;
        _deviceModel->updateSnapshot(snap);
        _logPage->addDeviceToFilter(cfg.deviceId);
    }

    if (_deviceModel->rowCount() > 0) {
        auto idx = _deviceModel->index(0, 0);
        _deviceList->setCurrentIndex(idx);
        onDeviceSelected(idx);
    }

    connect(_connManager.get(), &ConnectionManager::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(_connManager.get(), &ConnectionManager::telemetryReceived,
            this, &MainWindow::onTelemetryReceived);
    connect(_connManager.get(), &ConnectionManager::commandResultReceived,
            this, &MainWindow::onCommandResult);
    connect(_alarmEngine.get(), &AlarmEngine::alarmRaised,
            this, &MainWindow::onAlarmRaised);
    connect(_alarmEngine.get(), &AlarmEngine::alarmStateChanged,
            this, &MainWindow::onAlarmStateChanged);

    statusBar()->showMessage(QStringLiteral("就绪 - 点击上线连接设备"));

}

void MainWindow::onDeviceSelected(const QModelIndex& index)
{
    if (!index.isValid()) return;
    _selectedDevice = _deviceModel->data(index, DeviceListModel::DeviceIdRole).toString();
    updateDetailPanel(_selectedDevice);
}

void MainWindow::updateDetailPanel(const DeviceId& deviceId)
{
    for (auto* conn : _connManager->allConnections()) {
        if (conn->deviceId() == deviceId) {
            if (conn->isConnected()) {
                appendLog(QString("选中设备: %1 (在线)").arg(deviceId));
            } else {
                appendLog(QString("选中设备: %1 (离线)").arg(deviceId));
            }
            break;
        }
    }
}

void MainWindow::updateOperatingStateDisplay(OperatingState state)
{
    QString text;
    QString color;
    switch (state) {
    case OperatingState::Stopped:
        text = QStringLiteral("停止");
        color = QStringLiteral("#7f8c8d");
        break;
    case OperatingState::Starting:
        text = QStringLiteral("启动中");
        color = QStringLiteral("#f39c12");
        break;
    case OperatingState::Running:
        text = QStringLiteral("运行");
        color = QStringLiteral("#2ecc71");
        break;
    case OperatingState::Stopping:
        text = QStringLiteral("停止中");
        color = QStringLiteral("#f39c12");
        break;
    case OperatingState::EmergencyStopped:
        text = QStringLiteral("急停");
        color = QStringLiteral("#e74c3c");
        break;
    default:
        text = QStringLiteral("未知");
        color = QStringLiteral("#95a5a6");
        break;
    }
    _operatingStateLabel->setText(text);
    _operatingStateLabel->setStyleSheet(QString(
        "padding: 2px 8px; border-radius: 4px; font-weight: bold; color: white; background: %1;").arg(color));
}

void MainWindow::onTelemetryReceived(DeviceId deviceId, const Telemetry& telemetry)
{
    _messageCount++;

    DeviceSnapshot snap;
    snap.telemetry = telemetry;
    snap.connectionState = ConnectionState::Online;
    snap.receivedAtMs = QDateTime::currentMSecsSinceEpoch();
    snap.stale = false;
    snap.activeAlarmCount = static_cast<int>(_alarmEngine->activeAlarms(deviceId).size());
    _deviceModel->updateSnapshot(snap);

    _alarmEngine->processTelemetry(deviceId, telemetry, snap.receivedAtMs);

    if (_repo && _repo->isOpen()) {
        _repo->saveTelemetry(telemetry);
    }

    if (deviceId == _selectedDevice) {
        _latestTelemetry = telemetry;
        _chart->appendData(telemetry.temperatureC,
                           static_cast<double>(telemetry.speedRpm),
                           telemetry.currentA);
    }

    _needsRefresh = true;
}

void MainWindow::onConnectionStateChanged(DeviceId deviceId, ConnectionState oldState, ConnectionState newState)
{
    Q_UNUSED(oldState)
    DeviceSnapshot snap;
    snap.telemetry.deviceId = deviceId;
    snap.connectionState = newState;
    snap.receivedAtMs = QDateTime::currentMSecsSinceEpoch();
    snap.stale = (newState != ConnectionState::Online);
    snap.activeAlarmCount = static_cast<int>(_alarmEngine->activeAlarms(deviceId).size());
    _deviceModel->updateSnapshot(snap);

    int online = _connManager->onlineCount();
    int total = static_cast<int>(_connManager->allConnections().size());
    _listStatusLabel->setText(QStringLiteral("在线: %1 / %2").arg(online).arg(total));

    if (deviceId == _selectedDevice && newState == ConnectionState::Online) {
        appendLog(QString("[%1] %2 已连接成功").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))).arg(deviceId));
        updateDetailPanel(_selectedDevice);
    }

    QString stateText;
    QString color;
    switch (newState) {
    case ConnectionState::Online:
        stateText = QStringLiteral("在线");
        color = QStringLiteral("#2ecc71");
        break;
    case ConnectionState::Connecting:
        stateText = QStringLiteral("连接中");
        color = QStringLiteral("#f39c12");
        break;
    case ConnectionState::Registering:
        stateText = QStringLiteral("注册中");
        color = QStringLiteral("#f39c12");
        break;
    case ConnectionState::Disconnecting:
        stateText = QStringLiteral("断开中");
        color = QStringLiteral("#95a5a6");
        break;
    case ConnectionState::Disconnected:
        stateText = QStringLiteral("离线");
        color = QStringLiteral("#bdc3c7");
        break;
    default:
        stateText = QStringLiteral("未知");
        color = QStringLiteral("#95a5a6");
    }

    if (deviceId == _selectedDevice) {
        _statusLabel->setText(stateText);
        _statusLabel->setStyleSheet(QString(
            "padding: 2px 8px; border-radius: 4px; font-weight: bold; color: white; background: %1;").arg(color));
        updateDetailPanel(_selectedDevice);
    }

    appendLog(QString("[%1] %2 - 状态变更: %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(deviceId).arg(stateText));
}

void MainWindow::onAlarmRaised(const AlarmEvent& event)
{
    addAlarmEvent(event);
    if (_repo && _repo->isOpen()) {
        _repo->saveAlarmEvent(event);
    }
}

void MainWindow::addAlarmEvent(const AlarmEvent& event)
{
    int row = _alarmTable->rowCount();
    _alarmTable->insertRow(row);

    auto alarmIdStr = event.alarm.alarmId.toString();
    auto* timeItem = new QTableWidgetItem(
        QDateTime::fromMSecsSinceEpoch(event.eventTimeMs).toString(QStringLiteral("HH:mm:ss")));
    timeItem->setData(Qt::UserRole, alarmIdStr);
    auto* deviceItem = new QTableWidgetItem(event.alarm.deviceId);
    auto* ruleItem = new QTableWidgetItem(ruleName(event.alarm.rule));
    auto* severityItem = new QTableWidgetItem(
        event.alarm.severity == AlarmSeverity::Critical ? QStringLiteral("严重") : QStringLiteral("警告"));
    auto* stateItem = new QTableWidgetItem(QStringLiteral("激活"));

    if (event.alarm.severity == AlarmSeverity::Critical) {
        severityItem->setForeground(QBrush(QColor(231, 76, 60)));
    } else {
        severityItem->setForeground(QBrush(QColor(243, 156, 18)));
    }

    _alarmTable->setItem(row, 0, timeItem);
    _alarmTable->setItem(row, 1, deviceItem);
    _alarmTable->setItem(row, 2, ruleItem);
    _alarmTable->setItem(row, 3, severityItem);
    _alarmTable->setItem(row, 4, stateItem);

    _alarmRowMap[event.alarm.alarmId] = row;

    appendLog(QString("[%1] 告警: %2 - %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(event.alarm.deviceId)
        .arg(event.evidence));
}

void MainWindow::onCommandResult(DeviceId deviceId, const CommandResult& result)
{
    appendLog(QString("[%1] %2 - 命令结果: %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(deviceId)
        .arg(result.message));

    if (_repo && _repo->isOpen()) {
        _repo->saveCommandResult(deviceId, result);
    }
}

void MainWindow::onConnectClicked()
{
    if (_selectedDevice.isEmpty()) return;
    auto* conn = _connManager->connection(_selectedDevice);
    if (!conn) return;
    if (conn->isConnected()) {
        appendLog(QString("%1 已在线").arg(_selectedDevice));
        return;
    }
    conn->connectToDevice();
    appendLog(QString("[%1] 正在连接 %2...")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(_selectedDevice));
}

void MainWindow::onDisconnectClicked()
{
    if (_selectedDevice.isEmpty()) return;
    auto* conn = _connManager->connection(_selectedDevice);
    if (!conn) return;
    if (!conn->isConnected() && conn->state() == ConnectionState::Disconnected) {
        appendLog(QString("%1 已离线").arg(_selectedDevice));
        return;
    }
    appendLog(QString("[%1] %2 正在下线...")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(_selectedDevice));
    conn->disconnectDevice();
}

void MainWindow::onStartClicked()
{
    if (_selectedDevice.isEmpty()) return;
    auto* conn = _connManager->connection(_selectedDevice);
    if (!conn) return;
    if (!conn->isConnected()) {
        appendLog(QString("%1 未上线，无法启动").arg(_selectedDevice));
        return;
    }
    CommandRequest req;
    req.requestId = QDateTime::currentMSecsSinceEpoch();
    req.deviceId = _selectedDevice;
    req.type = CommandType::Start;
    req.sentAtMs = QDateTime::currentMSecsSinceEpoch();
    conn->sendCommand(req);
    appendLog(QString("[%1] %2 发送启动命令")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(_selectedDevice));
}

void MainWindow::onStopClicked()
{
    if (_selectedDevice.isEmpty()) return;
    auto* conn = _connManager->connection(_selectedDevice);
    if (!conn || !conn->isConnected()) return;
    CommandRequest req;
    req.requestId = QDateTime::currentMSecsSinceEpoch();
    req.deviceId = _selectedDevice;
    req.type = CommandType::Stop;
    req.sentAtMs = QDateTime::currentMSecsSinceEpoch();
    conn->sendCommand(req);
    appendLog(QString("[%1] %2 发送停止命令")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(_selectedDevice));
}

void MainWindow::onEmergencyStopClicked()
{
    if (_selectedDevice.isEmpty()) return;
    auto* conn = _connManager->connection(_selectedDevice);
    if (!conn || !conn->isConnected()) return;
    CommandRequest req;
    req.requestId = QDateTime::currentMSecsSinceEpoch();
    req.deviceId = _selectedDevice;
    req.type = CommandType::EmergencyStop;
    req.sentAtMs = QDateTime::currentMSecsSinceEpoch();
    conn->sendCommand(req);
    appendLog(QString("%1 已发送急停命令!").arg(_selectedDevice));
}

void MainWindow::onSetSpeedClicked()
{
    if (_selectedDevice.isEmpty()) return;
    auto* conn = _connManager->connection(_selectedDevice);
    if (!conn) return;
    if (!conn->isConnected()) {
        appendLog(QString("%1 未上线，无法设置转速").arg(_selectedDevice));
        return;
    }
    CommandRequest req;
    req.requestId = QDateTime::currentMSecsSinceEpoch();
    req.deviceId = _selectedDevice;
    req.type = CommandType::SetTargetSpeed;
    req.targetSpeedRpm = static_cast<quint16>(_speedSpin->value());
    req.sentAtMs = QDateTime::currentMSecsSinceEpoch();
    if (conn->sendCommand(req)) {
        appendLog(QString("[%1] %2 设置目标转速: %3 RPM")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
            .arg(_selectedDevice).arg(req.targetSpeedRpm));
    } else {
        appendLog(QString("[%1] %2 设置转速命令发送失败")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
            .arg(_selectedDevice));
    }
}

void MainWindow::onAckAlarmClicked()
{
    int row = _alarmTable->currentRow();
    if (row < 0) return;
    auto* timeItem = _alarmTable->item(row, 0);
    if (!timeItem) return;
    auto alarmIdStr = timeItem->data(Qt::UserRole).toString();
    auto alarmId = QUuid::fromString(alarmIdStr);
    if (alarmId.isNull()) return;
    _alarmEngine->acknowledgeAlarm(alarmId, QDateTime::currentMSecsSinceEpoch());
    appendLog(QString("[%1] 告警已确认")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
}

void MainWindow::onClearAlarmClicked()
{
    int row = _alarmTable->currentRow();
    if (row < 0) return;
    auto* timeItem = _alarmTable->item(row, 0);
    if (!timeItem) return;
    auto alarmIdStr = timeItem->data(Qt::UserRole).toString();
    auto alarmId = QUuid::fromString(alarmIdStr);
    if (alarmId.isNull()) return;
    _alarmEngine->closeAlarm(alarmId, QDateTime::currentMSecsSinceEpoch());
    _alarmRowMap.remove(alarmId);
    _alarmTable->removeRow(row);
    for (auto& kv : _alarmRowMap) {
        if (kv > row) kv--;
    }
    appendLog(QString("[%1] 告警已关闭")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
}

void MainWindow::appendLog(const QString& message)
{
    _logEdit->append(message);
    _logPage->appendLog(LogLevel::Info, LogCategory::Connection,
                        _selectedDevice, message);
}

void MainWindow::onAlarmStateChanged(const AlarmEvent& event)
{
    updateAlarmRow(event.alarm);
    if (_repo && _repo->isOpen()) {
        _repo->saveAlarmEvent(event);
    }

    QString stateText;
    switch (event.alarm.state) {
    case AlarmState::ActiveUnacknowledged: stateText = QStringLiteral("激活"); break;
    case AlarmState::ActiveAcknowledged:   stateText = QStringLiteral("已确认"); break;
    case AlarmState::RecoveredUnacknowledged: stateText = QStringLiteral("已恢复"); break;
    case AlarmState::RecoveredAcknowledged: stateText = QStringLiteral("已确认/恢复"); break;
    case AlarmState::Closed:               stateText = QStringLiteral("已关闭"); break;
    }
    appendLog(QString("[%1] 告警状态变更: %2 - %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(event.alarm.deviceId)
        .arg(stateText));
}

void MainWindow::updateAlarmRow(const Alarm& alarm)
{
    auto it = _alarmRowMap.find(alarm.alarmId);
    if (it == _alarmRowMap.end()) return;
    int row = it.value();
    auto* stateItem = _alarmTable->item(row, 4);
    if (!stateItem) return;

    switch (alarm.state) {
    case AlarmState::ActiveUnacknowledged:
        stateItem->setText(QStringLiteral("激活"));
        stateItem->setForeground(QBrush(QColor(231, 76, 60)));
        break;
    case AlarmState::ActiveAcknowledged:
        stateItem->setText(QStringLiteral("已确认"));
        stateItem->setForeground(QBrush(QColor(243, 156, 18)));
        break;
    case AlarmState::RecoveredUnacknowledged:
        stateItem->setText(QStringLiteral("已恢复"));
        stateItem->setForeground(QBrush(QColor(46, 204, 113)));
        break;
    case AlarmState::RecoveredAcknowledged:
        stateItem->setText(QStringLiteral("已确认/恢复"));
        stateItem->setForeground(QBrush(QColor(46, 204, 113)));
        break;
    case AlarmState::Closed:
        stateItem->setText(QStringLiteral("已关闭"));
        stateItem->setForeground(QBrush(QColor(149, 165, 166)));
        break;
    }
}

QString MainWindow::ruleName(RuleCode code) const
{
    switch (code) {
    case RuleCode::HighTemperature:  return QStringLiteral("高温预警");
    case RuleCode::CriticalOverheat: return QStringLiteral("严重过热");
    case RuleCode::Overcurrent:      return QStringLiteral("过流");
    case RuleCode::Stall:            return QStringLiteral("堵转");
    case RuleCode::HighVibration:    return QStringLiteral("振动异常");
    case RuleCode::AbnormalVoltage:  return QStringLiteral("电压异常");
    case RuleCode::HeartbeatTimeout: return QStringLiteral("心跳超时");
    case RuleCode::TelemetryStale:   return QStringLiteral("遥测停更");
    }
    return QStringLiteral("未知");
}

void MainWindow::updateGlobalStatusBar()
{
    int online = _connManager->onlineCount();
    int total = static_cast<int>(_connManager->allConnections().size());
    int activeAlarms = _alarmEngine->totalAlarmCount();
    int rate = _messageCount;
    _messageCount = 0;

    QString msg = QStringLiteral("在线: %1/%2 | 活动告警: %3 | 速率: %4 帧/秒 | %5")
        .arg(online).arg(total).arg(activeAlarms).arg(rate)
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
    statusBar()->showMessage(msg);
}

void MainWindow::onStatusBarRefresh()
{
    updateGlobalStatusBar();
}

void MainWindow::onRefreshTick()
{
    if (!_needsRefresh || _selectedDevice.isEmpty()) return;
    _needsRefresh = false;

    _tempLabel->setText(QString("%1 °C").arg(_latestTelemetry.temperatureC, 0, 'f', 1));
    _speedLabel->setText(QString("%1 RPM").arg(_latestTelemetry.speedRpm));
    _currentLabel->setText(QString("%1 A").arg(_latestTelemetry.currentA, 0, 'f', 2));
    _voltageLabel->setText(QString("%1 V").arg(_latestTelemetry.voltageV, 0, 'f', 1));
    _vibrationLabel->setText(QString("%1 mm/s").arg(_latestTelemetry.vibrationMmPerSec, 0, 'f', 2));

    updateOperatingStateDisplay(_latestTelemetry.operatingState);

    auto* conn = _connManager->connection(_selectedDevice);
    if (conn) {
        _statusLabel->setText(conn->isConnected() ? QStringLiteral("在线") : QStringLiteral("离线"));
        _statusLabel->setStyleSheet(conn->isConnected()
            ? QStringLiteral("color: #2ecc71; font-weight: bold;")
            : QStringLiteral("color: #e74c3c; font-weight: bold;"));
    }
}

void MainWindow::onAddDeviceClicked()
{
    DeviceConfigDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        auto cfg = dlg.deviceConfig();
        if (cfg.deviceId.isEmpty()) return;
        _connManager->addDevice(cfg);
        DeviceSnapshot snap;
        snap.telemetry.deviceId = cfg.deviceId;
        snap.connectionState = ConnectionState::Disconnected;
        snap.receivedAtMs = 0;
        snap.stale = true;
        snap.activeAlarmCount = 0;
        _deviceModel->updateSnapshot(snap);
        if (_repo && _repo->isOpen()) {
            _repo->saveDeviceConfig(cfg);
        }
        appendLog(QString("[%1] 已添加设备: %2")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
            .arg(cfg.deviceId));
        _historyPage->refreshDeviceList();
        _logPage->addDeviceToFilter(cfg.deviceId);
    }
}

void MainWindow::onEditDeviceClicked()
{
    if (_selectedDevice.isEmpty()) return;
    auto* conn = _connManager->connection(_selectedDevice);
    if (!conn) return;
    DeviceConfigDialog dlg(this);
    dlg.setDeviceConfig(conn->config());
    if (dlg.exec() == QDialog::Accepted) {
        appendLog(QString("[%1] 设备配置已更新: %2")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
            .arg(_selectedDevice));
    }
}

void MainWindow::onDeleteDeviceClicked()
{
    if (_selectedDevice.isEmpty()) return;
    auto* conn = _connManager->connection(_selectedDevice);
    if (!conn) return;
    if (conn->isConnected()) {
        conn->disconnectDevice();
    }
    _deviceModel->removeDevice(_selectedDevice);
    _connManager->removeDevice(_selectedDevice);
    appendLog(QString("[%1] 已删除设备: %2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(_selectedDevice));
    _historyPage->refreshDeviceList();
    _logPage->removeDeviceFromFilter(_selectedDevice);
    if (_repo && _repo->isOpen()) {
        _repo->deleteDeviceConfig(_selectedDevice);
    }
    _selectedDevice.clear();
}

void MainWindow::onFilterTextChanged(const QString& text)
{
    _deviceModel->setFilterText(text);
}

void MainWindow::onFilterStateChanged(int index)
{
    switch (index) {
    case 0: _deviceModel->setFilterMode(DeviceListModel::FilterAll); break;
    case 1: _deviceModel->setFilterMode(DeviceListModel::FilterOnline); break;
    case 2: _deviceModel->setFilterMode(DeviceListModel::FilterOffline); break;
    case 3: _deviceModel->setFilterMode(DeviceListModel::FilterHasAlarm); break;
    default: _deviceModel->setFilterMode(DeviceListModel::FilterAll); break;
    }
}

void MainWindow::onBatchGenerateClicked()
{
    DeviceConfig base;
    base.host = QStringLiteral("127.0.0.1");
    base.port = 9000;
    base.enabled = true;
    base.autoConnect = true;

    for (int i = 1; i <= 10; ++i) {
        DeviceConfig cfg = base;
        cfg.deviceId = QStringLiteral("MOTOR-%1").arg(i, 4, 10, QChar('0'));
        cfg.displayName = QStringLiteral("%1号电机").arg(i);

        if (_connManager->connection(cfg.deviceId)) continue;

        _connManager->addDevice(cfg);
        DeviceSnapshot snap;
        snap.telemetry.deviceId = cfg.deviceId;
        snap.connectionState = ConnectionState::Disconnected;
        snap.receivedAtMs = 0;
        snap.stale = true;
        snap.activeAlarmCount = 0;
        _deviceModel->updateSnapshot(snap);
        _logPage->addDeviceToFilter(cfg.deviceId);
        if (_repo && _repo->isOpen()) {
            _repo->saveDeviceConfig(cfg);
        }
    }

    _historyPage->refreshDeviceList();
    appendLog(QString("[%1] 已批量生成 MOTOR-0001 ~ MOTOR-0010")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
}

}
