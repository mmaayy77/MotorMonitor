#include "realtime_chart.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QDateTime>
#include <QtMath>

namespace motor {

RealtimeChart::RealtimeChart(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(180);
    setMinimumWidth(400);
    _buffer.reserve(static_cast<qsizetype>(_maxPoints));
}

void RealtimeChart::appendData(double temperature, double speed, double current)
{
    _buffer.append({temperature, speed, current});
    while (_buffer.size() > _maxPoints) {
        _buffer.removeFirst();
    }
    update();
}

void RealtimeChart::clearData()
{
    _buffer.clear();
    update();
}

void RealtimeChart::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), QColor(30, 30, 30));

    drawGrid(painter);
    drawCurves(painter);
    drawLabels(painter);
}

void RealtimeChart::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
    update();
}

QRectF RealtimeChart::chartRect() const
{
    return QRectF(_marginLeft, _marginTop,
                  width() - _marginLeft - _marginRight,
                  height() - _marginTop - _marginBottom);
}

void RealtimeChart::drawGrid(QPainter& painter)
{
    auto cr = chartRect();
    if (cr.width() <= 0 || cr.height() <= 0) return;

    painter.setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));
    int ySteps = 5;
    for (int i = 0; i <= ySteps; ++i) {
        double y = cr.top() + cr.height() * i / ySteps;
        painter.drawLine(QPointF(cr.left(), y), QPointF(cr.right(), y));
    }

    int xSteps = qMin(6, static_cast<int>(cr.width() / 80));
    if (xSteps < 1) xSteps = 1;
    for (int i = 0; i <= xSteps; ++i) {
        double x = cr.left() + cr.width() * i / xSteps;
        painter.drawLine(QPointF(x, cr.top()), QPointF(x, cr.bottom()));
    }
}

void RealtimeChart::drawCurves(QPainter& painter)
{
    auto cr = chartRect();
    if (cr.width() <= 0 || cr.height() <= 0 || _buffer.isEmpty()) return;

    updateRanges();

    double tempRange = _tempMax - _tempMin;
    double speedRange = _speedMax - _speedMin;
    double currentRange = _currentMax - _currentMin;
    if (tempRange < 1) tempRange = 1;
    if (speedRange < 1) speedRange = 1;
    if (currentRange < 1) currentRange = 1;

    int n = static_cast<int>(_buffer.size());
    double dx = cr.width() / static_cast<double>(_maxPoints - 1);

    auto drawCurve = [&](QColor color, double minVal, double range,
                          double (DataPoint::*member)) {
        QPainterPath path;
        bool first = true;
        for (int i = 0; i < n; ++i) {
            double x = cr.right() - (n - 1 - i) * dx;
            double val = (_buffer[i].*member);
            double y = cr.bottom() - (val - minVal) / range * cr.height();
            y = qBound(cr.top(), y, cr.bottom());
            if (first) {
                path.moveTo(x, y);
                first = false;
            } else {
                path.lineTo(x, y);
            }
        }
        painter.setPen(QPen(color, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    };

    drawCurve(QColor(231, 76, 60), _tempMin, tempRange, &DataPoint::temperature);
    drawCurve(QColor(52, 152, 219), _speedMin, speedRange, &DataPoint::speed);
    drawCurve(QColor(46, 204, 113), _currentMin, currentRange, &DataPoint::current);
}

void RealtimeChart::drawLabels(QPainter& painter)
{
    auto cr = chartRect();
    painter.setPen(QColor(180, 180, 180));
    QFont font(QStringLiteral("Consolas"), 9);
    painter.setFont(font);

    painter.setPen(QColor(231, 76, 60));
    painter.drawText(QPointF(cr.left(), 15), QStringLiteral("温度(°C)"));
    painter.setPen(QColor(52, 152, 219));
    painter.drawText(QPointF(cr.left() + 100, 15), QStringLiteral("转速(RPM)"));
    painter.setPen(QColor(46, 204, 113));
    painter.drawText(QPointF(cr.left() + 200, 15), QStringLiteral("电流(A)"));

    painter.setPen(QColor(140, 140, 140));
    int ySteps = 5;
    for (int i = 0; i <= ySteps; ++i) {
        double y = cr.top() + cr.height() * i / ySteps;
        double tempVal = _tempMax - (_tempMax - _tempMin) * i / ySteps;
        double speedVal = _speedMax - (_speedMax - _speedMin) * i / ySteps;
        double currentVal = _currentMax - (_currentMax - _currentMin) * i / ySteps;

        QString label = QStringLiteral("%1°C/%2R/%3A")
            .arg(tempVal, 0, 'f', 0)
            .arg(speedVal, 0, 'f', 0)
            .arg(currentVal, 0, 'f', 1);
        painter.drawText(QPointF(cr.right() + 5, y + 4), label);
    }

    painter.setPen(QColor(100, 100, 100));
    painter.drawText(QPointF(cr.left(), cr.bottom() + 20), QStringLiteral("← 60秒"));
    painter.drawText(QPointF(cr.right() - 60, cr.bottom() + 20), QStringLiteral("现在 →"));
}

void RealtimeChart::updateRanges()
{
    _tempMin = 0;
    _tempMax = 120;
    _speedMin = 0;
    _speedMax = 3000;
    _currentMin = 0;
    _currentMax = 20;

    for (const auto& p : _buffer) {
        if (p.temperature > _tempMax) _tempMax = p.temperature + 10;
        if (p.speed > _speedMax) _speedMax = p.speed + 200;
        if (p.current > _currentMax) _currentMax = p.current + 2;
    }
    if (_tempMax < 50) _tempMax = 50;
    if (_speedMax < 500) _speedMax = 500;
    if (_currentMax < 5) _currentMax = 5;
}

}