/*                   Q G T R E E V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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

#include <QStyledItemDelegate>
#include <QTreeView>

#include "qtcad/defines.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgTypes.h"


class QTCAD_EXPORT QgTreeView : public QTreeView {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(QgTreeView)


public:
	QgTreeView(QWidget *pparent, QgModel *treemodel);
	~QgTreeView() {};

	QModelIndex selected();

	void drawBranches(QPainter* painter, const QRect& rrect, const QModelIndex& index) const override;
	/* Return the QgModel backing this tree view. */
	QgModel *cadModel() const { return m; }

protected:
	void resizeEvent(QResizeEvent *pevent) override;
	void mousePressEvent(QMouseEvent *e) override;

signals:
	void view_changed(QgViewUpdateFlags);

public slots:
	void tree_column_size(const QModelIndex &index);
	void context_menu(const QPoint &point);
	//void expand_path(QString path);
	//void expand_link(const QUrl &link);
	void redo_expansions(void *);
	void redo_highlights();
	void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
	void do_draw_toggle(const QModelIndex &index);
	void qgitem_select_sync(QgItem *);
	void do_view_update(QgViewUpdateFlags);

private:
	void header_state();
	QModelIndex cached_selection_idx = QModelIndex();
	QgModel *m = nullptr;
};

class QTCAD_EXPORT gObjDelegate : public QStyledItemDelegate {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(gObjDelegate)


public:
	gObjDelegate(QgTreeView *tv = nullptr, QWidget *pparent = 0) : QStyledItemDelegate(pparent)
	{
		cadtreeview = tv  ;
	}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

	QgTreeView *cadtreeview = nullptr;
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

