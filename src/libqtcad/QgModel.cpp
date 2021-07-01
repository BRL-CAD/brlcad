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


unsigned long long
QgInstance::hash(int mode)
{
    if (!h_state)
	return 0;

    XXH64_hash_t hash_val;

    if (parent)
	XXH64_update(h_state, parent->d_namep, strlen(parent->d_namep));
    if (dp)
	XXH64_update(h_state, dp->d_namep, strlen(dp->d_namep));
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
qg_make_instances(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, void *pdp, void *vctx, void *vtinst, void *vninst)
{
    if (comb) RT_CK_COMB(comb);
    RT_CK_TREE(comb_leaf);
    QgModel_ctx *ctx = (QgModel_ctx *)vctx;
    QgInstance *tmp_inst = (QgInstance *)vtinst;
    struct directory *parent_dp = (struct directory *)pdp;
    std::unordered_map<unsigned long long, QgInstance *> *n_instances = (std::unordered_map<unsigned long long, QgInstance *> *)vninst;

    struct directory *dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET);
    db_op_t op = DB_OP_UNION;
    if (comb_leaf->tr_l.tl_op == OP_SUBTRACT) {
	op = DB_OP_SUBTRACT;
    }
    if (comb_leaf->tr_l.tl_op == OP_INTERSECT) {
	op = DB_OP_INTERSECT;
    }

    tmp_inst->parent = parent_dp;
    tmp_inst->dp = dp;
    tmp_inst->dp_name = std::string(comb_leaf->tr_l.tl_name);
    if (comb_leaf->tr_l.tl_mat) {
	MAT_COPY(tmp_inst->c_m, comb_leaf->tr_l.tl_mat);
    } else {
    MAT_IDN(tmp_inst->c_m);
    }
    tmp_inst->op = op;
    QgInstance *cinst = ctx->add(n_instances, tmp_inst);
    ctx->parent_children[parent_dp].push_back(cinst);
}


static int
dp_cmp(const void *d1, const void *d2, void *UNUSED(arg))
{
    struct directory *dp1 = *(struct directory **)d1;
    struct directory *dp2 = *(struct directory **)d2;
    return bu_strcmp(dp1->d_namep, dp2->d_namep);
}

extern "C" void
qgmodel_update_nref_callback(struct db_i *dbip, struct directory *parent_dp, struct directory *child_dp, const char *child_name, db_op_t op, matp_t m, void *u_data)
{

    if (parent_dp || child_dp || child_name || m != NULL || op != DB_OP_SUBTRACT) {
	// For this particular approach, we're only processing at the end of the
	// cycle - if the above conditions are met, we're not at the end yet.
	return;
    }

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

    std::vector<QgItem *> ntops;

    std::unordered_map<unsigned long long, QgInstance *> n_instances;

    // Get tops list.  They must be handled specially since they have no parent and
    // constitute the seeds of walking.
    ctx->parent_children[(struct directory *)0].clear();
    QgInstance *tmp_inst = new QgInstance();
    struct directory **db_objects = NULL;
    int path_cnt = db_ls(ctx->gedp->ged_wdbp->dbip, DB_LS_TOPS, NULL, &db_objects);

    if (path_cnt) {

	// Sort so the top level ordering is correct for listing in tree views
	bu_sort(db_objects, path_cnt, sizeof(struct directory *), dp_cmp, NULL);

	// These two parameters are the same for all top level instances - set
	// them once.
	MAT_IDN(tmp_inst->c_m);
	tmp_inst->op = DB_OP_UNION;

	for (int i = 0; i < path_cnt; i++) {
	    struct directory *curr_dp = db_objects[i];
	    // Make temporary QgInstance and call the add method to either
	    // get a reference to the existing instance or create a new one.
	    tmp_inst->parent = NULL;
	    tmp_inst->dp = curr_dp;
	    tmp_inst->dp_name = std::string(curr_dp->d_namep);
	    QgInstance *cinst = ctx->add(&n_instances, tmp_inst);
	    ctx->parent_children[(struct directory *)0].push_back(cinst);
	}
    }

    // Run through the objects that aren't tops objects - crack the
    // non-leaf objects to define non-tops instances.
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
		    tmp_inst->parent = dp;
		    tmp_inst->dp = db_lookup(dbip, extr->sketch_name, LOOKUP_QUIET);
		    tmp_inst->dp_name = std::string(extr->sketch_name);
		    MAT_IDN(tmp_inst->c_m);
		    tmp_inst->op = DB_OP_UNION;
		    QgInstance *cinst = ctx->add(&n_instances, tmp_inst);
		    ctx->parent_children[dp].push_back(cinst);
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
		    tmp_inst->parent = dp;
		    tmp_inst->dp = db_lookup(dbip, bu_vls_cstr(&revolve->sketch_name), LOOKUP_QUIET);
		    tmp_inst->dp_name = std::string(bu_vls_cstr(&revolve->sketch_name));
		    MAT_IDN(tmp_inst->c_m);
		    tmp_inst->op = DB_OP_UNION;
		    QgInstance *cinst = ctx->add(&n_instances, tmp_inst);
		    ctx->parent_children[dp].push_back(cinst);
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
		    tmp_inst->parent = dp;
		    tmp_inst->dp = db_lookup(dbip, bu_vls_cstr(&dsp->dsp_name), LOOKUP_QUIET);
		    tmp_inst->dp_name = std::string(bu_vls_cstr(&dsp->dsp_name));
		    MAT_IDN(tmp_inst->c_m);
		    tmp_inst->op = DB_OP_UNION;
		    QgInstance *cinst = ctx->add(&n_instances, tmp_inst);
		    ctx->parent_children[dp].push_back(cinst);
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
		db_tree_funcleaf(dbip, comb, comb->tree, qg_make_instances, (void *)dp, (void *)ctx, (void *)tmp_inst, (void *)&n_instances);
	    }
	}
    }

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

// This callback is needed for cases where an individual object is changed but the
// hierarchy is not disturbed (a rename, for example.)
//
// We also use it to catch cases where the internal logic SHOULD have called
// db_update_nref after adding or removing objects, but didn't.
extern "C" void
qgmodel_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data)
{
    QgModel_ctx *ctx = (QgModel_ctx *)u_data;
    switch(mode) {
	case 0:
	    bu_log("MOD: %s\n", dp->d_namep);
	    if (ctx->mdl)
		ctx->mdl->changed_dp.insert(dp);
	    break;
	case 1:
	    bu_log("ADD: %s\n", dp->d_namep);
	    if (ctx->mdl) {
		ctx->mdl->changed_dp.insert(dp);
		ctx->mdl->need_update_nref = true;
	    }
	    break;
	case 2:
	    bu_log("RM:  %s\n", dp->d_namep);
	    if (ctx->mdl)
		ctx->mdl->need_update_nref = true;
	    break;
	default:
	    bu_log("changed callback mode error: %d\n", mode);
    }
}


QgModel_ctx::QgModel_ctx(QgModel *pmdl, struct ged *ngedp)
    : mdl(pmdl), gedp(ngedp)
{
    if (!ngedp)
	return;

    // If we do have a dbip, we have some setup to do
    db_add_update_nref_clbk(gedp->ged_wdbp->dbip, &qgmodel_update_nref_callback, (void *)this);
    db_add_changed_clbk(gedp->ged_wdbp->dbip, &qgmodel_changed_callback, (void *)this);
}

QgModel_ctx::~QgModel_ctx()
{
    if (gedp)
	ged_close(gedp);

    while (free_instances.size()) {
	QgInstance *i = free_instances.front();
	free_instances.pop();
	delete i;
    }
}

QgInstance *
QgModel_ctx::add(std::unordered_map<unsigned long long, QgInstance *> *n_instances, QgInstance *tinst)
{
    unsigned long long tmp_hash = tinst->hash(3);
    if (instances.find(tmp_hash) != instances.end()) {
	// Exists, we can continue
	QgInstance *oinst = instances[tmp_hash];
	(*n_instances)[tmp_hash] = oinst;
	instances.erase(tmp_hash);
	return oinst;
    }

    // Nope - need a new one
    QgInstance *ninst = NULL;
    if (free_instances.size()) {
	ninst = free_instances.front();
	free_instances.pop();
    } else {
	ninst = new QgInstance();
    }
    ninst->parent = tinst->dp;
    ninst->dp = tinst->dp;
    ninst->dp_name = tinst->dp_name;
    ninst->op = tinst->op;
    MAT_COPY(ninst->c_m, tinst->c_m);
    (*n_instances)[tmp_hash] = ninst;
    return ninst;
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
QgModel::flags(const QModelIndex &UNUSED(index))
{
    return Qt::NoItemFlags;
}

QVariant
QgModel::data(const QModelIndex &UNUSED(index), int UNUSED(role)) const
{
    return QVariant();
}

QVariant
QgModel::headerData(int UNUSED(section), Qt::Orientation UNUSED(orientation), int UNUSED(role))
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
	  ctx = new QgModel_ctx(this, n);

	  // Callbacks are set in ctx - make sure reference counts and model
	  // hierarchy are properly initialized
	  db_update_nref(n->ged_wdbp->dbip, &rt_uniresource);
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

