#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QThread>
#include <QDebug>
#include <memory>
#include "ui/main_window.h"
#include "network/connection_manager.h"
#include "network/communication_worker.h"
#include "network/storage_worker.h"
#include "alarm_engine/alarm_engine.h"
#include "alarm_engine/standard_rules.h"
#include "core/app_config.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MotorMonitor"));
    QApplication::setOrganizationName(QStringLiteral("MotorMonitor"));

    app.setStyleSheet(QStringLiteral(R"(
        QMainWindow { background: #1e1e2e; }
        QGroupBox {
            font-weight: bold; border: 1px solid #45475a;
            border-radius: 6px; margin-top: 12px; padding-top: 12px;
            color: #cdd6f4;
        }
        QGroupBox::title {
            subcontrol-origin: margin; left: 10px; padding: 0 6px;
            color: #89b4fa;
        }
        QTableWidget {
            background: #181825; alternate-background-color: #1e1e2e;
            gridline-color: #313244; color: #cdd6f4;
            selection-background-color: #45475a;
        }
        QTableWidget::item { padding: 2px 6px; }
        QHeaderView::section {
            background: #313244; color: #a6adc8; font-weight: bold;
            padding: 4px; border: none; border-bottom: 1px solid #45475a;
        }
        QListView {
            background: #181825; color: #cdd6f4; border: 1px solid #45475a;
            border-radius: 4px;
        }
        QListView::item { padding: 6px 8px; }
        QListView::item:selected { background: #45475a; }
        QLabel { color: #cdd6f4; }
        QPushButton {
            background: #45475a; color: #cdd6f4; border: none;
            border-radius: 4px; padding: 6px 14px; font-weight: bold;
        }
        QPushButton:hover { background: #585b70; }
        QPushButton:pressed { background: #313244; }
        QPushButton:disabled { background: #313244; color: #585b70; }
        QSpinBox, QDoubleSpinBox, QComboBox, QDateTimeEdit {
            background: #313244; color: #cdd6f4; border: 1px solid #45475a;
            border-radius: 4px; padding: 4px;
        }
        QComboBox::drop-down { border: none; }
        QComboBox QAbstractItemView {
            background: #313244; color: #cdd6f4; selection-background-color: #45475a;
        }
        QLineEdit {
            background: #313244; color: #cdd6f4; border: 1px solid #45475a;
            border-radius: 4px; padding: 4px;
        }
        QStatusBar { background: #181825; color: #a6adc8; }
        QTabWidget::pane { border: 1px solid #45475a; background: #1e1e2e; }
        QTabBar::tab {
            background: #313244; color: #a6adc8; padding: 6px 16px;
            border: 1px solid #45475a; border-bottom: none;
        }
        QTabBar::tab:selected { background: #1e1e2e; color: #cdd6f4; }
        QScrollBar:vertical {
            background: #181825; width: 10px; border-radius: 5px;
        }
        QScrollBar::handle:vertical {
            background: #45475a; min-height: 30px; border-radius: 5px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )"));

    auto dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QString configPath = dataDir + QStringLiteral("/config.json");

    motor::AppConfig config;
    if (!config.loadFromFile(configPath)) {
        config.saveToFile(configPath);
    }

    if (config.autoStartSimulator) {
        QProcess::execute(QStringLiteral("cmd"), QStringList()
            << QStringLiteral("/c") << QStringLiteral("taskkill /F /IM motor_simulator.exe >nul 2>&1"));

        QString simPath = config.simulatorPath.isEmpty()
            ? QApplication::applicationDirPath() + QStringLiteral("/../simulator/motor_simulator.exe")
            : config.simulatorPath;
        if (QFile::exists(simPath)) {
            qint64 pid = 0;
            if (QProcess::startDetached(simPath, {}, {}, &pid)) {
                qInfo() << "模拟器已启动, PID:" << pid;
            } else {
                qWarning() << "模拟器启动失败:" << simPath;
            }
        } else {
            qWarning() << "未找到模拟器:" << simPath;
        }
    }

    QString dbPath = dataDir + QStringLiteral("/motor_monitor.db");

    auto connManager = std::make_shared<motor::ConnectionManager>();
    auto alarmEngine = std::make_shared<motor::AlarmEngine>();

    for (const auto& rule : motor::createStandardAlarmRules()) {
        alarmEngine->registerRule(rule);
    }

    auto commWorker = new motor::CommunicationWorker(connManager, alarmEngine);
    auto storageWorker = new motor::StorageWorker(dbPath);

    auto commThread = new QThread();
    auto storageThread = new QThread();

    commWorker->moveToThread(commThread);
    storageWorker->moveToThread(storageThread);

    QObject::connect(commThread, &QThread::finished, commWorker, &QObject::deleteLater);
    QObject::connect(storageThread, &QThread::finished, storageWorker, &QObject::deleteLater);

    commThread->start();
    storageThread->start();

    QMetaObject::invokeMethod(storageWorker, "initialize", Qt::BlockingQueuedConnection);

    motor::MainWindow window(commWorker, storageWorker);
    window.show();

    int ret = app.exec();

    commThread->quit();
    storageThread->quit();
    commThread->wait(5000);
    storageThread->wait(5000);

    return ret;
}