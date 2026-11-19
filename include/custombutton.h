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
    // Height of a tab, and therefore of the strip it sits in.
    static const int TAB_HEIGHT = 46;

    CustomButton(const QString &health, const QString &temperature, const QString &device,
                 const QColor &accent, StatusGlyph statusGlyph, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;

private:
    QFont healthFont() const;
    QFont tabFont() const;
    void adjustWidthToFitText();

    QString healthText;
    QString temperatureText;
    QString deviceText;
    QColor accentColor;
    StatusGlyph glyph;
    bool hovered = false;
};
