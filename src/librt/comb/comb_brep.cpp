/*                    C O M B _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
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
/** @file comb_brep.cpp
 *
 * Convert a combination to b-rep form (with NURBS boolean evaluations)
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "rt/comb.h"
#include "nmg.h"
#include "brep.h"

// TODO - there are multiple copies of this - need to find a good place for it...
static int
single_conversion(struct rt_db_internal* intern, ON_Brep** brep, const struct db_i* dbip)
{
    struct bn_tol tol;
    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = SMALL_FASTF;
    tol.para = 1.0 - tol.perp;

    if (intern->idb_type == ID_BREP) {
        // already a brep
        RT_BREP_CK_MAGIC(intern->idb_ptr);
        **brep = *((struct rt_brep_internal *)intern->idb_ptr)->brep;
    } else if (intern->idb_type == ID_COMBINATION) {
        rt_comb_brep(brep, intern, &tol, dbip);
    } else if (intern->idb_meth->ft_brep != NULL) {
        intern->idb_meth->ft_brep(brep, intern, &tol);
    } else {
        *brep = NULL;
        return -1;
    }
    if (*brep == NULL) {
        return -2;
    }
    return 0;
}

HIDDEN int
conv_tree(ON_Brep **b, const union tree *t, const struct db_i *dbip)
{
    ON_Brep *left = NULL, *right = NULL, *old = NULL;
    int ret = 0;
    switch (t->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    /* convert right */
	    old = right = ON_Brep::New();
	    ret = conv_tree(&right, t->tr_b.tb_right, dbip);
	    if (ret) {
		delete old;
		break;
	    }
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* convert left */
	    old = left = ON_Brep::New();
	    ret = conv_tree(&left, t->tr_b.tb_left, dbip);
	    if (!ret) {
		// Perform NURBS evaluations
		if (t->tr_op == OP_UNION)
		    ret = ON_Boolean(*b, left, right, BOOLEAN_UNION);
		else if (t->tr_op == OP_INTERSECT)
		    ret = ON_Boolean(*b, left, right, BOOLEAN_INTERSECT);
		else if (t->tr_op == OP_SUBTRACT)
		    ret = ON_Boolean(*b, left, right, BOOLEAN_DIFF);
		else {
		    bu_log("operation %d isn't supported yet.\n", t->tr_op);
		    ret = -1;
		}
		delete left;
		delete right;
	    } else {
		delete old;
		delete right;
	    }
	    break;
	case OP_DB_LEAF:
	    {
		char *name = t->tr_l.tl_name;
		directory *dir;
		dir = db_lookup(dbip, name, LOOKUP_QUIET);
		if (dir != RT_DIR_NULL) {
		    rt_db_internal intern;
		    rt_db_get_internal(&intern, dir, dbip, bn_mat_identity, &rt_uniresource);
		    RT_CK_DB_INTERNAL(&intern);
		    ret = single_conversion(&intern, b, dbip);
		    if (ret == 0 && *b != NULL) {
			if (t->tr_l.tl_mat != NULL && !bn_mat_is_identity(t->tr_l.tl_mat)) {
			    ON_Xform xform(t->tr_l.tl_mat);
			    ret = -1;
			    if ((*b)->Transform(xform)) {
				ret = 0;
			    }
			}
		    }
		    rt_db_free_internal(&intern);
		} else {
		    bu_log("Cannot find %s.\n", name);
		    ret = -1;
		}
		break;
	    }
	default:
	    bu_log("OPCODE NOT IMPLEMENTED: %d\n", t->tr_op);
	    ret = -1;
    }
    if (ret) {
	*b = NULL;
    }

    return ret;
}


void
rt_comb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol), const struct db_i *dbip)
{
    RT_CK_DB_INTERNAL(ip);
    RT_CK_DBI(dbip);

    struct rt_comb_internal *cip;
    cip = (struct rt_comb_internal *)ip->idb_ptr;
    RT_CK_COMB(cip);

    if (cip->tree == NULL) {
	*b = NULL;
	return;
    }

    conv_tree(b, cip->tree, dbip);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
