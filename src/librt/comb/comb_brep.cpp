/*                    C O M B _ B R E P . C P P
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
/** @file comb_brep.cpp
 *
 * Convert a combination to b-rep form (with NURBS boolean evaluations)
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "nmg.h"
#include "brep.h"


// Declaration
extern "C" void rt_comb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol, const struct db_i *dbip);
extern "C" int single_conversion(struct rt_db_internal* intern, ON_Brep** brep, const struct db_i *dbip);


HIDDEN int
conv_tree(ON_Brep **b, const union tree *t, const struct db_i *dbip)
{
    ON_Brep *left = NULL, *right = NULL, *old = NULL;
    int ret = 0;
    switch (t->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* convert right */
	    old = right = ON_Brep::New();
	    ret = conv_tree(&right, t->tr_b.tb_right, dbip);
	    if (ret) {
		if (right && old)
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
		else if (t->tr_op == OP_XOR)
		    ret = ON_Boolean(*b, left, right, BOOLEAN_XOR);
		else {
		    bu_log("operation %d isn't supported yet.\n", t->tr_op);
		    ret = -1;
		}
		if (left) delete left;
		if (right) delete right;
	    } else {
		if (old) delete old;
		if (right) delete right;
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
			    ret = (*b)->Transform(xform);
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

    return ret;
}


/**
 * R T _ C O M B _ B R E P
 */
extern "C" void
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
