/*                     Q G M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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
/** @file qgmodel.cpp
 *
 * Brief description
 *
 */

#include <iostream>
#include <unordered_map>

#include "bu/app.h"
#include "bu/log.h"
#include "qtcad/QgModel.h"

struct model_state {
    std::unordered_map<unsigned long long, QgInstance *> instances;
    std::unordered_map<unsigned long long, QgInstance *> tops_instances;
};

db_op_t int_to_op(int bool_op)
{
    switch (bool_op) {
	case OP_UNION:
	    return DB_OP_UNION;
	case OP_INTERSECT:
	    return DB_OP_INTERSECT;
	case OP_SUBTRACT:
	    return DB_OP_SUBTRACT;
	default:
	    return DB_OP_NULL;
    }
}

static void
_get_qg_instances(db_op_t curr_bool, struct db_i *dbip, struct directory *parent_dp, union tree *tp, struct model_state *s)
{
    db_op_t bool_op = curr_bool;
    QgInstance *qg = NULL;
    unsigned long long qg_hash = 0;
    std::string msg;

    if (!tp)
	return;

    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    bool_op = int_to_op(tp->tr_op);
	    _get_qg_instances(bool_op, dbip, parent_dp, tp->tr_b.tb_right, s);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    _get_qg_instances(bool_op, dbip, parent_dp, tp->tr_b.tb_left, s);
	    break;
	case OP_DB_LEAF:
	    qg = new QgInstance;
	    qg->parent = parent_dp;
	    qg->dp = db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET);
	    qg->dp_name = std::string(tp->tr_l.tl_name);
	    qg->op = bool_op;
	    if (tp->tr_l.tl_mat) {
		MAT_COPY(qg->c_m, tp->tr_l.tl_mat);
	    } else {
		MAT_IDN(qg->c_m);
	    }
	    qg_hash = qg->hash();
	    while (s->instances.find(qg_hash) != s->instances.end()) {
		// Note - for now this is the simplistic solution of checking
		// until we find an open number.  This won't scale well if we
		// have enormous numbers of identical comb entries - in that
		// case we'll have to add a lookup based on a hash without the
		// icnt variable and track the maximum number used there - but
		// not bothering to add that refinement until it's clear we
		// need it.
		qg->icnt++;
		qg_hash = qg->hash();
	    }

	    s->instances[qg_hash] = qg;

	    msg = qg->print();
	    std::cout << msg << "\n";
	    break;

	default:
	    bu_log("unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("qg_instances tree walk\n");
    }
}


int
make_qg_instances(struct db_i *dbip, struct directory *parent_dp, struct rt_comb_internal *comb, struct model_state *s)
{
    int node_count = db_tree_nleaves(comb->tree);
    if (!node_count) return 0;
    _get_qg_instances(int_to_op(OP_UNION), dbip, parent_dp, comb->tree, s);
    return 0;
}

int
make_tops_instances(struct db_i *dbip, struct model_state *s)
{
    if (!s)
	return -1;

    struct directory **tops_paths;
    int tops_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &tops_paths);

    if (!tops_cnt) {
	bu_log("Error - unable to find tops objects!\n");
	return -1;
    }

    QgInstance *qg = NULL;
    unsigned long long qg_hash = 0;
    for (int i = 0; i < tops_cnt; i++) {
	qg = new QgInstance;
	qg->parent = NULL;
	qg->dp = tops_paths[i];
	qg->dp_name = std::string(qg->dp->d_namep);
	qg->op = DB_OP_UNION;
	MAT_IDN(qg->c_m);
	qg_hash = qg->hash();
	s->tops_instances[qg_hash] = qg;
    }

    // Cleanup
    bu_free(tops_paths, "tops array");

    return tops_cnt;
}

int main(int argc, char *argv[])
{

    bu_setprogname(argv[0]);

    argc--; argv++;

    if (argc != 1)
	bu_exit(-1, "need to specify .g file\n");

    struct db_i *dbip = db_open(argv[0], DB_OPEN_READONLY);
    if (dbip == DBI_NULL)
	bu_exit(-1, "db_open failed on geometry database file %s\n", argv[0]);

    RT_CK_DBI(dbip);
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
    }
    db_update_nref(dbip, &rt_uniresource);

    struct model_state s;

    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp = RT_DIR_NULL;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & RT_DIR_HIDDEN) continue;
	    if (dp->d_flags & RT_DIR_COMB) {
		bu_log("Comb: %s\n", dp->d_namep);
		struct rt_db_internal intern;
		struct rt_comb_internal *comb;
		if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		    continue;
		}
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		make_qg_instances(dbip, dp, comb, &s);
	    }
	    // TODO - extrusions, other primitives that reference another obj
	}
    }
    bu_log("Hierarchy instance cnt: %zd\n", s.instances.size());

    // The above logic will create hierarchy instances, but it won't create the
    // top level instances (which, by definition, aren't under anything.)  Make
    // a second pass using db_ls to create those instances (this depends on
    // db_update_nref being up to date.)
    make_tops_instances(dbip, &s);
    bu_log("Top instance cnt: %zd\n", s.tops_instances.size());

    return s.instances.size() + s.tops_instances.size();
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
