#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QDateTime>
#include <QString>
#include <QList>

namespace motor {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

enum class LogCategory {
    Connection,
    Protocol,
    Command,
    Alarm,
    Storage,
    Performance
};

struct LogEntry {
    LogLevel level{LogLevel::Info};
    LogCategory category{LogCategory::Connection};
    QDateTime timestamp;
    QString deviceId;
    QString message;
};

class LogPage : public QWidget {
    Q_OBJECT
public:
    explicit LogPage(QWidget* parent = nullptr);

    void appendLog(LogLevel level, LogCategory category,
                   const QString& deviceId, const QString& message);
    void addDeviceToFilter(const QString& deviceId);
    void removeDeviceFromFilter(const QString& deviceId);

private slots:
    void onFilterChanged();
    void onClearClicked();

private:
    void setupUi();
    void applyFilter();
    QString levelText(LogLevel level) const;
    QString categoryText(LogCategory category) const;
    QColor levelColor(LogLevel level) const;

    QList<LogEntry> _buffer;
    QTextEdit* _logView{nullptr};
    QComboBox* _levelFilter{nullptr};
    QComboBox* _categoryFilter{nullptr};
    QComboBox* _deviceFilter{nullptr};
    QPushButton* _clearBtn{nullptr};
    bool _autoScroll{true};
};

}