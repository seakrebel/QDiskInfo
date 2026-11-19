#include "custombutton.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPoint>
#include <QRect>

namespace {
// Proportions of a CrystalDiskInfo drive tab.
const int TAB_HEIGHT = 46;
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

CustomButton::CustomButton(const QString &text1_, const QString &text2_, const QString &text3_,
                           const QColor &lineColor_, StatusGlyph glyph_, QWidget *parent)
    : QPushButton(parent), text1(text1_), text2(text2_), text3(text3_), lineColor(lineColor_), glyph(glyph_) {
    setFlat(true);
    setFocusPolicy(Qt::NoFocus);
    setMinimumHeight(TAB_HEIGHT);
    setMaximumHeight(TAB_HEIGHT);
    adjustWidthToFitText();
}

// CrystalDiskInfo uses a smaller font than the rest of the window for the tab.
QFont CustomButton::tabFont() const {
    QFont tabFont = font();
    const qreal baseSize = tabFont.pointSizeF() > 0 ? tabFont.pointSizeF() : 11.0;
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

    const int healthWidth = healthMetrics.horizontalAdvance(text1) + TEXT_GAP + tabMetrics.horizontalAdvance(text2);
    const int deviceWidth = tabMetrics.horizontalAdvance(text3);

    const int desiredWidth = qMax(healthWidth, deviceWidth) + BALL_LEFT + BALL_DIAMETER + TEXT_GAP + TEXT_RIGHT_PADDING;
    setMinimumWidth(desiredWidth);
    setMaximumWidth(desiredWidth);
}

void CustomButton::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    const bool selected = isChecked();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor background = selected ? TAB_BACKGROUND_SELECTED : TAB_BACKGROUND;
    if (hovered && !selected) {
        background = TAB_BACKGROUND_HOVER;
    }
    painter.fillRect(rect(), background);

    // The active tab is underlined instead of boxed, exactly like the real one.
    const int borderBottom = rect().bottom() - (selected ? UNDERLINE_HEIGHT : 0);
    painter.setPen(TAB_BORDER);
    painter.drawLine(rect().topLeft(), QPoint(rect().right(), rect().top()));
    painter.drawLine(rect().topLeft(), QPoint(rect().left(), borderBottom));
    painter.drawLine(QPoint(rect().right(), rect().top()), QPoint(rect().right(), borderBottom));

    if (selected) {
        painter.fillRect(QRect(rect().left(), rect().bottom() - UNDERLINE_HEIGHT + 1, rect().width(), UNDERLINE_HEIGHT),
                         TAB_UNDERLINE);
    }

    paintStatusBall(&painter, QRect(BALL_LEFT, (height() - BALL_DIAMETER) / 2, BALL_DIAMETER, BALL_DIAMETER),
                    lineColor, glyph);

    const QFont healthTextFont = healthFont();
    const QFont tabTextFont = tabFont();
    const QFontMetrics healthMetrics(healthTextFont);
    const QFontMetrics tabMetrics(tabTextFont);

    const int textLeft = BALL_LEFT + BALL_DIAMETER + TEXT_GAP;
    const int lineHeight = qMax(healthMetrics.height(), tabMetrics.height());
    const QRect healthLine(textLeft, (height() - 2 * lineHeight) / 2, width() - textLeft - TEXT_RIGHT_PADDING, lineHeight);
    const QRect deviceLine(healthLine.left(), healthLine.bottom() + 1, healthLine.width(), lineHeight);

    // "Good  33 °C" - the health in the status colour, the temperature in black.
    const int healthWidth = healthMetrics.horizontalAdvance(text1);
    painter.setFont(healthTextFont);
    painter.setPen(lineColor);
    painter.drawText(healthLine, Qt::AlignLeft | Qt::AlignVCenter, text1);

    painter.setFont(tabTextFont);
    painter.setPen(TAB_TEXT);
    painter.drawText(healthLine.adjusted(healthWidth + TEXT_GAP, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, text2);
    painter.drawText(deviceLine, Qt::AlignLeft | Qt::AlignVCenter, text3);

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

void CustomButton::setText1(const QString &newText1) {
    if (text1 != newText1) {
        text1 = newText1;
        adjustWidthToFitText();
        update();
    }
}

void CustomButton::setText2(const QString &newText2) {
    if (text2 != newText2) {
        text2 = newText2;
        adjustWidthToFitText();
        update();
    }
}

void CustomButton::setText3(const QString &newText3) {
    if (text3 != newText3) {
        text3 = newText3;
        adjustWidthToFitText();
        update();
    }
}
