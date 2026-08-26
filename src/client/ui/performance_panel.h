#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>

namespace motor {

class PerformancePanel : public QWidget {
    Q_OBJECT
public:
    explicit PerformancePanel(QWidget* parent = nullptr);

    void recordMessage();
    void recordWrite();
    void recordError();
    void recordReconnect();

private slots:
    void onRefresh();

private:
    void setupUi();

    QLabel* _connLabel{nullptr};
    QLabel* _msgRateLabel{nullptr};
    QLabel* _writeRateLabel{nullptr};
    QLabel* _errorLabel{nullptr};
    QTimer* _timer{nullptr};

    int _msgCount{0};
    int _writeCount{0};
    int _errorCount{0};
    int _reconnectCount{0};
};

}