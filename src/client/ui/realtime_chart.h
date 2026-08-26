#pragma once

#include <QWidget>
#include <QVector>
#include <QPointF>

namespace motor {

class RealtimeChart : public QWidget {
    Q_OBJECT
public:
    explicit RealtimeChart(QWidget* parent = nullptr);

    void appendData(double temperature, double speed, double current);
    void clearData();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct DataPoint {
        double temperature;
        double speed;
        double current;
    };

    QVector<DataPoint> _buffer;
    int _maxPoints{300};
    int _marginLeft{60};
    int _marginRight{20};
    int _marginTop{20};
    int _marginBottom{40};

    double _tempMin{0};
    double _tempMax{120};
    double _speedMin{0};
    double _speedMax{3000};
    double _currentMin{0};
    double _currentMax{20};

    QRectF chartRect() const;
    void drawGrid(QPainter& painter);
    void drawCurves(QPainter& painter);
    void drawLabels(QPainter& painter);
    void updateRanges();
};

}