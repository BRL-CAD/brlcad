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
#include "raytrace.h"
#include "qtcad/QgModel.h"

QgInstance::QgInstance()
{
}

QgInstance::~QgInstance()
{
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
	    // defined in pareht_children and store them for reuse.  Typically
	    // most of these will still be current, but that's by no means
	    // guaranteed and we have no way to know if they are or not until
	    // db_update_nref tells.  So we have to "start over" building up
	    // the instances, but at least we can avoid having to re-malloc all
	    // of them.
	    std::unordered_map<std::string, std::vector<QgInstance *>>::iterator p_it;
	    for (p_it = ctx->parent_children.begin(); p_it != ctx->parent_children.end(); p_it++) {
		for (size_t i = 0; i < p_it->second.size(); i++) {
		    ctx->free_instances.push(p_it->second[i]);
		}
	    }
	    ctx->parent_children.clear();

	    // child_parents should not contain any QgInstance containers not
	    // present in parent_children (even top level objects should be
	    // present in that map, with an empty string for the key), so we
	    // just clear child_parents without worrying about saving instances
	    // for reuse.
	    ctx->child_parents.clear();
	    return;
	}
	if (op == DB_OP_SUBTRACT) {
	    //bu_log("ending db_update_nref\n");

	    // Iterate over parent_children and build up child_parents.
	    //
	    // Main use of child->parents is going to be doing intelligent
	    // highlighting, which in principle should be more performant with an
	    // intelligent way to work our way back up the tree...
	    std::unordered_map<std::string, std::vector<QgInstance *>>::iterator p_it;
	    for (p_it = ctx->parent_children.begin(); p_it != ctx->parent_children.end(); p_it++) {
		for (size_t i = 0; i < p_it->second.size(); i++) {
		    ctx->child_parents[p_it->second[i]->dp_name][p_it->first].insert(p_it->second[i]);
		}
	    }

	    if (ctx->mdl) {
		bu_log("time to update Qt model\n");
		ctx->mdl->need_update_nref = false;
	    }
	    return;
	}

	// Any other op in this context indicates an error
	bu_log("Error - unknown terminating condition for update_nref callback\n");
	return;
    }

#if 0
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
	bn_mat_print("Instance matrix", m);
#endif

    // We use a lot of these instances - reuse if possible
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

    // By default everything is inactive - a view selection is what dictates
    // the active/inactive state.
    ninst->active = 0;

    // Top level objects have empty parents, but we need all objects to be in
    // the parent/child hierarchy so we add them with an empty string key.
    std::string key = (parent_dp) ? std::string(parent_dp->d_namep) : std::string("");
    ctx->parent_children[key].push_back(ninst);
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
}


QgModel::QgModel(QObject *, struct ged *ngedp)
{
    ctx = new QgModel_ctx(this, ngedp);
}

QgModel::~QgModel()
{
    delete ctx;
}

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


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

