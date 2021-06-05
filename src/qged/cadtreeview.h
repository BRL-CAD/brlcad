/*                  C A D T R E E V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file cadtreemodel.h
 *
 * Defines a Qt tree view for BRL-CAD's geometry database format.
 *
 */

#ifndef CAD_TREEVIEW_H
#define CAD_TREEVIEW_H

#include <QTreeView>
#include <QModelIndex>
#include <QHeaderView>
#include <QObject>
#include <QResizeEvent>
#include <QStyledItemDelegate>
#include <QUrl>

class GObjectDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
	GObjectDelegate(QWidget *pparent = 0) : QStyledItemDelegate(pparent) {}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

class CADTreeView : public QTreeView
{
    Q_OBJECT

    public:
	CADTreeView(QWidget *pparent);
	~CADTreeView() {};

	QModelIndex selected();

	void drawBranches(QPainter* painter, const QRect& rrect, const QModelIndex& index) const;

    protected:
	void resizeEvent(QResizeEvent *pevent);

    public slots:
	void tree_column_size(const QModelIndex &index);
	void context_menu(const QPoint &point);
	void expand_path(QString path);
	void expand_link(const QUrl &link);

    private:
        void header_state();
};

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

