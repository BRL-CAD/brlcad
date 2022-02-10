/*                    Q G T R E E V I E W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file QgTreeView.cpp
 *
 * BRL-CAD .g tree visualization widget.
 *
 */

#include "common.h"
#include <QPainter>
#include <QHeaderView>
#include <QMenu>
#include <QUrl>
#include "qtcad/QgModel.h"
#include "qtcad/QgSelectionProxyModel.h"
#include "qtcad/QgTreeView.h"

void gObjDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    struct directory *cdp = RT_DIR_NULL;
    if (!cadtreeview)
	return;
    QModelIndex selected = cadtreeview->selected();
    if (selected.isValid())
	cdp = (struct directory *)(selected.data(QgSelectionProxyModel::DirectoryInternalRole).value<void *>());
    struct directory *pdp = (struct directory *)(index.data(QgSelectionProxyModel::DirectoryInternalRole).value<void *>());
    if (option.state & QStyle::State_Selected) {
	painter->fillRect(option.rect, option.palette.highlight());
	goto text_string;
    }
    if ((!cadtreeview->isExpanded(index) && index.data(QgSelectionProxyModel::RelatedHighlightDisplayRole).toInt())
	    || (cdp == pdp && index.data(QgSelectionProxyModel::RelatedHighlightDisplayRole).toInt())) {
	painter->fillRect(option.rect, QBrush(QColor(220, 200, 30)));
	goto text_string;
    }
    if (index.data(QgSelectionProxyModel::InstanceHighlightDisplayRole).toInt()) {
	painter->fillRect(option.rect, QBrush(QColor(10, 10, 50)));
	goto text_string;
    }

text_string:
    QString text = index.data().toString();
    int bool_op = index.data(QgSelectionProxyModel::BoolInternalRole).toInt();
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

    QImage type_icon = index.data(QgSelectionProxyModel::TypeIconDisplayRole).value<QImage>().scaledToHeight(option.rect.height()-2);
    QRect image_rect = type_icon.rect();
    image_rect.moveTo(option.rect.topLeft());
    QRect text_rect(type_icon.rect().topRight(), option.rect.bottomRight());
    text_rect.moveTo(image_rect.topRight());
    image_rect.translate(0, 1);
    painter->drawImage(image_rect, type_icon);

    painter->drawText(text_rect, text, QTextOption(Qt::AlignLeft));
}

QSize gObjDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize name_size = option.fontMetrics.size(Qt::TextSingleLine, index.data().toString());
    QSize bool_size;
    int bool_op = index.data(QgSelectionProxyModel::BoolInternalRole).toInt();
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


QgTreeView::QgTreeView(QWidget *pparent, QgSelectionProxyModel *treemodel) : QTreeView(pparent)
{
    this->setContextMenuPolicy(Qt::CustomContextMenu);

    m = treemodel;

    setModel(treemodel);

    setItemDelegate(new gObjDelegate(this));

    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header()->setStretchLastSection(true);
    QObject::connect(this, &QgTreeView::expanded, this, &QgTreeView::tree_column_size);
    QObject::connect(this, &QgTreeView::collapsed, this, &QgTreeView::tree_column_size);
    QObject::connect(this, &QgTreeView::clicked, treemodel, &QgSelectionProxyModel::update_selected_node_relationships);
    QObject::connect(this, &QgTreeView::customContextMenuRequested, (QgTreeView *)this, &QgTreeView::context_menu);
}

void
QgTreeView::drawBranches(QPainter* painter, const QRect& rrect, const QModelIndex& index) const
{
    struct directory *cdp = RT_DIR_NULL;
    QModelIndex selected_idx = ((QgTreeView *)this)->selected();
    if (selected_idx.isValid())
	cdp = (struct directory *)(selected_idx.data(QgSelectionProxyModel::DirectoryInternalRole).value<void *>());
    struct directory *pdp = (struct directory *)(index.data(QgSelectionProxyModel::DirectoryInternalRole).value<void *>());
    if (!(index == selected_idx)) {
	if (!(QgTreeView *)this->isExpanded(index) && index.data(QgSelectionProxyModel::RelatedHighlightDisplayRole).toInt()) {
	    painter->fillRect(rrect, QBrush(QColor(220, 200, 30)));
	}
	if (cdp == pdp && index.data(QgSelectionProxyModel::RelatedHighlightDisplayRole).toInt()) {
	    painter->fillRect(rrect, QBrush(QColor(220, 200, 30)));
	}
	if (index.data(QgSelectionProxyModel::InstanceHighlightDisplayRole).toInt()) {
	    painter->fillRect(rrect, QBrush(QColor(10, 10, 50)));
	}
    }
    QTreeView::drawBranches(painter, rrect, index);
}

void QgTreeView::header_state()
{
    //header()->setStretchLastSection(true);
    //int header_length = header()->length();
    //header()->setSectionResizeMode(0, QHeaderView::Fixed);
    //resizeColumnToContents(0);
    //int column_width = columnWidth(0);

    // TODO - when the dock widget is resized, there is a missing
    // update somewhere - the scrollbar doesn't appear until the tree
    // is selected...

    header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    header()->setStretchLastSection(false);
    if (header()->count() == 1) {
	header()->setStretchLastSection(false);
    } else {
	header()->setStretchLastSection(true);
    }
    header()->setMaximumHeight(1);
}

void QgTreeView::resizeEvent(QResizeEvent *)
{
    header_state();
    emit m->layoutChanged();
}

void QgTreeView::tree_column_size(const QModelIndex &)
{
    header_state();
}

void QgTreeView::context_menu(const QPoint &point)
{
    QModelIndex index = indexAt(point);
    QAction* act = new QAction(index.data().toString(), NULL);
    QMenu *menu = new QMenu("Object Actions", NULL);
    menu->addAction(act);
    menu->exec(mapToGlobal(point));
}

QModelIndex QgTreeView::selected()
{
    if (selectedIndexes().count() == 1) {
	return selectedIndexes().first();
    } else {
	return QModelIndex();
    }
}

void QgTreeView::expand_path(QString path)
{
    int i = 0;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList path_items = path.split("/", QString::SkipEmptyParts);
#else
    QStringList path_items = path.split("/", Qt::SkipEmptyParts);
#endif
    QgSelectionProxyModel *view_model = (QgSelectionProxyModel *)model();
    QgModel *sm = (QgModel *)view_model->sourceModel();
    QgItem *r = sm->root();
    while (i < path_items.size()) {

	for (int j = 0; j < r->childCount(); j++) {
	    QgItem *c = r->child(j);
	    gInstance *g = c->instance();
	    if (!g)
		continue;
	    if (QString::fromStdString(g->dp_name) != path_items.at(i)) {
		continue;
	    }
	    QModelIndex path_component = view_model->NodeIndex(c);
	    if (i == path_items.size() - 1) {
		selectionModel()->clearSelection();
		selectionModel()->select(path_component, QItemSelectionModel::Select | QItemSelectionModel::Rows);
		emit (clicked(path_component));
	    } else {
		expand(path_component);
		r = c;
	    }
	    break;
	}
	i++;
    }
}

void QgTreeView::expand_link(const QUrl &link)
{
    expand_path(link.path());
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

