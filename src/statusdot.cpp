#include "statusdot.h"
#include "statusicon.h"

#include <QApplication>
#include <QStyle>

void StatusDot::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // The status colour belongs to the ball, not to the cell background: drop
    // the brush the model carries so that the striped row stays visible, then
    // let the style draw that row.
    opt.backgroundBrush = QBrush();
    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

    const int dotSize = 15;
    const QColor color = index.data(Qt::BackgroundRole).value<QColor>();
    const StatusGlyph glyph = static_cast<StatusGlyph>(index.data(Qt::UserRole).toInt());

    const QRect dotRect(opt.rect.center().x() - dotSize / 2, opt.rect.center().y() - dotSize / 2, dotSize, dotSize);
    paintStatusBall(painter, dotRect, color, glyph);
}
