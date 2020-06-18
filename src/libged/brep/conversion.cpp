/*                  C O N V E R S I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file conversion.cpp
 *
 * Routines for converting non-brep objects and hierarchies to brep
 * objects
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/comb.h"

#include "./ged_brep.h"

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


extern "C" int
brep_conversion(struct rt_db_internal* in, struct rt_db_internal* out, const struct db_i *dbip)
{
    if (out == NULL)
	return -1;

    ON_Brep *brep, *old;
    old = brep = ON_Brep::New();

    int ret;
    if ((ret = single_conversion(in, &brep, dbip)) < 0) {
	delete old;
	return ret;
    }

    struct rt_brep_internal *bip_out;
    BU_ALLOC(bip_out, struct rt_brep_internal);
    bip_out->magic = RT_BREP_INTERNAL_MAGIC;
    bip_out->brep = brep;
    RT_DB_INTERNAL_INIT(out);
    out->idb_ptr = (void *)bip_out;
    out->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    out->idb_meth = &OBJ[ID_BREP];
    out->idb_minor_type = ID_BREP;

    return 0;
}


static int
brep_conversion_tree(const struct db_i *dbip, const union tree *oldtree, union tree *newtree, const char *suffix, struct rt_wdb *wdbp, fastf_t local2mm)
{
    int ret = 0;
    *newtree = *oldtree;
    rt_comb_internal *comb = NULL;
    switch (oldtree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* convert right */
	    comb = new rt_comb_internal;
	    newtree->tr_b.tb_right = new tree;
	    RT_TREE_INIT(newtree->tr_b.tb_right);
	    ret = brep_conversion_tree(dbip, oldtree->tr_b.tb_right, newtree->tr_b.tb_right, suffix, wdbp, local2mm);
	    if (ret) {
		delete newtree->tr_b.tb_right;
		break;
	    }
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* convert left */
	    BU_ALLOC(newtree->tr_b.tb_left, union tree);
	    RT_TREE_INIT(newtree->tr_b.tb_left);
	    ret = brep_conversion_tree(dbip, oldtree->tr_b.tb_left, newtree->tr_b.tb_left, suffix, wdbp, local2mm);
	    if (!ret) {
		comb->tree = newtree;
	    } else {
		delete newtree->tr_b.tb_left;
		delete newtree->tr_b.tb_right;
	    }
	    break;
	case OP_DB_LEAF:
	    char *tmpname;
	    char *oldname;
	    oldname = oldtree->tr_l.tl_name;
	    tmpname = (char*)bu_malloc(strlen(oldname)+strlen(suffix)+1, "char");
	    newtree->tr_l.tl_name = (char*)bu_malloc(strlen(oldname)+strlen(suffix)+1, "char");
	    bu_strlcpy(tmpname, oldname, strlen(oldname)+1);
	    bu_strlcat(tmpname, suffix, strlen(oldname)+strlen(suffix)+1);
	    if (db_lookup(dbip, tmpname, LOOKUP_QUIET) == RT_DIR_NULL) {
		directory *dir;
		dir = db_lookup(dbip, oldname, LOOKUP_QUIET);
		if (dir != RT_DIR_NULL) {
		    rt_db_internal *intern;
		    BU_ALLOC(intern, struct rt_db_internal);
		    rt_db_get_internal(intern, dir, dbip, bn_mat_identity, &rt_uniresource);
		    if (BU_STR_EQUAL(intern->idb_meth->ft_name, "ID_COMBINATION")) {
			ret = brep_conversion_comb(intern, tmpname, suffix, wdbp, local2mm);
			if (ret) {
			    bu_free(tmpname, "char");
			    rt_db_free_internal(intern);
			    break;
			}
			bu_strlcpy(newtree->tr_l.tl_name, tmpname, strlen(tmpname)+1);
			bu_free(tmpname, "char");
			break;
		    }
		    // It's a primitive. If it's a b-rep object, just duplicate it. Otherwise call the
		    // function to convert it to b-rep.
		    ON_Brep** brep;

		    BU_ALLOC(brep, ON_Brep*);

		    if (BU_STR_EQUAL(intern->idb_meth->ft_name, "ID_BREP")) {
			*brep = ((struct rt_brep_internal *)intern->idb_ptr)->brep->Duplicate();
		    } else {
			*brep = ON_Brep::New();
			ret = single_conversion(intern, brep, dbip);
			if (ret == -1) {
			    bu_log("The brep conversion of %s is unsuccessful.\n", oldname);
			    newtree = NULL;
			    bu_free(tmpname, "char");
			    bu_free(brep, "ON_Brep*");
			    break;
			} else if (ret == -2) {
			    ret = wdb_export(wdbp, tmpname, intern->idb_ptr, intern->idb_type, local2mm);
			    if (ret) {
				bu_log("ERROR: failure writing [%s] to disk\n", tmpname);
			    } else {
				bu_log("The conversion of [%s] (type: %s) is skipped. Implicit form remains as %s.\n",
				       oldname, intern->idb_meth->ft_label, tmpname);
				bu_strlcpy(newtree->tr_l.tl_name, tmpname, strlen(tmpname)+1);
			    }
			    bu_free(tmpname, "char");
			    bu_free(brep, "ON_Brep*");
			    break;
			}
		    }
		    if (brep != NULL) {
			rt_brep_internal *bi;
			BU_ALLOC(bi, struct rt_brep_internal);
			bi->magic = RT_BREP_INTERNAL_MAGIC;
			bi->brep = *brep;
			ret = wdb_export(wdbp, tmpname, (void *)bi, ID_BREP, local2mm);
			if (ret) {
			    bu_log("ERROR: failure writing [%s] to disk\n", tmpname);
			} else {
			    bu_log("%s is made.\n", tmpname);
			    bu_strlcpy(newtree->tr_l.tl_name, tmpname, strlen(tmpname)+1);
			}
		    } else {
			bu_log("The brep conversion of %s is unsuccessful.\n", oldname);
			newtree = NULL;
			ret = -1;
		    }
		    bu_free(brep, "ON_Brep*");
		} else {
		    bu_log("Cannot find %s.\n", oldname);
		    newtree = NULL;
		    ret = -1;
		}
	    } else {
		bu_log("%s already exists.\n", tmpname);
		bu_strlcpy(newtree->tr_l.tl_name, tmpname, strlen(tmpname)+1);
	    }
	    bu_free(tmpname, "char");
	    break;
	default:
	    bu_log("OPCODE NOT IMPLEMENTED: %d\n", oldtree->tr_op);
	    ret = -1;
    }
    if (comb)
	delete comb;
    return ret;
}


extern "C" int
brep_conversion_comb(struct rt_db_internal *old_internal, const char *name, const char *suffix, struct rt_wdb *wdbp, fastf_t local2mm)
{
    RT_CK_COMB(old_internal->idb_ptr);
    rt_comb_internal *comb_internal;
    comb_internal = (rt_comb_internal *)old_internal->idb_ptr;
    int ret;
    if (comb_internal->tree == NULL) {
	// Empty tree. Also output an empty comb.
	ret = wdb_export(wdbp, name, comb_internal, ID_COMBINATION, local2mm);
	if (ret)
	    return ret;
	bu_log("%s is made.\n", name);
	return 0;
    }
    RT_CK_TREE(comb_internal->tree);
    union tree *oldtree = comb_internal->tree;
    rt_comb_internal *new_internal;

    BU_ALLOC(new_internal, struct rt_comb_internal);
    *new_internal = *comb_internal;
    BU_ALLOC(new_internal->tree, union tree);
    RT_TREE_INIT(new_internal->tree);

    union tree *newtree = new_internal->tree;

    ret = brep_conversion_tree(wdbp->dbip, oldtree, newtree, suffix, wdbp, local2mm);
    if (!ret) {
	ret = wdb_export(wdbp, name, (void *)new_internal, ID_COMBINATION, local2mm);
    } else {
	bu_free(new_internal->tree, "tree");
	bu_free(new_internal, "rt_comb_internal");
    }
    if (!ret)
	bu_log("%s is made.\n", name);

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
