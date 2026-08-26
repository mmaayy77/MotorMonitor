#pragma once

#include "domain/device_types.h"
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>

namespace motor {

class DeviceConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit DeviceConfigDialog(QWidget* parent = nullptr);

    void setDeviceConfig(const DeviceConfig& config);
    DeviceConfig deviceConfig() const;

private:
    void setupUi();

    QLineEdit* _idEdit{nullptr};
    QLineEdit* _nameEdit{nullptr};
    QLineEdit* _hostEdit{nullptr};
    QSpinBox* _portSpin{nullptr};
    QCheckBox* _enabledCheck{nullptr};
    QCheckBox* _autoConnectCheck{nullptr};
    QDoubleSpinBox* _ratedCurrentSpin{nullptr};
    QComboBox* _protocolCombo{nullptr};
};

}