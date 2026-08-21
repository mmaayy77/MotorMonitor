#include "log_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>

namespace motor {

LogPage::LogPage(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void LogPage::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    auto* filterRow = new QHBoxLayout();

    filterRow->addWidget(new QLabel(QStringLiteral("级别:")));
    _levelFilter = new QComboBox();
    _levelFilter->addItem(QStringLiteral("全部"));
    _levelFilter->addItem(QStringLiteral("Debug"));
    _levelFilter->addItem(QStringLiteral("Info"));
    _levelFilter->addItem(QStringLiteral("Warning"));
    _levelFilter->addItem(QStringLiteral("Error"));
    _levelFilter->addItem(QStringLiteral("Critical"));
    connect(_levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogPage::onFilterChanged);
    filterRow->addWidget(_levelFilter);

    filterRow->addWidget(new QLabel(QStringLiteral("分类:")));
    _categoryFilter = new QComboBox();
    _categoryFilter->addItem(QStringLiteral("全部"));
    _categoryFilter->addItem(QStringLiteral("连接"));
    _categoryFilter->addItem(QStringLiteral("协议"));
    _categoryFilter->addItem(QStringLiteral("控制"));
    _categoryFilter->addItem(QStringLiteral("告警"));
    _categoryFilter->addItem(QStringLiteral("存储"));
    _categoryFilter->addItem(QStringLiteral("性能"));
    connect(_categoryFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogPage::onFilterChanged);
    filterRow->addWidget(_categoryFilter);

    filterRow->addWidget(new QLabel(QStringLiteral("设备:")));
    _deviceFilter = new QComboBox();
    _deviceFilter->addItem(QStringLiteral("全部"));
    _deviceFilter->setMinimumWidth(100);
    connect(_deviceFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogPage::onFilterChanged);
    filterRow->addWidget(_deviceFilter);

    _clearBtn = new QPushButton(QStringLiteral("清空"));
    connect(_clearBtn, &QPushButton::clicked, this, &LogPage::onClearClicked);
    filterRow->addWidget(_clearBtn);

    filterRow->addStretch();
    mainLayout->addLayout(filterRow);

    _logView = new QTextEdit();
    _logView->setReadOnly(true);
    _logView->setFont(QFont(QStringLiteral("Consolas"), 10));
    _logView->setStyleSheet(QStringLiteral("background: #1e1e1e; color: #d4d4d4;"));
    mainLayout->addWidget(_logView, 1);
}

void LogPage::appendLog(LogLevel level, LogCategory category,
                         const QString& deviceId, const QString& message)
{
    LogEntry entry;
    entry.level = level;
    entry.category = category;
    entry.timestamp = QDateTime::currentDateTime();
    entry.deviceId = deviceId;
    entry.message = message;
    _buffer.append(entry);

    if (_buffer.size() > 10000) {
        _buffer.removeFirst();
    }

    bool levelMatch = _levelFilter->currentIndex() == 0 ||
                      static_cast<int>(level) == _levelFilter->currentIndex() - 1;
    bool categoryMatch = _categoryFilter->currentIndex() == 0 ||
                         static_cast<int>(category) == _categoryFilter->currentIndex() - 1;
    bool deviceMatch = _deviceFilter->currentIndex() == 0 ||
                       _deviceFilter->currentText() == deviceId;

    if (levelMatch && categoryMatch && deviceMatch) {
        QColor color = levelColor(level);
        QString html = QStringLiteral(
            "<span style='color:%1;'>[%2]</span> "
            "<span style='color:#888;'>[%3]</span> "
            "<span style='color:#aaa;'>%4</span> "
            "%5")
            .arg(color.name())
            .arg(entry.timestamp.toString(QStringLiteral("HH:mm:ss.zzz")))
            .arg(categoryText(category))
            .arg(deviceId.isEmpty() ? QStringLiteral("") : deviceId + QStringLiteral(" "))
            .arg(message.toHtmlEscaped());

        _logView->append(html);

        if (_autoScroll) {
            auto* sb = _logView->verticalScrollBar();
            sb->setValue(sb->maximum());
        }
    }
}

void LogPage::onFilterChanged()
{
    applyFilter();
}

void LogPage::onClearClicked()
{
    _buffer.clear();
    _logView->clear();
}

void LogPage::applyFilter()
{
    _logView->clear();
    int levelIdx = _levelFilter->currentIndex();
    int catIdx = _categoryFilter->currentIndex();
    int devIdx = _deviceFilter->currentIndex();

    for (const auto& entry : _buffer) {
        if (levelIdx > 0 && static_cast<int>(entry.level) != levelIdx - 1) continue;
        if (catIdx > 0 && static_cast<int>(entry.category) != catIdx - 1) continue;
        if (devIdx > 0 && entry.deviceId != _deviceFilter->currentText()) continue;

        QColor color = levelColor(entry.level);
        QString html = QStringLiteral(
            "<span style='color:%1;'>[%2]</span> "
            "<span style='color:#888;'>[%3]</span> "
            "<span style='color:#aaa;'>%4</span> "
            "%5")
            .arg(color.name())
            .arg(entry.timestamp.toString(QStringLiteral("HH:mm:ss.zzz")))
            .arg(categoryText(entry.category))
            .arg(entry.deviceId.isEmpty() ? QStringLiteral("") : entry.deviceId + QStringLiteral(" "))
            .arg(entry.message.toHtmlEscaped());

        _logView->append(html);
    }

    if (_autoScroll) {
        auto* sb = _logView->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
}

QString LogPage::levelText(LogLevel level) const
{
    switch (level) {
    case LogLevel::Debug:    return QStringLiteral("DEBUG");
    case LogLevel::Info:     return QStringLiteral("INFO");
    case LogLevel::Warning:  return QStringLiteral("WARN");
    case LogLevel::Error:    return QStringLiteral("ERROR");
    case LogLevel::Critical: return QStringLiteral("CRIT");
    }
    return QStringLiteral("UNKNOWN");
}

QString LogPage::categoryText(LogCategory category) const
{
    switch (category) {
    case LogCategory::Connection:  return QStringLiteral("连接");
    case LogCategory::Protocol:    return QStringLiteral("协议");
    case LogCategory::Command:     return QStringLiteral("控制");
    case LogCategory::Alarm:       return QStringLiteral("告警");
    case LogCategory::Storage:     return QStringLiteral("存储");
    case LogCategory::Performance: return QStringLiteral("性能");
    }
    return QStringLiteral("未知");
}

QColor LogPage::levelColor(LogLevel level) const
{
    switch (level) {
    case LogLevel::Debug:    return QColor(100, 100, 100);
    case LogLevel::Info:     return QColor(212, 212, 212);
    case LogLevel::Warning:  return QColor(243, 156, 18);
    case LogLevel::Error:    return QColor(231, 76, 60);
    case LogLevel::Critical: return QColor(255, 0, 0);
    }
    return QColor(212, 212, 212);
}

void LogPage::addDeviceToFilter(const QString& deviceId)
{
    for (int i = 0; i < _deviceFilter->count(); ++i) {
        if (_deviceFilter->itemText(i) == deviceId) return;
    }
    _deviceFilter->addItem(deviceId);
}

void LogPage::removeDeviceFromFilter(const QString& deviceId)
{
    for (int i = 0; i < _deviceFilter->count(); ++i) {
        if (_deviceFilter->itemText(i) == deviceId) {
            _deviceFilter->removeItem(i);
            return;
        }
    }
}

}