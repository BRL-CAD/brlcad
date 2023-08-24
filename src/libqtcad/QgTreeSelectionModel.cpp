/*            Q G T R E E S E L E C T I O N M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2023 United States Government as represented by
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
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <stack>
#include <vector>
#include <QGuiApplication>
#include "qtcad/QgUtil.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgTreeSelectionModel.h"
#include "qtcad/QgSignalFlags.h"

void
QgTreeSelectionModel::clear_all()
{
    QgModel *m = treeview->m;

    std::vector<BSelectState *> sv = m->gedp->dbi_state->get_selected_states(NULL);
    BSelectState *ss = sv[0];
    ss->clear();
    ss->characterize();
}

void
QgTreeSelectionModel::select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags flags)
{
    QTCAD_SLOT("QgTreeSelectionModel::select QItemSelection", 1);
    QgModel *m = treeview->m;
    struct ged *gedp = m->gedp;

    std::vector<BSelectState *> ssv = gedp->dbi_state->get_selected_states(NULL);

    if (ssv.size() != 1)
	return;

    BSelectState *ss = ssv[0];

#if 0
    QModelIndexList dl = selection.indexes();
    for (long int i = 0; i < dl.size(); i++) {
	QgItem *snode = static_cast<QgItem *>(dl.at(i).internalPointer());
#else
    // Above should work (and does on Linux) - using a Windows workaround from
    // https://stackoverflow.com/q/15123109/2037687 for the moment...
    QModelIndexList *dl = new QModelIndexList(selection.indexes());
    for (long int i = 0; i < dl->size(); i++) {
	QgItem *snode = static_cast<QgItem *>(dl->at(i).internalPointer());
#endif

	// If we are selecting an already selected node, clear it
	if (flags & QItemSelectionModel::Select && ss->is_selected(snode->path_hash())) {
	    if (!(QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier))) {
		if (flags & QItemSelectionModel::Clear && ss->selected.size() > 1) {
		    ss->clear();
		    std::vector<unsigned long long> path_hashes = snode->path_items();
		    ss->select_hpath(path_hashes);
		} else {
		    std::vector<unsigned long long> path_hashes = snode->path_items();
		    ss->deselect_hpath(path_hashes);
		}
	    }
	} else {
	    if (flags & QItemSelectionModel::Clear)
		ss->clear();

	    std::vector<unsigned long long> path_hashes = snode->path_items();
	    ss->select_hpath(path_hashes);
	}
    }

    // Done manipulating paths - update metadata
    ss->characterize();

    unsigned long long sflags = QG_VIEW_SELECT;
    if (ss->draw_sync())
	sflags |= QG_VIEW_REFRESH;

    emit treeview->view_changed(sflags);
    emit treeview->m->layoutChanged();
}

void
QgTreeSelectionModel::select(const QModelIndex &index, QItemSelectionModel::SelectionFlags flags)
{
    QTCAD_SLOT("QgTreeSelectionModel::select QModelIndex", 1);
    QgModel *m = treeview->m;
    struct ged *gedp = m->gedp;

    std::vector<BSelectState *> ssv = gedp->dbi_state->get_selected_states(NULL);

    if (ssv.size() != 1)
	return;

    BSelectState *ss = ssv[0];

    if (flags & QItemSelectionModel::Clear)
	ss->clear();

    QgItem *snode = static_cast<QgItem *>(index.internalPointer());
    if (!snode) {
	ss->clear();

	// Done manipulating paths - update metadata
	ss->characterize();

	unsigned long long sflags = QG_VIEW_SELECT;
	if (ss->draw_sync())
	    sflags |= QG_VIEW_REFRESH;

	emit treeview->view_changed(sflags);
	emit treeview->m->layoutChanged();
	return;
    }

    if (!(flags & QItemSelectionModel::Deselect)) {

	// If we are selecting an already selected node, clear it
	if (flags & QItemSelectionModel::Select && ss->is_selected(snode->path_hash())) {
	    std::vector<unsigned long long> path_hashes = snode->path_items();
	    ss->deselect_hpath(path_hashes);
	    // Done manipulating paths - update metadata
	    ss->characterize();
	    unsigned long long sflags = QG_VIEW_SELECT;
	    if (ss->draw_sync())
		sflags |= QG_VIEW_REFRESH;
	    emit treeview->view_changed(sflags);
	    emit treeview->m->layoutChanged();
	    return;
	}

	std::vector<unsigned long long> path_hashes = snode->path_items();
	ss->select_hpath(path_hashes);

    } else {

	std::vector<unsigned long long> path_hashes = snode->path_items();
	ss->deselect_hpath(path_hashes);

    }

    // Done manipulating paths - update metadata
    ss->characterize();

    unsigned long long sflags = QG_VIEW_SELECT;
    if (ss->draw_sync())
	sflags |= QG_VIEW_REFRESH;

    emit treeview->view_changed(sflags);
    emit treeview->m->layoutChanged();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

