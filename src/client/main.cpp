#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QDebug>
#include <memory>
#include "ui/main_window.h"
#include "network/connection_manager.h"
#include "alarm_engine/alarm_engine.h"
#include "alarm_engine/standard_rules.h"
#include "persistence/sqlite_repository.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MotorMonitor"));
    QApplication::setOrganizationName(QStringLiteral("MotorMonitor"));

    QProcess::execute(QStringLiteral("cmd"), QStringList()
        << QStringLiteral("/c") << QStringLiteral("taskkill /F /IM motor_simulator.exe >nul 2>&1"));

    QString simPath = QApplication::applicationDirPath()
                      + QStringLiteral("/../simulator/motor_simulator.exe");
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

    auto dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QString dbPath = dataDir + QStringLiteral("/motor_monitor.db");

    auto connManager = std::make_shared<motor::ConnectionManager>();
    auto alarmEngine = std::make_shared<motor::AlarmEngine>();
    auto repo = std::make_shared<motor::SqliteRepository>(dbPath);

    if (!repo->initialize()) {
        qWarning() << "Failed to initialize database at" << dbPath;
    }

    for (const auto& rule : motor::createStandardAlarmRules()) {
        alarmEngine->registerRule(rule);
    }

    motor::MainWindow window(connManager, alarmEngine, repo);
    window.show();

    return app.exec();
}