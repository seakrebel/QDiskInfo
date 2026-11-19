#pragma once

#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QtMath>

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
        QPen markPen(Qt::white, qMax(1.2, diameter * 0.13), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
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

// The health boxes of CrystalDiskInfo are glossy status-coloured panels with a
// thin darker border, not flat colour fills.
struct StatusBoxStyle
{
    const char *gradient;
    const char *border;
};

inline QString statusBoxStyleSheet(StatusGlyph glyph, bool pill = false)
{
    StatusBoxStyle style;
    switch (glyph) {
    case StatusGlyph::Cross:
        style = {"qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFF0EF, stop:0.40 #FF9E97, "
                 "stop:0.56 #E8453A, stop:0.80 #FF7C6F, stop:1 #FFD5D0)", "#B5402F"};
        break;
    case StatusGlyph::Exclamation:
        style = {"qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFFBE8, stop:0.40 #FFDD80, "
                 "stop:0.56 #F6B32B, stop:0.80 #FFD06A, stop:1 #FFEFC2)", "#C39A2A"};
        break;
    case StatusGlyph::Check:
        style = {"qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #E9F2FF, stop:0.40 #77BBF8, "
                 "stop:0.56 #15ABF4, stop:0.80 #4FBEE8, stop:1 #AEEEFF)", "#4A7EBB"};
        break;
    case StatusGlyph::None:
    default:
        style = {"qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FAFAFA, stop:0.40 #DEDEDE, "
                 "stop:0.56 #BEBEBE, stop:0.80 #D2D2D2, stop:1 #F0F0F0)", "#A3A3A3"};
        break;
    }

    return QStringLiteral("background: %1; border: 1px solid %2; border-radius: %3px;")
        .arg(QLatin1String(style.gradient), QLatin1String(style.border),
             QString::number(pill ? 17 : 3));
}
