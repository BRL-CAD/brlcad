/*                   Q G T R E E V I E W . H
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
/** @file QgTreeView.h
 *
 * A .g tree view has some custom drawing to do for its entries, so we
 * subclass QTreeView to establish them.
 */

#ifndef QGTREEVIEW_H
#define QGTREEVIEW_H

#include "common.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <QStyledItemDelegate>
#include <QTreeView>

#include "qtcad/defines.h"
#include "qtcad/QgSelectionProxyModel.h"


class QTCAD_EXPORT QgTreeView : public QTreeView
{
    Q_OBJECT

    public:
	QgTreeView(QWidget *pparent, QgSelectionProxyModel *treemodel);
	~QgTreeView() {};

	QModelIndex selected();

	void drawBranches(QPainter* painter, const QRect& rrect, const QModelIndex& index) const;
	QgSelectionProxyModel *m = NULL;

    protected:
	void resizeEvent(QResizeEvent *pevent);

    public slots:
	void tree_column_size(const QModelIndex &index);
	void context_menu(const QPoint &point);
	void expand_path(QString path);
	void expand_link(const QUrl &link);
	void redo_expansions(void *);
	void redo_highlights();

    private:
	void header_state();
	QModelIndex cached_selection_idx = QModelIndex();
};

class QTCAD_EXPORT gObjDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
	gObjDelegate(QgTreeView *tv = NULL, QWidget *pparent = 0) : QStyledItemDelegate(pparent) {cadtreeview = tv  ;}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

	QgTreeView *cadtreeview = NULL;
};

#endif //QGTREEVIEW_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

