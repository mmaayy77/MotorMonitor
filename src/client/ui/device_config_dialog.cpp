#include "device_config_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>

namespace motor {

DeviceConfigDialog::DeviceConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void DeviceConfigDialog::setupUi()
{
    setWindowTitle(QStringLiteral("设备配置"));
    setMinimumWidth(400);

    auto* mainLayout = new QVBoxLayout(this);

    auto* basicGroup = new QGroupBox(QStringLiteral("基本信息"));
    auto* basicForm = new QFormLayout(basicGroup);

    _idEdit = new QLineEdit();
    _idEdit->setPlaceholderText(QStringLiteral("MOTOR-0001"));
    basicForm->addRow(QStringLiteral("设备ID:"), _idEdit);

    _nameEdit = new QLineEdit();
    _nameEdit->setPlaceholderText(QStringLiteral("1号电机"));
    basicForm->addRow(QStringLiteral("显示名称:"), _nameEdit);

    _hostEdit = new QLineEdit();
    _hostEdit->setText(QStringLiteral("127.0.0.1"));
    basicForm->addRow(QStringLiteral("主机地址:"), _hostEdit);

    _portSpin = new QSpinBox();
    _portSpin->setRange(1, 65535);
    _portSpin->setValue(9000);
    basicForm->addRow(QStringLiteral("端口:"), _portSpin);

    mainLayout->addWidget(basicGroup);

    auto* settingsGroup = new QGroupBox(QStringLiteral("连接设置"));
    auto* settingsForm = new QFormLayout(settingsGroup);

    _enabledCheck = new QCheckBox(QStringLiteral("启用设备"));
    _enabledCheck->setChecked(true);
    settingsForm->addRow(_enabledCheck);

    _autoConnectCheck = new QCheckBox(QStringLiteral("自动重连"));
    _autoConnectCheck->setChecked(true);
    settingsForm->addRow(_autoConnectCheck);

    _ratedCurrentSpin = new QDoubleSpinBox();
    _ratedCurrentSpin->setRange(0.1, 100.0);
    _ratedCurrentSpin->setValue(12.0);
    _ratedCurrentSpin->setSuffix(QStringLiteral(" A"));
    settingsForm->addRow(QStringLiteral("额定电流:"), _ratedCurrentSpin);

    mainLayout->addWidget(settingsGroup);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void DeviceConfigDialog::setDeviceConfig(const DeviceConfig& config)
{
    _idEdit->setText(config.deviceId);
    _nameEdit->setText(config.displayName);
    _hostEdit->setText(config.host);
    _portSpin->setValue(static_cast<int>(config.port));
    _enabledCheck->setChecked(config.enabled);
    _autoConnectCheck->setChecked(config.autoConnect);
    _ratedCurrentSpin->setValue(static_cast<double>(config.ratedCurrentA));
}

DeviceConfig DeviceConfigDialog::deviceConfig() const
{
    DeviceConfig cfg;
    cfg.deviceId = _idEdit->text().trimmed();
    cfg.displayName = _nameEdit->text().trimmed();
    cfg.host = _hostEdit->text().trimmed();
    cfg.port = static_cast<quint16>(_portSpin->value());
    cfg.enabled = _enabledCheck->isChecked();
    cfg.autoConnect = _autoConnectCheck->isChecked();
    cfg.ratedCurrentA = static_cast<float>(_ratedCurrentSpin->value());
    return cfg;
}

}