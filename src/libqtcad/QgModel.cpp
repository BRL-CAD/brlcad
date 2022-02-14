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

#include <set>
#include <unordered_set>
#include <QFileInfo>

#include "xxhash.h"

#include "bu/env.h"
#include "bu/sort.h"
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
	ihash = g->hash();
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
QgItem::remove_children()
{
    if (!ctx)
	return;
    ctx->remove_children(this);
    children.clear();
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

    std::unordered_map<unsigned long long, gInstance *>::iterator g_it;
    g_it = ctx->instances->find(ihash);
    if (g_it == ctx->instances->end())
	return 0;

    gInstance *gi = g_it->second;

    return (int)gi->children(ctx->instances).size();
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
qgmodel_update_nref_callback(struct db_i *dbip, struct directory *parent_dp, struct directory *child_dp, const char *child_name, db_op_t op, matp_t m, void *u_data)
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

	// Do the major bookkeeping work of an update
	update_tops_instances(ctx->tops_instances, ctx->instances, dbip);
    }
}

extern "C" void
qgmodel_changed_callback(struct db_i *dbip, struct directory *dp, int mode, void *u_data)
{
    std::queue<std::unordered_map<unsigned long long, gInstance *>::iterator> rmq;
    std::unordered_map<unsigned long long, gInstance *>::iterator i_it, trm_it;
    QgModel *ctx = (QgModel *)u_data;
    gInstance *inst = NULL;

    switch(mode) {
	case 0:
	    bu_log("MOD: %s\n", dp->d_namep);

	    // The view needs to regenerate the wireframe(s) for this dp, as
	    // it may have changed
	    ctx->changed_dp.insert(dp);

	    // In theory the librt callbacks should take care of this, and we
	    // may not want to do this for all mods (updates can be
	    // expensive)...  Need would be if a comb tree changes, and if the
	    // backend has that covered we don't want to force the issue here
	    // for something like a sph radius change...
	    // ctx->need_update_nref = true;

	    // Need to handle edits to comb/extrude/revolve/dsp, which have potential
	    // implications for gInstances.
	    update_child_instances(ctx->instances, dp, dbip);

	    ctx->changed_db_flag = 1;
	    break;
	case 1:
	    bu_log("ADD: %s\n", dp->d_namep);

	    // In theory the librt callbacks should take care of this...
	    ctx->need_update_nref = true;

	    // If this is a new tops solid we'll only know to create that
	    // instance after an update_nref pass, but if it's a new comb or
	    // one of the other instance defining primitives create the new
	    // instances now.
	    add_instances(ctx->instances, dp, dbip);

	    // If we have any invalid instances that are now valid, update them
	    for (i_it = ctx->instances->begin(); i_it != ctx->instances->end(); i_it++) {
		inst = i_it->second;
		if (!inst->dp && inst->dp_name == std::string(dp->d_namep)) {
		    inst->dp = dp;
		}
	    }

	    ctx->changed_db_flag = 1;
	    break;
	case 2:
	    bu_log("RM:  %s\n", dp->d_namep);

	    // In theory the librt callbacks should take care of this...
	    ctx->need_update_nref = true;

	    // For removal, we need to 1) remove any instances where the parent is
	    // dp, and 2) invalidate the dps in any instances where they match.
	    for (i_it = ctx->instances->begin(); i_it != ctx->instances->end(); i_it++) {
		inst = i_it->second;
		// NOTE:  tops instances are handled separately in the update_nref callback.
		if (!inst->parent)
		    continue;
		if (inst->dp == dp)
		    inst->dp = NULL;
		if (inst->parent == dp) {
		    rmq.push(i_it);
		}
	    }
	    while (!rmq.empty()) {
		i_it = rmq.front();
		rmq.pop();
		ctx->instances->erase(i_it->first);
	    }

	    ctx->changed_db_flag = 1;
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
    BU_GET(empty_gvp, struct bview);
    bv_init(empty_gvp);
    gedp->ged_gvp = empty_gvp;

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

void
QgModel::reset(struct db_i *n_dbip)
{
    // If the dbip changed, we have a new database and we need to completely
    // reset the model contents.
    // NOTE:  tops objects in the .g file are "instances" beneath the .g
    // itself, which is the analogy to Qt's hidden "root" node in a model.  To
    // handle them, we define "NULL root" instances.

    beginResetModel();

    // Remove old instances and re-initialize containers (clears out the Qt
    // model data.)
    if (items) {
	std::unordered_set<QgItem *>::iterator i_it;
	for (i_it = (*items).begin(); i_it != (*items).end(); i_it++) {
	    QgItem *itm = *i_it;
	    delete itm;
	}
    }
    tops_items.clear();
    changed_dp.clear();

    // Reset and initialize the .g level data (gInstances)
    initialize_instances(tops_instances, instances, n_dbip);

    // Primary driver of model updates is when individual objects are changed
    db_add_changed_clbk(n_dbip, &qgmodel_changed_callback, (void *)this);

    // If the tops list changes, we need to update that vector as well.  Unlike
    // local dp changes, we can only (re)build the tops list after an
    // update_nref pass is complete.
    db_add_update_nref_clbk(n_dbip, &qgmodel_update_nref_callback, (void *)this);

    // tops entries get a QgItem by default
    std::unordered_map<unsigned long long, gInstance *>::iterator top_it;
    for (top_it = tops_instances->begin(); top_it != tops_instances->end(); top_it++) {
	gInstance *ninst = top_it->second;
	QgItem *nitem = new QgItem(ninst, this);
	nitem->parentItem = rootItem;
	tops_items.push_back(nitem);
	items->insert(nitem);
    }

    // Sort tops_items according to alphanum
    std::sort(tops_items.begin(), tops_items.end(), QgItem_cmp());

    // Make tops items children of the rootItem
    rootItem->children.clear();
    for (size_t i = 0; i < tops_items.size(); i++) {
	rootItem->appendChild(tops_items[i]);
    }

    endResetModel();

    // Inform the world the database has changed.
    emit mdl_changed_db((void *)gedp);
}

void
QgModel::g_update()
{
    if (changed_db_flag) {
	beginResetModel();
	update_tops_items();
	endResetModel();
    }
    if (changed_db_flag) {
	emit mdl_changed_db((void *)gedp);
	emit layoutChanged();
    }
    changed_db_flag = 0;
}

void
QgModel::update_tops_items()
{
    std::unordered_map<unsigned long long, gInstance *>::iterator i_it;

    // Build up sets of the old and new tops QgItems, and identify differences.
    // We want to keep as many of the old QgItems unchanged as possible to
    // minimize the difficulties of updating.  We will create a "full" new
    // QgItems tops vector, but the set_difference comparison will be based on
    // comparison of the directory pointers.
    std::vector<QgItem *> modded_tops_items;
    std::vector<QgItem *> removed, added;

    // Build up a "candidate" vector of new QgItems based on the now-updated tops_instances data.
    std::vector<QgItem *> ntops_items;
    for (i_it = tops_instances->begin(); i_it != tops_instances->end(); i_it++) {
	QgItem *nitem = new QgItem(i_it->second, this);
	nitem->parentItem = NULL;
	ntops_items.push_back(nitem);
    }
    std::sort(ntops_items.begin(), ntops_items.end(), QgItem_cmp());

    // Compare the context's current tops_items array with the proposed new array, using QgItem_cmp.
    // Build up sets of both added and removed items.
    std::set_difference(tops_items.begin(), tops_items.end(), ntops_items.begin(), ntops_items.end(), std::back_inserter(removed), QgItem_cmp());
    std::set_difference(ntops_items.begin(), ntops_items.end(), tops_items.begin(), tops_items.end(), std::back_inserter(added), QgItem_cmp());

    // Delete anything in the ntops_items array that we're not adding - the existing tops_items
    // QgItems will be reused in those cases
    std::unordered_set<QgItem *> added_set(added.begin(), added.end());
    for (size_t i = 0; i < ntops_items.size(); i++) {
	if (added_set.find(ntops_items[i]) == added_set.end()) {
	    delete ntops_items[i];
	}
    }

    // Assemble the new composite tops vector, combining the original
    // QgItems that aren't being obsoleted and any new additions
    std::unordered_set<QgItem *> removed_set(removed.begin(), removed.end());
    for (size_t i = 0; i < tops_items.size(); i++) {
	if (removed_set.find(tops_items[i]) == removed_set.end())
	    modded_tops_items.push_back(tops_items[i]);
    }
    for (size_t i = 0; i < added.size(); i++) {
	items->insert(added[i]);
	modded_tops_items.push_back(added[i]);
    }

    // Make sure what will become the final tops list is alphanum sorted
    std::sort(modded_tops_items.begin(), modded_tops_items.end(), QgItem_cmp());

    // TODO - should we just be using the children array of rootItem for this?
    tops_items.clear();
    tops_items = modded_tops_items;

    // Now that we have the tops_items array updated, we're ready to alter
    // the rootItem's children array.  TODO - would it be better to go through
    // and individually remove and add rows?
    rootItem->children.clear();
    for (size_t i = 0; i < tops_items.size(); i++) {
	rootItem->appendChild(tops_items[i]);
    }

    // We've updated the tops array now - delete the old QgItems that were removed.
    for (size_t i = 0; i < removed.size(); i++) {
	items->erase(removed[i]);
	delete removed[i];
    }
}

int
QgModel::NodeRow(QgItem *node) const
{
    if (!node->parent())
	return -1;
    for (size_t i = 0; i < node->parent()->children.size(); i++) {
	if (node->parent()->children[i] == node)
	    return i;
    }
    return -1;
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

    std::vector<unsigned long long> nh = (*instances)[item->ihash]->children(instances);
    if (!nh.size())
	return false;

    return true;
}

void
QgModel::fetchMore(const QModelIndex &idx)
{
    if (!idx.isValid())
	return;

    QgItem *item = static_cast<QgItem*>(idx.internalPointer());

    // TODO - do we rebuild tops in this case?
    if (item == rootItem) {
	bu_log("rootItem fetchMore\n");
	return;
    }

    bu_log("fetchMore: %s\n", item->instance()->dp_name.c_str());

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
	    bu_log("new item: %llu\n", nh[i]);
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

    // Anything still left in the queues is now obsolete - we reused or created
    // above all the QgItems we needed for the current state, and we need to
    // clear out whatever remains (both the items themselves and any populated
    // QgItem subtrees they may have had populated below them, since their
    // parent is being removed.)
    std::unordered_map<unsigned long long, std::queue<QgItem *>>::iterator oc_it;
    for (oc_it = oc.begin(); oc_it != oc.end(); oc_it++) {
	while (!oc_it->second.empty()) {
	    QgItem *itm = oc_it->second.front();
	    oc_it->second.pop();
	    items->erase(itm);
	    delete itm;
	}
    }

    // If anything changed, we need to replace children's contents with the new
    // vector.
    if (nc != item->children) {
	bu_log("fetchMore rebuild: %s\n", item->instance()->dp_name.c_str());
	beginInsertRows(idx, 0, nc.size() - 1);
	item->children = nc;
	endInsertRows();
    }
}

void
QgModel::remove_children(QgItem *itm)
{
    if (!itm)
	return;
    QModelIndex idx = NodeIndex(itm);
    if (itm->children.size()) {
	beginRemoveRows(idx, 0, itm->children.size() - 1);
	itm->children.clear();
	endRemoveRows();
    }
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

    return Qt::ItemIsEditable | QAbstractItemModel::flags(index);
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

bool
QgModel::setData(const QModelIndex &index, const QVariant &UNUSED(value), int UNUSED(role))
{
    if (!index.isValid())
	return false;

    // TODO - study the previous CAD Tree implementation for this - there are a number of
    // highlighting related components that need to be considered.
    return false;
}

bool
QgModel::setHeaderData(int UNUSED(section), Qt::Orientation UNUSED(orientation), const QVariant &UNUSED(value), int UNUSED(role))
{
    return false;
}

bool
QgModel::insertRows(int UNUSED(row), int UNUSED(count), const QModelIndex &UNUSED(parent))
{
    return false;
}

bool
QgModel::removeRows(int UNUSED(row), int UNUSED(count), const QModelIndex &UNUSED(parent))
{
    return false;
}

bool QgModel::insertColumns(int UNUSED(column), int UNUSED(count), const QModelIndex &UNUSED(parent))
{
    return false;
}

bool QgModel::removeColumns(int UNUSED(column), int UNUSED(count), const QModelIndex &UNUSED(parent))
{
    return false;
}

///////////////////////////////////////////////////////////////////////
//                  .g Centric Methods
///////////////////////////////////////////////////////////////////////
int QgModel::run_cmd(struct bu_vls *msg, int argc, const char **argv)
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

    // If we have the need_update_nref flag set, we need to do db_update_nref
    // ourselves - the backend logic made a dp add/remove but didn't trigger
    // the nref updates (can that happen?).
    if (gedp->dbip && need_update_nref) {
	// bu_log("missing callback in librt?\n");
	db_update_nref(gedp->dbip, &rt_uniresource);
    }

    // Assuming we're not doing a full rebuild, trigger the post-cmd updates
    if (gedp->dbip == model_dbip) {
	g_update();
    }

    if (msg && gedp)
	bu_vls_printf(msg, "%s", bu_vls_cstr(gedp->ged_result_str));

    // If it's not a whole new database, we're done
    if (gedp->dbip == model_dbip)
	return ret;

    // If the dbip changed, we have a new database and we need to completely
    // reset the model contents.
    reset(gedp->dbip);

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

