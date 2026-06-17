/*                         Q G M O D E L . C P P
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
/** @file QgModel.cpp
 *
 * QAbstractItemModel of a BRL-CAD .g database.
 *
 *
 * We make instances from the tops objects, as well as all immediate children
 * of all combs, sketches, etc.  We store these in an unordered map with their
 * hash as the key.  The old and new tops lists become the seed vectors used to
 * check what has changed in the tree.  QgItems will store hashes to their
 * instances, and we can tell via a failed lookup whether a particular instance
 * in the QgItem is still valid.
 *
 * QgItems form the explicit hierarchical representation implied by the instances,
 * which means instance changes have potentially global impacts on the QgItems
 * hierarchy and it must be fully checked for validity (and repaired as
 * appropriate) after each cycle.  A validity check would be something like:
 * 1) check the hash of the QgItem against the GED database index to see if it
 * is still valid.  If not, it must be either removed or replaced
 */

#include "common.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <QAction>
#include <QFileInfo>
#include <QVector>

#include "bu/env.h"
#include "bu/hash.h"
#include "bu/sort.h"
#include "bu/time.h"
#include "ged/bsg_ged_draw.h"
#include "ged/db_index.h"
#include "ged/event_txn.h"
#include "ged/selection_state.h"
#include "raytrace.h"
#define ALPHANUM_IMPL
#include "../libged/alphanum.h"

#include "qtcad/QgModel.h"
#include "qtcad/QgSession.h"
#include "qtcad/QgUtil.h"
#include "qtcad/QgSignalFlags.h"

/* Validate the character constant stored in QgItem::op matches the enum. */
static_assert(DB_OP_UNION == 'u', "DB_OP_UNION enum value changed; update QgItem::op default in QgModel.h");

static void
qgmodel_append_unique_path(std::vector<std::string> &paths, const char *path)
{
	if (!path || !path[0])
		return;
	std::string spath(path);
	if (std::find(paths.begin(), paths.end(), spath) == paths.end())
		paths.push_back(spath);
}

static void
qgmodel_append_unique_paths_from_words(std::vector<std::string> &paths,
				       const char *words)
{
	if (!words || !words[0])
		return;

	std::string names(words);
	size_t pos = 0;
	while (pos < names.size()) {
		pos = names.find_first_not_of(" \t\r\n", pos);
		if (pos == std::string::npos)
			break;
		size_t end = names.find_first_of(" \t\r\n", pos);
		std::string path = names.substr(pos,
			(end == std::string::npos) ? std::string::npos : end - pos);
		qgmodel_append_unique_path(paths, path.c_str());
		if (end == std::string::npos)
			break;
		pos = end + 1;
	}
}

static std::string
qgmodel_parent_path(const char *path)
{
	if (!path || !path[0])
		return std::string();

	std::string parent(path);
	while (!parent.empty() && parent.back() == '/')
		parent.pop_back();
	if (parent.empty())
		return std::string();

	size_t slash = parent.rfind('/');
	if (slash == std::string::npos)
		return std::string();

	parent.erase(slash);
	while (!parent.empty() && parent.back() == '/')
		parent.pop_back();
	return parent;
}

static bool
qgmodel_path_has_queued_ancestor(const std::vector<std::string> &paths,
				 const std::string &path)
{
	std::string parent = qgmodel_parent_path(path.c_str());
	while (!parent.empty()) {
		if (std::find(paths.begin(), paths.end(), parent) !=
			paths.end())
			return true;
		parent = qgmodel_parent_path(parent.c_str());
	}
	return false;
}

static std::unordered_map<const QgModel *, QgModel::DrawTimingStats>
qgmodel_draw_timing_stats;
static std::unordered_set<const QgModel *> qgmodel_draw_timing_enabled;

static bool
qgmodel_draw_timing_is_enabled(const QgModel *model)
{
	return model && qgmodel_draw_timing_enabled.find(model) !=
		qgmodel_draw_timing_enabled.end();
}

static QgModel::DrawTimingStats &
qgmodel_draw_timing_stats_for(const QgModel *model)
{
	return qgmodel_draw_timing_stats[model];
}

static int
qgmodel_event_affects_hierarchy(ged_event_kind kind)
{
	switch (kind) {
	case GED_EVENT_OBJECT_ADDED:
	case GED_EVENT_OBJECT_REMOVED:
	case GED_EVENT_OBJECT_RENAMED:
	case GED_EVENT_COMB_TREE_CHANGED:
	case GED_EVENT_COMB_INSTANCE_REMOVED:
	case GED_EVENT_OBJECT_REFERENCES_REMOVED:
	case GED_EVENT_BATCH_REBUILD:
		return 1;
	case GED_EVENT_NONE:
	case GED_EVENT_OBJECT_MODIFIED:
	case GED_EVENT_OBJECT_VISIBILITY_CHANGED:
	case GED_EVENT_ATTRIBUTE_CHANGED:
	case GED_EVENT_MATERIAL_CHANGED:
	case GED_EVENT_DATABASE_METADATA_CHANGED:
	default:
		return 0;
	}
}

static int
qgmodel_event_requires_model_reset(ged_event_kind kind)
{
	switch (kind) {
	case GED_EVENT_BATCH_REBUILD:
		return 1;
	default:
		return 0;
	}
}

static bool
qgmodel_index_id_less(struct ged *gedp,
		      unsigned long long id1,
		      unsigned long long id2)
{
	struct ged_db_index_record rec1;
	struct ged_db_index_record rec2;
	int have1 = ged_db_index_record_get(gedp, id1, &rec1);
	int have2 = ged_db_index_record_get(gedp, id2, &rec2);

	if (!have1 && !have2)
		return id1 < id2;
	if (!have1)
		return true;
	if (!have2)
		return false;

	const char *n1 = rec1.name ? rec1.name : "";
	const char *n2 = rec2.name ? rec2.name : "";
	int acmp = alphanum_impl(n1, n2, nullptr);
	if (acmp != 0)
		return acmp < 0;
	return id1 < id2;
}

struct QgItem_cmp {
	inline bool operator() (const QgItem *i1, const QgItem *i2)
	{
		if (!i1 && !i2)
			return false;
		if (!i1 && i2)
			return true;
		if (i1 && !i2)
			return false;
		if (!i1->instanceHash() && !i2->instanceHash())
			return false;
		if (!i1->instanceHash() && i2->instanceHash())
			return true;
		if (i1->instanceHash() && !i2->instanceHash())
			return false;

		struct ged_db_index_record rec1;
		struct ged_db_index_record rec2;
		int have1 = ged_db_index_record_get(i1->model()->ged(), i1->instanceHash(), &rec1);
		int have2 = ged_db_index_record_get(i2->model()->ged(), i2->instanceHash(), &rec2);

		if (!have1 && !have2)
			return false;
		if (!have1 && have2)
			return true;
		if (have1 && !have2)
			return false;

		const char *n1 = rec1.name ? rec1.name : "";
		const char *n2 = rec2.name ? rec2.name : "";
		if (alphanum_impl(n1, n2, nullptr) < 0)
			return true;

		return false;
	}
};

static void
qgmodel_mark_invalid_subtree(std::unordered_set<QgItem *> &invalid, QgItem *item)
{
	if (!item || invalid.find(item) != invalid.end())
		return;

	invalid.insert(item);
	for (QgItem *child : item->childItems())
		qgmodel_mark_invalid_subtree(invalid, child);
}

QgItem::QgItem(unsigned long long hash, QgModel *ictx)
{
	mdl = ictx;
	ihash = hash;
	parentItem = nullptr;

	// Allocate the name buffer
	name_ptr = new struct bu_vls;
	bu_vls_init(name_ptr);

	if (!mdl || !mdl->ged())
		return;

	struct ged_db_index_record rec;
	if (!ged_db_index_record_get(mdl->ged(), ihash, &rec))
		return;

	// Local item information
	if (rec.name)
		bu_vls_sprintf(name_ptr, "%s", rec.name);
	c_count = rec.child_count;
	dp = rec.dp;
	/* Use session icon provider so the same image data is shared across
	 * all items of the same visual type. */
	icon = ictx->session()->icon(dp);
}

QgItem::~QgItem()
{
	bu_vls_free(name_ptr);
	delete name_ptr;
}

void
QgItem::open()
{
	if (!mdl)
		return;
	open_itm = true;
	QModelIndex ind = mdl->NodeIndex(this);
	mdl->fetchMore(ind);
}


void
QgItem::close()
{
	open_itm = false;
}

void
QgItem::appendChild(QgItem *c)
{
	children.push_back(c);
}

QgItem *
QgItem::child(int n)
{
	if (n < 0 || n >= (int)children.size())
		return nullptr;

	return children[n];
}

int
QgItem::childCount() const
{
	return children.size();
}

int
QgItem::columnCount() const
{
	return 1;
}

QgItem *
QgItem::parent()
{
	return parentItem;
}

int
QgItem::childNumber() const
{
	if (parentItem) {
		auto row_it = parentItem->c_noderow.find(const_cast<QgItem *>(this));
		if (row_it != parentItem->c_noderow.end())
			return row_it->second;
		for (size_t i = 0; i < parentItem->children.size(); i++) {
			if (parentItem->children[i] == this)
				return i;
		}
		bu_log("WARNING - invalid parent/child inquiry\n");
	}

	return 0;
}

std::vector<unsigned long long>
QgItem::path_items() const
{
	std::vector<unsigned long long> pitems;
	const QgItem *citem = this;
	pitems.push_back(this->ihash);
	while (citem->parentItem) {
		citem = citem->parentItem;
		if (citem->ihash)
			pitems.push_back(citem->ihash);
	}
	std::reverse(pitems.begin(), pitems.end());
	return pitems;
}

unsigned long long
QgItem::path_hash() const
{
	std::vector<unsigned long long> pitems = path_items();
	return ged_db_index_path_hash(mdl->ged(), pitems.data(), pitems.size(), 0);
}

QgModel::QgModel(QObject *p, const char *npath)
	: QAbstractItemModel(p)
{
	// Create the session that owns the GED context for this model.
	// The session constructor allocates struct ged, initializes the GED
	// compatibility services, and creates the default fallback view.
	m_session = new QgSession(this);
	struct ged *gedp = m_session->ged();

	/* Independent view state is managed through BSG view-scope records. */

	// Set up the root item
	rootItem = new QgItem(0, this);
	rootItem->mdl = this;

	items = new std::unordered_set<QgItem *>;
	items->clear();
	itemIndexClear();

	draw_observer_token = ged_draw_observer_add(gedp,
		&QgModel::drawObserverCallback, this);
	event_observer_token = ged_event_observer_add(gedp,
		GED_EVENT_OBSERVER_POST_RECONCILE,
		&QgModel::eventObserverCallback, this);

	// If there's no path, at least for the moment we have nothing to model.
	// In the future we may want to consider setting up a default environment
	// backed by a temp file...
	if (!npath)
		return;

	if (npath) {
		int ac = 2;
		const char *av[3];
		av[0] = "open";
		av[1] = npath;
		av[2] = nullptr;
		run_cmd(gedp->ged_result_str, ac, (const char **)av);
	}
}

QgModel::~QgModel()
{
	qgmodel_draw_timing_enabled.erase(this);
	qgmodel_draw_timing_stats.erase(this);

	if (m_session && m_session->ged() && draw_observer_token) {
		ged_draw_observer_remove(m_session->ged(), draw_observer_token);
		draw_observer_token = 0;
	}
	if (m_session && m_session->ged() && event_observer_token) {
		ged_event_observer_remove(m_session->ged(), event_observer_token);
		event_observer_token = 0;
	}

	if (items) {
		for (QgItem *item : *items) {
			if (item && item != rootItem)
				delete item;
		}
		items->clear();
		delete items;
		items = nullptr;
	}
	itemIndexClear();

	/* m_session is parented to this QObject and will be deleted automatically;
	 * it owns the GED context. */
	delete rootItem;
	rootItem = nullptr;
}

QgModel::DrawTimingStats
QgModel::drawTimingStats() const
{
	auto it = qgmodel_draw_timing_stats.find(this);
	if (it == qgmodel_draw_timing_stats.end())
		return DrawTimingStats();
	return it->second;
}

void
QgModel::resetDrawTimingStats()
{
	qgmodel_draw_timing_stats[this] = DrawTimingStats();
}

void
QgModel::setDrawTimingStatsEnabled(bool enabled)
{
	if (enabled) {
		qgmodel_draw_timing_enabled.insert(this);
		(void)qgmodel_draw_timing_stats_for(this);
	} else {
		qgmodel_draw_timing_enabled.erase(this);
	}
}

// Note - this is a private method and must be run from within g_update's
// begin/end RebuildModel block.  Also, it assumes all invalid QgItem pointers
// have been removed from the children arrays. (The ability to recognize an
// invalid QgItem is defined at that level - we don't want to re-create it
// here.)
void
QgModel::item_rebuild(QgItem *item, std::unordered_set<QgItem *> *invalid)
{
	// Top level is a special case and is handled separately
	if (item == rootItem) {
		return;
	}

	unsigned long long chash = item->ihash;
	struct ged *gedp = m_session->ged();
	struct ged_db_index_record rec;
	if (!ged_db_index_record_get(gedp, chash, &rec)) {
		if (invalid) {
			for (QgItem *old_child : item->children)
				qgmodel_mark_invalid_subtree(*invalid, old_child);
		}
		item->children.clear();
		item->c_noderow.clear();
		item->c_count = 0;
		return;
	}

	if (rec.child_count == 0) {
		if (invalid) {
			for (QgItem *old_child : item->children)
				qgmodel_mark_invalid_subtree(*invalid, old_child);
		}
		item->children.clear();
		item->c_noderow.clear();
		item->c_count = 0;
		return;
	}

	// If we have no cached children, update only the child count - fetchMore
	// will populate it if and when it is expanded, and there will be no
	// QgItems we need to reuse.  However, an edit operation may have
	// invalidated the original c_count stored when the item was created.
	if (!item->children.size()) {
		item->c_count = rec.child_count;
		return;
	}

	std::unordered_map<unsigned long long, QgItem *> oc;
	for (size_t i = 0; i < item->children.size(); i++) {
		QgItem *qii = item->children[i];
		oc[qii->ihash] = qii;
	}

	// Iterate the instance's array of child hashes, building up the
	// corresponding QgItems array using either the stored QgItems from the
	// previous state or new items.
	std::vector<QgItem *> nc;
	for (size_t row = 0; row < rec.child_count; row++) {
		struct ged_db_index_child child;
		if (!ged_db_index_child_at(gedp, chash, row, &child))
			continue;
		// For each new child, look up its instance in the original data to see
		// if we have a corresponding QgItem available.
		if (oc.find(child.record.id) != oc.end()) {
			// We have a viable QgItem from the previous state - reuse it
			nc.push_back(oc[child.record.id]);
		}
		else {
			// Previous tree did not have an appropriate QgItem -
			// make a new one
			QgItem *nitem = new QgItem(child.record.id, this);
			nitem->parentItem = item;
			nitem->op = child.bool_op;
			nc.push_back(nitem);
			items->insert(nitem);
		}
	}
	item->c_count = nc.size();

	// If anything changed since the last time children was built, we need to
	// replace children's contents with the new vector.  This is being run
	// inside g_update's beginRebuildModel block, so we don't need to try and
	// manage individual rows here.
	if (nc != item->children) {
		if (invalid) {
			std::unordered_set<QgItem *> reused(nc.begin(), nc.end());
			for (QgItem *old_child : item->children) {
				if (reused.find(old_child) == reused.end())
					qgmodel_mark_invalid_subtree(*invalid, old_child);
			}
		}
		item->children = nc;
		// define a map for quick QgItem * -> index lookups
		item->c_noderow.clear();
		for (size_t i = 0; i < nc.size(); i++) {
			item->c_noderow[nc[i]] = i;
		}
	}
}

void
QgModel::g_update(struct db_i *n_dbip)
{
	struct ged *gedp = m_session->ged();

	if (!n_dbip) {
		// if we have no dbip, clear out everything
		model_reset_in_progress = true;
		beginResetModel();
		std::unordered_set<QgItem *>::iterator s_it;
		for (s_it = items->begin(); s_it != items->end(); s_it++) {
			QgItem *itm = *s_it;
			delete itm;
		}
		// Deleted all items, but we need a root item regardless
		// of whether a .g is open - recreate it
		rootItem = new QgItem(0, this);
		rootItem->mdl = this;

		items->clear();
		itemIndexClear();
		tops_items.clear();
		emit mdl_changed_db((void *)gedp);
		emit view_changed(QG_VIEW_DRAWN);
		changed_db_flag = 0;
		endResetModel();
		model_reset_in_progress = false;
		return;
	}

	// If we have a dbip and the changed flag is set, figure out what's different
	if (changed_db_flag) {
		model_reset_in_progress = true;
		beginResetModel();

		// Step 1 - make sure our instances are current - i.e. they match the
		// .g database state
		ged_db_index_refresh(gedp);

		// Clear out any QgItems with invalid info.  We need to be fairly
		// aggressive here - first we find all the existing invalid ones, and
		// then we invalidate all their children recursively - any item with an
		// invalid parent is invalid.
		std::unordered_set<QgItem *> invalid;
		std::unordered_set<QgItem *>::iterator s_it;
		for (s_it = items->begin(); s_it != items->end(); s_it++) {
			QgItem *itm = *s_it;
			struct ged_db_index_record item_rec;
			if (!ged_db_index_record_get(gedp, itm->ihash, &item_rec)) {
				qgmodel_mark_invalid_subtree(invalid, itm);
				continue;
			}
			if (!itm->dp && item_rec.dp)
				qgmodel_mark_invalid_subtree(invalid, itm);
		}

		std::vector<QgItem *> item_snapshot(items->begin(), items->end());
		for (QgItem *i_itm : item_snapshot) {
			if (!i_itm || items->find(i_itm) == items->end())
				continue;
			if (invalid.find(i_itm) != invalid.end())
				continue;
			// Remove any invalid QgItem references from the children arrays
			std::vector<QgItem *> vchildren;
			for (size_t i = 0; i < i_itm->children.size(); i++) {
				QgItem *itm = i_itm->children[i];
				if (invalid.find(itm) == invalid.end()) {
					// Valid - keep it
					vchildren.push_back(itm);
				}
			}
			i_itm->children = vchildren;
			// Child QgItem pointers are now all valid - rebuild full children
			// array to match current .g state
			item_rebuild(i_itm, &invalid);
		}

		// Validate existing tops QgItems based on the tops data.
		size_t tops_count = ged_db_index_tops(gedp, 1, nullptr, 0);
		std::vector<unsigned long long> tops(tops_count);
		if (tops_count)
			ged_db_index_tops(gedp, 1, tops.data(), tops.size());
		std::unordered_set<unsigned long long> tset(tops.begin(), tops.end());
		std::unordered_map<unsigned long long, QgItem *> vtops_items;
		for (size_t i = 0; i < tops_items.size(); i++) {
			QgItem *titem = tops_items[i];
			if (tset.find(titem->ihash) != tset.end()) {
				// Still a tops item
				vtops_items[titem->ihash] = titem;
			}
		}

		// Using tops_instances, construct a new tops vector.  Reuse any still valid
		// QgItems, and make new ones
		std::vector<QgItem *> ntops_items;

		for (size_t i = 0; i < tops.size(); i++) {
			std::unordered_map<unsigned long long, QgItem *>::iterator v_it;
			v_it = vtops_items.find(tops[i]);
			if (v_it != vtops_items.end()) {
				ntops_items.push_back(v_it->second);
			}
			else {
				QgItem *nitem = new QgItem(tops[i], this);
				nitem->parentItem = rootItem;
				nitem->op = DB_OP_UNION;
				ntops_items.push_back(nitem);
				items->insert(nitem);
			}
		}

		// Set the new tops items as children of the rootItem.
		std::sort(ntops_items.begin(), ntops_items.end(), QgItem_cmp());
		std::unordered_set<QgItem *> reused_tops(ntops_items.begin(), ntops_items.end());
		for (QgItem *old_top : tops_items) {
			if (reused_tops.find(old_top) == reused_tops.end())
				qgmodel_mark_invalid_subtree(invalid, old_top);
		}
		tops_items = ntops_items;
		rootItem->children.clear();
		for (size_t i = 0; i < tops_items.size(); i++) {
			rootItem->appendChild(tops_items[i]);
		}
		rootItem->c_noderow.clear();
		for (size_t i = 0; i < tops_items.size(); i++) {
			rootItem->c_noderow[tops_items[i]] = i;
		}

		// Finally, delete the invalid QgItems
		std::unordered_set<QgItem *>::iterator iv_it;
		for (iv_it = invalid.begin(); iv_it != invalid.end(); iv_it++) {
			QgItem *iv_itm = *iv_it;
			items->erase(iv_itm);
			delete iv_itm;
		}
		itemIndexRebuild();

		endResetModel();
		model_reset_in_progress = false;
	}

	// If we did change something, we need to let the application know
	if (changed_db_flag) {
		emit mdl_changed_db((void *)gedp);
		/* Fan out via session so subscribers (e.g. QgAttributesModel) that
		 * connected directly to the session are also notified. */
		m_session->notifyDbChanged(gedp->dbip);
		emit check_highlights();
	}

	// Reset flag - we're in sync now
	changed_db_flag = 0;
}

int
QgModel::NodeRow(QgItem *node) const
{
	QgItem *np = node->parent();
	if (!np)
		return -1;
	if (np->c_noderow.find(node) == np->c_noderow.end())
		return -1;
	return np->c_noderow[node];
}


QModelIndex
QgModel::NodeIndex(QgItem *node) const
{
	if (node == rootItem)
		return QModelIndex();
	int nr = NodeRow(node);
	if (nr == -1)
		return QModelIndex();
	return createIndex(nr, 0, node);
}


void
QgModel::itemIndexInsert(QgItem *item)
{
	if (!item || item == rootItem)
		return;

	items_by_instance_hash[item->ihash].insert(item);

	unsigned long long phash = item->path_hash();
	if (phash)
		items_by_path_hash[phash].insert(item);
}


void
QgModel::itemIndexRemove(QgItem *item)
{
	if (!item || item == rootItem)
		return;

	auto iit = items_by_instance_hash.find(item->ihash);
	if (iit != items_by_instance_hash.end()) {
		iit->second.erase(item);
		if (iit->second.empty())
			items_by_instance_hash.erase(iit);
	}

	unsigned long long phash = item->path_hash();
	if (phash) {
		auto pit = items_by_path_hash.find(phash);
		if (pit != items_by_path_hash.end()) {
			pit->second.erase(item);
			if (pit->second.empty())
				items_by_path_hash.erase(pit);
		}
	}
}


void
QgModel::itemIndexInsertSubtree(QgItem *item)
{
	if (!item || item == rootItem)
		return;

	itemIndexInsert(item);
	for (QgItem *child : item->children)
		itemIndexInsertSubtree(child);
}


void
QgModel::itemIndexRemoveSubtree(QgItem *item)
{
	if (!item || item == rootItem)
		return;

	for (QgItem *child : item->children)
		itemIndexRemoveSubtree(child);
	itemIndexRemove(item);
}


void
QgModel::itemIndexClear()
{
	items_by_instance_hash.clear();
	items_by_path_hash.clear();
}


void
QgModel::itemIndexRebuild()
{
	itemIndexClear();
	if (!items)
		return;

	for (QgItem *item : *items)
		itemIndexInsert(item);
}


void
QgModel::deleteItemSubtree(QgItem *item)
{
	if (!item || item == rootItem)
		return;

	std::vector<QgItem *> children = item->children;
	for (QgItem *child : children)
		deleteItemSubtree(child);

	itemIndexRemove(item);
	if (items)
		items->erase(item);
	delete item;
}


int
QgModel::applyLoadedHierarchyDeltas(const std::vector<std::string> *candidate_paths)
{
	if (!m_session || !m_session->ged() || !rootItem || !items)
		return 0;

	struct ged *gedp = m_session->ged();
	int64_t start_us = bu_gettime();
	hierarchy_delta_stats.attempts++;
	auto finish_delta = [this, start_us](int applied) {
		int64_t elapsed_us = bu_gettime() - start_us;
		if (elapsed_us > 0)
			hierarchy_delta_stats.elapsed_us +=
				(uint64_t)elapsed_us;
		if (applied)
			hierarchy_delta_stats.applied++;
		return applied;
	};

	if (!ged_db_index_refresh(gedp))
		return finish_delta(0);
	hierarchy_delta_stats.db_index_refreshes++;

	struct ChildEntry {
		unsigned long long id = 0;
		int op = DB_OP_UNION;
	};
	struct ParentPlan {
		QgItem *parent = nullptr;
		std::vector<ChildEntry> children;
	};
	struct CrossMoveRemoval {
		QgItem *parent = nullptr;
		QgItem *item = nullptr;
	};
	struct CrossMoveInsertion {
		QgItem *parent = nullptr;
		int row = -1;
		int op = DB_OP_UNION;
	};
	struct CrossParentMove {
		unsigned long long id = 0;
		QgItem *source_parent = nullptr;
		QgItem *dest_parent = nullptr;
		QgItem *item = nullptr;
		int dest_row = -1;
		int op = DB_OP_UNION;
	};

	auto rebuild_child_rows = [](QgItem *parent) {
		if (!parent)
			return;
		parent->c_noderow.clear();
		for (size_t i = 0; i < parent->children.size(); i++)
			parent->c_noderow[parent->children[i]] = (int)i;
	};

	auto plan_child_counts = [](const std::vector<ChildEntry> &entries,
		std::unordered_map<unsigned long long, size_t> &counts) {
		counts.clear();
		for (const ChildEntry &entry : entries) {
			if (!entry.id)
				return false;
			counts[entry.id]++;
		}
		return true;
	};

	auto loaded_child_counts = [](QgItem *parent,
		std::unordered_map<unsigned long long, size_t> &counts) {
		counts.clear();
		if (!parent)
			return false;
		for (QgItem *child : parent->children) {
			if (!child || !child->ihash)
				return false;
			counts[child->ihash]++;
		}
		return true;
	};

	auto parent_rows_materialized = [this](QgItem *parent) {
		return parent == rootItem || !parent->children.empty() ||
			parent->open_itm;
	};

	auto common_id_sequence = [](const std::vector<unsigned long long> &ids,
		const std::unordered_map<unsigned long long, size_t> &other_counts) {
		std::unordered_map<unsigned long long, size_t> emitted;
		std::vector<unsigned long long> common;
		for (unsigned long long id : ids) {
			auto oit = other_counts.find(id);
			if (oit == other_counts.end())
				continue;
			size_t &count = emitted[id];
			if (count >= oit->second)
				continue;
			common.push_back(id);
			count++;
		}
		return common;
	};

	auto build_plan = [this, gedp](QgItem *parent, ParentPlan &plan) {
		if (!parent)
			return false;

		plan.parent = parent;
		plan.children.clear();

		if (parent == rootItem) {
			size_t tops_count = ged_db_index_tops(gedp, 1, nullptr, 0);
			std::vector<unsigned long long> tops(tops_count);
			if (tops_count)
				ged_db_index_tops(gedp, 1, tops.data(), tops.size());
			std::sort(tops.begin(), tops.end(),
				[gedp](unsigned long long id1,
				       unsigned long long id2) {
					return qgmodel_index_id_less(gedp, id1, id2);
				});
			for (unsigned long long id : tops) {
				ChildEntry entry;
				entry.id = id;
				entry.op = DB_OP_UNION;
				plan.children.push_back(entry);
			}
			return true;
		}

		struct ged_db_index_record rec;
		if (!ged_db_index_record_get(gedp, parent->ihash, &rec))
			return false;

		for (size_t row = 0; row < rec.child_count; row++) {
			struct ged_db_index_child child;
			if (!ged_db_index_child_at(gedp, parent->ihash, row,
				&child))
				return false;
			ChildEntry entry;
			entry.id = child.record.id;
			entry.op = child.bool_op;
			plan.children.push_back(entry);
		}
		return true;
	};

	auto common_order_supported = [&plan_child_counts,
		&loaded_child_counts, &common_id_sequence,
		&parent_rows_materialized](QgItem *parent,
			const ParentPlan &plan) {
		if (!parent)
			return false;
		if (!parent_rows_materialized(parent))
			return false;

		std::unordered_map<unsigned long long, size_t> new_counts;
		std::unordered_map<unsigned long long, size_t> old_counts;
		if (!plan_child_counts(plan.children, new_counts) ||
			!loaded_child_counts(parent, old_counts))
			return false;

		std::vector<unsigned long long> old_ids;
		old_ids.reserve(parent->children.size());
		for (QgItem *child : parent->children)
			old_ids.push_back(child->ihash);

		std::vector<unsigned long long> new_ids;
		new_ids.reserve(plan.children.size());
		for (const ChildEntry &entry : plan.children) {
			new_ids.push_back(entry.id);
		}

		std::vector<unsigned long long> old_common =
			common_id_sequence(old_ids, new_counts);
		std::vector<unsigned long long> new_common =
			common_id_sequence(new_ids, old_counts);

		return old_common == new_common;
	};

	auto unique_plan_supported = [&plan_child_counts,
		&loaded_child_counts, &parent_rows_materialized](QgItem *parent,
			const ParentPlan &plan) {
		if (!parent)
			return false;
		if (!parent_rows_materialized(parent))
			return false;

		std::unordered_map<unsigned long long, size_t> new_counts;
		std::unordered_map<unsigned long long, size_t> old_counts;
		if (!plan_child_counts(plan.children, new_counts) ||
			!loaded_child_counts(parent, old_counts))
			return false;
		if (new_counts.size() != plan.children.size() ||
			old_counts.size() != parent->children.size())
			return false;
		return true;
	};

	auto plan_changes_rows = [&parent_rows_materialized](QgItem *parent,
		const ParentPlan &plan) {
		if (!parent)
			return false;
		if (!parent_rows_materialized(parent))
			return false;
		if (parent->children.size() != plan.children.size())
			return true;
		for (size_t i = 0; i < plan.children.size(); i++) {
			if (parent->children[i]->ihash != plan.children[i].id)
				return true;
			if (parent->children[i]->op != plan.children[i].op)
				return true;
		}
		return false;
	};

	auto child_row = [](QgItem *parent, QgItem *child) {
		if (!parent || !child)
			return -1;
		auto row_it = parent->c_noderow.find(child);
		if (row_it != parent->c_noderow.end())
			return row_it->second;
		for (size_t row = 0; row < parent->children.size(); row++) {
			if (parent->children[row] == child)
				return (int)row;
		}
		return -1;
	};

	auto item_is_ancestor = [](QgItem *ancestor, QgItem *item) {
		if (!ancestor || !item)
			return false;
		for (QgItem *curr = item; curr; curr = curr->parentItem) {
			if (curr == ancestor)
				return true;
		}
		return false;
	};

	auto plan_cross_parent_moves = [&unique_plan_supported,
		&item_is_ancestor](
		const std::vector<ParentPlan> &plans,
		std::vector<CrossParentMove> &moves) {
		std::unordered_map<unsigned long long,
			std::vector<CrossMoveRemoval>> removals;
		std::unordered_map<unsigned long long,
			std::vector<CrossMoveInsertion>> insertions;

		moves.clear();
		for (const ParentPlan &plan : plans) {
			QgItem *parent = plan.parent;
			if (!unique_plan_supported(parent, plan))
				continue;

			std::unordered_map<unsigned long long, int> old_rows;
			for (size_t row = 0; row < parent->children.size(); row++) {
				QgItem *child = parent->children[row];
				if (child)
					old_rows[child->ihash] = (int)row;
			}

			std::unordered_map<unsigned long long, int> new_rows;
			std::unordered_map<unsigned long long, int> new_ops;
			for (size_t row = 0; row < plan.children.size(); row++) {
				const ChildEntry &entry = plan.children[row];
				new_rows[entry.id] = (int)row;
				new_ops[entry.id] = entry.op;
			}

			for (size_t row = 0; row < parent->children.size(); row++) {
				QgItem *child = parent->children[row];
				if (!child || new_rows.find(child->ihash) !=
					new_rows.end())
					continue;
				CrossMoveRemoval removal;
				removal.parent = parent;
				removal.item = child;
				removals[child->ihash].push_back(removal);
			}

			for (const ChildEntry &entry : plan.children) {
				if (old_rows.find(entry.id) != old_rows.end())
					continue;
				CrossMoveInsertion insertion;
				insertion.parent = parent;
				insertion.row = new_rows[entry.id];
				insertion.op = new_ops[entry.id];
				insertions[entry.id].push_back(insertion);
			}
		}

		for (const auto &rit : removals) {
			auto iit = insertions.find(rit.first);
			if (iit == insertions.end())
				continue;
			if (rit.second.size() != 1 || iit->second.size() != 1)
				continue;
			const CrossMoveRemoval &removal = rit.second[0];
			const CrossMoveInsertion &insertion = iit->second[0];
			if (!removal.parent || !insertion.parent || !removal.item ||
				removal.parent == insertion.parent)
				continue;
			if (removal.parent == removal.parent->mdl->rootItem ||
				insertion.parent == insertion.parent->mdl->rootItem)
				continue;
			if (item_is_ancestor(removal.item, insertion.parent))
				continue;
			CrossParentMove move;
			move.id = rit.first;
			move.source_parent = removal.parent;
			move.dest_parent = insertion.parent;
			move.item = removal.item;
			move.dest_row = insertion.row;
			move.op = insertion.op;
			moves.push_back(move);
		}

		std::sort(moves.begin(), moves.end(),
			[](const CrossParentMove &a,
			   const CrossParentMove &b) {
				if (a.id != b.id)
					return a.id < b.id;
				return a.dest_row < b.dest_row;
			});
	};

	auto apply_cross_parent_moves = [this, &child_row,
		&item_is_ancestor, &rebuild_child_rows](
			const std::vector<CrossParentMove> &moves) {
		for (const CrossParentMove &move : moves) {
			QgItem *source_parent = move.source_parent;
			QgItem *dest_parent = move.dest_parent;
			QgItem *item = move.item;
			if (!source_parent || !dest_parent || !item ||
				source_parent == dest_parent)
				continue;
			if ((source_parent != rootItem &&
				items->find(source_parent) == items->end()) ||
				(dest_parent != rootItem &&
				items->find(dest_parent) == items->end()) ||
				items->find(item) == items->end())
				return false;
			if (item->parentItem != source_parent)
				continue;
			if (item_is_ancestor(item, dest_parent))
				continue;

			int source_row = child_row(source_parent, item);
			if (source_row < 0)
				return false;
			int dest_row = move.dest_row;
			if (dest_row < 0)
				return false;
			if (dest_row > (int)dest_parent->children.size())
				dest_row = (int)dest_parent->children.size();

			QModelIndex source_idx = (source_parent == rootItem) ?
				QModelIndex() : NodeIndex(source_parent);
			QModelIndex dest_idx = (dest_parent == rootItem) ?
				QModelIndex() : NodeIndex(dest_parent);

			model_structure_change_in_progress = true;
			if (!beginMoveRows(source_idx, source_row, source_row,
				dest_idx, dest_row)) {
				model_structure_change_in_progress = false;
				return false;
			}
			itemIndexRemoveSubtree(item);
			source_parent->children.erase(
				source_parent->children.begin() + source_row);
			source_parent->c_count = source_parent->children.size();
			item->parentItem = dest_parent;
			item->op = move.op;
			dest_parent->children.insert(dest_parent->children.begin() +
				dest_row, item);
			dest_parent->c_count = dest_parent->children.size();
			rebuild_child_rows(source_parent);
			rebuild_child_rows(dest_parent);
			itemIndexInsertSubtree(item);
			endMoveRows();
			model_structure_change_in_progress = false;
			hierarchy_delta_stats.moved_rows++;
			hierarchy_delta_stats.moved_row_ranges++;
		}

		return true;
	};

	auto apply_plan = [this, &rebuild_child_rows,
		&plan_child_counts](const ParentPlan &plan) {
		QgItem *parent = plan.parent;
		if (!parent)
			return false;

		std::unordered_map<unsigned long long, size_t> keep_counts;
		if (!plan_child_counts(plan.children, keep_counts))
			return false;

		QModelIndex parent_idx = (parent == rootItem) ?
			QModelIndex() : NodeIndex(parent);

		std::vector<int> rows_to_remove;
		for (size_t row = 0; row < parent->children.size(); row++) {
			QgItem *child = parent->children[(size_t)row];
			if (!child)
				return false;

			auto kit = keep_counts.find(child->ihash);
			if (kit != keep_counts.end() && kit->second > 0) {
				kit->second--;
				continue;
			}

			rows_to_remove.push_back((int)row);
		}

		for (auto rit = rows_to_remove.rbegin();
			rit != rows_to_remove.rend();) {
			int last_row = *rit;
			int first_row = last_row;
			++rit;
			while (rit != rows_to_remove.rend() &&
				*rit == first_row - 1) {
				first_row = *rit;
				++rit;
			}

			std::vector<QgItem *> removed_children;
			removed_children.reserve((size_t)(last_row - first_row + 1));
			for (int row = first_row; row <= last_row; row++) {
				QgItem *child = parent->children[(size_t)row];
				if (!child)
					return false;
				removed_children.push_back(child);
			}

			model_structure_change_in_progress = true;
			beginRemoveRows(parent_idx, first_row, last_row);
			parent->children.erase(parent->children.begin() + first_row,
				parent->children.begin() + last_row + 1);
			rebuild_child_rows(parent);
			parent->c_count = parent->children.size();
			endRemoveRows();
			model_structure_change_in_progress = false;
			hierarchy_delta_stats.removed_rows +=
				(uint64_t)removed_children.size();
			hierarchy_delta_stats.removed_row_ranges++;
			for (QgItem *child : removed_children)
				deleteItemSubtree(child);
		}

		for (size_t row = 0; row < plan.children.size(); row++) {
			const ChildEntry &entry = plan.children[row];
			if (row < parent->children.size() &&
				parent->children[row] &&
				parent->children[row]->ihash == entry.id) {
				parent->children[row]->op = entry.op;
				continue;
			}

			size_t insert_first = row;
			size_t insert_last = row;
			while (insert_last < plan.children.size()) {
				if (row < parent->children.size() &&
					parent->children[row] &&
					parent->children[row]->ihash ==
					plan.children[insert_last].id)
					break;
				insert_last++;
			}
			if (insert_last == insert_first)
				return false;

			std::vector<QgItem *> new_items;
			new_items.reserve(insert_last - insert_first);
			for (size_t insert_row = insert_first;
				insert_row < insert_last; insert_row++) {
				const ChildEntry &insert_entry =
					plan.children[insert_row];
				QgItem *nitem = new QgItem(insert_entry.id, this);
				nitem->parentItem = parent;
				nitem->op = insert_entry.op;
				new_items.push_back(nitem);
			}

			model_structure_change_in_progress = true;
			beginInsertRows(parent_idx, (int)insert_first,
				(int)insert_last - 1);
			parent->children.insert(parent->children.begin() +
				(ptrdiff_t)insert_first, new_items.begin(),
				new_items.end());
			parent->c_count = parent->children.size();
			rebuild_child_rows(parent);
			for (QgItem *nitem : new_items) {
				items->insert(nitem);
				itemIndexInsert(nitem);
			}
			endInsertRows();
			model_structure_change_in_progress = false;
			hierarchy_delta_stats.inserted_rows +=
				(uint64_t)new_items.size();
			hierarchy_delta_stats.inserted_row_ranges++;
			row = insert_last - 1;
		}

		parent->c_count = plan.children.size();
		rebuild_child_rows(parent);
		return true;
	};

	auto apply_unique_plan = [this, &rebuild_child_rows](
		const ParentPlan &plan) {
		QgItem *parent = plan.parent;
		if (!parent)
			return false;

		QModelIndex parent_idx = (parent == rootItem) ?
			QModelIndex() : NodeIndex(parent);

		std::unordered_set<unsigned long long> wanted;
		wanted.reserve(plan.children.size());
		for (const ChildEntry &entry : plan.children) {
			if (!entry.id || wanted.find(entry.id) != wanted.end())
				return false;
			wanted.insert(entry.id);
		}

		std::vector<int> rows_to_remove;
		for (size_t row = 0; row < parent->children.size(); row++) {
			QgItem *child = parent->children[row];
			if (!child || !child->ihash)
				return false;
			if (wanted.find(child->ihash) == wanted.end())
				rows_to_remove.push_back((int)row);
		}

		for (auto rit = rows_to_remove.rbegin();
			rit != rows_to_remove.rend();) {
			int last_row = *rit;
			int first_row = last_row;
			++rit;
			while (rit != rows_to_remove.rend() &&
				*rit == first_row - 1) {
				first_row = *rit;
				++rit;
			}

			std::vector<QgItem *> removed_children;
			removed_children.reserve((size_t)(last_row - first_row + 1));
			for (int row = first_row; row <= last_row; row++) {
				QgItem *child = parent->children[(size_t)row];
				if (!child)
					return false;
				removed_children.push_back(child);
			}

			model_structure_change_in_progress = true;
			beginRemoveRows(parent_idx, first_row, last_row);
			parent->children.erase(parent->children.begin() + first_row,
				parent->children.begin() + last_row + 1);
			parent->c_count = parent->children.size();
			rebuild_child_rows(parent);
			endRemoveRows();
			model_structure_change_in_progress = false;
			hierarchy_delta_stats.removed_rows +=
				(uint64_t)removed_children.size();
			hierarchy_delta_stats.removed_row_ranges++;
			for (QgItem *child : removed_children)
				deleteItemSubtree(child);
		}

		for (size_t row = 0; row < plan.children.size(); row++) {
			const ChildEntry &entry = plan.children[row];
			if (row < parent->children.size() &&
				parent->children[row] &&
				parent->children[row]->ihash == entry.id) {
				parent->children[row]->op = entry.op;
				continue;
			}

			size_t source_row = row;
			while (source_row < parent->children.size()) {
				QgItem *candidate = parent->children[source_row];
				if (candidate && candidate->ihash == entry.id)
					break;
				source_row++;
			}

			if (source_row < parent->children.size()) {
				model_structure_change_in_progress = true;
				if (!beginMoveRows(parent_idx, (int)source_row,
					(int)source_row, parent_idx, (int)row)) {
					model_structure_change_in_progress = false;
					return false;
				}
				QgItem *moved = parent->children[source_row];
				parent->children.erase(parent->children.begin() +
					(ptrdiff_t)source_row);
				parent->children.insert(parent->children.begin() +
					(ptrdiff_t)row, moved);
				parent->children[row]->op = entry.op;
				parent->c_count = parent->children.size();
				rebuild_child_rows(parent);
				endMoveRows();
				model_structure_change_in_progress = false;
				hierarchy_delta_stats.moved_rows++;
				hierarchy_delta_stats.moved_row_ranges++;
				continue;
			}

			size_t insert_first = row;
			size_t insert_last = row;
			while (insert_last < plan.children.size()) {
				size_t search_row = row;
				while (search_row < parent->children.size()) {
					QgItem *candidate =
						parent->children[search_row];
					if (candidate &&
						candidate->ihash ==
						plan.children[insert_last].id)
						break;
					search_row++;
				}
				if (search_row < parent->children.size())
					break;
				insert_last++;
			}
			if (insert_last == insert_first)
				return false;

			std::vector<QgItem *> new_items;
			new_items.reserve(insert_last - insert_first);
			for (size_t insert_row = insert_first;
				insert_row < insert_last; insert_row++) {
				const ChildEntry &insert_entry =
					plan.children[insert_row];
				QgItem *nitem = new QgItem(insert_entry.id, this);
				nitem->parentItem = parent;
				nitem->op = insert_entry.op;
				new_items.push_back(nitem);
			}

			model_structure_change_in_progress = true;
			beginInsertRows(parent_idx, (int)insert_first,
				(int)insert_last - 1);
			parent->children.insert(parent->children.begin() +
				(ptrdiff_t)insert_first, new_items.begin(),
				new_items.end());
			parent->c_count = parent->children.size();
			rebuild_child_rows(parent);
			for (QgItem *nitem : new_items) {
				items->insert(nitem);
				itemIndexInsert(nitem);
			}
			endInsertRows();
			model_structure_change_in_progress = false;
			hierarchy_delta_stats.inserted_rows +=
				(uint64_t)new_items.size();
			hierarchy_delta_stats.inserted_row_ranges++;
			row = insert_last - 1;
		}

		parent->c_count = plan.children.size();
		rebuild_child_rows(parent);
		return true;
	};

	std::vector<QgItem *> parents;
	std::unordered_set<QgItem *> parent_seen;
	auto add_parent_candidate = [&parents, &parent_seen,
		&parent_rows_materialized](QgItem *parent) {
		if (!parent || !parent_rows_materialized(parent))
			return;
		if (parent_seen.insert(parent).second)
			parents.push_back(parent);
	};
	add_parent_candidate(rootItem);

	auto add_matching_path_prefix = [this, gedp, &add_parent_candidate](
		const std::vector<unsigned long long> &ids, size_t prefix_len) {
		if (!prefix_len || prefix_len > ids.size())
			return;
		unsigned long long phash = ged_db_index_path_hash(gedp,
			ids.data(), ids.size(), prefix_len);
		if (!phash)
			return;
		auto bit = items_by_path_hash.find(phash);
		if (bit == items_by_path_hash.end())
			return;
		for (QgItem *item : bit->second) {
			if (!item)
				continue;
			std::vector<unsigned long long> item_ids =
				item->path_items();
			if (item_ids.size() != prefix_len)
				continue;
			if (!std::equal(item_ids.begin(), item_ids.end(),
				ids.begin()))
				continue;
			add_parent_candidate(item);
		}
	};

	auto add_path_candidates = [gedp, &add_matching_path_prefix](
		const char *path) {
		if (!path || !path[0])
			return;
		size_t path_len = ged_db_index_path_resolve(gedp, path,
			nullptr, 0);
		if (!path_len)
			return;
		std::vector<unsigned long long> ids(path_len);
		if (ged_db_index_path_resolve(gedp, path, ids.data(),
			ids.size()) != path_len || ids.empty())
			return;
		for (size_t prefix_len = 1; prefix_len <= ids.size();
			prefix_len++)
			add_matching_path_prefix(ids, prefix_len);
	};

	int use_targeted_parents = candidate_paths && !candidate_paths->empty();
	if (use_targeted_parents) {
		for (const std::string &path : *candidate_paths) {
			add_path_candidates(path.c_str());
			std::string parent = qgmodel_parent_path(path.c_str());
			while (!parent.empty()) {
				add_path_candidates(parent.c_str());
				parent = qgmodel_parent_path(parent.c_str());
			}
		}

		/* If the hints did not resolve to any loaded/open branch, fall
		 * back to the broad loaded-parent plan to preserve correctness
		 * for older or low-level event publishers with incomplete names. */
		if (parents.size() <= 1)
			use_targeted_parents = 0;
	}

	if (!use_targeted_parents) {
		std::vector<QgItem *> snapshot(items->begin(), items->end());
		for (QgItem *item : snapshot) {
			if (!item || items->find(item) == items->end())
				continue;
			add_parent_candidate(item);
		}
	}
	hierarchy_delta_stats.planned_parents += parents.size();

	std::vector<ParentPlan> plans;
	plans.reserve(parents.size());
	for (QgItem *parent : parents) {
		ParentPlan plan;
		if (!build_plan(parent, plan))
			return finish_delta(0);
		hierarchy_delta_stats.planned_child_rows +=
			plan.children.size();
		if (!common_order_supported(parent, plan) &&
			!unique_plan_supported(parent, plan))
			return finish_delta(0);
		plans.push_back(plan);
	}

	int changed_parent_count = 0;
	for (const ParentPlan &plan : plans) {
		if (plan_changes_rows(plan.parent, plan))
			changed_parent_count++;
	}
	hierarchy_delta_stats.changed_parents += changed_parent_count;
	if (!changed_parent_count)
		return finish_delta(1);

	std::vector<CrossParentMove> cross_parent_moves;
	plan_cross_parent_moves(plans, cross_parent_moves);
	if (!apply_cross_parent_moves(cross_parent_moves))
		return finish_delta(0);

	for (const ParentPlan &plan : plans) {
		if (!plan_changes_rows(plan.parent, plan))
			continue;
		if (!plan.parent || (plan.parent != rootItem &&
			items->find(plan.parent) == items->end()))
			continue;
		if (unique_plan_supported(plan.parent, plan)) {
			if (!apply_unique_plan(plan))
				return finish_delta(0);
			continue;
		}
		if (common_order_supported(plan.parent, plan)) {
			if (!apply_plan(plan))
				return finish_delta(0);
			continue;
		}
		return finish_delta(0);
	}

	std::vector<QgItem *> ccount_update_items;
	if (use_targeted_parents) {
		ccount_update_items.reserve(parents.size());
		for (QgItem *parent : parents)
			ccount_update_items.push_back(parent);
	} else {
		ccount_update_items.assign(items->begin(), items->end());
	}
	for (QgItem *item : ccount_update_items) {
		if (!item || !item->children.empty())
			continue;
		struct ged_db_index_record rec;
		if (ged_db_index_record_get(gedp, item->ihash, &rec))
			item->c_count = rec.child_count;
	}

	tops_items = rootItem->children;
	return finish_delta(1);
}


bool
QgModel::hasChildren(const QModelIndex &idx) const
{
	if (idx.column() > 0)
		return false;

	if (!idx.isValid())
		return rootItem && rootItem->childCount() > 0;

	QgItem *item = static_cast<QgItem*>(idx.internalPointer());
	if (!item || item == rootItem)
		return false;

	if (item->childCount() > 0)
		return true;

	return ged_db_index_child_count(m_session->ged(), item->ihash) > 0;
}

bool
QgModel::canFetchMore(const QModelIndex &idx) const
{
	if (model_reset_in_progress || model_structure_change_in_progress)
		return false;

	if (!idx.isValid())
		return false;

	QgItem *item = static_cast<QgItem*>(idx.internalPointer());
	if (item == rootItem)
		return false;

	if (item->children.size())
		return false;

	// If there are children to be fetched, we can fetch them
	return ged_db_index_child_count(m_session->ged(), item->ihash) > 0;
}

void
QgModel::fetchMore(const QModelIndex &idx)
{
	if (model_reset_in_progress || model_structure_change_in_progress)
		return;

	if (!idx.isValid())
		return;

	QgItem *item = static_cast<QgItem*>(idx.internalPointer());

	if (UNLIKELY(item == rootItem)) {
		return;
	}

	int64_t start_us = bu_gettime();
	fetch_more_stats.calls++;

	// If we're already populated, don't need to do it again
	if (item->children.size())
		return;

	unsigned long long chash = item->ihash;
	size_t child_count = ged_db_index_child_count(m_session->ged(), chash);
	if (!child_count) {
		// TODO - invalid hash
		return;
	}

	std::vector<QgItem *> nc;
	for (size_t row = 0; row < child_count; row++) {
		struct ged_db_index_child child;
		if (!ged_db_index_child_at(m_session->ged(), chash, row, &child))
			continue;
		QgItem *nitem = new QgItem(child.record.id, this);
		nitem->parentItem = item;
		nitem->op = child.bool_op;
		nc.push_back(nitem);
		items->insert(nitem);
		itemIndexInsert(nitem);
	}

	if (nc.empty())
		return;

	// All done - let the Qt model know
	model_structure_change_in_progress = true;
	beginInsertRows(idx, 0, nc.size() - 1);
	item->children = nc;
	// define a map for quick QgItem * -> index lookups
	item->c_noderow.clear();
	for (size_t i = 0; i < nc.size(); i++) {
		item->c_noderow[nc[i]] = i;
	}
	endInsertRows();
	model_structure_change_in_progress = false;
	fetch_more_stats.populated_calls++;
	fetch_more_stats.inserted_rows += (uint64_t)nc.size();
	fetch_more_stats.inserted_row_ranges++;
	if ((uint64_t)nc.size() > fetch_more_stats.max_child_count)
		fetch_more_stats.max_child_count = (uint64_t)nc.size();
	int64_t elapsed_us = bu_gettime() - start_us;
	if (elapsed_us > 0)
		fetch_more_stats.elapsed_us += (uint64_t)elapsed_us;
	emit check_highlights();
	emit item->mdl->opened_item(item);
}


///////////////////////////////////////////////////////////////////////
//          Qt abstract model interface implementation
///////////////////////////////////////////////////////////////////////

QgItem *
QgModel::root()
{
	return rootItem;
}

QgItem *
QgModel::getItem(const QModelIndex &index) const
{
	if (index.isValid()) {
		QgItem *item = static_cast<QgItem*>(index.internalPointer());
		if (item)
			return item;
	}
	return rootItem;
}

QModelIndex
QgModel::index(int row, int column, const QModelIndex &parent_idx) const
{
	if (parent_idx.isValid() && parent_idx.column() != 0)
		return QModelIndex();

	QgItem *parentItem = getItem(parent_idx);
	if (!parentItem)
		return QModelIndex();

	QgItem *childItem = parentItem->child(row);
	if (childItem)
		return createIndex(row, column, childItem);

	return QModelIndex();
}

QModelIndex
QgModel::parent(const QModelIndex &index) const
{
	if (!index.isValid())
		return QModelIndex();

	QgItem *childItem = getItem(index);
	QgItem *parentItem = childItem ? childItem->parent() : nullptr;

	if (parentItem == rootItem || !parentItem)
		return QModelIndex();

	int parent_row = NodeRow(parentItem);
	if (parent_row < 0)
		return QModelIndex();

	return createIndex(parent_row, 0, parentItem);
}

Qt::ItemFlags
QgModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	Qt::ItemFlags flags = QAbstractItemModel::flags(index);

	// flags |= Qt::ItemIsEditable;

	return flags;
}

QVariant
QgModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();
	QgItem *qi= getItem(index);
	struct ged *gedp = m_session->ged();
	if (role == Qt::DisplayRole)
		return QVariant(bu_vls_cstr(qi->name_ptr));
	if (role == BoolInternalRole)
		return QVariant(qi->op);
	if (role == DirectoryInternalRole)
		return QVariant::fromValue((void *)(qi->dp));
	if (role == DrawnDisplayRole) {
		std::string path = item_path(qi);
		if (path.empty())
			return QVariant(0);
		return QVariant(ged_draw_path_state(gedp, gedp->ged_gvp, path.c_str(), -1));
	}
	if (role == SelectDisplayRole) {
		return QVariant(ged_selection_is_path_selected(gedp, nullptr, qi->path_hash()));
	}

	if (role == TypeIconDisplayRole)
		return QVariant(qi->icon);

	if (role == HighlightDisplayRole) {
		switch (qi->mdl->interaction_mode) {
		case 0:
			if (qi->open_itm == false &&
				ged_selection_is_path_active_parent(gedp, nullptr, qi->path_hash()))
				return QVariant(1);
			return QVariant(0);
		case 1:
			if (ged_selection_is_object_parent(gedp, nullptr, qi->ihash))
				return QVariant(2);
			return QVariant(0);
		case 2:
			if (ged_selection_is_object_immediate_parent(gedp, nullptr, qi->ihash))
				return QVariant(3);
			if (ged_selection_is_object_grand_parent(gedp, nullptr, qi->ihash))
				return QVariant(2);
			return QVariant(0);
		default:
			return QVariant(0);
		}
	}

	return QVariant();
}

std::string
QgModel::item_path(const QgItem *item) const
{
	if (!item || !m_session || !m_session->ged())
		return std::string();

	std::vector<unsigned long long> path_hashes = item->path_items();
	if (path_hashes.empty())
		return std::string();

	struct bu_vls path_str = BU_VLS_INIT_ZERO;
	if (!ged_db_index_path_print(m_session->ged(), &path_str,
		path_hashes.data(), path_hashes.size(), 0, 0)) {
		bu_vls_free(&path_str);
		return std::string();
	}
	std::string ret = bu_vls_cstr(&path_str);
	bu_vls_free(&path_str);
	return ret;
}


void
QgModel::notifyItemsChanged(const QVector<int> &roles)
{
	if (!items)
		return;

	for (QgItem *item : *items) {
		if (!item)
			continue;
		QModelIndex idx = NodeIndex(item);
		if (idx.isValid()) {
			emit dataChanged(idx, idx, roles);
			notification_stats.items_notified++;
			notification_stats.full_items_notified++;
		}
	}
}


void
QgModel::notifyItemSubtreeChanged(QgItem *item, const QVector<int> &roles,
				  std::unordered_set<QgItem *> &notified)
{
	if (!item)
		return;

	if (item == rootItem) {
		for (QgItem *child : rootItem->childItems())
			notifyItemSubtreeChanged(child, roles, notified);
		return;
	}

	if (notified.find(item) == notified.end()) {
		QModelIndex idx = NodeIndex(item);
		if (idx.isValid()) {
			emit dataChanged(idx, idx, roles);
			notification_stats.items_notified++;
			notification_stats.subtree_items_notified++;
		}
		notified.insert(item);
	}

	for (QgItem *child : item->childItems())
		notifyItemSubtreeChanged(child, roles, notified);
}


int
QgModel::notifyPathItemsChanged(const char *path, const QVector<int> &roles,
				bool terminal_subtree)
{
	int64_t start_us = bu_gettime();
	auto finish_notify = [this, start_us](int changed) {
		int64_t elapsed_us = bu_gettime() - start_us;
		if (elapsed_us > 0)
			notification_stats.path_notify_us +=
				(uint64_t)elapsed_us;
		return changed;
	};

	if (!items || !path || !path[0] || !m_session || !m_session->ged())
		return finish_notify(0);

	struct ged *gedp = m_session->ged();
	auto notify_resolved_ids = [this, &roles, gedp](
		const std::vector<unsigned long long> &ids,
		bool notify_terminal_subtree) {
		int changed = 0;
		std::unordered_set<QgItem *> notified;
		for (size_t prefix_len = 1; prefix_len <= ids.size();
			prefix_len++) {
			unsigned long long phash = ged_db_index_path_hash(gedp,
				ids.data(), ids.size(), prefix_len);
			if (!phash)
				continue;

			notification_stats.path_queries++;
			auto bit = items_by_path_hash.find(phash);
			if (bit == items_by_path_hash.end())
				continue;

			notification_stats.path_candidates += bit->second.size();
			for (QgItem *item : bit->second) {
				if (!item)
					continue;
				std::vector<unsigned long long> candidate_path =
					item->path_items();
				if (candidate_path.size() != prefix_len)
					continue;
				if (!std::equal(candidate_path.begin(),
					candidate_path.end(), ids.begin()))
					continue;

				if (prefix_len == ids.size() &&
					notify_terminal_subtree) {
					size_t before = notified.size();
					notifyItemSubtreeChanged(item, roles,
						notified);
					changed += (int)(notified.size() -
						before);
				} else if (notified.find(item) == notified.end()) {
					QModelIndex idx = NodeIndex(item);
					if (idx.isValid()) {
						emit dataChanged(idx, idx, roles);
						notification_stats.items_notified++;
						notified.insert(item);
						changed++;
					}
				}
			}
		}

		return changed;
	};

	size_t path_len = ged_db_index_path_resolve(gedp, path, nullptr, 0);
	if (!path_len) {
		std::string parent = qgmodel_parent_path(path);
		while (!parent.empty()) {
			size_t parent_len = ged_db_index_path_resolve(gedp,
				parent.c_str(), nullptr, 0);
			if (parent_len) {
				std::vector<unsigned long long> parent_ids(
					parent_len);
				if (ged_db_index_path_resolve(gedp,
					parent.c_str(), parent_ids.data(),
					parent_ids.size()) == parent_len &&
					!parent_ids.empty())
					return finish_notify(notify_resolved_ids(
						parent_ids, false));
			}
			parent = qgmodel_parent_path(parent.c_str());
		}
		return finish_notify(0);
	}

	std::vector<unsigned long long> ids(path_len);
	if (ged_db_index_path_resolve(gedp, path, ids.data(), ids.size()) !=
		path_len || ids.empty())
		return finish_notify(0);

	return finish_notify(notify_resolved_ids(ids, terminal_subtree));
}


void
QgModel::notifyDrawnItemsChanged()
{
	QVector<int> roles;
	roles << DrawnDisplayRole;
	bool timing_enabled = qgmodel_draw_timing_is_enabled(this);
	int64_t start_us = timing_enabled ? bu_gettime() : 0;
	notifyItemsChanged(roles);
	if (timing_enabled)
		qgmodel_draw_timing_stats_for(this).notify_all_us +=
			(uint64_t)(bu_gettime() - start_us);
}


void
QgModel::notifySelectionItemsChanged()
{
	QVector<int> roles;
	roles << SelectDisplayRole << HighlightDisplayRole;
	notifyItemsChanged(roles);
}


void
QgModel::notifyDrawnPathChanged(const char *path)
{
	QVector<int> roles;
	roles << DrawnDisplayRole;
	bool timing_enabled = qgmodel_draw_timing_is_enabled(this);
	int64_t start_us = timing_enabled ? bu_gettime() : 0;
	notifyPathItemsChanged(path, roles);
	if (timing_enabled)
		qgmodel_draw_timing_stats_for(this).notify_path_us +=
			(uint64_t)(bu_gettime() - start_us);
}


void
QgModel::notifyDrawnTransactionChanged(const struct ged_draw_transaction_result *result,
				       const char *fallback_path)
{
	if (result && bu_vls_strlen(&result->names)) {
		std::vector<std::string> paths;
		qgmodel_append_unique_paths_from_words(paths,
			bu_vls_cstr(&result->names));
		for (const std::string &path : paths)
			notifyDrawnPathChanged(path.c_str());
		return;
	}

	if (fallback_path && fallback_path[0])
		notifyDrawnPathChanged(fallback_path);
	else
		notifyDrawnItemsChanged();
}


void
QgModel::flushPendingDrawNotifications()
{
	if (pending_draw_event_all ||
		(!pending_draw_event_view_only && pending_draw_event_paths.empty())) {
		notifyDrawnItemsChanged();
	} else if (!pending_draw_event_paths.empty()) {
		for (const std::string &path : pending_draw_event_paths)
			notifyDrawnPathChanged(path.c_str());
	}

	pending_draw_event_paths.clear();
	pending_draw_event_all = 0;
	pending_draw_event_view_only = 0;
}


void
QgModel::handleDrawTransactionEvent(const struct ged_draw_transaction *txn,
				    const struct ged_draw_transaction_result *result)
{
	draw_observer_event_count++;

	if (txn && (txn->kind == GED_DRAW_TXN_MATERIAL_CHANGED ||
		    txn->kind == GED_DRAW_TXN_REFRESH_MATERIAL_COLORS)) {
		pending_draw_event_view_only = 1;
		return;
	}

	if (draw_observer_defer_depth > 0) {
		int recorded = 0;
		if (result && bu_vls_strlen(&result->names)) {
			size_t before = pending_draw_event_paths.size();
			qgmodel_append_unique_paths_from_words(pending_draw_event_paths,
				bu_vls_cstr(&result->names));
			if (pending_draw_event_paths.size() > before)
				recorded = 1;
		}
		if (!recorded && txn && txn->path && txn->path[0]) {
			qgmodel_append_unique_path(pending_draw_event_paths, txn->path);
			recorded = 1;
		}
		if (!recorded && txn && txn->paths && txn->path_count > 0) {
			for (int i = 0; i < txn->path_count; i++) {
				if (txn->paths[i] && txn->paths[i][0]) {
					qgmodel_append_unique_path(pending_draw_event_paths, txn->paths[i]);
					recorded = 1;
				}
			}
		}
		if (!recorded)
			pending_draw_event_all = 1;
		return;
	}

	notifyDrawnTransactionChanged(result, txn ? txn->path : nullptr);
}


void
QgModel::recordPendingDatabaseEventPaths(const struct ged_event_txn_result *result)
{
	if (!result || !BU_VLS_IS_INITIALIZED(&result->affected_names) ||
		!bu_vls_strlen(&result->affected_names))
		return;

	qgmodel_append_unique_paths_from_words(pending_db_event_paths,
		bu_vls_cstr(&result->affected_names));
}


void
QgModel::notifyPendingDatabaseEventItemsChanged(bool terminal_subtree)
{
	QVector<int> roles;
	roles << Qt::DisplayRole << BoolInternalRole << DirectoryInternalRole
	      << DrawnDisplayRole << SelectDisplayRole << TypeIconDisplayRole
	      << HighlightDisplayRole;

	if (pending_db_event_all || pending_db_event_paths.empty()) {
		notifyItemsChanged(roles);
	} else {
		std::vector<std::string> notify_paths;
		notify_paths.reserve(pending_db_event_paths.size());
		for (const std::string &path : pending_db_event_paths) {
			if (!terminal_subtree &&
				qgmodel_path_has_queued_ancestor(
					pending_db_event_paths, path))
				continue;
			notify_paths.push_back(path);
		}
		for (const std::string &path : notify_paths)
			notifyPathItemsChanged(path.c_str(), roles,
				terminal_subtree);
	}

	pending_db_event_paths.clear();
	pending_db_event_metadata_only = 0;
	pending_db_event_all = 0;
	pending_db_event_terminal_subtree = 0;
}


void
QgModel::flushPendingDatabaseEventNotifications()
{
	if (!pending_db_event_notify && !pending_db_event_model_reset)
		return;

	struct ged *gedp = m_session ? m_session->ged() : nullptr;
	if (!gedp) {
		pending_db_event_notify = 0;
		pending_db_event_model_reset = 0;
		pending_db_event_force_reset = 0;
		pending_db_event_metadata_only = 0;
		pending_db_event_paths.clear();
		pending_db_event_all = 0;
		pending_db_event_terminal_subtree = 0;
		return;
	}

	if (pending_db_event_model_reset) {
		int applied_delta = !pending_db_event_force_reset &&
			applyLoadedHierarchyDeltas(&pending_db_event_paths);
		if (!applied_delta && !pending_db_event_force_reset &&
			!pending_db_event_paths.empty())
			applied_delta = applyLoadedHierarchyDeltas();
		pending_db_event_notify = 0;
		pending_db_event_model_reset = 0;
		pending_db_event_force_reset = 0;
		pending_db_event_metadata_only = 0;
		if (applied_delta) {
			notifyPendingDatabaseEventItemsChanged(false);
			emit mdl_changed_db((void *)gedp);
			if (m_session)
				m_session->notifyDbChanged(gedp->dbip);
			emit check_highlights();
		} else {
			hierarchy_delta_stats.reset_fallbacks++;
			changed_db_flag = 1;
			pending_db_event_paths.clear();
			pending_db_event_all = 0;
			pending_db_event_metadata_only = 0;
			pending_db_event_terminal_subtree = 0;
			g_update(gedp->dbip);
		}
		return;
	}

	pending_db_event_notify = 0;
	pending_db_event_model_reset = 0;
	pending_db_event_force_reset = 0;
	int metadata_only = pending_db_event_metadata_only;
	pending_db_event_metadata_only = 0;
	if (metadata_only) {
		pending_db_event_paths.clear();
		pending_db_event_all = 0;
		pending_db_event_terminal_subtree = 0;
	} else {
		notifyPendingDatabaseEventItemsChanged(
			pending_db_event_terminal_subtree != 0);
	}
	emit mdl_changed_db((void *)gedp);
	if (m_session)
		m_session->notifyDbChanged(gedp->dbip);
	emit check_highlights();
}


void
QgModel::handleDatabaseEventTxn(const struct ged_event *events,
				size_t event_count,
				const struct ged_event_txn_result *result)
{
	event_observer_event_count++;

	if (events && event_count) {
		int metadata_only = 1;
		pending_db_event_notify = 1;
		recordPendingDatabaseEventPaths(result);
		for (size_t i = 0; i < event_count; i++) {
			if (events[i].kind != GED_EVENT_DATABASE_METADATA_CHANGED)
				metadata_only = 0;
			if (qgmodel_event_affects_hierarchy(events[i].kind))
				pending_db_event_model_reset = 1;
			if (qgmodel_event_requires_model_reset(events[i].kind))
				pending_db_event_force_reset = 1;
			if (events[i].kind == GED_EVENT_OBJECT_MODIFIED)
				pending_db_event_terminal_subtree = 1;
			if (events[i].kind == GED_EVENT_MATERIAL_CHANGED &&
			    !events[i].name && !events[i].path)
				pending_db_event_all = 1;
			qgmodel_append_unique_path(pending_db_event_paths,
				events[i].name);
			qgmodel_append_unique_path(pending_db_event_paths,
				events[i].path);
			qgmodel_append_unique_path(pending_db_event_paths,
				events[i].new_name);
			qgmodel_append_unique_path(pending_db_event_paths,
				events[i].parent_name);
			qgmodel_append_unique_path(pending_db_event_paths,
				events[i].child_name);
		}
		if (metadata_only && !pending_db_event_model_reset &&
			pending_db_event_paths.empty() &&
			!pending_db_event_all &&
			!pending_db_event_terminal_subtree)
			pending_db_event_metadata_only = 1;
		else
			pending_db_event_metadata_only = 0;
	}

	if (event_observer_defer_depth > 0)
		return;

	flushPendingDatabaseEventNotifications();
}


void
QgModel::drawObserverCallback(struct ged *gedp,
			      const struct ged_draw_transaction *txn,
			      const struct ged_draw_transaction_result *result,
			      void *client_data)
{
	(void)gedp;

	QgModel *model = (QgModel *)client_data;
	if (model) {
		bool timing_enabled = qgmodel_draw_timing_is_enabled(model);
		int64_t start_us = timing_enabled ?
			bu_gettime() : 0;
		model->handleDrawTransactionEvent(txn, result);
		if (timing_enabled)
			qgmodel_draw_timing_stats_for(model).observer_callback_us +=
				(uint64_t)(bu_gettime() - start_us);
	}
}


void
QgModel::eventObserverCallback(struct ged *gedp,
			       const struct ged_event *events,
			       size_t event_count,
			       const struct ged_event_txn_result *result,
			       void *client_data)
{
	(void)gedp;

	QgModel *model = (QgModel *)client_data;
	if (model)
		model->handleDatabaseEventTxn(events, event_count, result);
}

QVariant
QgModel::headerData(int section, Qt::Orientation UNUSED(orientation), int role) const
{
	// Until we add more columns for attributes, there is only one thing to return here
	if (role != Qt::DisplayRole)
		return QVariant();
	if (section == 0)
		return QString("Object Names");
	return QVariant();
}

int
QgModel::rowCount(const QModelIndex &p) const
{
	QgItem *pItem;
	if (p.column() > 0)
		return 0;

	if (!p.isValid())
		pItem = rootItem;
	else
		pItem = static_cast<QgItem*>(p.internalPointer());

	return pItem->childCount();
}

int
QgModel::columnCount(const QModelIndex &p) const
{
	if (p.isValid())
		return static_cast<QgItem*>(p.internalPointer())->columnCount();
	return rootItem->columnCount();
}

///////////////////////////////////////////////////////////////////////
//                  .g Centric Methods
///////////////////////////////////////////////////////////////////////
int
QgModel::run_cmd(struct bu_vls *msg, int argc, const char **argv)
{
	struct ged *gedp = m_session->ged();
	model_dbip = gedp->dbip;
	uint64_t draw_rev_before = ged_draw_scene_revision(gedp);
	uint64_t draw_event_count_before = draw_observer_event_count;
	uint64_t db_event_count_before = event_observer_event_count;

	pending_draw_event_paths.clear();
	pending_draw_event_all = 0;
	pending_draw_event_view_only = 0;
	pending_db_event_notify = 0;
	pending_db_event_model_reset = 0;
	pending_db_event_force_reset = 0;
	pending_db_event_metadata_only = 0;
	pending_db_event_paths.clear();
	pending_db_event_all = 0;
	pending_db_event_terminal_subtree = 0;

	if (!ged_cmd_exists(argv[0])) {
		const char *ccmd = nullptr;
		int edist = ged_cmd_lookup(&ccmd, argv[0]);
		if (edist) {
			if (msg)
				bu_vls_sprintf(msg, "Command %s not found, did you mean %s (edit distance %d)?\n", argv[0],   ccmd, edist);
		}
		return BRLCAD_ERROR;
	}

	draw_observer_defer_depth++;
	event_observer_defer_depth++;
	int ret = ged_exec(gedp, argc, argv);
	event_observer_defer_depth--;
	draw_observer_defer_depth--;

	// If this is one of the GED commands supporting incremental input, this
	// return code means we need more input from the application before any
	// command can be run - no need to do the remainder of the logic below.
	if (ret & GED_MORE) {
		if (event_observer_event_count != db_event_count_before)
			flushPendingDatabaseEventNotifications();
		if (draw_observer_event_count != draw_event_count_before) {
			flushPendingDrawNotifications();
			emit view_changed(QG_VIEW_DRAWN);
		} else {
			pending_draw_event_paths.clear();
			pending_draw_event_all = 0;
			pending_draw_event_view_only = 0;
		}
		pending_db_event_notify = 0;
		pending_db_event_model_reset = 0;
		pending_db_event_force_reset = 0;
		pending_db_event_metadata_only = 0;
		pending_db_event_paths.clear();
		pending_db_event_all = 0;
		pending_db_event_terminal_subtree = 0;
		return ret;
	}

	// If we have a new .g file, set the changed flag
	int dbip_changed = (model_dbip != gedp->dbip);
	if (dbip_changed)
		changed_db_flag = 1;

	if (dbip_changed) {
		pending_db_event_notify = 0;
		pending_db_event_model_reset = 0;
		pending_db_event_force_reset = 0;
		pending_db_event_metadata_only = 0;
		pending_db_event_paths.clear();
		pending_db_event_all = 0;
		pending_db_event_terminal_subtree = 0;
		g_update(gedp->dbip);
	} else if (pending_db_event_notify || pending_db_event_model_reset) {
		flushPendingDatabaseEventNotifications();
	} else {
		g_update(gedp->dbip);
	}

	uint64_t draw_rev_after = ged_draw_scene_revision(gedp);
	if (draw_rev_after != draw_rev_before ||
		draw_observer_event_count != draw_event_count_before) {
		if (draw_observer_event_count != draw_event_count_before)
			flushPendingDrawNotifications();
		else
			notifyDrawnItemsChanged();
		emit view_changed(QG_VIEW_DRAWN);
	}

	model_dbip = gedp->dbip;

	if (msg && gedp)
		bu_vls_printf(msg, "%s", bu_vls_cstr(gedp->ged_result_str));

	return ret;
}

// Normally the model won't define methods for specific GED commands, but there
// are a few exceptions related to common, standard operations like drawing that
// are potentially triggered from QActions.  TODO - it might be better to have
// these live in the selection proxy model...
int
QgModel::draw_action()
{
	QTCAD_SLOT("QgModel::draw_action", 1);
	// https://stackoverflow.com/a/28647342/2037687
	QAction *a = qobject_cast<QAction *>(sender());
	QVariant v = a->data();
	QgItem *cnode  = (QgItem *) v.value<void *>();
	if (!cnode)
		return BRLCAD_ERROR;
	std::string path_str = item_path(cnode);
	if (path_str.empty())
		return BRLCAD_ERROR;
	int ret = draw(path_str.c_str());
	return ret;
}

int
QgModel::drawPaths(const std::vector<std::string> &paths)
{
	QTCAD_SLOT("QgModel::drawPaths", 1);
	struct ged *gedp = m_session ? m_session->ged() : NULL;
	if (!gedp || !gedp->ged_gvp || paths.empty())
		return BRLCAD_ERROR;

	std::vector<const char *> draw_paths;
	draw_paths.reserve(paths.size());
	for (const std::string &path : paths) {
		if (!path.empty())
			draw_paths.push_back(path.c_str());
	}
	if (draw_paths.empty())
		return BRLCAD_ERROR;

	bool timing_enabled = qgmodel_draw_timing_is_enabled(this);
	if (timing_enabled)
		qgmodel_draw_timing_stats_for(this).draw_calls++;
	int64_t blank_slate_start_us = timing_enabled ?
		bu_gettime() : 0;
	int blank_slate = !ged_draw_has_paths(gedp, gedp->ged_gvp, -1);
	if (timing_enabled)
		qgmodel_draw_timing_stats_for(this).blank_slate_check_us +=
			(uint64_t)(bu_gettime() - blank_slate_start_us);

	struct ged_draw_transaction txn =
		ged_draw_transaction_make(GED_DRAW_TXN_DRAW, NULL);
	txn.view = gedp->ged_gvp;
	txn.autoview = blank_slate;
	txn.paths = draw_paths.data();
	txn.path_count = (int)draw_paths.size();

	uint64_t draw_rev_before = ged_draw_scene_revision(gedp);
	uint64_t draw_event_count_before = draw_observer_event_count;
	int manage_draw_notifications = (draw_observer_defer_depth == 0);
	if (manage_draw_notifications) {
		pending_draw_event_paths.clear();
		pending_draw_event_all = 0;
		pending_draw_event_view_only = 0;
	}
	draw_observer_defer_depth++;
	int64_t transaction_start_us = timing_enabled ?
		bu_gettime() : 0;
	struct ged_draw_transaction_result result;
	ged_draw_transaction_result_init(&result);
	int ret = ged_draw_apply_transaction(gedp, &txn, &result);
	if (timing_enabled)
		qgmodel_draw_timing_stats_for(this).transaction_us +=
			(uint64_t)(bu_gettime() - transaction_start_us);
	draw_observer_defer_depth--;
	ged_draw_transaction_result_free(&result);

	uint64_t draw_rev_after = ged_draw_scene_revision(gedp);
	int draw_events_seen = (draw_observer_event_count !=
		draw_event_count_before);
	int draw_changed = (ret > 0 || draw_events_seen ||
		draw_rev_after != draw_rev_before);
	if (manage_draw_notifications && draw_changed) {
		if (draw_events_seen)
			flushPendingDrawNotifications();
		else
			notifyDrawnItemsChanged();
		int64_t view_signal_start_us = timing_enabled ?
			bu_gettime() : 0;
		emit view_changed(QG_VIEW_DRAWN);
		if (timing_enabled)
			qgmodel_draw_timing_stats_for(this).view_signal_us +=
				(uint64_t)(bu_gettime() - view_signal_start_us);
	}
	if (ret < 0) {
		return BRLCAD_ERROR;
	}
	if (timing_enabled)
		qgmodel_draw_timing_stats_for(this).successful_draw_calls++;
	return BRLCAD_OK;
}

int
QgModel::draw(const char *inst_path)
{
	QTCAD_SLOT("QgModel::draw", 1);
	if (!inst_path || !inst_path[0])
		return BRLCAD_ERROR;
	std::vector<std::string> paths;
	paths.emplace_back(inst_path);
	return drawPaths(paths);
}

int
QgModel::erase_action()
{
	QTCAD_SLOT("QgModel::erase_action", 1);
	// https://stackoverflow.com/a/28647342/2037687
	QAction *a = qobject_cast<QAction *>(sender());
	QVariant v = a->data();
	QgItem *cnode  = (QgItem *) v.value<void *>();
	if (!cnode)
		return BRLCAD_ERROR;
	std::string path_str = item_path(cnode);
	if (path_str.empty())
		return BRLCAD_ERROR;
	int ret = erase(path_str.c_str());
	return ret;
}

int
QgModel::erase(const char *inst_path)
{
	QTCAD_SLOT("QgModel::erase", 1);
	struct ged *gedp = m_session ? m_session->ged() : NULL;
	if (!gedp || !gedp->ged_gvp || !inst_path || !inst_path[0])
		return BRLCAD_ERROR;

	struct ged_draw_transaction txn =
		ged_draw_transaction_make(GED_DRAW_TXN_ERASE, inst_path);
	txn.view = gedp->ged_gvp;

	struct ged_draw_transaction_result result;
	ged_draw_transaction_result_init(&result);
	int ret = ged_draw_apply_transaction(gedp, &txn, &result);
	if (ret < 0) {
		ged_draw_transaction_result_free(&result);
		return BRLCAD_ERROR;
	}

	if (ret > 0) {
		emit view_changed(QG_VIEW_DRAWN);
	}
	ged_draw_transaction_result_free(&result);
	return BRLCAD_OK;
}

void
QgModel::toggle_hierarchy()
{
	QTCAD_SLOT("QgModel::toggle_hierarchy", 1);
	struct ged *gedp = m_session->ged();
	if (!gedp || !gedp->dbip)
		return;

	flatten_hierarchy = !flatten_hierarchy;
	changed_db_flag = 1;
	g_update(gedp->dbip);
}

void
QgModel::item_collapsed(const QModelIndex &index)
{
	QTCAD_SLOT("QgModel::item_collapsed", 1);
	QgItem *itm = getItem(index);
	itm->open_itm = false;
}

void
QgModel::item_expanded(const QModelIndex &index)
{
	QTCAD_SLOT("QgModel::item_expanded", 1);
	QgItem *itm = getItem(index);
	itm->open_itm = true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
