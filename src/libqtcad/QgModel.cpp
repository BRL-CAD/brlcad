/*                         Q G M O D E L . C P P
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
 * QgItems form the explicit hierarchical representation implied by the gInstances,
 * which means gInstance changes have potentially global impacts on the QgItems
 * hierarchy and it must be fully checked for validity (and repaired as appropriate)
 * after each cycle.  A validity check would be something like:  1) check the hash
 * of the QgItem against the gInstance array to see if it is still a valid instance.
 * If not, it must be either removed or replaced
 */

#include "common.h"

#include <algorithm>
#include <set>
#include <unordered_set>
#include <vector>
#include <QAction>
#include <QFileInfo>

#include "xxhash.h"

#include "bu/env.h"
#include "bu/sort.h"
#include "bg/lod.h"
#define ALPHANUM_IMPL
#include "../libged/alphanum.h"
#include "raytrace.h"
#include "qtcad/gInstance.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgUtil.h"

struct QgItem_cmp {
    inline bool operator() (const QgItem *i1, const QgItem *i2)
    {
	if (!i1 && !i2)
	    return false;
	if (!i1 && i2)
	    return true;
	if (i1 && !i2)
	    return false;
	if (!i1->ihash && !i2->ihash)
	    return false;
	if (!i1->ihash && i2->ihash)
	    return true;
	if (i1->ihash && !i2->ihash)
	    return false;

	gInstance *inst1 = NULL;
	gInstance *inst2 = NULL;
	if (i1->ctx->instances->find(i1->ihash) != i1->ctx->instances->end()) {
	    inst1 = (*i1->ctx->instances)[i1->ihash];
	}
	if (i2->ctx->instances->find(i2->ihash) != i2->ctx->instances->end()) {
	    inst2 = (*i2->ctx->instances)[i2->ihash];
	}

	if (!inst1 && !inst2)
	    return false;
	if (!inst1 && inst2)
	    return true;
	if (inst1 && !inst2)
	    return false;

	if (!inst1->dp_name.length() && !inst2->dp_name.length())
	    return false;
	if (!inst1->dp_name.length() && inst2->dp_name.length())
	    return true;
	if (inst1->dp_name.length() && !inst2->dp_name.length())
	    return false;

	const char *n1 = inst1->dp_name.c_str();
	const char *n2 = inst2->dp_name.c_str();
	if (alphanum_impl(n1, n2, NULL) < 0)
	    return true;

	return false;
    }
};

QgItem::QgItem(gInstance *g, QgModel *ictx)
{
    ctx = ictx;
    if (g) {
	parentItem = NULL;
	c_count = g->children(ictx->instances).size();
	ihash = g->hash;
	// TODO - these copies may be moot - the child relationship needs
	// the gInstance, so we may have to just ensure gInstances aren't
	// removed until after the QgItems are updated.  If that's the case,
	// there's no point in these copies.
	bu_vls_sprintf(&name, "%s", g->dp_name.c_str());
	icon = QgIcon(g->dp, g->dbip);
	op = g->op;
	dp = g->dp;
    }
}

QgItem::~QgItem()
{
    bu_vls_free(&name);
}

void
QgItem::open()
{
    if (!ctx)
	return;
    open_itm = true;
    QModelIndex ind = ctx->NodeIndex(this);
    ctx->fetchMore(ind);
}


void
QgItem::close()
{
    open_itm = false;
}

gInstance *
QgItem::instance()
{
    if (!ctx)
	return NULL;

    std::unordered_map<unsigned long long, gInstance *>::iterator g_it;
    g_it = ctx->instances->find(ihash);
    if (g_it == ctx->instances->end())
	return NULL;

    return g_it->second;
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
	return NULL;

    return children[n];
}

int
QgItem::childCount() const
{
    if (!ctx)
	return 0;

    if (this == ctx->root())
	return children.size();

    return (int)c_count;
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
	for (size_t i = 0; i < parentItem->children.size(); i++) {
	    if (parentItem->children[i] == this)
		return i;
	}
	bu_log("WARNING - invalid parent/child inquiry\n");
    }

    return 0;
}

QString
QgItem::toString()
{
    std::vector<QgItem *> path_items;
    QgItem *citem = this;
    path_items.push_back(this);
    while (citem->parent() && citem->parent()->instance()) {
	citem = citem->parent();
	path_items.push_back(citem);
    }
    std::reverse(path_items.begin(), path_items.end());
    struct db_full_path ifp;
    db_full_path_init(&ifp);
    for (size_t i = 0; i < path_items.size(); i++) {
	db_add_node_to_full_path(&ifp, path_items[i]->dp);
	DB_FULL_PATH_SET_CUR_COMB_INST(&ifp, path_items[i]->instance()->icnt);
    }
    struct bu_vls fpstr = BU_VLS_INIT_ZERO;
    db_path_to_vls(&fpstr, &ifp);
    QString fpqstr(bu_vls_cstr(&fpstr));
    bu_vls_free(&fpstr);
    db_free_full_path(&ifp);
    return fpqstr;
}

// 0 = exact, 1 = name + op, 2 = name + mat, 3 = name only, -1 name mismatch
int
qgitem_cmp_score(QgItem *i1, QgItem *i2)
{
    int ret = -1;
    if (!i1 || !i2 || !i1->ihash || !i2->ihash)
	return ret;

    gInstance *inst1 = (*i1->ctx->instances)[i1->ihash];
    gInstance *inst2 = (*i2->ctx->instances)[i2->ihash];
    if (inst1->dp_name != inst2->dp_name)
	return ret;

    // Names match
    ret = 3;

    // Look for a matrix match
    struct bn_tol mtol = BG_TOL_INIT;
    if (bn_mat_is_equal(inst1->c_m, inst2->c_m, &mtol))
	ret = 2;

    // Look for a boolean op match
    if (inst1->op == inst2->op)
	ret = (ret == 2) ? 0 : 1;

    return ret;
}


QgItem *
find_similar_qgitem(QgItem *c, std::vector<QgItem *> &v)
{
    QgItem *m = NULL;
    int lret = INT_MAX;
    for (size_t i = 0; i < v.size(); i++) {
	int score = qgitem_cmp_score(c, v[i]);
	if (score < 0)
	    continue;
	m = (score < lret) ? v[i] : m;
	if (!score)
	    return m;
    }

    return m;
}



extern "C" void
qgmodel_update_nref_callback(struct db_i *UNUSED(dbip), struct directory *parent_dp, struct directory *child_dp, const char *child_name, db_op_t op, matp_t m, void *u_data)
{
    // If all the input parameters other than dbip/op are NULL and op is set to
    // subtraction, the update_nref logic has completed its work and is making
    // its final callback call.  Because in this use case we need a fully
    // updated data state before doing our processing (as opposed to, for
    // example, triggering events during the update treewalk) we only process
    // when the termination conditions are fully set.
    if (!parent_dp && !child_dp && !child_name && m == NULL && op == DB_OP_SUBTRACT) {

	std::cout << "update nref callback\n";

	// Cycle complete, nref count is current.  Start analyzing.
	QgModel *ctx = (QgModel *)u_data;

	// If anybody was requesting an nref update, the fact that we're here
	// means we've done it.
	ctx->need_update_nref = false;
    }
}

extern "C" void
qgmodel_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data)
{
    std::queue<std::unordered_map<unsigned long long, gInstance *>::iterator> rmq;
    std::unordered_map<unsigned long long, gInstance *>::iterator i_it, trm_it;
    QgModel *ctx = (QgModel *)u_data;
    gInstance *inst = NULL;
    ctx->need_update_nref = true;
    ctx->changed_db_flag = 1;

    // Need to invalidate any LoD caches associated with this dp
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT && ctx->gedp) {
	unsigned long long key = bg_mesh_lod_key_get(ctx->gedp->ged_lod, dp->d_namep);
	if (key) {
	    bg_mesh_lod_clear_cache(ctx->gedp->ged_lod, key);
	    bg_mesh_lod_key_put(ctx->gedp->ged_lod, dp->d_namep, 0);
	}
    }

    switch(mode) {
	case 0:
	    // The view needs to regenerate the wireframe(s) for anything drawn
	    // using this dp, as it may have changed
	    ctx->changed_dp.insert(dp);

	    break;
	case 1:
	    // If we have any name-only references that can now point to a dp, update them
	    for (i_it = ctx->instances->begin(); i_it != ctx->instances->end(); i_it++) {
		inst = i_it->second;
		if (!inst->dp && inst->dp_name == std::string(dp->d_namep)) {
		    inst->dp = dp;
		}
	    }

	    break;
	case 2:
	    // invalidate the dps in any instances where they match.
	    for (i_it = ctx->instances->begin(); i_it != ctx->instances->end(); i_it++) {
		inst = i_it->second;
		if (inst->dp == dp)
		    inst->dp = NULL;
	    }

	    ctx->changed_dp.insert(dp);
	    break;
	default:
	    bu_log("changed callback mode error: %d\n", mode);
    }
}


QgModel::QgModel(QObject *p, const char *npath)
: QAbstractItemModel(p)
{
    // There are commands such as open that we want to work even without
    // a database instance opened - rather than using ged_open to create
    // a gedp, we sure one is always available - callers should use the
    // libged "open" command to open a .g file.
    BU_GET(gedp, struct ged);
    ged_init(gedp);

    // By default we will use this built-in view, to guarantee that ged_gvp is
    // always valid.  It will usually be overridden by application provided views,
    // but this is our hard guarantee that a QgModel will always be able to work
    // with commands needing a view.
    BU_GET(empty_gvp, struct bview);
    bv_init(empty_gvp, &gedp->ged_views);
    gedp->ged_gvp = empty_gvp;
    bu_vls_sprintf(&gedp->ged_gvp->gv_name, "default");
    gedp->ged_gvp->independent = 0;

    // Set up the root item
    rootItem = new QgItem(NULL, this);
    rootItem->ctx = this;

    // Initialize the instance containers
    tops_instances = new std::unordered_map<unsigned long long, gInstance *>;
    tops_instances->clear();
    instances = new std::unordered_map<unsigned long long, gInstance *>;
    instances->clear();

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
	run_cmd(gedp->ged_result_str, ac, (const char **)av);
    }
}

QgModel::~QgModel()
{
    delete items;
    delete tops_instances;
    delete instances;

    bv_free(empty_gvp);
    BU_PUT(empty_gvp, struct bview);
    ged_close(gedp);
    delete rootItem;
}

// Note - this is a private method and must be run from within g_update's
// begin/end RebuildModel block.  Also, it assumes all invalid QgItem pointers
// have been removed from the children arrays. (The ability to recognize an
// invalid QgItem is defined at that level - we don't want to re-create it
// here.)
void
QgModel::item_rebuild(QgItem *item)
{
    // Top level is a special case and is handled separately
    if (item == rootItem) {
	return;
    }

    // If we have no cached children, update only the child count - fetchMore
    // will populate it if and when it is expanded, and there will be no
    // QgItems we need to re-use.  However, an edit operation may have
    // invalidated the original c_count stored when the item was created.
    if (!item->children.size()) {
	std::unordered_map<unsigned long long, gInstance *>::iterator g_it;
	gInstance *g = NULL;
	g_it = instances->find(item->ihash);
	if (g_it != instances->end()) {
	    g = g_it->second;
	    item->c_count = g->children(instances).size();
	} else {
	    item->c_count = 0;
	}
	return;
    }

    // Get the current child instances
    std::vector<unsigned long long> nh = (*instances)[item->ihash]->children(instances);

    // Note that the ordering is important here - when we "pop" off the queue
    // in the next step, we need consistent ordering so repeated fetchMore
    // calls don't end up swapping around QgItems unnecessarily.  Remember -
    // the gInstance hashes are not unique by themselves, so they are not
    // enough to ensure stable ordering.  When we pushed these into the oc
    // containers the wrong way, trying to expand havoc's
    // havoc/havoc_front/nose_skin/r.nos1 made a mess out of the model and
    // display due to multiple instances of s.rad1 subtractions being present.
    std::unordered_map<unsigned long long, std::queue<QgItem *>> oc;
    for (size_t i = 0; i < item->children.size(); i++) {
	QgItem *qii = item->children[i];
	oc[qii->ihash].push(qii);
    }

    // Iterate the gInstance's array of child hashes, building up  the
    // corresponding QgItems array using either the stored QgItems from the
    // previous state or new items.  Remember, all QgItems in a queue
    // correspond to the same gInstance (i.e. their parent, bool op, matrix
    // and child obj are all identical) so if there are multiple QgItems in the
    // same queue it doesn't matter what "order" the QgItems are re-added in as
    // far as the correctness of the tree is concerned.  We are attempting to
    // preserve the "open/closed" state of the items via the queue ordering,
    // but that won't always be possible. "Preserving" an existing open state
    // after a tree change isn't always well defined - if, for example, we have
    // two QgItems that both point to the same instance and were duplicated in
    // the same child list, but only one of them was opened, if the tree
    // rebuild now only has one QgItem being produced you could make the case
    // that either of the previous QgItems was the one being removed.  So if we
    // do hit that duplicate case, the QgItems that are left over in the queues
    // once the new state is built up are the ones removed.  This is arbitrary,
    // but that arbitrariness is an accurate reflection of the underlying data
    // model.
    std::vector<QgItem *> nc;
    for (size_t i = 0; i < nh.size(); i++) {
	// For each new child, look up its instance in the original data to see
	// if we have a corresponding QgItem available.
	if (oc.find(nh[i]) != oc.end() && !oc[nh[i]].empty()) {
	    // We have a viable QgItem from the previous state - reuse it
	    QgItem *itm = oc[nh[i]].front();
	    oc[nh[i]].pop();
	    nc.push_back(itm);
	} else {
	    // Previous tree did not have an appropriate QgItem -
	    // make a new one
	    std::unordered_map<unsigned long long, gInstance *>::iterator g_it;
	    gInstance *g = NULL;
	    g_it = instances->find(nh[i]);
	    if (g_it != instances->end())
		g = g_it->second;
	    QgItem *nitem = new QgItem(g, this);
	    nitem->parentItem = item;
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

    // In case we have opened a completely new .g file, set the callbacks
    if (n_dbip && !BU_PTBL_LEN(&n_dbip->dbi_changed_clbks)) {
	// Primary driver of model updates is when individual objects are changed
	db_add_changed_clbk(n_dbip, &qgmodel_changed_callback, (void *)this);

	// If the tops list changes, we need to update that vector as well.  Unlike
	// local dp changes, we can only (re)build the tops list after an
	// update_nref pass is complete.
	db_add_update_nref_clbk(n_dbip, &qgmodel_update_nref_callback, (void *)this);
    }

    if (!n_dbip) {
	// if we have no dbip, clear out everything
	beginResetModel();
	sync_instances(tops_instances, instances, n_dbip);
	std::unordered_set<QgItem *>::iterator s_it;
	for (s_it = items->begin(); s_it != items->end(); s_it++) {
	    QgItem *itm = *s_it;
	    delete itm;
	}
	items->clear();
	tops_items.clear();
	emit mdl_changed_db((void *)gedp);
	emit layoutChanged();
	changed_db_flag = 0;
	endResetModel();
	return;
    }

    // If we have a dbip and the changed flag is set, figure out what's different
    if (changed_db_flag) {
	beginResetModel();

	// Step 1 - make sure our instances are current - i.e. they match the
	// .g database state
	sync_instances(tops_instances, instances, n_dbip);

	// Clear out any QgItems with invalid info.  We need to be fairly
	// aggressive here - first we find all the existing invalid ones, and
	// then we invalidate all their children recursively - any item with an
	// invalid parent is invalid.
	std::queue<QgItem *> to_clear;
	std::unordered_set<QgItem *> invalid;
	std::unordered_set<QgItem *>::iterator s_it;
	for (s_it = items->begin(); s_it != items->end(); s_it++) {
	    QgItem *itm = *s_it;
	    if (instances->find(itm->ihash) == instances->end()) {
		invalid.insert(itm);
		to_clear.push(itm);
	    }
	}
	while (!to_clear.empty()) {
	    QgItem *i_itm = to_clear.front();
	    to_clear.pop();
	    for (size_t i = 0; i < i_itm->children.size(); i++) {
		QgItem *itm = i_itm->children[i];
		invalid.insert(itm);
		to_clear.push(itm);
	    }
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
	    item_rebuild(i_itm);
	}

	// Validate existing tops QgItems based on the now-updated tops_instances data.
	std::unordered_map<unsigned long long, QgItem *> vtops_items;
	for (size_t i = 0; i < tops_items.size(); i++) {
	    QgItem *titem = tops_items[i];
	    if (tops_instances->find(titem->ihash) != tops_instances->end()) {
		// Still a tops item
		vtops_items[titem->ihash] = titem;
	    }
	}

	// Using tops_instances, construct a new tops vector.  Reuse any still valid
	// QgItems, and make new ones 
	std::vector<QgItem *> ntops_items;
	std::unordered_map<unsigned long long, gInstance *>::iterator i_it;
	for (i_it = tops_instances->begin(); i_it != tops_instances->end(); i_it++) {
	    std::unordered_map<unsigned long long, QgItem *>::iterator v_it;
	    v_it = vtops_items.find(i_it->first);
	    if (v_it != vtops_items.end()) {
		ntops_items.push_back(v_it->second);
	    } else {
		gInstance *ninst = i_it->second;
		QgItem *nitem = new QgItem(ninst, this);
		nitem->parentItem = rootItem;
		ntops_items.push_back(nitem);
		items->insert(nitem);
	    }
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
	    items->erase(iv_itm);
	    delete iv_itm;
	}

	endResetModel();
    }

    // If we did change something, we need to let the application know
    if (changed_db_flag) {
	emit mdl_changed_db((void *)gedp);
	emit layoutChanged();
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


bool
QgModel::canFetchMore(const QModelIndex &idx) const
{
    if (!idx.isValid())
	return false;

    QgItem *item = static_cast<QgItem*>(idx.internalPointer());
    if (item == rootItem)
       	return false;

    // If we are trying to update a QgItem and it references an
    // invalid instance, there's nothing to update.
    if (instances->find(item->ihash) == instances->end()) {
	return false;
    }

    return (*instances)[item->ihash]->has_children();
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

    // We need the QgItem child array to reflect the current state of the comb
    // tree, but we also want to keep the old QgItems to minimize disturbances
    // to the model that aren't necessary due to .g changes. However, unlike
    // the tops case we can't just alphanum sort and compare old to new - we
    // need to use the ordering derived from the comb tree.  Moreover, we have
    // to add new QgItems for new instances *at the right point in the array*
    // to reflect the tree ordering - we can't just tack them on at the end.
    // To make it even more complicated, we don't have a uniqueness guarantee
    // for instance hashes - we may have the same hash in multiple distinct
    // QgItems.
    //
    // To manage this, we make a map of queues and iterate over the old QgItems
    // array, queueing up the old items in a manner that allows us to look them
    // up using their hash and pop the item closest to the "top" of the tree
    // off the queue.  This way, whenever we need a particular item, we are
    // trying to keep the ordering of the childern close to the previous
    // positions.

    // We depend on the instance's set of child hashes being current, either
    // from initialization or from the per-object editing callback.  If this
    // set is not current, we need to fix logic elsewhere to ensure that list
    // IS correct before this logic is called - we're not going to try and fix
    // it here.
    std::vector<unsigned long long> nh = (*instances)[item->ihash]->children(instances);
    std::vector<QgItem *> nc;
    for (size_t i = 0; i < nh.size(); i++) {
	// For each new child, look up its instance in the original data to see
	// if we have a corresponding QgItem available.
	std::unordered_map<unsigned long long, gInstance *>::iterator g_it;
	gInstance *g = NULL;
	g_it = instances->find(nh[i]);
	if (g_it != instances->end())
	    g = g_it->second;
	QgItem *nitem = new QgItem(g, this);
	nitem->parentItem = item;
	nc.push_back(nitem);
	items->insert(nitem);
    }

    // All done - let the Qt model know
    beginInsertRows(idx, 0, nc.size() - 1);
    item->children = nc;
    // define a map for quick QgItem * -> index lookups
    item->c_noderow.clear();
    for (size_t i = 0; i < nc.size(); i++) {
	item->c_noderow[nc[i]] = i;
    }
    endInsertRows();
    emit check_highlights();
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

    return QAbstractItemModel::flags(index);
    //return Qt::ItemIsEditable | QAbstractItemModel::flags(index);
}

QVariant
QgModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
	return QVariant();
    QgItem *qi= getItem(index);
    gInstance *gi = qi->instance();
    if (!gi)
	return QVariant();
    if (role == Qt::DisplayRole)
	return QVariant(gi->dp->d_namep);
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
    model_dbip = gedp->dbip;

    changed_dp.clear();

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    if (ged_cmd_valid(argv[0], NULL)) {
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
    if (ret & BRLCAD_MORE)
	return ret;

    // If we have the need_update_nref flag set, we need to do db_update_nref
    // ourselves - the backend logic made a dp add/remove but didn't trigger
    // the nref updates (can that happen?).
    if (gedp->dbip && need_update_nref) {
	// bu_log("missing callback in librt?\n");
	db_update_nref(gedp->dbip, &rt_uniresource);
    }

    // If we have a new .g file, set the changed flag
    if (model_dbip != gedp->dbip)
	changed_db_flag = 1;

    // Assuming we're not doing a full rebuild, trigger the post-cmd updates
    g_update(gedp->dbip);

    model_dbip = gedp->dbip;

    if (msg && gedp)
	bu_vls_printf(msg, "%s", bu_vls_cstr(gedp->ged_result_str));

    return ret;
}

// Normally the model won't define methods for specific GED commands, but there
// are a few exceptions related to common, standard operations like drawing that
// are potentially triggered from QActions.
int
QgModel::draw()
{
    // https://stackoverflow.com/a/28647342/2037687
    QAction *a = qobject_cast<QAction *>(sender());
    QVariant v = a->data();
    QgItem *cnode  = (QgItem *) v.value<void *>();


    QString cnode_str = cnode->toString();
    const char *inst_path = bu_strdup(cnode_str.toLocal8Bit().data());
    const char *argv[2];
    argv[0] = "draw";
    argv[1] = inst_path;

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);
    int ret = ged_exec(gedp, 2, argv);

    bu_free((void *)inst_path, "inst_path");

    emit view_change(&gedp->ged_gvp);

    return ret;
}

int
QgModel::erase()
{
    // https://stackoverflow.com/a/28647342/2037687
    QAction *a = qobject_cast<QAction *>(sender());
    QVariant v = a->data();
    QgItem *cnode  = (QgItem *) v.value<void *>();


    QString cnode_str = cnode->toString();
    const char *inst_path = bu_strdup(cnode_str.toLocal8Bit().data());
    const char *argv[2];
    argv[0] = "erase";
    argv[1] = inst_path;

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);
    int ret = ged_exec(gedp, 2, argv);

    bu_free((void *)inst_path, "inst_path");

    emit view_change(&gedp->ged_gvp);

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

