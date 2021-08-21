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

#include <QFileInfo>

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

#include "bu/env.h"
#include "bu/sort.h"
#include "raytrace.h"
#include "qtcad/QgModel.h"

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


QgItem::QgItem()
{
}

QgItem::~QgItem()
{
}

// 0 = exact, 1 = name + op, 2 = name + mat, 3 = name only, -1 name mismatch
int
qgitem_cmp_score(QgItem *i1, QgItem *i2)
{
    int ret = -1;
    if (!i1 || !i2 || !i1->inst || !i2->inst)
	return ret;

    if (i1->inst->dp_name != i2->inst->dp_name)
	return ret;

    // Names match
    ret = 3;

    // Look for a matrix match
    struct bn_tol mtol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 };
    if (bn_mat_is_equal(i1->inst->c_m, i2->inst->c_m, &mtol))
	ret = 2;

    // Look for a boolean op match
    if (i1->inst->op == i2->inst->op)
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

static void
qg_make_instances(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, void *pdp, void *vctx, void *, void *)
{
    if (comb) RT_CK_COMB(comb);
    RT_CK_TREE(comb_leaf);
    QgModel_ctx *ctx = (QgModel_ctx *)vctx;
    struct directory *parent_dp = (struct directory *)pdp;

    struct directory *dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET);
    db_op_t op = DB_OP_UNION;
    if (comb_leaf->tr_l.tl_op == OP_SUBTRACT) {
	op = DB_OP_SUBTRACT;
    }
    if (comb_leaf->tr_l.tl_op == OP_INTERSECT) {
	op = DB_OP_INTERSECT;
    }

    // TODO - Instance creation is going to be tricky when we initialize and/or
    // a comb's member instances - we may be adding a duplicate instance that
    // matches already existing instances, so we don't have enough context
    // based solely on the comb leaf to recognize if the intent is to add a
    // duplicate or find and hook up an already existing instance.  That info
    // is going to have to come from the caller, which will have to base that
    // info on an analysis of all of the comb children.  Probably that means we
    // can't use the db_tree_funcleaf walk for anything except initial
    // information collection - we'll have to analyze the set of comb_leafs to
    // spot duplicates in the original .g comb data read off of disk, flag
    // them, and process accordingly.

    QgInstance *ninst = new QgInstance();
    ninst->parent = parent_dp;
    ninst->dp = dp;
    ninst->dp_name = std::string(comb_leaf->tr_l.tl_name);
    if (comb_leaf->tr_l.tl_mat) {
	MAT_COPY(ninst->c_m, comb_leaf->tr_l.tl_mat);
    } else {
	MAT_IDN(ninst->c_m);
    }
    ninst->op = op;
    ctx->parent_children[parent_dp].push_back(ninst);
    unsigned long long nhash = ninst->hash();
    ctx->ilookup[parent_dp][nhash] = ninst;
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

extern "C" void
qgmodel_update_nref_callback(struct db_i *dbip, struct directory *parent_dp, struct directory *child_dp, const char *child_name, db_op_t op, matp_t m, void *u_data)
{
    if (!parent_dp && !child_dp && !child_name && m == NULL && op == DB_OP_SUBTRACT) {

	// Cycle complete, nref count is current.  Start analyzing.
	QgModel_ctx *ctx = (QgModel_ctx *)u_data;

	if (!ctx->mdl) {
	    // If we don't have a model there's not much we can do...
	    return;
	}

	// Activity flag values are selection based, but even if the
	// selection hasn't changed since the last cycle the parent/child
	// relationships may have, which can invalidate previously active
	// dp flags.  We clear all flags, to present a clean slate when the
	// selection is re-applied.
	for (size_t i = 0; i < ctx->active_flags.size(); i++) {
	    ctx->active_flags[i] = 0;
	}


	// maybe do a sanity check here for parent_children items in the
	// null key entries whose dp->d_nref is now > 0.
	//
	// The db_diradd callback would have had no way to know when introducing
	// the new primitive and so would create a toplevel QgInstance, but the
	// individual callback check of db_update_nref should have spotted this
	// situation and corrected it.  An invalid reference in a comb should be
	// checked to see if it is still invalid, and if not the appropriate
	// corrective QgInstance actions taken - i.e., the empty top level gets
	// removed if present.  Another option might be to just do the sanity
	// check above, and always double check a null dp when we need to...

	// Note:  we don't delete any QgInstance objects here, nor do we create them.
	// Those operations are handled in the other callback.

	// What we need to do here is identify the current tops objects, and make
	// sure the QgItem hierarchy is updated to reflect them with minimal
	// disruption.
	std::vector<QgItem *> ntops;
	std::unordered_set<struct directory *> tops_dp;
	struct directory **db_objects = NULL;
	int path_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &db_objects);
	if (path_cnt) {

	    bu_sort(db_objects, path_cnt, sizeof(struct directory *), dp_cmp, NULL);

	    // We take advantage of the sorted vectors to do set differences.
	    // Unlike comb trees, which are ordered based on their boolean
	    // expressions, tops sorting is by obj names.
	    //
	    // NOTE:  we may be able to do the same thing for comb vectors with
	    // the hashes, as far as spotting added and removed children...
	    std::vector<struct directory *> ndps;
	    for (int i = 0; i < path_cnt; i++) {
		ndps.push_back(db_objects[i]);
	    }
	    bu_free(db_objects, "tops obj list");

	    std::vector<struct directory *> odps;
	    std::unordered_map<struct directory *, QgItem *> olookup;
	    for (size_t i = 0; i < ctx->tops.size(); i++) {
		odps.push_back(ctx->tops[i]->inst->dp);
		olookup[ctx->tops[i]->inst->dp] = ctx->tops[i];
	    }

	    // Find out what the differences are (if any)
	    std::vector<struct directory *> removed, added;
	    std::set_difference(odps.begin(), odps.end(), ndps.begin(), ndps.end(),
		    std::back_inserter(removed), DpCmp);
	    std::set_difference(ndps.begin(), ndps.end(), odps.begin(), odps.end(),
		    std::back_inserter(added), DpCmp);

	    // If we do have differences, there's more work to do
	    if (removed.size() || added.size()) {

		std::vector<struct directory *> common;
		std::set_intersection(odps.begin(), odps.end(), ndps.begin(), ndps.end(),
			std::back_inserter(common), DpCmp);

		// Construct unordered_sets of the above results for easy lookup
		std::unordered_set<struct directory *> added_dp(added.begin(), added.end());
		std::unordered_set<struct directory *> common_dp(common.begin(), common.end());

		for (int i = 0; i < path_cnt; i++) {
		    struct directory *dp = db_objects[i];
		    if (common_dp.find(dp) != common_dp.end()) {
			QgItem *itm = olookup[dp];
			ntops.push_back(itm);
		    }
		    if (added_dp.find(dp) != added_dp.end()) {
			// New entry.  We should already have a new instance
			// from the addition of the object, but we need the
			// hash to look it up so make a temporary copy.
			QgInstance tmp_inst;
			tmp_inst.parent = NULL;
			tmp_inst.dp = dp;
			tmp_inst.dp_name = std::string(dp->d_namep);
			tmp_inst.op = DB_OP_UNION;
			MAT_IDN(tmp_inst.c_m);

			QgItem *nitem = new QgItem();
			nitem->parent = NULL;
			nitem->inst = ctx->ilookup[(struct directory *)0][tmp_inst.hash()];
			ntops.push_back(nitem);
		    }
		}
		ctx->tops = ntops;

		std::queue<QgItem *> itm_del;
		for (size_t i = 0; i < removed.size(); i++) {
		    QgItem *itm = olookup[removed[i]];
		    itm_del.push(itm);
		}
		while (itm_del.size()) {
		    QgItem *itm = itm_del.front();
		    itm_del.pop();
		    for (size_t i = 0; i < itm->children.size(); i++) {
			itm_del.push(itm->children[i]);
		    }
		    delete itm;
		}
	    }
	}
    }


    // TODO - we do have work to do on an individual basis to check for
    // previously invalid comb entries that are no longer invalid, so we
    // shouldn't be skipping everything else... need to implement those checks



#if 0


			// Make new QgItem
			QgItem *nitem = NULL;
			if (ctx->free_items.size()) {
			    nitem = ctx->free_items.front();
			    ctx->free_items.pop();
			} else {
			    nitem = new QgItem();
			}
			nitem->parent = NULL;
			nitem->inst = ninst;
			ntops.push_back(nitem);

			// TODO - Given new item, find analog (if any) in old tops vector, and see
			// what the status of its children vector is.  If we find an analog
			// and it has a non-empty child vector, we need to populate the nitem
			// child vector as well.
			std::queue<QgItem *> to_expand;
			QgItem *citem = find_similar_qgitem(nitem, ctx->tops);
			if (citem) {
			    if (citem->children.size()) {
				bu_log("%s size: %zd\n", citem->inst->dp_name.c_str(), citem->children.size());
				to_expand.push(citem);
			    } else {
				bu_log("%s not open\n", citem->inst->dp_name.c_str());
			    }
			}
			while (to_expand.size()) {
			    // Work our way through the children of citem down the tree, until we
			    // don't find any more expanded children
			}

			ctx->tops.clear();
			ctx->tops = ntops;

		    }
		    bu_free(db_objects, "objs list");
		}

		std::vector<QgInstance*> &tops = ctx->parent_children[(struct directory *)0];
		bu_log("tops size: %zd\n", tops.size());
		for (size_t i = 0; i < tops.size(); i++) {
		    std::cout << tops[i]->dp->d_namep << "\n";
		    std::vector<QgInstance*> &tops_children = ctx->parent_children[tops[i]->dp];
		    for (size_t j = 0; j < tops_children.size(); j++) {
			if (tops_children[j]->dp) {
			    std::cout << "\t" << tops_children[j]->dp->d_namep << "\n";
			} else {
			    std::cout << "\t" << tops_children[j]->dp_name << " (no dp)\n";
			}
		    }
		}

		bu_log("time to update Qt model\n");
		ctx->mdl->need_update_nref = false;

		// TODO - QgItems are created lazily from QgInstances.  We need
		// to recreate all of them that existed previously, since they
		// are subject to the same inherent uncertainties as
		// QgInstances, but we also try as much as possible to restore
		// the previous state of population that existed before the
		// rebuild so things like expanded tree view states are changed
		// as little as possible within the constraints imposed by
		// actual .g geometry alterations.
		//
		// This probably means at the beginning of an update pass we
		// will need to iterate over the items and stash any
		// QgInstances referenced by them in an unordered map so we can
		// skip adding them to the free pile.  We will need to be able
		// to use them when we build up a new item hierarchy from the
		// newly realized QgInstances, since we will have to compare
		// the original QgInstance data from the prior state to the new
		// QgInstance children in order to find the correct
		// associations to form when we rebuild.  This is where items
		// that are removed but visible in a tree hierarchy will
		// "disappear" by failing to be regenerated.  Objects that
		// still exist in some form will be used as a basis for further
		// recreation if the original has children, if analogs for
		// those children (either exact or, if no exact analog is found
		// the closest match) are present.
		//
		// After rebuilding, we clear the old items set and replace its
		// contents with the new info.  The QgInstances saved for the
		// items rebuild are then added to the free queue for the next
		// round.
		//
		// We'll need an intelligent comparison function that finds the
		// "closest" QgInstance in a set to a candidate QgInstance.

	    }
	    return;
	}

	// Any other op in this context indicates an error
	bu_log("Error - unknown terminating condition for update_nref callback\n");
	return;
    }

    if (parent_dp) {
	bu_log("PARENT: %s\n", parent_dp->d_namep);
    } else {
	bu_log("PARENT: NULL\n");
    }

    if (child_name) {
	if (child_dp) {
	    bu_log("CHILD: %s\n", child_name);
	} else {
	    bu_log("CHILD(invalid): %s\n", child_name);
	}
    }
    switch (op) {
	case DB_OP_UNION:
	    bu_log("union\n");
	    break;
	case DB_OP_SUBTRACT:
	    bu_log("subtract\n");
	    break;
	case DB_OP_INTERSECT:
	    bu_log("intersect\n");
	    break;
	default:
	    bu_log("unknown op\n");
	    break;
    }
    if (m)
	bn_mat_print("Matrix", m);

    // We use a lot of these - reuse if possible
    QgInstance *ninst = NULL;
    if (ctx->free_instances.size()) {
	ninst = ctx->free_instances.front();
	ctx->free_instances.pop();
    } else {
	ninst = new QgInstance();
    }

    // Important:  need to make sure to assign everything in the QgInstance
    // here to avoid stale data, since we may be reusing an old instance.
    ninst->pcnt = 0;
    ninst->parent = parent_dp;
    ninst->dp = child_dp;
    ninst->dp_name = std::string(child_name);
    ninst->op = op;
    if (m) {
	MAT_COPY(ninst->c_m, m);
    } else {
	MAT_IDN(ninst->c_m);
    }
    if (ninst->dp != RT_DIR_NULL) {
	if (ctx->dp_to_active_ind.find(ninst->dp) != ctx->dp_to_active_ind.end()) {
	    ninst->active_ind = ctx->dp_to_active_ind[ninst->dp];
	} else {
	    // New dp - add it.  By default a new dp will be inactive - a view
	    // selection is what dictates, activity, and we don't (yet) have the
	    // info to decide if this dp is activated by a view selection.
	    ctx->active_flags.push_back(0);
	    ninst->active_ind = ctx->active_flags.size() - 1;
	    ctx->dp_to_active_ind[ninst->dp] = ninst->active_ind;
	}
    }

    ctx->parent_children[parent_dp].push_back(ninst);
#endif
}

extern "C" void
qgmodel_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data)
{
    QgModel_ctx *ctx = (QgModel_ctx *)u_data;
    switch(mode) {
	case 0:
	    bu_log("MOD: %s\n", dp->d_namep);
	    if (ctx->mdl)
		ctx->mdl->changed_dp.insert(dp);

	    // What needs to happen here for combs is the construction of a
	    // vector of QgInstances corresponding to the new children of the
	    // comb.  We then construct two vectors of the hashes of the old
	    // and new children sets, sort and use set_difference to find
	    // QgInstances that have been removed from the new comb.  Those are
	    // deleted, and the old QgInstance vector is replaced by the new.
	    // QgItems will also need similar treatment, with the additional wrinkle
	    // that any deleted QgItems also have all their children removed.
	    // Initially that will mean that editing a comb instance will close it in
	    // a tree view even if the edit is to its matrix or op - we may be able
	    // to do a fuzzy matching to re-populate new QgItems that correspond
	    // closely to a removed item, but that's for down the road.
	    //
	    // Will also need handlers for extrude/revolve/dsp.
	    //
	    // This is a variation on the logic used in initialization that sets
	    // up the initial set of QgInstances - need to either refactor that
	    // so it can be used for both situations or implement something suitable
	    // for this case.


	    break;
	case 1:
	    bu_log("ADD: %s\n", dp->d_namep);
	    if (ctx->mdl) {
		ctx->mdl->changed_dp.insert(dp);
		ctx->mdl->need_update_nref = true;
	    }

	    // We can either do it here or in the update_nref callback, but we need
	    // to check whether any QgInstances are referencing the added name as
	    // a child.  If so, the added object is not a tops object and doesn't
	    // get a top level instance/item.  Thinking it may be better to do it
	    // here - check only QgInstances with a null dp - for those, do a name
	    // comparison.  If it matches, assign the dp and note that the item
	    // isn't top level.
	    //
	    // If the added object is itself a comb or one of the primitives referencing
	    // another object, we need to add new QgInstances.

	    break;
	case 2:
	    bu_log("RM:  %s\n", dp->d_namep);
	    if (ctx->mdl)
		ctx->mdl->need_update_nref = true;

	    // Check QgInstances using parent_children array to see if any of
	    // them have become invalid.  If the parent_dp == removed dp,
	    // they're removed (the easy case, as that's the vector stored by
	    // parent_children).  However, we also need to check the others: if
	    // dp == the removed dp, the QgInstance is now invalid (set dp to
	    // NULL).  QgItems also need to be updated, not sure yet whether
	    // it's better do that here or in update_nref callback.  New tops
	    // objects at least will be more simply dealt with the update_nref
	    // callback, after the d_nref counts are finalized.

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


    // tops objects in the .g file are "instances" beneath the .g itself, which
    // is the analogy to Qt's hidden "root" node in a model.  To handle them,
    // we define "NULL root" instances.
    parent_children[(struct directory *)0].clear();
    ilookup[(struct directory *)0].clear();
    struct directory **db_objects = NULL;
    int path_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &db_objects);

    if (path_cnt) {

	// Sort so the top level ordering is correct for listing in tree views
	bu_sort(db_objects, path_cnt, sizeof(struct directory *), dp_cmp, NULL);

	for (int i = 0; i < path_cnt; i++) {
	    struct directory *curr_dp = db_objects[i];
	    // Make temporary QgInstance and call the add method to either
	    // get a reference to the existing instance or create a new one.
	    QgInstance *ninst = new QgInstance();
	    ninst->parent = NULL;
	    ninst->dp = curr_dp;
	    ninst->dp_name = std::string(curr_dp->d_namep);
	    ninst->op = DB_OP_UNION;
	    MAT_IDN(ninst->c_m);
	    parent_children[(struct directory *)0].push_back(ninst);
	    ilookup[(struct directory *)0][ninst->hash()] = ninst;

	    // tops entries get a QgItem by default
	    QgItem *nitem = new QgItem();
	    nitem->parent = NULL;
	    nitem->inst = ninst;
	    tops.push_back(nitem);
	}
    }
    bu_free(db_objects, "tops obj list");

    // Run through the objects and crack the non-leaf objects to define
    // non-tops instances (i.e. all comb instances and some primitive
    // types that reference other objects).
    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & RT_DIR_HIDDEN)
		continue;

	    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
		struct rt_db_internal intern;
		RT_DB_INTERNAL_INIT(&intern);
		if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
		    continue;
		struct rt_extrude_internal *extr = (struct rt_extrude_internal *)intern.idb_ptr;
		RT_EXTRUDE_CK_MAGIC(extr);
		if (extr->sketch_name) {
		    QgInstance *ninst = new QgInstance();
		    ninst->parent = dp;
		    ninst->dp = db_lookup(dbip, extr->sketch_name, LOOKUP_QUIET);
		    ninst->dp_name = std::string(extr->sketch_name);
		    MAT_IDN(ninst->c_m);
		    ninst->op = DB_OP_UNION;
		    parent_children[dp].push_back(ninst);
		}
	    }
	    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE) {
		struct rt_db_internal intern;
		RT_DB_INTERNAL_INIT(&intern);
		if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
		    continue;
		struct rt_revolve_internal *revolve = (struct rt_revolve_internal *)intern.idb_ptr;
		RT_REVOLVE_CK_MAGIC(revolve);
		if (bu_vls_strlen(&revolve->sketch_name) > 0) {
		    QgInstance *ninst = new QgInstance();
		    ninst->parent = dp;
		    ninst->dp = db_lookup(dbip, bu_vls_cstr(&revolve->sketch_name), LOOKUP_QUIET);
		    ninst->dp_name = std::string(bu_vls_cstr(&revolve->sketch_name));
		    MAT_IDN(ninst->c_m);
		    ninst->op = DB_OP_UNION;
		    parent_children[dp].push_back(ninst);
		}
	    }
	    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP) {
		struct rt_db_internal intern;
		RT_DB_INTERNAL_INIT(&intern);
		if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
		    continue;
		struct rt_dsp_internal *dsp = (struct rt_dsp_internal *)intern.idb_ptr;
		RT_DSP_CK_MAGIC(dsp);
		if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ && bu_vls_strlen(&dsp->dsp_name) > 0) {
		    QgInstance *ninst = new QgInstance();
		    ninst->parent = dp;
		    ninst->dp = db_lookup(dbip, bu_vls_cstr(&dsp->dsp_name), LOOKUP_QUIET);
		    ninst->dp_name = std::string(bu_vls_cstr(&dsp->dsp_name));
		    MAT_IDN(ninst->c_m);
		    ninst->op = DB_OP_UNION;
		    parent_children[dp].push_back(ninst);
		}
	    }

	    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
		struct rt_db_internal intern;
		RT_DB_INTERNAL_INIT(&intern);
		if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
		    continue;
		if (intern.idb_type != ID_COMBINATION) {
		    bu_log("NOTICE: %s was marked a combination, but isn't one?  Clearing flag\n", dp->d_namep);
		    dp->d_flags &= ~RT_DIR_COMB;
		    rt_db_free_internal(&intern);
		    continue;
		}

		struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
		db_tree_funcleaf(dbip, comb, comb->tree, qg_make_instances, (void *)dp, (void *)this, NULL, NULL);
	    }
	}
    }

}

QgModel_ctx::~QgModel_ctx()
{
    if (gedp)
	ged_close(gedp);

    std::unordered_map<struct directory *, std::vector<QgInstance *>>::iterator pc_it;
    for (pc_it = parent_children.begin(); pc_it != parent_children.end(); pc_it++) {
	for (size_t i = 0; i < pc_it->second.size(); i++) {
	    QgInstance *inst = pc_it->second[i];
	    delete inst;
	}
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

