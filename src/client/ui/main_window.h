#pragma once

#include "domain/device_types.h"
#include "domain/command_types.h"
#include "domain/alarm_types.h"
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QSpinBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QTimer>
#include <QHash>
#include <QUuid>
#include <memory>

class QSplitter;
class QListView;
class QWidget;
class QTabWidget;

namespace motor {

class ConnectionManager;
class AlarmEngine;
class DeviceListModel;
class Repository;
class RealtimeChart;
class HistoryPage;
class LogPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(std::shared_ptr<ConnectionManager> connManager,
                        std::shared_ptr<AlarmEngine> alarmEngine,
                        std::shared_ptr<Repository> repo,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onDeviceSelected(const QModelIndex& index);
    void onTelemetryReceived(DeviceId deviceId, const Telemetry& telemetry);
    void onConnectionStateChanged(DeviceId deviceId, ConnectionState oldState, ConnectionState newState);
    void onAlarmRaised(const AlarmEvent& event);
    void onAlarmStateChanged(const AlarmEvent& event);
    void onCommandResult(DeviceId deviceId, const CommandResult& result);

    void onConnectClicked();
    void onDisconnectClicked();
    void onStartClicked();
    void onStopClicked();
    void onEmergencyStopClicked();
    void onSetSpeedClicked();
    void onAckAlarmClicked();
    void onClearAlarmClicked();
    void onStatusBarRefresh();
    void onRefreshTick();

    void onAddDeviceClicked();
    void onEditDeviceClicked();
    void onDeleteDeviceClicked();
    void onBatchGenerateClicked();
    void onFilterTextChanged(const QString& text);
    void onFilterStateChanged(int index);

private:
    void setupUi();
    void setupDeviceList();
    void setupDetailPanel();
    void setupAlarmPanel();
    void setupTrendPanel();
    void loadDefaultDevices();
    void updateDetailPanel(const DeviceId& deviceId);
    void updateOperatingStateDisplay(OperatingState state);
    void updateGlobalStatusBar();
    void appendLog(const QString& message);
    void addAlarmEvent(const AlarmEvent& event);
    void updateAlarmRow(const Alarm& alarm);
    QString ruleName(RuleCode code) const;

    std::shared_ptr<ConnectionManager> _connManager;
    std::shared_ptr<AlarmEngine> _alarmEngine;
    std::shared_ptr<Repository> _repo;

    DeviceListModel* _deviceModel{nullptr};
    DeviceId _selectedDevice;

    QSplitter* _mainSplitter{nullptr};
    QTabWidget* _tabWidget{nullptr};
    QWidget* _listPanel{nullptr};
    QWidget* _rightPanel{nullptr};
    QWidget* _monitorPanel{nullptr};
    QListView* _deviceList{nullptr};
    QWidget* _detailPanel{nullptr};
    QWidget* _alarmPanel{nullptr};
    QWidget* _trendPanel{nullptr};
    HistoryPage* _historyPage{nullptr};
    LogPage* _logPage{nullptr};

    QLabel* _statusLabel{nullptr};
    QLabel* _listStatusLabel{nullptr};
    QLabel* _operatingStateLabel{nullptr};
    QLabel* _tempLabel{nullptr};
    QLabel* _speedLabel{nullptr};
    QLabel* _currentLabel{nullptr};
    QLabel* _voltageLabel{nullptr};
    QLabel* _vibrationLabel{nullptr};

    QPushButton* _connectBtn{nullptr};
    QPushButton* _disconnectBtn{nullptr};
    QPushButton* _startBtn{nullptr};
    QPushButton* _stopBtn{nullptr};
    QPushButton* _emergencyBtn{nullptr};
    QSpinBox* _speedSpin{nullptr};
    QPushButton* _setSpeedBtn{nullptr};

    QTableWidget* _alarmTable{nullptr};
    QTextEdit* _logEdit{nullptr};

    RealtimeChart* _chart{nullptr};

    QLineEdit* _searchEdit{nullptr};
    QComboBox* _filterCombo{nullptr};
    QPushButton* _addDeviceBtn{nullptr};
    QPushButton* _editDeviceBtn{nullptr};
    QPushButton* _deleteDeviceBtn{nullptr};

    QHash<QUuid, int> _alarmRowMap;
    QTimer* _statusTimer{nullptr};
    QTimer* _refreshTimer{nullptr};
    int _messageCount{0};
    bool _needsRefresh{false};
    Telemetry _latestTelemetry;
};

}