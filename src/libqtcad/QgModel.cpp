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
 * 1) check the hash of the QgItem against the GED database index to see if it is
 * still valid.  If not, it must be either removed or replaced
 */

#include "common.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <QAction>
#include <QFileInfo>

#include "bu/env.h"
#include "bu/sort.h"
#include "ged/database.h"
#include "ged/db_index.h"
#include "ged/scene.h"
#include "ged/selection.h"
#include "raytrace.h"
#define ALPHANUM_IMPL
#include "../libged/alphanum.h"

#include "qtcad/QgModel.h"
#include "qtcad/QgSession.h"
#include "qtcad/QgUtil.h"
#include "qtcad/QgSignalFlags.h"

using QgDbChangeSet = std::unique_ptr<struct ged_db_change_set,
    decltype(&ged_db_change_set_destroy)>;

static QgDbChangeSet
qg_db_changes(struct ged *gedp)
{
    return QgDbChangeSet(ged_db_change_set_create(gedp, 0),
	ged_db_change_set_destroy);
}

static void
qg_db_change_names(std::set<std::string> &names,
    const struct ged_db_change_set *changes,
    enum ged_db_change_object_kind kind)
{
    const size_t count = ged_db_change_set_object_count(changes, kind);
    for (size_t i = 0; i < count; i++) {
	const char *name = ged_db_change_set_object_at(changes, kind, i);
	if (name)
	    names.insert(name);
    }
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

	struct directory *inst1 = i1->directoryEntry();
	struct directory *inst2 = i2->directoryEntry();

	if (!inst1 && !inst2)
	    return false;
	if (!inst1 && inst2)
	    return true;
	if (inst1 && !inst2)
	    return false;

	const char *n1 = inst1->d_namep;
	const char *n2 = inst2->d_namep;
	if (alphanum_impl(n1, n2, NULL) < 0)
	    return true;

	return false;
    }
};

static bool
qg_index_children(struct ged *gedp, unsigned long long parent_id,
	std::vector<unsigned long long> &children,
	std::unordered_map<unsigned long long, db_op_t> *operations = NULL)
{
    children.clear();
    if (operations)
	operations->clear();

    struct ged_db_index_record parent;
    if (!ged_db_index_record_get(gedp, parent_id, &parent))
	return false;

    children.reserve(parent.child_count);
    for (size_t row = 0; row < parent.child_count; row++) {
	struct ged_db_index_child child;
	if (!ged_db_index_child_at(gedp, parent_id, row, &child)) {
	    children.clear();
	    if (operations)
		operations->clear();
	    return false;
	}
	children.push_back(child.record.id);
	if (operations)
	    (*operations)[child.record.id] =
		static_cast<db_op_t>(child.bool_op);
    }
    return true;
}

static std::vector<unsigned long long>
qg_index_tops(struct ged *gedp)
{
    const size_t count = ged_db_index_tops(gedp, 1, NULL, 0);
    std::vector<unsigned long long> tops(count);
    if (count && ged_db_index_tops(gedp, 1, tops.data(), tops.size()) !=
	    count)
	tops.clear();
    return tops;
}

static void
qgmodel_mark_invalid_subtree(std::unordered_set<QgItem *> &invalid,
	QgItem *item)
{
    if (!item || !invalid.insert(item).second)
	return;

    for (QgItem *child : item->childItems())
	qgmodel_mark_invalid_subtree(invalid, child);
}

static bool
qgmodel_item_matches_name(const QgItem *item, const std::string &name)
{
    if (!item)
	return false;

    std::string item_name = item->displayName();
    if (item_name == name)
	return true;
    if (item_name.size() <= name.size() + 1 ||
	item_name.compare(0, name.size(), name) ||
	item_name[name.size()] != '@')
	return false;
    return std::all_of(item_name.begin() + name.size() + 1,
	item_name.end(), [](unsigned char c) { return std::isdigit(c); });
}

static int
qgmodel_item_path(struct bu_vls *path, QgItem *item)
{
    if (!path || !item || !item->model())
	return 0;
    std::vector<unsigned long long> ids = item->path_items();
    return ged_db_index_path_print(item->model()->ged(), path, ids.data(),
	ids.size(), 0, 0);
}

QgItem::QgItem(unsigned long long hash, QgModel *ictx)
{
    mdl = ictx;
    ihash = hash;
    parentItem = NULL;
    if (!mdl || !mdl->ged())
	return;

    struct ged_db_index_record record;
    if (!ged_db_index_record_get(mdl->ged(), ihash, &record))
	return;

    if (record.name)
	bu_vls_sprintf(&name, "%s", record.name);
    c_count = record.child_count;
    dp = record.dp;
    icon = QgIcon(dp, ictx->ged()->dbip);
}

QgItem::~QgItem()
{
    bu_vls_free(&name);
}

void
QgItem::open()
{
    if (!mdl)
	return;
    setOpen(true);
    QModelIndex ind = mdl->NodeIndex(this);
    mdl->fetchMore(ind);
}


void
QgItem::close()
{
    setOpen(false);
}

void
QgItem::appendChild(QgItem *c)
{
    children.push_back(c);
}

QgItem *
QgItem::child(int n) const
{
    if (n < 0 || n >= (int)children.size())
	return NULL;

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
QgItem::parent() const
{
    return parentItem;
}

int
QgItem::childNumber() const
{
    if (parentItem) {
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
    while (citem->parent()) {
	citem = citem->parent();
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
    return ged_db_index_path_hash(mdl->ged(), pitems.data(), pitems.size(),
	0);
}

extern "C" void
qgmodel_event_callback(struct ged *UNUSED(gedp),
	const struct ged_event *UNUSED(events), size_t UNUSED(event_count),
	const struct ged_event_result *UNUSED(result), void *client_data)
{
    QgModel *mdl = static_cast<QgModel *>(client_data);
    mdl->changed_db_flag = 1;
}

extern "C" void
qgmodel_scene_callback(struct ged *UNUSED(gedp),
	const struct ged_scene_delta *delta, void *client_data)
{
    QgModel *mdl = static_cast<QgModel *>(client_data);
    if (ged_scene_delta_view(delta) == mdl->ged()->ged_gvp)
	mdl->notifyDrawnItemsChanged();
}


QgModel::QgModel(QObject *p, const char *npath)
: QAbstractItemModel(p)
{
    // There are commands such as open that we want to work even without
    // a database instance opened - rather than using ged_open to create
    // a gedp, be sure one is always available - callers should use the
    // libged "open" command to open a .g file.
    m_session = new QgSession(this);
    gedp = m_session->ged();
    QObject::connect(this, &QgModel::view_change,
	m_session, &QgSession::notifyViewChanged);
    event_observer = ged_event_observer_add(ged(),
	GED_EVENT_OBSERVER_POST_RECONCILE, qgmodel_event_callback, this);
    scene_observer = ged_scene_observer_add(ged(), qgmodel_scene_callback,
	this);
    bu_setenv("DM_SWRAST", "1", 1);

    // Set up the root item
    rootItem = new QgItem(0, this);
    rootItem->mdl = this;

    items = new std::unordered_set<QgItem *>;
    items->clear();


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
	av[2] = NULL;
	run_cmd(ged()->ged_result_str, ac, (const char **)av);
    }
}

QgModel::~QgModel()
{
    if (selection_batch_open)
	(void)ged_selection_batch_end(ged());
    if (event_observer)
	ged_event_observer_remove(ged(), event_observer);
    if (scene_observer)
	ged_scene_observer_remove(ged(), scene_observer);
    items_clear();
    delete items;
    items = NULL;
    delete rootItem;
    rootItem = NULL;
}

QgSession *
QgModel::session() const
{
    return m_session;
}

struct ged *
QgModel::ged() const
{
    return m_session ? m_session->ged() : NULL;
}

const char *
QgModel::selectionSetKey() const
{
    return selection_set_name.empty() ? NULL : selection_set_name.c_str();
}


void
QgModel::selectionBatchBegin()
{
    if (!selection_batch_open && ged_selection_batch_begin(ged()))
	selection_batch_open = true;
}


void
QgModel::setSelectionSetName(const char *set_name)
{
    std::string new_name = set_name ? set_name : "";
    if (new_name == selection_set_name)
	return;

    if (selection_batch_open) {
	(void)ged_selection_recompute(ged(), selectionSetKey());
	(void)ged_selection_batch_end(ged());
	selection_batch_open = false;
    }

    selection_set_name = new_name;
    notifySelectionItemsChanged();
}

size_t
QgModel::selectionCount() const
{
    return ged_selection_count(ged(), selectionSetKey());
}

bool
QgModel::itemSelected(const QgItem *item) const
{
    return item && ged_selection_is_path_selected(ged(), selectionSetKey(),
	item->path_hash());
}

int
QgModel::itemHighlight(const QgItem *item) const
{
    if (!item)
	return 0;

    struct ged_db_index_record record;
    ged_db_index_id object_id = item->instanceHash();
    if (ged_db_index_record_get(ged(), object_id, &record))
	object_id = record.object_id;

    switch (interaction_mode) {
	case 0:
	    return (!item->isOpen() &&
		ged_selection_is_path_active_parent(ged(), selectionSetKey(),
		    item->path_hash())) ? 1 : 0;
	case 1:
	    return ged_selection_is_object_parent(ged(), selectionSetKey(),
		object_id) ? 2 : 0;
	case 2:
	    if (ged_selection_is_object_immediate_parent(ged(),
		    selectionSetKey(), object_id))
		return 3;
	    return ged_selection_is_object_grand_parent(ged(),
		selectionSetKey(), object_id) ? 2 : 0;
	default:
	    return 0;
    }
}

void
QgModel::selectionClear()
{
    selectionBatchBegin();
    (void)ged_selection_clear(ged(), selectionSetKey());
}

void
QgModel::itemSelect(QgItem *item)
{
    if (!hasItem(item))
	return;

    selectionBatchBegin();
    std::vector<unsigned long long> path = item->path_items();
    (void)ged_selection_select_path_ids(ged(), selectionSetKey(), path.data(),
	path.size(), 0);
}

void
QgModel::itemDeselect(QgItem *item)
{
    if (!hasItem(item))
	return;

    selectionBatchBegin();
    std::vector<unsigned long long> path = item->path_items();
    (void)ged_selection_deselect_path_ids(ged(), selectionSetKey(),
	path.data(), path.size(), 0);
}

unsigned long long
QgModel::selectionCommit()
{
    (void)ged_selection_recompute(ged(), selectionSetKey());
    if (selection_batch_open) {
	(void)ged_selection_batch_end(ged());
	selection_batch_open = false;
    }
    notifySelectionItemsChanged();
    return QG_VIEW_SELECT | QG_VIEW_REFRESH;
}

void
QgModel::notifySelectionItemsChanged()
{
    using HashSet = std::unordered_set<unsigned long long>;
    HashSet old_selected = std::move(selected_path_hashes);
    HashSet old_active_parents = std::move(active_parent_path_hashes);
    HashSet old_immediate_parents =
	std::move(immediate_parent_object_ids);
    HashSet old_grand_parents = std::move(grand_parent_object_ids);

    selected_path_hashes.clear();
    active_parent_path_hashes.clear();
    immediate_parent_object_ids.clear();
    grand_parent_object_ids.clear();

    auto load_hashes = [this](enum ged_selection_hash_kind kind) {
	HashSet hashes;
	size_t count = ged_selection_hashes(ged(), selectionSetKey(), kind,
	    NULL, 0);
	std::vector<ged_db_index_id> values(count);
	if (count)
	    (void)ged_selection_hashes(ged(), selectionSetKey(), kind,
		values.data(), values.size());
	hashes.insert(values.begin(), values.end());
	return hashes;
    };
    selected_path_hashes = load_hashes(GED_SELECTION_HASH_SELECTED_PATH);
    active_parent_path_hashes =
	load_hashes(GED_SELECTION_HASH_ACTIVE_PARENT_PATH);
    immediate_parent_object_ids =
	load_hashes(GED_SELECTION_HASH_IMMEDIATE_PARENT_OBJECT);
    grand_parent_object_ids =
	load_hashes(GED_SELECTION_HASH_GRAND_PARENT_OBJECT);

    QVector<int> roles;
    roles << QgDataRoles::SelectDisplayRole
	<< QgDataRoles::HighlightDisplayRole;
    std::unordered_set<QgItem *> notified;
    auto notify_item = [this, &roles, &notified](QgItem *item) {
	if (!item || !notified.insert(item).second)
	    return;
	QModelIndex index = NodeIndex(item);
	if (index.isValid())
	    emit dataChanged(index, index, roles);
    };
    auto notify_paths = [this, &notify_item](const HashSet &hashes) {
	for (unsigned long long hash : hashes) {
	    auto item_it = items_by_path_hash.find(hash);
	    if (item_it == items_by_path_hash.end())
		continue;
	    for (QgItem *item : item_it->second)
		notify_item(item);
	}
    };
    auto notify_objects = [this, &notify_item](const HashSet &hashes) {
	if (hashes.empty())
	    return;
	for (QgItem *item : allItems()) {
	    struct ged_db_index_record record;
	    ged_db_index_id object_id = item->instanceHash();
	    if (ged_db_index_record_get(ged(), object_id, &record))
		object_id = record.object_id;
	    if (hashes.find(object_id) != hashes.end())
		notify_item(item);
	}
    };

    notify_paths(old_selected);
    notify_paths(selected_path_hashes);
    notify_paths(old_active_parents);
    notify_paths(active_parent_path_hashes);
    notify_objects(old_immediate_parents);
    notify_objects(immediate_parent_object_ids);
    notify_objects(old_grand_parents);
    notify_objects(grand_parent_object_ids);
}

void
QgModel::notifyDrawnItemsChanged()
{
    QVector<int> roles;
    roles << QgDataRoles::DrawnDisplayRole;
    for (QgItem *item : allItems()) {
	QModelIndex index = NodeIndex(item);
	if (index.isValid())
	    emit dataChanged(index, index, roles);
    }
}

void
QgModel::setInteractionMode(int mode)
{
    if (interaction_mode == mode)
	return;
    interaction_mode = mode;
    notifySelectionItemsChanged();
}

void
QgModel::item_register(QgItem *item)
{
    if (!item || item == rootItem || !items)
	return;

    if (!items->insert(item).second)
	return;

    items_by_instance_hash[item->ihash].insert(item);
    if (item->dp)
	items_by_directory[item->dp].insert(item);
    unsigned long long path_hash = item->path_hash();
    if (path_hash) {
	items_by_path_hash[path_hash].insert(item);
	item_path_hashes[item] = path_hash;
    }
}

void
QgModel::item_unregister(QgItem *item)
{
    if (!item || item == rootItem || !items)
	return;

    auto instance_it = items_by_instance_hash.find(item->ihash);
    if (instance_it != items_by_instance_hash.end()) {
	instance_it->second.erase(item);
	if (instance_it->second.empty())
	    items_by_instance_hash.erase(instance_it);
    }

    if (item->dp) {
	auto directory_it = items_by_directory.find(item->dp);
	if (directory_it != items_by_directory.end()) {
	    directory_it->second.erase(item);
	    if (directory_it->second.empty())
		items_by_directory.erase(directory_it);
	}
    }

    auto item_path_it = item_path_hashes.find(item);
    if (item_path_it != item_path_hashes.end()) {
	unsigned long long path_hash = item_path_it->second;
	auto path_it = items_by_path_hash.find(path_hash);
	if (path_it != items_by_path_hash.end()) {
	    path_it->second.erase(item);
	    if (path_it->second.empty())
		items_by_path_hash.erase(path_it);
	}
	item_path_hashes.erase(item_path_it);
    }

    items->erase(item);
}

void
QgModel::item_indexes_clear()
{
    items_by_instance_hash.clear();
    items_by_path_hash.clear();
    items_by_directory.clear();
    item_path_hashes.clear();
}

void
QgModel::items_clear()
{
    if (!items)
	return;

    for (QgItem *item : *items)
	delete item;

    items->clear();
    item_indexes_clear();
    tops_items.clear();
    if (rootItem) {
	rootItem->children.clear();
	rootItem->c_noderow.clear();
	rootItem->c_count = 0;
    }
}

bool
QgModel::item_is_indexed(QgItem *item) const
{
    if (!item || !items || items->find(item) == items->end())
	return false;

    auto instance_it = items_by_instance_hash.find(item->ihash);
    if (instance_it == items_by_instance_hash.end() ||
	instance_it->second.find(item) == instance_it->second.end())
	return false;

    auto item_path_it = item_path_hashes.find(item);
    if (item_path_it == item_path_hashes.end())
	return false;
    unsigned long long path_hash = item_path_it->second;
    auto path_it = items_by_path_hash.find(path_hash);
    return path_hash && path_it != items_by_path_hash.end() &&
	path_it->second.find(item) != path_it->second.end();
}

void
QgModel::item_rows_rebuild(QgItem *item)
{
    if (!item)
	return;

    item->c_noderow.clear();
    for (size_t i = 0; i < item->children.size(); i++)
	item->c_noderow[item->children[i]] = (int)i;
    item->c_count = item->children.size();
}

void
QgModel::item_subtree_delete(QgItem *item)
{
    if (!item || item == rootItem)
	return;

    std::vector<QgItem *> children = item->children;
    for (QgItem *child : children)
	item_subtree_delete(child);

    item_unregister(item);
    delete item;
}

bool
QgModel::item_children_sync(QgItem *item,
	const std::vector<unsigned long long> &children)
{
    if (!item)
	return false;

    std::unordered_map<unsigned long long, db_op_t> operations;
    if (item != rootItem) {
	std::vector<unsigned long long> indexed_children;
	if (!qg_index_children(ged(), item->ihash, indexed_children,
		&operations) || indexed_children != children)
	    return false;
    }
    if (item != rootItem && item->children.empty() && !item->open_itm) {
	item->c_count = children.size();
	return true;
    }

    QModelIndex parent_index = NodeIndex(item);
    for (size_t row = 0; row < children.size(); row++) {
	unsigned long long child_hash = children[row];
	if (row < item->children.size() &&
		item->children[row]->ihash == child_hash) {
	    item->children[row]->op = item == rootItem ? DB_OP_UNION :
		operations[child_hash];
	    continue;
	}

	size_t source_row = row;
	while (source_row < item->children.size() &&
		item->children[source_row]->ihash != child_hash)
	    source_row++;

	if (source_row < item->children.size()) {
	    model_structure_change_in_progress = true;
	    if (!beginMoveRows(parent_index, (int)source_row, (int)source_row,
		    parent_index, (int)row)) {
		model_structure_change_in_progress = false;
		return false;
	    }
	    QgItem *child = item->children[source_row];
	    item->children.erase(item->children.begin() + source_row);
	    item->children.insert(item->children.begin() + row, child);
	    child->op = item == rootItem ? DB_OP_UNION :
		operations[child_hash];
	    item_rows_rebuild(item);
	    endMoveRows();
	    model_structure_change_in_progress = false;
	    continue;
	}

	QgItem *child = new QgItem(child_hash, this);
	child->parentItem = item;
	child->op = item == rootItem ? DB_OP_UNION : operations[child_hash];
	model_structure_change_in_progress = true;
	beginInsertRows(parent_index, (int)row, (int)row);
	item->children.insert(item->children.begin() + row, child);
	item_register(child);
	item_rows_rebuild(item);
	endInsertRows();
	model_structure_change_in_progress = false;
    }

    while (item->children.size() > children.size()) {
	int row = (int)item->children.size() - 1;
	QgItem *child = item->children[row];
	model_structure_change_in_progress = true;
	beginRemoveRows(parent_index, row, row);
	item->children.pop_back();
	item_rows_rebuild(item);
	item_subtree_delete(child);
	endRemoveRows();
	model_structure_change_in_progress = false;
    }

    item_rows_rebuild(item);
    if (item == rootItem)
	tops_items = rootItem->children;
    return true;
}

void
QgModel::items_changed_notify(const std::set<std::string> &names)
{
    for (const std::string &name : names) {
	std::unordered_set<QgItem *> changed_items;
	struct directory *dp = db_lookup(ged()->dbip, name.c_str(), LOOKUP_QUIET);
	if (dp) {
	    auto directory_it = items_by_directory.find(dp);
	    if (directory_it != items_by_directory.end())
		changed_items = directory_it->second;
	}
	if (changed_items.empty()) {
	    for (QgItem *item : *items) {
		if (qgmodel_item_matches_name(item, name))
		    changed_items.insert(item);
	    }
	}
	for (QgItem *item : changed_items) {
	    if (!item_is_indexed(item))
		continue;
	    item->icon = QgIcon(item->dp, ged()->dbip);
	    QModelIndex index = NodeIndex(item);
	    if (index.isValid())
		emit dataChanged(index, index);
	}
    }
}

bool
QgModel::change_deltas_apply()
{
    QgDbChangeSet changes = qg_db_changes(ged());
    if (!changes || !ged_db_change_set_revision(changes.get()))
	return false;

    const unsigned int flags = ged_db_change_set_flags(changes.get());
    if ((flags & GED_DB_CHANGE_REBUILD) ||
	ged_db_change_set_rename_count(changes.get()))
	return false;

    const bool structural = flags & GED_DB_CHANGE_STRUCTURAL;
    std::set<std::string> added;
    std::set<std::string> modified;
    std::set<std::string> removed;
    std::set<std::string> paths;
    qg_db_change_names(added, changes.get(), GED_DB_CHANGE_OBJECT_ADDED);
    qg_db_change_names(modified, changes.get(),
	GED_DB_CHANGE_OBJECT_MODIFIED);
    qg_db_change_names(removed, changes.get(), GED_DB_CHANGE_OBJECT_REMOVED);
    const size_t path_count = ged_db_change_set_path_count(changes.get());
    for (size_t i = 0; i < path_count; i++) {
	const char *path = ged_db_change_set_path_at(changes.get(), i);
	if (path)
	    paths.insert(path);
    }

    if (structural && added.empty() && modified.empty() &&
	    removed.empty() && paths.empty())
	return false;

    std::unordered_set<QgItem *> parent_candidates;
    if (structural)
	parent_candidates.insert(rootItem);

    for (const std::string &name : modified) {
	struct directory *dp = db_lookup(ged()->dbip, name.c_str(), LOOKUP_QUIET);
	if (dp) {
	    auto directory_it = items_by_directory.find(dp);
	    if (directory_it != items_by_directory.end()) {
		for (QgItem *item : directory_it->second) {
		    // librt's legacy rename notification reports the renamed
		    // directory as modified.  Its pointer identity is unchanged,
		    // but its name-derived instance hash is not.  Do not partially
		    // reconcile that ambiguous identity transition.
		    if (!qgmodel_item_matches_name(item, name))
			return false;
		    parent_candidates.insert(item);
		}
	    }
	}
	if (!dp || items_by_directory.find(dp) == items_by_directory.end()) {
	    for (QgItem *item : *items) {
		if (qgmodel_item_matches_name(item, name))
		    parent_candidates.insert(item);
	    }
	}
    }

    for (const std::string &name : removed) {
	for (QgItem *item : *items) {
	    if (qgmodel_item_matches_name(item, name) && item->parent())
		parent_candidates.insert(item->parent());
	}
    }

    for (const std::string &path : paths) {
	const size_t path_len = ged_db_index_path_resolve(ged(), path.c_str(),
	    NULL, 0);
	std::vector<unsigned long long> path_items(path_len);
	if (!path_len || ged_db_index_path_resolve(ged(), path.c_str(),
		path_items.data(), path_items.size()) != path_len)
	    continue;
	unsigned long long path_hash = ged_db_index_path_hash(ged(),
	    path_items.data(), path_items.size(), 0);
	auto path_it = items_by_path_hash.find(path_hash);
	if (path_it == items_by_path_hash.end())
	    continue;
	for (QgItem *item : path_it->second) {
	    parent_candidates.insert(item);
	    if (item->parent())
		parent_candidates.insert(item->parent());
	}
    }

    (void)ged_db_change_reconcile(ged());
    if (!structural) {
	items_changed_notify(modified);
	return true;
    }

    std::vector<QgItem *> parents;
    for (QgItem *parent : parent_candidates) {
	if (parent && parent != rootItem)
	    parents.push_back(parent);
    }
    parents.push_back(rootItem);

    for (QgItem *parent : parents) {
	if (parent != rootItem && items->find(parent) == items->end())
	    continue;

	std::vector<unsigned long long> children;
	if (parent == rootItem) {
	    children = qg_index_tops(ged());
	    std::sort(children.begin(), children.end(),
		[this](unsigned long long left, unsigned long long right) {
		    struct ged_db_index_record left_record;
		    struct ged_db_index_record right_record;
		    if (!ged_db_index_record_get(ged(), left, &left_record) ||
			    !ged_db_index_record_get(ged(), right,
				&right_record) || !left_record.name ||
			    !right_record.name)
			return left < right;
		    return alphanum_impl(left_record.name, right_record.name,
			NULL) < 0;
		});
	} else {
	    if (!qg_index_children(ged(), parent->ihash, children))
		return false;
	}
	if (!item_children_sync(parent, children))
	    return false;
    }

    items_changed_notify(modified);
    return true;
}

// Note - this is a private method and must be run from within g_update's
// begin/end RebuildModel block.  Also, it assumes all invalid QgItem pointers
// have been removed from the children arrays. (The ability to recognize an
// invalid QgItem is defined at that level - we don't want to re-create it
// here.)
void
QgModel::item_rebuild(QgItem *item, std::unordered_set<QgItem *> &invalid)
{
    // Top level is a special case and is handled separately
    if (item == rootItem) {
	return;
    }

    std::vector<unsigned long long> child_ids;
    std::unordered_map<unsigned long long, db_op_t> operations;
    if (!qg_index_children(ged(), item->ihash, child_ids, &operations)) {
	for (QgItem *old_child : item->children)
	    qgmodel_mark_invalid_subtree(invalid, old_child);
	item->children.clear();
	item->c_noderow.clear();
	item->c_count = 0;
	return;
    }

    if (child_ids.empty()) {
	for (QgItem *old_child : item->children)
	    qgmodel_mark_invalid_subtree(invalid, old_child);
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
	item->c_count = child_ids.size();
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
    for (unsigned long long child_id : child_ids) {
	// For each new child, look up its instance in the original data to see
	// if we have a corresponding QgItem available.
	auto old_child = oc.find(child_id);
	if (old_child != oc.end()) {
	    // We have a viable QgItem from the previous state - reuse it
	    old_child->second->op = operations[child_id];
	    nc.push_back(old_child->second);
	} else {
	    // Previous tree did not have an appropriate QgItem -
	    // make a new one
	    QgItem *nitem = new QgItem(child_id, this);
	    nitem->parentItem = item;
	    nitem->op = operations[child_id];
	    nc.push_back(nitem);
	    item_register(nitem);
	}
    }
    item->c_count = nc.size();

    // If anything changed since the last time children was built, we need to
    // replace children's contents with the new vector.  This is being run
    // inside g_update's beginRebuildModel block, so we don't need to try and
    // manage individual rows here.
    if (nc != item->children) {
	std::unordered_set<QgItem *> reused(nc.begin(), nc.end());
	for (QgItem *old_child : item->children) {
	    if (reused.find(old_child) == reused.end())
		qgmodel_mark_invalid_subtree(invalid, old_child);
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
    const bool new_database = model_dbip != n_dbip;
    model_dbip = n_dbip;
    if (!n_dbip) {
	// if we have no dbip, clear out everything
	model_structure_change_in_progress = true;
	beginResetModel();
	items_clear();
	emit mdl_changed_db((void *)ged());
	m_session->notifyDbChanged(nullptr);
	emit view_change(QG_VIEW_DRAWN);
	changed_db_flag = 0;
	endResetModel();
	model_structure_change_in_progress = false;
	notifySelectionItemsChanged();
	return;
    }

    QgDbChangeSet changes = qg_db_changes(ged());
    uint64_t change_revision = ged_db_change_set_revision(changes.get());
    if (new_database || change_revision)
	changed_db_flag = 1;

    bool deltas_applied = false;
    if (changed_db_flag && !new_database)
	deltas_applied = change_deltas_apply();

    // If we have a dbip and the changed flag is set, figure out what's different
    if (changed_db_flag && !deltas_applied) {
	model_structure_change_in_progress = true;
	beginResetModel();

	// Step 1 - make sure our instances are current - i.e. they match the
	// .g database state
	(void)ged_db_change_reconcile(ged());

	std::vector<unsigned long long> tops = qg_index_tops(ged());
	std::unordered_set<unsigned long long> top_hashes(tops.begin(), tops.end());

	// Clear out any QgItems with invalid info.  We need to be fairly
	// aggressive here - first we find all the existing invalid ones, and
	// then we invalidate all their children recursively - any item with an
	// invalid parent is invalid.
	std::queue<QgItem *> to_clear;
	std::unordered_set<QgItem *> invalid;
	std::unordered_set<QgItem *>::iterator s_it;
	for (QgItem *top_item : tops_items) {
	    if (top_hashes.find(top_item->ihash) == top_hashes.end())
		to_clear.push(top_item);
	}
	for (s_it = items->begin(); s_it != items->end(); s_it++) {
	    QgItem *itm = *s_it;
	    struct ged_db_index_record record;
	    if (new_database ||
		!ged_db_index_record_get(ged(), itm->ihash, &record)) {
		to_clear.push(itm);
		continue;
	    }
	    if (itm->dp != record.dp)
		to_clear.push(itm);
	}
	while (!to_clear.empty()) {
	    QgItem *i_itm = to_clear.front();
	    to_clear.pop();
	    invalid.insert(i_itm);
	    for (size_t i = 0; i < i_itm->children.size(); i++)
		to_clear.push(i_itm->children[i]);
	}

	for (s_it = items->begin(); s_it != items->end(); s_it++) {
	    if (invalid.find(*s_it) != invalid.end())
		continue;
	    QgItem *i_itm = *s_it;
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
	    item_rebuild(i_itm, invalid);
	}

	// Validate existing tops QgItems based on the tops data.
	// Using tops_instances, construct a new tops vector.  Reuse any still valid
	// QgItems, and make new ones 
	std::vector<QgItem *> ntops_items;

	for (size_t i = 0; i < tops.size(); i++) {
	    QgItem *top_item = NULL;
	    auto instance_it = items_by_instance_hash.find(tops[i]);
	    if (instance_it != items_by_instance_hash.end()) {
		for (QgItem *candidate : instance_it->second) {
		    if (candidate->parentItem == rootItem &&
			invalid.find(candidate) == invalid.end()) {
			top_item = candidate;
			break;
		    }
		}
	    }
	    if (!top_item) {
		QgItem *nitem = new QgItem(tops[i], this);
		nitem->parentItem = rootItem;
		nitem->op = DB_OP_UNION;
		item_register(nitem);
		top_item = nitem;
	    }
	    ntops_items.push_back(top_item);
	}

	// Set the new tops items as children of the rootItem.
	std::sort(ntops_items.begin(), ntops_items.end(), QgItem_cmp());
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
	    item_unregister(iv_itm);
	    delete iv_itm;
	}

	endResetModel();
	model_structure_change_in_progress = false;
    }

    // If we did change something, we need to let the application know
    if (changed_db_flag) {
	(void)ged_selection_recompute(ged(), selectionSetKey());
	notifySelectionItemsChanged();
	emit mdl_changed_db((void *)ged());
	m_session->notifyDbChanged(n_dbip);
	emit check_highlights();
    }

    if (change_revision)
	(void)ged_db_change_acknowledge(ged(), change_revision);

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
    if (!item_is_indexed(node))
	return QModelIndex();
    int nr = NodeRow(node);
    if (nr == -1)
	return QModelIndex();
    return createIndex(nr, 0, node);
}


bool
QgModel::canFetchMore(const QModelIndex &idx) const
{
    if (!idx.isValid())
	return false;

    QgItem *item = static_cast<QgItem*>(idx.internalPointer());
    if (item == rootItem)
       	return false;

    if (item->children.size())
	return false;

    // If there are children to be fetched, we can fetch them
    return ged_db_index_child_count(ged(), item->ihash) > 0;
}

void
QgModel::fetchMore(const QModelIndex &idx)
{
    if (!idx.isValid())
	return;

    QgItem *item = static_cast<QgItem*>(idx.internalPointer());

    if (UNLIKELY(item == rootItem)) {
	return;
    }

    // If we're already populated, don't need to do it again
    if (item->children.size())
	return;

    size_t child_count = ged_db_index_child_count(ged(), item->ihash);
    if (!child_count)
	return;

    std::vector<QgItem *> nc;
    for (size_t row = 0; row < child_count; row++) {
	struct ged_db_index_child child;
	if (!ged_db_index_child_at(ged(), item->ihash, row, &child))
	    continue;
	QgItem *nitem = new QgItem(child.record.id, this);
	nitem->parentItem = item;
	nitem->op = static_cast<db_op_t>(child.bool_op);
	nc.push_back(nitem);
	item_register(nitem);
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

    return createIndex(parentItem->childNumber(), 0, parentItem);
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
    if (role == Qt::DisplayRole)
	return QVariant(qi->displayName());
    if (role == QgDataRoles::BoolInternalRole)
	return QVariant(qi->booleanOperation());
    if (role == QgDataRoles::DirectoryInternalRole)
	return QVariant::fromValue((void *)(qi->directoryEntry()));
    if (role == QgDataRoles::DrawnDisplayRole) {
	std::vector<unsigned long long> path = qi->path_items();
	return QVariant(ged_scene_path_state_ids(ged(), ged()->ged_gvp,
	    path.data(), path.size(), GED_SCENE_DRAW_DEFAULT));
    }
    if (role == QgDataRoles::SelectDisplayRole)
	return QVariant(itemSelected(qi));

    if (role == QgDataRoles::TypeIconDisplayRole)
	return QVariant(qi->typeIcon());

    if (role == QgDataRoles::HighlightDisplayRole)
	return QVariant(itemHighlight(qi));

    return QVariant();
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
    struct ged *gedp = ged();
    model_dbip = gedp->dbip;

    changed_dp.clear();

    if (!ged_cmd_exists(argv[0])) {
	const char *ccmd = NULL;
	int edist = ged_cmd_lookup(&ccmd, argv[0]);
	if (edist) {
	    if (msg)
		bu_vls_sprintf(msg, "Command %s not found, did you mean %s (edit distance %d)?\n", argv[0],   ccmd, edist);
	}
	return BRLCAD_ERROR;
    }

    int ret = ged_exec(gedp, argc, argv);

    // If this is one of the GED commands supporting incremental input, this
    // return code means we need more input from the application before any
    // command can be run - no need to do the remainder of the logic below.
    if (ret & GED_MORE)
	return ret;

    g_update(gedp->dbip);

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
    return draw(cnode);
}

int
QgModel::draw(const char *inst_path)
{
    QTCAD_SLOT("QgModel::draw", 1);
    const char *paths[] = {inst_path};
    struct ged_scene_draw_request request;
    ged_scene_draw_request_init(&request);
    request.view = ged()->ged_gvp;
    request.paths = paths;
    request.path_count = 1;
    request.autoview = 1;
    struct ged_scene_result *result = ged_scene_result_create();
    if (!result)
	return BRLCAD_ERROR;
    enum ged_scene_status status = ged_scene_draw(ged(), &request, result);
    int changed = ged_scene_result_changed(result);
    ged_scene_result_destroy(result);
    if (status == GED_SCENE_OK && changed)
	emit view_change(QG_VIEW_DRAWN);
    return status == GED_SCENE_OK ? BRLCAD_OK : BRLCAD_ERROR;
}

int
QgModel::draw(QgItem *item)
{
    if (!item || item->model() != this)
	return BRLCAD_ERROR;
    struct bu_vls path_str = BU_VLS_INIT_ZERO;
    int valid = qgmodel_item_path(&path_str, item);
    int ret = valid ? draw(bu_vls_cstr(&path_str)) : BRLCAD_ERROR;
    bu_vls_free(&path_str);
    return ret;
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
    return erase(cnode);
}

int
QgModel::erase(const char *inst_path)
{
    QTCAD_SLOT("QgModel::erase", 1);
    struct ged_scene_erase_request request;
    ged_scene_erase_request_init(&request);
    request.view = ged()->ged_gvp;
    request.path = inst_path;
    struct ged_scene_result *result = ged_scene_result_create();
    if (!result)
	return BRLCAD_ERROR;
    enum ged_scene_status status = ged_scene_erase(ged(), &request, result);
    int changed = ged_scene_result_changed(result);
    ged_scene_result_destroy(result);
    if (status == GED_SCENE_OK && changed)
	emit view_change(QG_VIEW_DRAWN);
    return status == GED_SCENE_OK ? BRLCAD_OK : BRLCAD_ERROR;
}

int
QgModel::erase(QgItem *item)
{
    if (!item || item->model() != this)
	return BRLCAD_ERROR;
    struct bu_vls path_str = BU_VLS_INIT_ZERO;
    int valid = qgmodel_item_path(&path_str, item);
    int ret = valid ? erase(bu_vls_cstr(&path_str)) : BRLCAD_ERROR;
    bu_vls_free(&path_str);
    return ret;
}

void
QgModel::toggle_hierarchy()
{
    QTCAD_SLOT("QgModel::toggle_hierarchy", 1);
    struct ged *gedp = ged();
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
    itm->setOpen(false);
}

void
QgModel::item_expanded(const QModelIndex &index)
{
    QTCAD_SLOT("QgModel::item_expanded", 1);
    QgItem *itm = getItem(index);
    itm->setOpen(true);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

