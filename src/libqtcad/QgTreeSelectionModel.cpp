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

#include "ged/selection_state.h"

void
QgTreeSelectionModel::clear_all()
{
	QgModel *m = treeview->cadModel();

	ged_selection_clear(m->ged(), nullptr);
}

void
QgTreeSelectionModel::select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags flags)
{
	QTCAD_SLOT("QgTreeSelectionModel::select QItemSelection", 1);
	QgModel *m = treeview->cadModel();
	struct ged *gedp = m->ged();

	QModelIndexList dl = selection.indexes();
	for (long int i = 0; i < dl.size(); i++) {
		QgItem *snode = static_cast<QgItem *>(dl.at(i).internalPointer());
		if (!snode)
			continue;

		// If we are selecting an already selected node, clear it
		if (flags & QItemSelectionModel::Select &&
			ged_selection_is_path_selected(gedp, nullptr, snode->path_hash())) {
			if (!(QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier))) {
				if (flags & QItemSelectionModel::Clear &&
					ged_selection_count(gedp, nullptr) > 1) {
					ged_selection_clear(gedp, nullptr);
					std::vector<unsigned long long> path_hashes = snode->path_items();
					ged_selection_select_path_ids(gedp, nullptr,
						path_hashes.data(), path_hashes.size(), 0);
				}
				else {
					std::vector<unsigned long long> path_hashes = snode->path_items();
					ged_selection_deselect_path_ids(gedp, nullptr,
						path_hashes.data(), path_hashes.size(), 0);
				}
			}
		}
		else {
			if (flags & QItemSelectionModel::Clear)
				ged_selection_clear(gedp, nullptr);

			std::vector<unsigned long long> path_hashes = snode->path_items();
			ged_selection_select_path_ids(gedp, nullptr,
				path_hashes.data(), path_hashes.size(), 0);
		}
	}

	// Done manipulating paths - update metadata
	ged_selection_recompute(gedp, nullptr);

	QgViewUpdateFlags sflags = QG_VIEW_SELECT;
	if (ged_selection_draw_sync(gedp, nullptr))
		sflags |= QG_VIEW_REFRESH;

	emit treeview->view_changed(sflags);
	treeview->cadModel()->notifySelectionItemsChanged();
}

void
QgTreeSelectionModel::select(const QModelIndex &index, QItemSelectionModel::SelectionFlags flags)
{
	QTCAD_SLOT("QgTreeSelectionModel::select QModelIndex", 1);
	QgModel *m = treeview->cadModel();
	struct ged *gedp = m->ged();

	if (flags & QItemSelectionModel::Clear)
		ged_selection_clear(gedp, nullptr);

	QgItem *snode = static_cast<QgItem *>(index.internalPointer());
	if (!snode) {
		ged_selection_clear(gedp, nullptr);

		// Done manipulating paths - update metadata
		ged_selection_recompute(gedp, nullptr);

		QgViewUpdateFlags sflags = QG_VIEW_SELECT;
		if (ged_selection_draw_sync(gedp, nullptr))
			sflags |= QG_VIEW_REFRESH;

		emit treeview->view_changed(sflags);
		treeview->cadModel()->notifySelectionItemsChanged();
		return;
	}

	if (!(flags & QItemSelectionModel::Deselect)) {

		// If we are selecting an already selected node, clear it
		if (flags & QItemSelectionModel::Select &&
			ged_selection_is_path_selected(gedp, nullptr, snode->path_hash())) {
			std::vector<unsigned long long> path_hashes = snode->path_items();
			ged_selection_deselect_path_ids(gedp, nullptr,
				path_hashes.data(), path_hashes.size(), 0);
			// Done manipulating paths - update metadata
			ged_selection_recompute(gedp, nullptr);
			QgViewUpdateFlags sflags = QG_VIEW_SELECT;
			if (ged_selection_draw_sync(gedp, nullptr))
				sflags |= QG_VIEW_REFRESH;
			emit treeview->view_changed(sflags);
			treeview->cadModel()->notifySelectionItemsChanged();
			return;
		}

		std::vector<unsigned long long> path_hashes = snode->path_items();
		ged_selection_select_path_ids(gedp, nullptr,
			path_hashes.data(), path_hashes.size(), 0);

	}
	else {

		std::vector<unsigned long long> path_hashes = snode->path_items();
		ged_selection_deselect_path_ids(gedp, nullptr,
			path_hashes.data(), path_hashes.size(), 0);

	}

	// Done manipulating paths - update metadata
	ged_selection_recompute(gedp, nullptr);

	QgViewUpdateFlags sflags = QG_VIEW_SELECT;
	if (ged_selection_draw_sync(gedp, nullptr))
		sflags |= QG_VIEW_REFRESH;

	emit treeview->view_changed(sflags);
	treeview->cadModel()->notifySelectionItemsChanged();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
