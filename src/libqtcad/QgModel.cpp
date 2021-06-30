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
 */

#include "common.h"

#include <QFileInfo>

#include "bu/env.h"
#include "bu/sort.h"
#include "raytrace.h"
#include "qtcad/QgModel.h"

QgInstance::QgInstance()
{
}

QgInstance::~QgInstance()
{
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
    QgModel_ctx *ctx = (QgModel_ctx *)u_data;

    if (!dbip && !parent_dp && !child_dp && !child_name && m == NULL) {

	// If all parameters except the op are NULL, we're either starting or ending
	// the update pass.  Accordingly, we need to either initialize or finalize
	// the QgModel state.

	if (op == DB_OP_UNION) {
	    //bu_log("starting db_update_nref\n");

	    // At the start of a hierarchy refresh cycle, grab all QgInstances
	    // defined in parent_children and store them for reuse.  Typically
	    // most of these will still be current, but that's by no means
	    // guaranteed and we have no way to know if they are or not until
	    // db_update_nref tells us.  We have to "start over" building up
	    // the info, but at least we can avoid having to re-malloc all
	    // of them again.
	    std::unordered_map<struct directory *, std::vector<QgInstance*>>::iterator p_it;
	    for (p_it = ctx->parent_children.begin(); p_it != ctx->parent_children.end(); p_it++) {
		for (size_t i = 0; i < p_it->second.size(); i++) {
		    ctx->free_instances.push(p_it->second[i]);
		}
	    }
	    ctx->parent_children.clear();
	    return;
	}
	if (op == DB_OP_SUBTRACT) {
	    //bu_log("ending db_update_nref\n");

	    // Activity flag values are selection based, but even if the
	    // selection hasn't changed since the last cycle the parent/child
	    // relationships may have, which can invalidate previously active
	    // dp flags.  We clear all flags, to present a clean slate when the
	    // selection is re-applied.
	    for (size_t i = 0; i < ctx->active_flags.size(); i++) {
		ctx->active_flags[i] = 0;
	    }

	    if (ctx->mdl) {
	
		// Get tops list and add them as children of the null parent
		struct directory **db_objects = NULL;
		int path_cnt = db_ls(ctx->gedp->ged_wdbp->dbip, DB_LS_TOPS, NULL, &db_objects);
		if (path_cnt) {
		    bu_sort(db_objects, path_cnt, sizeof(struct directory *), dp_cmp, NULL);
		    for (int i = 0; i < path_cnt; i++) {
			struct directory *curr_dp = db_objects[i];
			QgInstance *nobj = NULL;
			if (ctx->free_instances.size()) {
			    nobj = ctx->free_instances.front();
			    ctx->free_instances.pop();
			} else {
			    nobj = new QgInstance();
			}
			nobj->parent = NULL;
			nobj->dp = curr_dp;
			nobj->dp_name = std::string(curr_dp->d_namep);
			nobj->op = DB_OP_UNION;
			MAT_IDN(nobj->c_m);
			if (ctx->dp_to_active_ind.find(nobj->dp) != ctx->dp_to_active_ind.end()) {
			    nobj->active_ind = ctx->dp_to_active_ind[nobj->dp];
			} else {
			    // New dp - add it.  By default a new dp will be inactive - a view
			    // selection is what dictates, activity, and we don't (yet) have the
			    // info to decide if this dp is activated by a view selection.
			    ctx->active_flags.push_back(0);
			    nobj->active_ind = ctx->active_flags.size() - 1;
			    ctx->dp_to_active_ind[nobj->dp] = nobj->active_ind;
			}
			ctx->parent_children[(struct directory *)0].push_back(nobj);
		    }
		    bu_free(db_objects, "objs list");
		}

#if 0
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
#endif
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

#if 1
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
#endif

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

