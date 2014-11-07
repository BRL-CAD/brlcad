#include "cadtreemodel.h"
#include "cadtreeview.h"

#include <iostream>
#include <QPainter>
#include <qmath.h>
#include "raytrace.h"
#include "cadapp.h"


void GObjectDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{

    if (option.state & QStyle::State_Selected)
	painter->fillRect(option.rect, option.palette.highlight());

    QString text = index.data().toString();
    int bool_op = index.data(BoolInternalRole).toInt();
    switch (bool_op) {
	case OP_INTERSECT:
	    text.prepend("  + ");
	    break;
	case OP_SUBTRACT:
	    text.prepend("  - ");
	    break;
    }
    text.prepend(" ");

    // rect with width proportional to value
    //QRect rect(option.rect.adjusted(4,4,-4,-4));

    // draw the value bar
    //painter->fillRect(rect, QBrush(QColor("steelblue")));
    //
    // draw text label

    QImage type_icon = index.data(TypeIconDisplayRole).value<QImage>().scaledToHeight(option.rect.height()-2);
    QRect image_rect = type_icon.rect();
    image_rect.moveTo(option.rect.topLeft());
    QRect text_rect(type_icon.rect().topRight(), option.rect.bottomRight());
    text_rect.moveTo(image_rect.topRight());
    image_rect.translate(0, 1);
    painter->drawImage(image_rect, type_icon);

    painter->drawText(text_rect, text, QTextOption(Qt::AlignLeft));
}

QSize GObjectDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize name_size = option.fontMetrics.size(Qt::TextSingleLine, index.data().toString());
    QSize bool_size;
    int bool_op = index.data(BoolInternalRole).toInt();
    switch (bool_op) {
	case OP_UNION:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, " ");
	    break;
	case OP_INTERSECT:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, "   + ");
	    break;
	case OP_SUBTRACT:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, "   - ");
	    break;
    }
    int item_width = name_size.rwidth() + bool_size.rwidth() + 18;
    int item_height = name_size.rheight();
    return QSize(item_width, item_height);
}


CADTreeView::CADTreeView(QWidget *pparent) : QTreeView(pparent)
{
}

void CADTreeView::header_state()
{
    header()->setStretchLastSection(true);
    int header_length = header()->length();
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Fixed);
    resizeColumnToContents(0);
    int column_width = columnWidth(0);
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    if (column_width > header_length) {
	header()->setStretchLastSection(false);
    } else {
	header()->setStretchLastSection(true);
    }
}

void CADTreeView::resizeEvent(QResizeEvent *)
{
    header_state();
}

void CADTreeView::tree_column_size(const QModelIndex &)
{
    header_state();
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

