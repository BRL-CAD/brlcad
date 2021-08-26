/*                         Q G M O D E L . C P P
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
/** @file QgModel.cpp
 *
 * QAbstractItemModel of a BRL-CAD .g database.
 *
 *
 * TODO: Replace intermediate instance creation in update_ref callback with a
 * processing step at the end that iterates over the array of all dps twice.
 * The first to get instances for tops objects, and the second to get all
 * immediate children of all combs, sketches, etc.  and make instances from
 * them.  We store these in an unordered map with their hash as the key.  The
 * old and new tops lists become the seed vectors used to check what has changed
 * in the tree.  QgItems will store hashes to their instances, and we can tell
 * via a failed lookup whether a particular instance in the QgItem is still valid.
 *
 * QgItems form the explicit hierarchical representation implied by the QgInstances,
 * which means QgInstance changes have potentially global impacts on the QgItems
 * hierarchy and it must be fully checked for validity (and repaired as appropriate)
 * after each cycle.  A validity check would be something like:  1) check the hash
 * of the QgItem against the QgInstance array to see if it is still a valid instance.
 * If not, it must be either removed or replaced
 *
 * To achieve a minimal-disruption tree update, we literally need a diffing
 * algorithm for the vector holding the child instances of an item.  We have to
 * find the minimal editing operation (insert two items here and shift the rest
 * down, for example) that will move from the old vector to the new, so we can
 * minimize the number of rows changed.  I this this is related to the
 * Levenshtein editing distance sequence needed to translate the original
 * vector into the new, with the insertions, deletions and substitutions being
 * QgInstances instead of characters. We need to actually do the translations,
 * not just calculate the distance, but the idea is similar. 
 */

#include "common.h"

#include <set>
#include <unordered_set>
#include <QFileInfo>

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

#include "bu/env.h"
#include "bu/sort.h"
#include "../libged/alphanum.h"
#include "raytrace.h"
#include "qtcad/QgModel.h"

struct QgItem_cmp {
    inline bool operator() (const QgItem *i1, const QgItem *i2)
    {
	if (!i1 && i2)
	    return true;
	if (i1 && !i2)
	    return false;
	if (!i1->ihash && i2->ihash)
	    return true;
	if (i1->ihash && !i2->ihash)
	    return false;

	QgInstance *inst1 = (*i1->ctx->instances)[i1->ihash];
	QgInstance *inst2 = (*i2->ctx->instances)[i2->ihash];
	const char *n1 = inst1->dp_name.c_str();
	const char *n2 = inst2->dp_name.c_str();
	if (alphanum_impl(n1, n2, NULL) < 0)
	    return true;
	return false;
    }
};

static void
qg_instance(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, void *pdp, void *vctx, void *vchash, void *)
{
    // Validate
    if (comb) RT_CK_COMB(comb);
    RT_CK_TREE(comb_leaf);

    // Unpack
    QgModel_ctx *ctx = (QgModel_ctx *)vctx;
    struct directory *parent_dp = (struct directory *)pdp;
    std::vector<unsigned long long> *chash = (std::vector<unsigned long long> *)vchash;

    // Translate the op into the db_op_t type
    struct directory *dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET);
    db_op_t op = DB_OP_UNION;
    if (comb_leaf->tr_l.tl_op == OP_SUBTRACT) {
	op = DB_OP_SUBTRACT;
    }
    if (comb_leaf->tr_l.tl_op == OP_INTERSECT) {
	op = DB_OP_INTERSECT;
    }

    // We need to do the lookup to see if we already have this instance stored
    // - to do the lookup we need the hash, so create a (potentially) temporary
    // version of the QgInstance and generate it.
    QgInstance *ninst = new QgInstance();
    ninst->ctx = ctx;
    ninst->parent = parent_dp;
    ninst->dp = dp;
    ninst->dp_name = std::string(comb_leaf->tr_l.tl_name);
    if (comb_leaf->tr_l.tl_mat) {
	MAT_COPY(ninst->c_m, comb_leaf->tr_l.tl_mat);
    } else {
	MAT_IDN(ninst->c_m);
    }
    ninst->op = op;
    unsigned long long nhash = ninst->hash();

    // See if we already have this or not.  If not, add it.
    if (ctx->instances->find(nhash) == ctx->instances->end()) {
	(*ctx->instances)[nhash] = ninst;
    } else {
	// Already got it, don't need this copy
	delete ninst;
    }

    // If we're collecting child hashes of the dp, push the hash onto the
    // vector to denote it as a child of the parent.
    if (chash)
	(*chash).push_back(nhash);
}

static int
dp_cmp(const void *d1, const void *d2, void *UNUSED(arg))
{
    struct directory *dp1 = *(struct directory **)d1;
    struct directory *dp2 = *(struct directory **)d2;
    return bu_strcmp(dp1->d_namep, dp2->d_namep);
}

bool DpCmp(const struct directory *p1, const struct directory *p2)
{
    return (bu_strcmp(p1->d_namep, p2->d_namep) < 0);
}

QgInstance::QgInstance()
{
    h_state = XXH64_createState();
    if (h_state)
	XXH64_reset(h_state, 0);
}

QgInstance::~QgInstance()
{
    XXH64_freeState(h_state);
}

std::string
QgInstance::print()
{
    std::string s;
    if (parent) {
	s.append(std::string(parent->d_namep));
	s.append(std::string(" ->"));
    }
    switch (op) {
	case DB_OP_UNION:
	    s.append(std::string(" u "));
	    break;
	case DB_OP_SUBTRACT:
	    s.append(std::string(" - "));
	    break;
	case DB_OP_INTERSECT:
	    s.append(std::string(" + "));
	    break;
	default:
	    bu_log("Warning - unknown op");
	    break;
    }
    if (!bn_mat_is_identity(c_m)) {
	s.append(std::string("[M]"));
    }
    s.append(dp_name);
    if (!dp) {
	s.append(std::string("[missing]"));
    }

    return s;
}

unsigned long long
QgInstance::hash(int mode)
{
    if (!h_state)
	return 0;

    XXH64_hash_t hash_val;

    if (parent)
	XXH64_update(h_state, parent->d_namep, strlen(parent->d_namep));
    // We use dp_name instead of the dp because we want the same hash
    // whether we have a dp or an invalid comb entry - the hash lookup
    // should return both with the same key.
    if (dp_name.length())
	XXH64_update(h_state, dp_name.c_str(), dp_name.length());

    int int_op = 0;
    switch (op) {
	case DB_OP_UNION:
	    int_op = 1;
	    break;
	case DB_OP_SUBTRACT:
	    int_op = 2;
	    break;
	case DB_OP_INTERSECT:
	    int_op = 3;
	    break;
	default:
	    int_op = 0;
	    break;
    }
    if (mode == 1 || mode == 3)
	XXH64_update(h_state, &int_op, sizeof(int));

    if (mode == 2 || mode == 3)
	XXH64_update(h_state, c_m, sizeof(mat_t));

    hash_val = XXH64_digest(h_state);
    XXH64_reset(h_state, 0);

    return (unsigned long long)hash_val;
}

std::vector<unsigned long long>
QgInstance::children()
{
    std::vector<unsigned long long> chash;
    struct db_i *dbip = ctx->gedp->ged_wdbp->dbip;
    if (!dp)
	return chash;

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return chash;
	if (intern.idb_type != ID_COMBINATION) {
	    bu_log("NOTICE: %s was marked a combination, but isn't one?  Clearing flag\n", dp->d_namep);
	    dp->d_flags &= ~RT_DIR_COMB;
	    rt_db_free_internal(&intern);
	    return chash;
	}

	struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
	db_tree_funcleaf(dbip, comb, comb->tree, qg_instance, (void *)dp, (void *)ctx, (void *)&chash, NULL);
	rt_db_free_internal(&intern);
    }

    return chash;
}

QgItem::QgItem()
{
}

QgItem::~QgItem()
{
}

void
QgItem::open()
{
    if (!ctx)
	return;
    if (ctx->instances->find(ihash) == ctx->instances->end()) {
	bu_log("Invalid ihash\n");
	return;
    }

    open_itm = true;
    update_children();
}


void
QgItem::close()
{
    open_itm = false;
}


bool
QgItem::update_children()
{
    if (!ctx || !open_itm)
	return false;
    if (ctx->instances->find(ihash) == ctx->instances->end()) {
	bu_log("Invalid ihash in child\n");
	return false;
    }
    std::cout << "Update child " << ihash << "\n";

    // Now the tricky part - we need the QgItem child array to reflect the
    // current state of the comb tree, but we also want to keep the old QgItems
    // to minimize disturbances to the model that aren't necessary due to .g
    // changes. However, unlike the tops case we can't just alphanum sort and
    // compare old to new - we need to use the ordering derived from the comb
    // tree.  Moreover, we have to add new QgItems for new instances *at the
    // right point in the array* to reflect the tree ordering - we can't just
    // tack them on at the end.  To make it even more complicated, we don't
    // have a uniqueness guarantee for instance hashes - we may have the same
    // hash in multiple distinct QgItems.
    //
    // To manage this, we make a map of queues and iterate over the old QgItems
    // array, queueing up the old items in a manner that allows us to look them
    // up using their hash and pop the item closest to the "top" of the tree
    // off the queue.  This way, whenever we need a particular item, we are
    // trying to minimize its change in the child ordering relative to its
    // previous position.

    // We need to compare the hash sets of the current instance and the
    // (potentially) not current items array.  We also want to reuse as
    // many QgItems as possible, .
    std::vector<unsigned long long> nh = (*ctx->instances)[ihash]->children();

    // Since we will be popping items off the queues, to get a similar order to
    // the original usage when we extract them we go backwards here and push
    // the "last" child items onto the queue first.
    std::unordered_map<unsigned long long, std::queue<QgItem *>> oc;
    for (int i = childCount()-1; i >= 0; i--) {
	QgItem *qii = child(i);
	oc[qii->ihash].push(qii);
    }

    // Iterate the QgInstance's array of hashes, building up a QgItems array
    // using either the stored QgItems from the previous state or new items.
    std::vector<QgItem *> nc;
    for (size_t i = 0; i < nh.size(); i++) {
	if (oc.find(nh[i]) != oc.end() && !oc[nh[i]].empty()) {
	    QgItem *itm = oc[nh[i]].front();
	    oc[nh[i]].pop();
	    nc.push_back(itm);
	} else {
	    QgItem *nitem = new QgItem();
	    nitem->parent = this;
	    nitem->ihash = nh[i];
	    nitem->ctx = ctx;
	    nc.push_back(nitem);
	}
    }

    // Anything still left in the queues is now obsolete
    std::unordered_map<unsigned long long, std::queue<QgItem *>>::iterator oc_it;
    for (oc_it = oc.begin(); oc_it != oc.end(); oc_it++) {
	while (!oc_it->second.empty()) {
	    QgItem *itm = oc_it->second.front();
	    oc_it->second.pop();
	    itm->remove_children();
	    delete itm;
	}
    }

    // If any prior existing children were open, propagate down the tree
    for (size_t i = 0; i < nc.size(); i++) {
	QgItem *itm = nc[i];
	itm->update_children();
    }

    // TODO - do set differences on old and new children arrays.  If nothing changed,
    // we can skip the assignment and return false.
    children.clear();
    children = nc;

    return true;
}

void
QgItem::remove_children()
{
}

void
QgItem::appendChild(QgItem *c)
{
    children.push_back(c);
}

QgItem *
QgItem::child(int n)
{
    if (n < 0 || n > (int)children.size())
	return NULL;

    return children[n];
}

int
QgItem::childCount() const
{
    return (int)children.size();
}

// 0 = exact, 1 = name + op, 2 = name + mat, 3 = name only, -1 name mismatch
int
qgitem_cmp_score(QgItem *i1, QgItem *i2)
{
    int ret = -1;
    if (!i1 || !i2 || !i1->ihash || !i2->ihash)
	return ret;

    QgInstance *inst1 = (*i1->ctx->instances)[i1->ihash];
    QgInstance *inst2 = (*i2->ctx->instances)[i2->ihash];
    if (inst1->dp_name != inst2->dp_name)
	return ret;

    // Names match
    ret = 3;

    // Look for a matrix match
    struct bn_tol mtol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 };
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
    if (!parent_dp && !child_dp && !child_name && m == NULL && op == DB_OP_SUBTRACT) {
	std::unordered_map<unsigned long long, QgInstance *>::iterator i_it;

	std::cout << "update nref callback\n";

	// Cycle complete, nref count is current.  Start analyzing.
	QgModel_ctx *ctx = (QgModel_ctx *)u_data;

#if 0
	if (!ctx->mdl) {
	    // If we don't have a model there's not much we can do...
	    return;
	}
#endif

	// 1.  Rebuild top instances
	{
	    for (i_it = ctx->tops_instances->begin(); i_it != ctx->tops_instances->end(); i_it++) {
		QgInstance *t = i_it->second;
		delete t;
		ctx->instances->erase(i_it->first);
	    }
	    ctx->tops_instances->clear();

	    struct directory **db_objects = NULL;
	    int path_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &db_objects);

	    if (path_cnt) {

		// Sort so the top level ordering is correct for listing in tree views
		bu_sort(db_objects, path_cnt, sizeof(struct directory *), dp_cmp, NULL);

		for (int i = 0; i < path_cnt; i++) {
		    struct directory *curr_dp = db_objects[i];
		    QgInstance *ninst = new QgInstance();
		    ninst->ctx = ctx;
		    ninst->parent = NULL;
		    ninst->dp = curr_dp;
		    ninst->dp_name = std::string(curr_dp->d_namep);
		    ninst->op = DB_OP_UNION;
		    MAT_IDN(ninst->c_m);
		    unsigned long long nhash = ninst->hash();
		    (*ctx->tops_instances)[nhash] = ninst;
		    (*ctx->instances)[nhash] = ninst;
		}
	    }
	    bu_free(db_objects, "tops obj list");
	}

	// 2.  Build up sets of the old and new tops QgItems, and identify differences.
	// We want to keep as many of the old QgItems unchanged as possible to minimize
	// the difficulties of updating.  We will create a "full" new QgItems tops vector,
	// but the set_difference comparison will be based on comparison of the
	// directory pointers.
	std::vector<QgItem *> ntops_items;
	for (i_it = ctx->tops_instances->begin(); i_it != ctx->tops_instances->end(); i_it++) {
	    QgItem *nitem = new QgItem();
	    nitem->parent = NULL;
	    nitem->ihash = i_it->first;
	    nitem->ctx = ctx;
	    ntops_items.push_back(nitem);
	}
	std::sort(ntops_items.begin(), ntops_items.end(), QgItem_cmp());
	std::vector<QgItem *> removed, added;
	std::set_difference(ctx->tops_items.begin(), ctx->tops_items.end(), ntops_items.begin(), ntops_items.end(), std::back_inserter(removed), QgItem_cmp());
	std::set_difference(ntops_items.begin(), ntops_items.end(), ctx->tops_items.begin(), ctx->tops_items.end(), std::back_inserter(added), QgItem_cmp());

	// Delete anything in the ntops_items array that we're not adding
	std::unordered_set<QgItem *> added_set(added.begin(), added.end());
	for (size_t i = 0; i < ntops_items.size(); i++) {
	    if (added_set.find(ntops_items[i]) == added_set.end())
		delete ntops_items[i];
	}

	// Assemble the new tops vector, combining the original old items that
	// aren't being removed and the new additions
	std::vector<QgItem *> modded_tops_items;
	std::unordered_set<QgItem *> removed_set(removed.begin(), removed.end());
	for (size_t i = 0; i < ctx->tops_items.size(); i++) {
	    QgItem *itm = ctx->tops_items[i];
	    if (removed_set.find(itm) == removed_set.end()) {
		modded_tops_items.push_back(itm);
	    } else {
		std::cout << "removed: " << itm->ihash << "\n";
	    }
	}
	for (size_t i = 0; i < added.size(); i++) {
	    std::cout << "added: " << added[i]->ihash << "\n";
	    modded_tops_items.push_back(added[i]);
	}
	std::sort(modded_tops_items.begin(), modded_tops_items.end(), QgItem_cmp());

	// TODO - this is probably about where we would need to start doing the
	// proper steps for updating a Qt model (see Qt docs).  Up until this step
	// we've not been modding the QtItem data exposed to the model - once we
	// update the tops list and start walking any open children to validate
	// them, we're modding the Qt data
	ctx->tops_items.clear();
	ctx->tops_items = modded_tops_items;

	// We've updated the tops array now - delete the old QgItems that were removed.
	for (size_t i = 0; i < removed.size(); i++) {
	    delete removed[i];
	}


	// 3.  With the tops array updated, now we must walk the child items and
	// find any that require updating vs. their parent instances.
	for (size_t i = 0; i < ctx->tops_items.size(); i++) {
	    QgItem *itm = ctx->tops_items[i];
	    itm->update_children();
	}



	// Activity flag values are selection based, but even if the
	// selection hasn't changed since the last cycle the parent/child
	// relationships may have, which can invalidate previously active
	// dp flags.  We clear all flags, to present a clean slate when the
	// selection is re-applied.
	for (size_t i = 0; i < ctx->active_flags.size(); i++) {
	    ctx->active_flags[i] = 0;
	}
    }
}

extern "C" void
qgmodel_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data)
{
    std::queue<std::unordered_map<unsigned long long, QgInstance *>::iterator> rmq;
    std::unordered_map<unsigned long long, QgInstance *>::iterator i_it, trm_it;
    QgModel_ctx *ctx = (QgModel_ctx *)u_data;
    QgInstance *inst = NULL;

    switch(mode) {
	case 0:
	    bu_log("MOD: %s\n", dp->d_namep);

	    // TODO - don't know if we'll actually need this in the end...
	    if (ctx->mdl)
		ctx->mdl->changed_dp.insert(dp);

	    // Need to handle edits to comb/extrude/revolve/dsp, which have potential
	    // implications for QgInstances.
	    ctx->update_instances(dp);

	    break;
	case 1:
	    bu_log("ADD: %s\n", dp->d_namep);

	    // If this is a new tops solid we'll only know to create that
	    // instance after an update_nref pass, but if it's a new comb or
	    // one of the other instance defining primitives create the new
	    // instances now.
	    ctx->add_instances(dp);

	    // TODO - don't know if we'll actually need this in the end...
	    if (ctx->mdl) {
		ctx->mdl->need_update_nref = true;
	    }

	    // If we have any invalid instances that are now valid, update them
	    for (i_it = ctx->instances->begin(); i_it != ctx->instances->end(); i_it++) {
		inst = i_it->second;
		if (!inst->dp && inst->dp_name == std::string(dp->d_namep)) {
		    inst->dp = dp;
		}
	    }

	    break;
	case 2:
	    bu_log("RM:  %s\n", dp->d_namep);

	    // For removal, we need to 1) remove any instances where the parent is
	    // dp, and 2) invalidate the dps in any instances where they match.
	    //
	    // NOTE:  we'll need to make sure QgItems can sort out removed QgInstances -
	    // current thought is a failed lookup of the hash will be workable, but
	    // we may need to have the QgInstance directly track associated QgItems if
	    // that is too slow...
	    for (i_it = ctx->instances->begin(); i_it != ctx->instances->end(); i_it++) {
		inst = i_it->second;
		if (inst->dp == dp)
		    inst->dp = NULL;
		if (inst->parent == dp) {
		    rmq.push(i_it);
		    delete inst;
		}
	    }
	    while (!rmq.empty()) {
		i_it = rmq.front();
		rmq.pop();
		ctx->instances->erase(i_it);
	    }

	    // If the object is being removed, we know for sure it's not going to be
	    // a tops instance any longer...
	    trm_it = ctx->tops_instances->end();
	    for (i_it = ctx->tops_instances->begin(); i_it != ctx->tops_instances->end(); i_it++) {
		inst = i_it->second;
		if (inst->dp_name == std::string(dp->d_namep)) {
		    trm_it = i_it;
		    delete inst;
		    break;
		}
	    }
	    if (trm_it != ctx->tops_instances->end()) {
		ctx->tops_instances->erase(trm_it);
	    }

	    // TODO - don't know if we'll actually need this in the end...
	    if (ctx->mdl)
		ctx->mdl->need_update_nref = true;

	    // TODO: QgItems also need to be updated, not sure yet whether
	    // it's better do that here or in update_nref callback.  Leaning
	    // towards the latter, since update_nref will have to be called after
	    // an addition or removal for a tree view to work anyway and if a
	    // command does a bunch of individual removals we don't want to slow
	    // the UI by doing huge numbers of intra-command updates.

	    break;
	default:
	    bu_log("changed callback mode error: %d\n", mode);
    }
}


QgModel_ctx::QgModel_ctx(QgModel *pmdl, struct ged *ngedp)
    : mdl(pmdl), gedp(ngedp)
{
    if (!gedp)
	return;

    struct db_i *dbip = gedp->ged_wdbp->dbip;

    // Primary driver of model updates is when individual objects are changed
    db_add_changed_clbk(dbip, &qgmodel_changed_callback, (void *)this);

    // If the tops list changes, we need to update that vector as well.  Unlike
    // local dp changes, we can only (re)build the tops list after an
    // update_nref pass is complete.
    db_add_update_nref_clbk(dbip, &qgmodel_update_nref_callback, (void *)this);

    // Initialize the instance containers
    tops_instances = new std::unordered_map<unsigned long long, QgInstance *>;
    tops_instances->clear();
    instances = new std::unordered_map<unsigned long long, QgInstance *>;
    instances->clear();

    // tops objects in the .g file are "instances" beneath the .g itself, which
    // is the analogy to Qt's hidden "root" node in a model.  To handle them,
    // we define "NULL root" instances.
    struct directory **db_objects = NULL;
    int path_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &db_objects);

    if (path_cnt) {

	// Sort so the top level ordering is correct for listing in tree views
	bu_sort(db_objects, path_cnt, sizeof(struct directory *), dp_cmp, NULL);

	for (int i = 0; i < path_cnt; i++) {
	    struct directory *curr_dp = db_objects[i];
	    QgInstance *ninst = new QgInstance();
	    ninst->ctx = this;
	    ninst->parent = NULL;
	    ninst->dp = curr_dp;
	    ninst->dp_name = std::string(curr_dp->d_namep);
	    ninst->op = DB_OP_UNION;
	    MAT_IDN(ninst->c_m);
	    unsigned long long nhash = ninst->hash();

	    // We store the tops instances in both maps so we can always use
	    // instances to look up any QgInstance.  When we rebuild tops_instances
	    // we must be sure to remove the obsolete QgInstances from instances
	    // as well as tops_instances.
	    (*tops_instances)[nhash] = ninst;
	    (*instances)[nhash] = ninst;

	    // tops entries get a QgItem by default
	    QgItem *nitem = new QgItem();
	    nitem->parent = NULL;
	    nitem->ihash = nhash;
	    nitem->ctx = this;
	    tops_items.push_back(nitem);
	}
    }
    bu_free(db_objects, "tops obj list");

    // Sort tops_items according to alphanum
    std::sort(tops_items.begin(), tops_items.end(), QgItem_cmp());


    // TODO - need to decide what to do about cyclic paths...


    // Run through the objects and crack the non-leaf objects to define
    // non-tops instances (i.e. all comb instances and some primitive
    // types that reference other objects).
    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    add_instances(dp);
	}
    }

}

QgModel_ctx::~QgModel_ctx()
{
    if (gedp)
	ged_close(gedp);

    std::unordered_map<unsigned long long, QgInstance *>::iterator i_it;
    for (i_it = (*instances).begin(); i_it != (*instances).end(); i_it++) {
	QgInstance *inst = i_it->second;
	delete inst;
    }

    delete instances;
    delete tops_instances;
}

void
QgModel_ctx::add_instances(struct directory *dp)
{
    struct db_i *dbip = gedp->ged_wdbp->dbip;

    if (dp->d_flags & RT_DIR_HIDDEN)
	return;

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return;
	struct rt_extrude_internal *extr = (struct rt_extrude_internal *)intern.idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extr);
	if (extr->sketch_name) {
	    QgInstance *ninst = new QgInstance();
	    ninst->ctx = this;
	    ninst->parent = dp;
	    ninst->dp = db_lookup(dbip, extr->sketch_name, LOOKUP_QUIET);
	    ninst->dp_name = std::string(extr->sketch_name);
	    MAT_IDN(ninst->c_m);
	    ninst->op = DB_OP_UNION;
	    unsigned long long nhash = ninst->hash();
	    if (instances->find(nhash) != instances->end()) {
		delete ninst;
		rt_db_free_internal(&intern);
		return;
	    }
	    (*instances)[nhash] = ninst;
	}
	rt_db_free_internal(&intern);
	return;
    }

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return;
	struct rt_revolve_internal *revolve = (struct rt_revolve_internal *)intern.idb_ptr;
	RT_REVOLVE_CK_MAGIC(revolve);
	if (bu_vls_strlen(&revolve->sketch_name) > 0) {
	    QgInstance *ninst = new QgInstance();
	    ninst->ctx = this;
	    ninst->parent = dp;
	    ninst->dp = db_lookup(dbip, bu_vls_cstr(&revolve->sketch_name), LOOKUP_QUIET);
	    ninst->dp_name = std::string(bu_vls_cstr(&revolve->sketch_name));
	    MAT_IDN(ninst->c_m);
	    ninst->op = DB_OP_UNION;
	    unsigned long long nhash = ninst->hash();
	    if (instances->find(nhash) != instances->end()) {
		delete ninst;
		rt_db_free_internal(&intern);
		return;
	    }
	    (*instances)[nhash] = ninst;
	}
	rt_db_free_internal(&intern);
	return;
    }

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return;
	struct rt_dsp_internal *dsp = (struct rt_dsp_internal *)intern.idb_ptr;
	RT_DSP_CK_MAGIC(dsp);
	if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ && bu_vls_strlen(&dsp->dsp_name) > 0) {
	    QgInstance *ninst = new QgInstance();
	    ninst->ctx = this;
	    ninst->parent = dp;
	    ninst->dp = db_lookup(dbip, bu_vls_cstr(&dsp->dsp_name), LOOKUP_QUIET);
	    ninst->dp_name = std::string(bu_vls_cstr(&dsp->dsp_name));
	    MAT_IDN(ninst->c_m);
	    ninst->op = DB_OP_UNION;
	    unsigned long long nhash = ninst->hash();
	    if (instances->find(nhash) != instances->end()) {
		delete ninst;
		rt_db_free_internal(&intern);
		return;
	    }
	    (*instances)[nhash] = ninst;
	}
	rt_db_free_internal(&intern);
	return;
    }

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return;
	if (intern.idb_type != ID_COMBINATION) {
	    bu_log("NOTICE: %s was marked a combination, but isn't one?  Clearing flag\n", dp->d_namep);
	    dp->d_flags &= ~RT_DIR_COMB;
	    rt_db_free_internal(&intern);
	    return;
	}

	struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
	db_tree_funcleaf(dbip, comb, comb->tree, qg_instance, (void *)dp, (void *)this, NULL, NULL);
	rt_db_free_internal(&intern);
	return;
    }

}

void
QgModel_ctx::update_instances(struct directory *dp)
{
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    std::unordered_map<unsigned long long, QgInstance *>::iterator i_it;
    QgInstance *inst = NULL;

    if (dp->d_flags & RT_DIR_HIDDEN)
	return;


    // Non-comb cases are simpler - there is at most one
    // instance below such objects.
    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_COMBINATION) {

	unsigned long long ohash = 0;
	for (i_it = instances->begin(); i_it != instances->end(); i_it++) {
	    inst = i_it->second;
	    if (inst->parent == dp) {
		ohash = i_it->first;
		break;
	    }
	}

	if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
	    struct rt_db_internal intern;
	    RT_DB_INTERNAL_INIT(&intern);
	    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
		return;
	    struct rt_extrude_internal *extr = (struct rt_extrude_internal *)intern.idb_ptr;
	    RT_EXTRUDE_CK_MAGIC(extr);
	    if (extr->sketch_name) {
		QgInstance *ninst = new QgInstance();
		ninst->ctx = this;
		ninst->parent = dp;
		ninst->dp = db_lookup(dbip, extr->sketch_name, LOOKUP_QUIET);
		ninst->dp_name = std::string(extr->sketch_name);
		MAT_IDN(ninst->c_m);
		ninst->op = DB_OP_UNION;
		unsigned long long nhash = ninst->hash();
		if (nhash != ohash) {
		    // Old and new hashes do not match - mod changed contents.
		    // Remove old instance, add new.
		    if (ohash)
			(*instances).erase(ohash);
		    (*instances)[nhash] = ninst;
		    return;
		} else {
		    // Same - no action needed
		    delete ninst;
		}
	    } else {
		// There was an instance, but now there isn't a sketch name.
		// Just remove the old instance.
		if (ohash)
		    (*instances).erase(i_it->first);
	    }
	    rt_db_free_internal(&intern);
	    return;
	}

	if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE) {
	    struct rt_db_internal intern;
	    RT_DB_INTERNAL_INIT(&intern);
	    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
		return;
	    struct rt_revolve_internal *revolve = (struct rt_revolve_internal *)intern.idb_ptr;
	    RT_REVOLVE_CK_MAGIC(revolve);
	    if (bu_vls_strlen(&revolve->sketch_name) > 0) {
		QgInstance *ninst = new QgInstance();
		ninst->ctx = this;
		ninst->parent = dp;
		ninst->dp = db_lookup(dbip, bu_vls_cstr(&revolve->sketch_name), LOOKUP_QUIET);
		ninst->dp_name = std::string(bu_vls_cstr(&revolve->sketch_name));
		MAT_IDN(ninst->c_m);
		ninst->op = DB_OP_UNION;
		unsigned long long nhash = ninst->hash();
		if (nhash != ohash) {
		    // Old and new hashes do not match - mod changed contents.
		    // Remove old instance, add new.
		    if (ohash)
			(*instances).erase(ohash);
		    (*instances)[nhash] = ninst;
		    return;
		} else {
		    // Same - no action needed
		    delete ninst;
		}
	    } else {
		// There was an instance, but now there isn't a sketch name.
		// Just remove the old instance.
		if (ohash)
		    (*instances).erase(ohash);
	    }
	    rt_db_free_internal(&intern);
	    return;
	}

	if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP) {
	    struct rt_db_internal intern;
	    RT_DB_INTERNAL_INIT(&intern);
	    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
		return;
	    struct rt_dsp_internal *dsp = (struct rt_dsp_internal *)intern.idb_ptr;
	    RT_DSP_CK_MAGIC(dsp);
	    if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ && bu_vls_strlen(&dsp->dsp_name) > 0) {
		QgInstance *ninst = new QgInstance();
		ninst->ctx = this;
		ninst->parent = dp;
		ninst->dp = db_lookup(dbip, bu_vls_cstr(&dsp->dsp_name), LOOKUP_QUIET);
		ninst->dp_name = std::string(bu_vls_cstr(&dsp->dsp_name));
		MAT_IDN(ninst->c_m);
		ninst->op = DB_OP_UNION;
		unsigned long long nhash = ninst->hash();
		if (nhash != ohash) {
		    // Old and new hashes do not match - mod changed contents.
		    // Remove old instance, add new.
		    if (ohash)
			(*instances).erase(ohash);
		    (*instances)[nhash] = ninst;
		    return;
		} else {
		    // Same - no action needed
		    delete ninst;
		}
	    } else {
		// There was an instance, but now there isn't a sketch name.
		// Just remove the old instance.
		if (ohash)
		    (*instances).erase(ohash);
	    }

	    rt_db_free_internal(&intern);
	    return;
	}

	return;
    }


    // Combs are a little different.  Unlike the rest, we have (potentially)
    // multiple instances derived from a comb.
    std::set<unsigned long long> ohash;
    for (i_it = instances->begin(); i_it != instances->end(); i_it++) {
	inst = i_it->second;
	if (inst->parent == dp) {
	    ohash.insert(i_it->first);
	}
    }

    // Add any new instances (the qg_instance routine will avoid making
    // duplicates, but it will not clear out the old instances.)
    std::vector<unsigned long long> chashv;
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	return;
    if (intern.idb_type != ID_COMBINATION) {
	bu_log("NOTICE: %s was marked a combination, but isn't one?  Clearing flag\n", dp->d_namep);
	dp->d_flags &= ~RT_DIR_COMB;
	rt_db_free_internal(&intern);
	return;
    }
    struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
    db_tree_funcleaf(dbip, comb, comb->tree, qg_instance, (void *)dp, (void *)this, (void *)&chashv, NULL);
    rt_db_free_internal(&intern);

    // To clear out the old instances, we need to see if ohash contains anything
    // that chashv does not - if so those need to go, because they represent comb
    // structures no longer present in the tree.
    std::set<unsigned long long> chash(chashv.begin(), chashv.end());
    std::vector<unsigned long long> removed;
    std::set_difference(ohash.begin(), ohash.end(), chash.begin(), chash.end(), std::back_inserter(removed));
    for (size_t i = 0; i < removed.size(); i++) {
	inst = (*instances)[removed[i]];
	delete inst;
	instances->erase(removed[i]);
    }

}


QgModel::QgModel(QObject *, struct ged *ngedp)
{
    ctx = new QgModel_ctx(this, ngedp);
}

QgModel::~QgModel()
{
    delete ctx;
}

///////////////////////////////////////////////////////////////////////
//          Qt abstract model interface implementation
///////////////////////////////////////////////////////////////////////

QModelIndex
QgModel::index(int , int , const QModelIndex &) const
{
    return QModelIndex();
}

QModelIndex
QgModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

Qt::ItemFlags
QgModel::flags(const QModelIndex &UNUSED(index)) const
{
    return Qt::NoItemFlags;
}

QVariant
QgModel::data(const QModelIndex &UNUSED(index), int UNUSED(role)) const
{
    return QVariant();
}

QVariant
QgModel::headerData(int UNUSED(section), Qt::Orientation UNUSED(orientation), int UNUSED(role)) const
{
    return QVariant();
}

int
QgModel::rowCount(const QModelIndex &UNUSED(p)) const
{
    return 0;
}

int
QgModel::columnCount(const QModelIndex &UNUSED(p)) const
{
    return 0;
}

bool
QgModel::setData(const QModelIndex &UNUSED(index), const QVariant &UNUSED(value), int UNUSED(role))
{
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
bool QgModel::run_cmd(struct bu_vls *msg, int argc, const char **argv)
{
    if (!ctx || !ctx->gedp)
	return false;

    changed_dp.clear();

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    if (ged_cmd_valid(argv[0], NULL)) {
	const char *ccmd = NULL;
	int edist = ged_cmd_lookup(&ccmd, argv[0]);
	if (edist) {
	    if (msg)
		bu_vls_sprintf(msg, "Command %s not found, did you mean %s (edit distance %d)?\n", argv[0],   ccmd, edist);
	}
	return false;
    }

    ged_exec(ctx->gedp, argc, argv);
    if (msg && ctx->gedp)
	bu_vls_printf(msg, "%s", bu_vls_cstr(ctx->gedp->ged_result_str));

    // If we have the need_update_nref flag set, we need to do db_update_nref
    // ourselves - the backend logic made a dp add/remove but didn't do the
    // nref updates.
    if (ctx->gedp && need_update_nref)
	db_update_nref(ctx->gedp->ged_wdbp->dbip, &rt_uniresource);

    return true;
}

void
QgModel::closedb()
{
    // Close an old context, if we have one
    if (ctx)
	delete ctx;

    // Clear changed dps - we're starting over
    changed_dp.clear();
}

int
QgModel::opendb(QString filename)
{
    /* First, make sure the file is actually there */
      QFileInfo fileinfo(filename);
      if (!fileinfo.exists()) return 1;

      closedb();

      char *npath = bu_strdup(filename.toLocal8Bit().data());
      struct ged *n = ged_open("db", (const char *)npath, 1);

      if (n) {
	  db_update_nref(n->ged_wdbp->dbip, &rt_uniresource);
	  ctx = new QgModel_ctx(this, n);
      }
      bu_free(npath, "path");

      return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

