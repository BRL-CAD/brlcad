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

#include "AP214e3.h"
#include "G_Objects.h"

struct AP214_info {
    const struct db_i *dbip;
    std::map<struct directory *, STEPentity *> objects;
    std::map<std::pair<STEPentity *,STEPentity *>, STEPentity *> union_boolean_results;
    std::map<std::pair<STEPentity *,STEPentity *>, STEPentity *> intersection_boolean_results;
    std::map<std::pair<STEPentity *,STEPentity *>, STEPentity *> subtraction_boolean_results;
};

HIDDEN int
conv_tree(struct directory **d, int depth, int parent_branch, struct directory **solid, STEPentity **stepobj, const union tree *t, AP203_Contents *sc)
{
    struct directory *left = NULL, *right = NULL, *old = NULL;
    STEPentity *leftstep = NULL;
    STEPentity *rightstep = NULL;
    int ret = 0;
    int left_ret = 0;
    int right_ret = 0;
    if (depth > 0) bu_log("%*s", depth, "");
    bu_log("Processing: %s, depth: %d", (*d)->d_namep, depth);
    if (parent_branch == 1) bu_log(", branch = right\n");
    if (parent_branch == 2) bu_log(", branch = left\n");
    if (parent_branch == 0) bu_log("\n");
    switch (t->tr_op) {
        case OP_UNION:
        case OP_INTERSECT:
        case OP_SUBTRACT:
        case OP_XOR:
            /* convert right */
            ret = conv_tree(d, depth+1, 1, &right, &rightstep, t->tr_b.tb_right, sc);
	    right_ret = ret;
	    if (ret == -1) {
		break;
            }
	    /* fall through */
        case OP_NOT:
        case OP_GUARD:
        case OP_XNOP:
            /* convert left */
            ret = conv_tree(d, depth+1, 2, &left, &leftstep, t->tr_b.tb_left, sc);
	    left_ret = ret;
            if (ret == -1) {
		break;
	    }
	    if (depth > 0) bu_log("%*s", depth, "");
	    bu_log("Returning boolean_result: ");
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
	    if (right_ret == 1) bu_log("solid\n");
	    if (right_ret == 2) bu_log("boolean_result\n");
	    if (right_ret == 3) bu_log("comb(boolean_result)\n");

	    /* If we've got something here, anything above this is a boolean_result */
	    ret = 2;
            break;
        case OP_DB_LEAF:
            {
                char *name = t->tr_l.tl_name;
                struct directory *dir;
                dir = db_lookup(sc->dbip, name, LOOKUP_QUIET);
		if (dir != RT_DIR_NULL) {
		    struct rt_db_internal intern;
		    rt_db_get_internal(&intern, dir, sc->dbip, bn_mat_identity, &rt_uniresource);
		    if (intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
			RT_CK_DB_INTERNAL(&intern);
			struct rt_comb_internal *comb = (struct rt_comb_internal *)(intern.idb_ptr);
			RT_CK_COMB(comb);
			if (comb->tree == NULL) {
			    /* Probably should return empty object... */
			    return NULL;
			} else {
			    if (depth > 0) bu_log("%*s", depth, "");
			    if (sc->comb_to_step->find(dir) != sc->comb_to_step->end()) {
				bu_log("Combination object %s already exists - returning\n", dir->d_namep);
			    } else {
				/* TODO - if a comb has only one solid under it, return that solid for the
				 * comb */
				int tree_construct = conv_tree(&dir, depth+1, 0, NULL, stepobj, comb->tree, sc);
				if (tree_construct == 1) ret = 1;
				if (tree_construct != 1) ret = 3;
				bu_log("Returning comb object's boolean_representation %s (%d)\n", dir->d_namep, tree_construct);
				sc->comb_to_step->insert(std::make_pair(dir, *stepobj));
			    }
			}
		    } else {
			/* TODO - if we have a matrix being applied to a solid, investigate
			 * solid_replica in AP214 */
			if (solid) (*solid) = dir;
			if (sc->solid_to_step->find(dir) != sc->solid_to_step->end()) {
			    if (depth > 0) bu_log("%*s", depth, "");
			    bu_log("Solid object %s already exists - Returning\n", dir->d_namep);
			} else {
			    if (depth > 0) bu_log("%*s", depth, "");
			    bu_log("Returning solid object %s\n", dir->d_namep);
			    Object_To_STEP(dir, &intern, sc->wdbp, sc);
			}
			if (stepobj) (*stepobj) = (*(sc->solid_to_step->find(dir))).second;
			std::cout << "Step entity in stepobj: " << (*stepobj)->EntityName() << "\n";
			ret = 1;
		    }
                } else {
		    if (depth > 0) bu_log("%*s", depth, "");
		    bu_log("Cannot find leaf %s.\n", name);
                    ret = -1;
                }
                break;
            }
        default:
	    if (depth > 0) bu_log("%*s", depth, "");
	    bu_log("OPCODE NOT IMPLEMENTED: %d\n", t->tr_op);
            ret = -1;
    }

    return ret;
}


void
Comb_Tree_to_STEP(struct directory *dp, struct rt_wdb *wdbp, AP203_Contents *sc)
{
    size_t i = 0;
    //TODO - need to build sets of regions and assemblies.  Each region tree should
    //be fed into the tree walker above and return a representation item which will
    //henceforth define the shape of that region object.  Items *above* regions will
    //be handled the way such combs are handled in AP203, with any appropriate
    //modifications for AP214

    /* Once we have the list of assemblies, search for a variety of db problems - be sure
     * there are no non-union booleans above assembly comb, make sure there aren't any stray
     * solids floating around in assembly boolean structures, and look for nested regions.
     * (The first two tests are the real reason
     * we need a full list of all paths instead of just the unique set of assembly
     * combs - we generate that *after* these results have been vetted) */

    const char *no_nonsub_region_below_region_search = "-type region -above -type region ! -bool -";
    struct bu_ptbl *problem_regions;
    BU_ALLOC(problem_regions, struct bu_ptbl);
    (void)db_search(problem_regions, no_nonsub_region_below_region_search, 1, &dp, wdbp, DB_SEARCH_TREE);
    if (BU_PTBL_LEN(problem_regions) > 0) {
	bu_log("Error - found nested regions within the .g file: \n");
	for(i = 0; i < BU_PTBL_LEN(problem_regions); i++) {
	    struct db_full_path *dfp = (struct db_full_path *)BU_PTBL_GET(problem_regions, i);
	    bu_log("   %s\n", db_path_to_string(dfp));
	}
	db_free_search_tbl(problem_regions);
	bu_exit(1, "problem regions");
    } else {
	db_free_search_tbl(problem_regions);
    }

    /* Look for any assembly objects with problems - probably should break this out for reporting purposes... */
    struct bu_ptbl *invalid_assemblies;
    BU_ALLOC(invalid_assemblies, struct bu_ptbl);
    const char *invalid_assembly_under_region =      "-below -type region ! -type region -above -type region";
    (void)db_search(invalid_assemblies, invalid_assembly_under_region, 1, &dp, wdbp, DB_SEARCH_TREE);
    const char *invalid_assembly_includes_non_comb = "-below -type region ! -type region -below=1 -type shape";
    (void)db_search(invalid_assemblies, invalid_assembly_includes_non_comb, 1, &dp, wdbp, DB_SEARCH_TREE);
    const char *invalid_assembly_below_subtract =    "-below -type region ! -type region -below=1 -bool -";
    (void)db_search(invalid_assemblies, invalid_assembly_below_subtract, 1, &dp, wdbp, DB_SEARCH_TREE);
    const char *invalid_assembly_below_intersect =   "-below -type region ! -type region -below=1 -bool +";
    (void)db_search(invalid_assemblies, invalid_assembly_below_intersect, 1, &dp, wdbp, DB_SEARCH_TREE);
    const char *invalid_assembly_above_subtract =    "-below -type region ! -type region -above -bool -";
    (void)db_search(invalid_assemblies, invalid_assembly_above_subtract, 1, &dp, wdbp, DB_SEARCH_TREE);
    const char *invalid_assembly_above_intersect =   "-below -type region ! -type region -above -bool +";
    (void)db_search(invalid_assemblies, invalid_assembly_above_intersect, 1, &dp, wdbp, DB_SEARCH_TREE);
    if (BU_PTBL_LEN(invalid_assemblies) > 0) {
	bu_log("Error - found invalid assemblies within the .g file: \n");
	for(i = 0; i < BU_PTBL_LEN(problem_regions); i++) {
	    struct db_full_path *dfp = (struct db_full_path *)BU_PTBL_GET(invalid_assemblies, i);
	    bu_log("   %s\n", db_path_to_string(dfp));
	}
	db_free_search_tbl(invalid_assemblies);
	bu_exit(1, "problem assemblies");
    } else {
	db_free_search_tbl(invalid_assemblies);
    }

    /* Get the list of region objects */
    const char *region_search = "-type region";
    struct bu_ptbl *regions;
    BU_ALLOC(regions, struct bu_ptbl);
    (void)db_search(regions, region_search, 1, &dp, wdbp, DB_SEARCH_RETURN_UNIQ_DP);

    if (BU_PTBL_LEN(regions) > 0) {
	for (i = 0; i < BU_PTBL_LEN(regions); i++) {
	    struct directory *rdp = (struct directory *)BU_PTBL_GET(regions, i);
	    if (sc->comb_to_step->find(rdp) != sc->comb_to_step->end()) {
		bu_log("Region object %s already exists - skipping\n", rdp->d_namep);
	    } else {
		struct rt_db_internal comb_intern;
		sc->dbip = wdbp->dbip;
		sc->wdbp = wdbp;
		rt_db_get_internal(&comb_intern, rdp, sc->dbip, bn_mat_identity, &rt_uniresource);
		RT_CK_DB_INTERNAL(&comb_intern);
		struct rt_comb_internal *comb = (struct rt_comb_internal *)(comb_intern.idb_ptr);
		RT_CK_COMB(comb);
		if (comb->tree == NULL) {
		    /* Probably should return empty object... */
		} else {
		    (void)conv_tree(&rdp, 0, 0, NULL, NULL, comb->tree, sc);
		}
	    }
	}
    }

    /* Get the list of assembly objects */
    const char *assembly_search = "-below -type region ! -type region";
    struct bu_ptbl *assemblies;
    BU_ALLOC(assemblies, struct bu_ptbl);
    (void)db_search(assemblies, assembly_search, 1, &dp, wdbp, DB_SEARCH_RETURN_UNIQ_DP);

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
