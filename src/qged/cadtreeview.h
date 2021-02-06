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

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 6) && !defined(__clang__)
#  pragma message "Disabling GCC float equality comparison warnings via pragma due to Qt headers..."
#endif
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#undef Success
#include <QTreeView>
#undef Success
#include <QModelIndex>
#undef Success
#include <QHeaderView>
#undef Success
#include <QObject>
#undef Success
#include <QResizeEvent>
#undef Success
#include <QStyledItemDelegate>
#undef Success
#include <QUrl>
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

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

