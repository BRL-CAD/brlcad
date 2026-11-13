/*            Q G T R E E S E L E C T I O N M O D E L . C P P
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
/** @file QgTreeSelectionModel.cpp
 *
 */

#include "common.h"
#include <unordered_set>
#include <QGuiApplication>
#include "qtcad/QgUtil.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgTreeSelectionModel.h"
#include "qtcad/QgSignalFlags.h"

static void
qg_selection_commit(QgTreeView *treeview, QgModel *model)
{
    if (!treeview || !model)
	return;

    unsigned long long flags = model->selectionCommit();
    emit treeview->view_changed(flags);
}

void
QgTreeSelectionModel::clear_all()
{
    if (!treeview || !treeview->m)
	return;

    treeview->m->selectionClear();
    qg_selection_commit(treeview, treeview->m);
}

void
QgTreeSelectionModel::select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags flags)
{
    QTCAD_SLOT("QgTreeSelectionModel::select QItemSelection", 1);
    QgModel *m = treeview ? treeview->m : NULL;
    if (!m || m->structureChangeInProgress()) {
	QItemSelectionModel::select(selection, flags);
	return;
    }

    QModelIndexList indexes = selection.indexes();
    std::unordered_set<QgItem *> visited;
    for (const QModelIndex &index : indexes) {
	QgItem *snode = static_cast<QgItem *>(index.internalPointer());
	if (!snode || !visited.insert(snode).second)
	    continue;

	// If we are selecting an already selected node, clear it
	if (flags & QItemSelectionModel::Select && m->itemSelected(snode)) {
	    if (!(QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier))) {
		if (flags & QItemSelectionModel::Clear && m->selectionCount() > 1) {
		    m->selectionClear();
		    m->itemSelect(snode);
		} else {
		    m->itemDeselect(snode);
		}
	    }
	} else {
	    if (flags & QItemSelectionModel::Clear)
		m->selectionClear();

	    m->itemSelect(snode);
	}
    }

    qg_selection_commit(treeview, m);
}

void
QgTreeSelectionModel::select(const QModelIndex &index, QItemSelectionModel::SelectionFlags flags)
{
    QTCAD_SLOT("QgTreeSelectionModel::select QModelIndex", 1);
    QgModel *m = treeview ? treeview->m : NULL;
    if (!m || m->structureChangeInProgress()) {
	QItemSelectionModel::select(index, flags);
	return;
    }

    if (flags & QItemSelectionModel::Clear)
	m->selectionClear();

    QgItem *snode = static_cast<QgItem *>(index.internalPointer());
    if (!snode) {
	m->selectionClear();
	qg_selection_commit(treeview, m);
	return;
    }

    if (!(flags & QItemSelectionModel::Deselect)) {

	// If we are selecting an already selected node, clear it
	if (flags & QItemSelectionModel::Select && m->itemSelected(snode)) {
	    m->itemDeselect(snode);
	    qg_selection_commit(treeview, m);
	    return;
	}

	m->itemSelect(snode);

    } else {

	m->itemDeselect(snode);

    }

    qg_selection_commit(treeview, m);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

