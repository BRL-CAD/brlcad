/*                   A D D _ T R E E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file Comb_Tree.cpp
 *
 * File for writing out a combination and its tree contents
 * into the STEPcode containers
 *
 */

#include "AP214.h"

HIDDEN int
conv_tree(struct directory **d, int depth, int parent_branch, struct directory **solid, const union tree *t, const struct db_i *dbip)
{
    struct directory *left = NULL, *right = NULL, *old = NULL;
    int ret = 0;
    int left_ret = 0;
    int right_ret = 0;
    bu_log("Processing: %s, depth: %d", (*d)->d_namep, depth);
    if (parent_branch == 1) bu_log(", branch = right\n");
    if (parent_branch == 2) bu_log(", branch = left\n");
    if (parent_branch == 0) bu_log("\n");
    switch (t->tr_op) {
        case OP_UNION:
	    if (t->tr_op == OP_UNION && !parent_branch) bu_log(".UNION.\n");
        case OP_INTERSECT:
	    if (t->tr_op == OP_INTERSECT && !parent_branch) bu_log(".INTERSECT.\n");
        case OP_SUBTRACT:
	    if (t->tr_op == OP_SUBTRACT && !parent_branch) bu_log(".SUBTRACT.\n");
        case OP_XOR:
	    if (t->tr_op == OP_XOR) bu_log(".XOR.\n");
            /* convert right */
            ret = conv_tree(d, depth+1, 1, &right, t->tr_b.tb_right, dbip);
	    right_ret = ret;
	    if (ret == -1) {
		break;
            }
	    /* fall through */
        case OP_NOT:
	    if (t->tr_op == OP_NOT) bu_log(".NOT.\n");
        case OP_GUARD:
	    if (t->tr_op == OP_GUARD) bu_log(".GUARD.\n");
        case OP_XNOP:
	    if (t->tr_op == OP_XNOP) bu_log(".XNOP.\n");
            /* convert left */
            ret = conv_tree(d, depth+1, 2, &left, t->tr_b.tb_left, dbip);
	    left_ret = ret;
            if (ret == -1) {
		break;
	    } else {
		bu_log("(left) boolean_result type %d.\n", ret);
            }
	    if (left_ret && right_ret) {
		if (left && right) {
		    bu_log("Constructed boolean for %s (left: %s,right: %s): ", (*d)->d_namep, left->d_namep, right->d_namep);
		}
		if (left && !right) {
		    bu_log("Constructed boolean for %s (left: %s,right: boolean_result): ", (*d)->d_namep, left->d_namep);
		}
		if (!left && right) {
		    bu_log("Constructed boolean for %s (left: boolean_result, right: %s): ", (*d)->d_namep, right->d_namep);
		}
		if (!left && !right) {
		    bu_log("Constructed boolean for %s: ", (*d)->d_namep);
		}
		if (left_ret == 1) bu_log("solid");
		if (left_ret == 2) bu_log("boolean_result");
		if (left_ret == 3) bu_log("comb(boolean_result)");
		switch (t->tr_op) {
		    case OP_UNION:
			bu_log(" u ");
			break;
		    case OP_INTERSECT:
			bu_log(" + ");
			break;
		    case OP_SUBTRACT:
			bu_log(" - ");
			break;
		}
		if (right_ret == 1) bu_log("solid\n\n");
		if (right_ret == 2) bu_log("boolean_result\n\n");
		if (right_ret == 3) bu_log("comb(boolean_result)\n\n");

		/* If we've got something here, anything above this is a boolean_result */
		ret = 2;
	    }
            break;
        case OP_DB_LEAF:
            {
                char *name = t->tr_l.tl_name;
                struct directory *dir;
                dir = db_lookup(dbip, name, LOOKUP_QUIET);
		if (dir != RT_DIR_NULL) {
		    struct rt_db_internal intern;
		    if (solid) (*solid) = dir;
		    rt_db_get_internal(&intern, dir, dbip, bn_mat_identity, &rt_uniresource);
		    if (intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
			RT_CK_DB_INTERNAL(&intern);
			struct rt_comb_internal *comb = (struct rt_comb_internal *)(intern.idb_ptr);
			RT_CK_COMB(comb);
			if (comb->tree == NULL) {
			    /* Probably should return empty object... */
			    return NULL;
			} else {
			    conv_tree(&dir, depth+1, 0, NULL, comb->tree, dbip);
			    ret = 3;
			}
		    } else {
			ret = 1;
		    }
                } else {
                    bu_log("Cannot find leaf %s.\n", name);
                    ret = -1;
                }
                break;
            }
        default:
            bu_log("OPCODE NOT IMPLEMENTED: %d\n", t->tr_op);
            ret = -1;
    }

    return ret;
}


struct AP214_tree_result *
AP214_Add_Tree(struct directory *dp, struct rt_wdb *wdbp, Registry *registry, InstMgr *instance_list)
{
    struct AP214_tree_result *result = new struct AP214_tree_result;
    struct db_i *dbip = wdbp->dbip;
    struct rt_db_internal comb_intern;
    rt_db_get_internal(&comb_intern, dp, dbip, bn_mat_identity, &rt_uniresource);
    RT_CK_DB_INTERNAL(&comb_intern);
    struct rt_comb_internal *comb = (struct rt_comb_internal *)(comb_intern.idb_ptr);
    RT_CK_COMB(comb);
    if (comb->tree == NULL) {
	/* Probably should return empty object... */
	return NULL;
    } else {
        (void)conv_tree(&dp, 0, 0, NULL, comb->tree, dbip);
    }
    return result;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
