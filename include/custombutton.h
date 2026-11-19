#pragma once

#include "statusicon.h"

#include <QColor>
#include <QEvent>
#include <QFont>
#include <QPushButton>
#include <QString>
#include <QWidget>

// A disk tab, drawn the way CrystalDiskInfo draws its drive selector: a flat
// tab with a status ball, the health and the temperature on the first line and
// the device node on the second one. The selected tab is white with a blue
// underline, the rest keep the grey of the tab strip.
class CustomButton : public QPushButton {
    Q_OBJECT

public:
    CustomButton(const QString &health_, const QString &temperature_, const QString &device_,
                 const QColor &accent_, StatusGlyph glyph_, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void setText1(const QString &newText1);
    void setText2(const QString &newText2);
    void setText3(const QString &newText3);
    void adjustWidthToFitText();

private:
    QFont healthFont() const;
    QFont tabFont() const;

    QString text1;
    QString text2;
    QString text3;
    QColor lineColor;
    StatusGlyph glyph;
    bool hovered = false;
};
