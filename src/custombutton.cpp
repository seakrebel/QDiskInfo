#include "custombutton.h"

#include <QFontInfo>
#include <QFontMetrics>
#include <QPainter>
#include <QPoint>
#include <QRect>

namespace {
// Proportions of a CrystalDiskInfo drive tab.
const int BALL_DIAMETER = 15;
const int BALL_LEFT = 9;
const int TEXT_GAP = 8;
const int TEXT_RIGHT_PADDING = 10;
const int UNDERLINE_HEIGHT = 3;
const QColor TAB_BACKGROUND(0xEC, 0xEC, 0xEC);
const QColor TAB_BACKGROUND_HOVER(0xF6, 0xF6, 0xF6);
const QColor TAB_BACKGROUND_SELECTED(0xFF, 0xFF, 0xFF);
const QColor TAB_BORDER(0xC6, 0xC6, 0xC6);
const QColor TAB_UNDERLINE(0x1E, 0x9C, 0xF0);
const QColor TAB_TEXT(0x20, 0x20, 0x20);
}

CustomButton::CustomButton(const QString &health, const QString &temperature, const QString &device,
                           const QColor &accent, StatusGlyph statusGlyph, QWidget *parent)
    : QPushButton(parent), healthText(health), temperatureText(temperature), deviceText(device),
      accentColor(accent), glyph(statusGlyph) {
    setFlat(true);
    setFocusPolicy(Qt::NoFocus);
    setMinimumHeight(TAB_HEIGHT);
    setMaximumHeight(TAB_HEIGHT);
    adjustWidthToFitText();
}

// CrystalDiskInfo uses a smaller font for the tab than for the rest of the
// window. QFontInfo is asked for the point size because a font may describe
// itself in pixels instead, which leaves pointSizeF() at -1.
QFont CustomButton::tabFont() const {
    QFont tabFont = font();
    const qreal baseSize = qMax(6.0, QFontInfo(tabFont).pointSizeF());
    tabFont.setPointSizeF(qBound(8.0, baseSize * 0.78, 11.0));
    return tabFont;
}

QFont CustomButton::healthFont() const {
    QFont healthFont = tabFont();
    healthFont.setBold(true);
    return healthFont;
}

void CustomButton::adjustWidthToFitText() {
    const QFontMetrics healthMetrics(healthFont());
    const QFontMetrics tabMetrics(tabFont());

    const int healthWidth = healthMetrics.horizontalAdvance(healthText) + TEXT_GAP
                            + tabMetrics.horizontalAdvance(temperatureText);
    const int deviceWidth = tabMetrics.horizontalAdvance(deviceText);

    const int desiredWidth = qMax(healthWidth, deviceWidth) + BALL_LEFT + BALL_DIAMETER + TEXT_GAP + TEXT_RIGHT_PADDING;
    setMinimumWidth(desiredWidth);
    setMaximumWidth(desiredWidth);
}

void CustomButton::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    const bool selected = isChecked();

    QPainter painter(this);

    QColor background = selected ? TAB_BACKGROUND_SELECTED : TAB_BACKGROUND;
    if (hovered && !selected) {
        background = TAB_BACKGROUND_HOVER;
    }
    painter.fillRect(rect(), background);

    // The active tab is underlined instead of boxed, exactly like the real one.
    // The hairlines are drawn without antialiasing so that they stay one pixel
    // wide instead of smearing over two.
    const int borderBottom = rect().bottom() - (selected ? UNDERLINE_HEIGHT : 0);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(TAB_BORDER);
    painter.drawLine(rect().topLeft(), QPoint(rect().right(), rect().top()));
    painter.drawLine(rect().topLeft(), QPoint(rect().left(), borderBottom));
    painter.drawLine(QPoint(rect().right(), rect().top()), QPoint(rect().right(), borderBottom));

    if (selected) {
        painter.fillRect(QRect(rect().left(), rect().bottom() - UNDERLINE_HEIGHT + 1, rect().width(), UNDERLINE_HEIGHT),
                         TAB_UNDERLINE);
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    paintStatusBall(&painter, QRect(BALL_LEFT, (height() - BALL_DIAMETER) / 2, BALL_DIAMETER, BALL_DIAMETER),
                    accentColor, glyph);

    const QFont healthTextFont = healthFont();
    const QFont tabTextFont = tabFont();
    const QFontMetrics healthMetrics(healthTextFont);
    const QFontMetrics tabMetrics(tabTextFont);

    const int textLeft = BALL_LEFT + BALL_DIAMETER + TEXT_GAP;
    const int lineHeight = qMax(healthMetrics.height(), tabMetrics.height());
    const QRect healthLine(textLeft, (height() - 2 * lineHeight) / 2, width() - textLeft - TEXT_RIGHT_PADDING, lineHeight);
    const QRect deviceLine(healthLine.left(), healthLine.bottom() + 1, healthLine.width(), lineHeight);

    // "Good  33 °C" - the health in the status colour, the temperature in black.
    const int healthWidth = healthMetrics.horizontalAdvance(healthText);
    painter.setFont(healthTextFont);
    painter.setPen(accentColor);
    painter.drawText(healthLine, Qt::AlignLeft | Qt::AlignVCenter, healthText);

    painter.setFont(tabTextFont);
    painter.setPen(TAB_TEXT);
    painter.drawText(healthLine.adjusted(healthWidth + TEXT_GAP, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, temperatureText);
    painter.drawText(deviceLine, Qt::AlignLeft | Qt::AlignVCenter, deviceText);

    painter.end();
}

void CustomButton::resizeEvent(QResizeEvent *event) {
    QPushButton::resizeEvent(event);
    adjustWidthToFitText();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void CustomButton::enterEvent(QEnterEvent *event) {
    hovered = true;
    update();
    QPushButton::enterEvent(event);
}
#else
void CustomButton::enterEvent(QEvent *event) {
    hovered = true;
    update();
    QPushButton::enterEvent(event);
}
#endif

void CustomButton::leaveEvent(QEvent *event) {
    hovered = false;
    update();
    QPushButton::leaveEvent(event);
}
