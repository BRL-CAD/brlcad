/*                 C A D T R E E V I E W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file cadtreeview.cpp
 *
 * Brief description
 *
 */

#include <iostream>
#include <QPainter>
#include <qmath.h>
#include <QAction>
#include <QMenu>

#include "raytrace.h"
#include "cadapp.h"
#include "cadtreemodel.h"
#include "cadtreeview.h"
#include "cadtreenode.h"

void GObjectDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    struct directory *cdp = RT_DIR_NULL;
    QModelIndex selected = ((CADApp *)qApp)->cadtreeview->selected();
    if (selected.isValid())
	cdp = (struct directory *)(selected.data(DirectoryInternalRole).value<void *>());
    struct directory *pdp = (struct directory *)(index.data(DirectoryInternalRole).value<void *>());
    if (option.state & QStyle::State_Selected) {
	painter->fillRect(option.rect, option.palette.highlight());
	goto text_string;
    }
    if ((!((CADApp *)qApp)->cadtreeview->isExpanded(index) && index.data(RelatedHighlightDisplayRole).toInt())
	    || (cdp == pdp && index.data(RelatedHighlightDisplayRole).toInt())) {
	painter->fillRect(option.rect, QBrush(QColor(220, 200, 30)));
	goto text_string;
    }
    if (index.data(InstanceHighlightDisplayRole).toInt()) {
	painter->fillRect(option.rect, QBrush(QColor(10, 10, 50)));
	goto text_string;
    }

text_string:
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
    this->setContextMenuPolicy(Qt::CustomContextMenu);
}

void
CADTreeView::drawBranches(QPainter* painter, const QRect& rrect, const QModelIndex& index) const
{
    struct directory *cdp = RT_DIR_NULL;
    QModelIndex selected_idx = ((CADTreeView *)this)->selected();
    if (selected_idx.isValid())
	cdp = (struct directory *)(selected_idx.data(DirectoryInternalRole).value<void *>());
    struct directory *pdp = (struct directory *)(index.data(DirectoryInternalRole).value<void *>());
    if (!(index == selected_idx)) {
	if (!((CADApp *)qApp)->cadtreeview->isExpanded(index) && index.data(RelatedHighlightDisplayRole).toInt()) {
	    painter->fillRect(rrect, QBrush(QColor(220, 200, 30)));
	}
	if (cdp == pdp && index.data(RelatedHighlightDisplayRole).toInt()) {
	    painter->fillRect(rrect, QBrush(QColor(220, 200, 30)));
	}
	if (index.data(InstanceHighlightDisplayRole).toInt()) {
	    painter->fillRect(rrect, QBrush(QColor(10, 10, 50)));
	}
    }
    QTreeView::drawBranches(painter, rrect, index);
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

void CADTreeView::context_menu(const QPoint &point)
{
    QModelIndex index = indexAt(point);
    QAction* act = new QAction(index.data().toString(), NULL);
    QMenu *menu = new QMenu("Object Actions", NULL);
    menu->addAction(act);
    menu->exec(mapToGlobal(point));
}

QModelIndex CADTreeView::selected()
{
    if (selectedIndexes().count() == 1) {
	return selectedIndexes().first();
    } else {
	return QModelIndex();
    }
}

void CADTreeView::expand_path(QString path)
{
    int i = 0;
    QStringList path_items = path.split("/", Qt::SkipEmptyParts);
    CADTreeModel *view_model = (CADTreeModel *)model();
    QList<CADTreeNode*> *tree_children = &(view_model->m_root->children);
    while (i < path_items.size()) {
	for (int j = 0; j < tree_children->size(); ++j) {
	    CADTreeNode *test_node = tree_children->at(j);
	    if (test_node->name == path_items.at(i)) {
		QModelIndex path_component = view_model->NodeIndex(test_node);
		if (i == path_items.size() - 1) {
		    selectionModel()->clearSelection();
		    selectionModel()->select(path_component, QItemSelectionModel::Select | QItemSelectionModel::Rows);
		    emit (clicked(path_component));
		} else {
		    expand(path_component);
		    tree_children = &(test_node->children);
		}
		break;
	    }
	}
	i++;
    }
}

void CADTreeView::expand_link(const QUrl &link)
{
    expand_path(link.path());
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

