#include "performance_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

namespace motor {

PerformancePanel::PerformancePanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, this, &PerformancePanel::onRefresh);
    _timer->start(1000);
}

void PerformancePanel::setupUi()
{
    auto* group = new QGroupBox(QStringLiteral("性能观测"));
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(5, 5, 5, 5);

    _connLabel = new QLabel(QStringLiteral("连接: 0/0"));
    _msgRateLabel = new QLabel(QStringLiteral("消息: 0 帧/秒"));
    _writeRateLabel = new QLabel(QStringLiteral("写入: 0 条/秒"));
    _errorLabel = new QLabel(QStringLiteral("异常: 0 错误, 0 重连"));

    layout->addWidget(_connLabel);
    layout->addWidget(_msgRateLabel);
    layout->addWidget(_writeRateLabel);
    layout->addWidget(_errorLabel);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(group);
}

void PerformancePanel::recordMessage()
{
    _msgCount++;
}

void PerformancePanel::recordWrite()
{
    _writeCount++;
}

void PerformancePanel::recordError()
{
    _errorCount++;
}

void PerformancePanel::recordReconnect()
{
    _reconnectCount++;
}

void PerformancePanel::onRefresh()
{
    _msgRateLabel->setText(QStringLiteral("消息: %1 帧/秒").arg(_msgCount));
    _writeRateLabel->setText(QStringLiteral("写入: %1 条/秒").arg(_writeCount));
    _errorLabel->setText(QStringLiteral("异常: %1 错误, %2 重连").arg(_errorCount).arg(_reconnectCount));
    _msgCount = 0;
    _writeCount = 0;
}

}