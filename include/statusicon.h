#pragma once

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRadialGradient>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QtGlobal>

// The white mark CrystalDiskInfo draws inside a status ball.
enum class StatusGlyph
{
    None,
    Check,
    Exclamation,
    Cross
};

// Draws the glossy status ball of CrystalDiskInfo: a radial gradient in the
// status colour with a darker rim and, on top, the matching white mark.
inline void paintStatusBall(QPainter *painter, const QRect &rect, const QColor &accent, StatusGlyph glyph)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal diameter = qMin(rect.width(), rect.height());
    const QRectF ball(rect.center().x() - diameter / 2.0, rect.center().y() - diameter / 2.0, diameter, diameter);

    QRadialGradient gradient(QPointF(ball.left() + ball.width() * 0.36, ball.top() + ball.height() * 0.30),
                             diameter * 1.10);
    gradient.setColorAt(0.00, accent.lighter(160));
    gradient.setColorAt(0.55, accent);
    gradient.setColorAt(1.00, accent.darker(140));

    painter->setBrush(gradient);
    painter->setPen(QPen(accent.darker(155), qMax(1.0, diameter / 16.0) * 0.5));
    painter->drawEllipse(ball.adjusted(0.5, 0.5, -0.5, -0.5));

    if (glyph != StatusGlyph::None) {
        const QPen markPen(Qt::white, qMax(1.2, diameter * 0.13), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(markPen);

        const QPointF origin = ball.topLeft();
        const qreal w = ball.width();
        const qreal h = ball.height();

        switch (glyph) {
        case StatusGlyph::Check:
            painter->drawPolyline(QPolygonF({QPointF(origin.x() + w * 0.28, origin.y() + h * 0.52),
                                             QPointF(origin.x() + w * 0.44, origin.y() + h * 0.68),
                                             QPointF(origin.x() + w * 0.73, origin.y() + h * 0.33)}));
            break;
        case StatusGlyph::Exclamation:
            painter->drawLine(QPointF(origin.x() + w * 0.50, origin.y() + h * 0.24),
                              QPointF(origin.x() + w * 0.50, origin.y() + h * 0.58));
            painter->drawPoint(QPointF(origin.x() + w * 0.50, origin.y() + h * 0.76));
            break;
        case StatusGlyph::Cross:
            painter->drawLine(QPointF(origin.x() + w * 0.31, origin.y() + h * 0.31),
                              QPointF(origin.x() + w * 0.69, origin.y() + h * 0.69));
            painter->drawLine(QPointF(origin.x() + w * 0.69, origin.y() + h * 0.31),
                              QPointF(origin.x() + w * 0.31, origin.y() + h * 0.69));
            break;
        case StatusGlyph::None:
            break;
        }
    }

    painter->restore();
}

// CrystalDiskInfo paints a health box as a vertical gloss: a pale tint at the
// top, the saturated status colour in the middle and a pale tint again at the
// bottom, framed by a darker border. Every shade is derived from that one
// colour, so the box, the ball next to it and the health word on the tab
// describing the same disk cannot drift apart. Tinting in integer arithmetic
// keeps the result identical whatever qreal is (float on Qt6, double on Qt5)
// and avoids the -Wfloat-conversion / -Wuseless-cast traps between the two.
inline QColor statusBoxTint(const QColor &accent, int whitePercent)
{
    return QColor((accent.red() * (100 - whitePercent) + 255 * whitePercent) / 100,
                  (accent.green() * (100 - whitePercent) + 255 * whitePercent) / 100,
                  (accent.blue() * (100 - whitePercent) + 255 * whitePercent) / 100);
}

inline QString statusBoxStyleSheet(const QColor &accent, bool pill = false)
{
    const QString gradient = QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:0.40 %2, stop:0.56 %3, stop:0.80 %4, stop:1 %5)")
        .arg(statusBoxTint(accent, 85).name(), statusBoxTint(accent, 35).name(), accent.name(),
             statusBoxTint(accent, 28).name(), statusBoxTint(accent, 73).name());

    return QStringLiteral("background: %1; border: 1px solid %2; border-radius: %3px;")
        .arg(gradient, accent.darker(130).name(), QString::number(pill ? 17 : 3));
}
