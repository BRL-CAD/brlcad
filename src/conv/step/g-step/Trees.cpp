/*                   T R E E S . C P P
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
/** @file Trees.cpp
 *
 * File for writing out the tree structure associated with a comb
 * into the STEPcode containers
 *
 */

#include "AP203.h"
#include "bu.h"
#include "ON_Brep.h"
#include "Assembly_Product.h"
#include "Comb.h"
#include "Trees.h"
#include "G_Objects.h"

STEPentity *
Comb_Tree_to_STEP(struct directory *dp, struct rt_wdb *wdbp, AP203_Contents *sc)
{
    STEPentity *toplevel_comb = NULL;

    std::set<struct directory *> non_wrapper_combs;


    /* Find all solids, make instances of them, insert them, and stick in *dp to
     * STEPentity* map.
     *
     * NOTE: Right now, without boolean evaluations, we need all primitive
     * solid objects as BReps.  Once we *do* have boolean objects, this logic will
     * change - probably to making solids out of regions.  Ironically, the AP203 logic
     * for combs and solids as it exists here will most likely be preserved by
     * moving it to AP214, where it will still be needed for boolean exports*/
    const char *solid_search = "! -type comb";
    struct bu_ptbl breps = BU_PTBL_INIT_ZERO;

    (void)db_search(&breps, DB_SEARCH_RETURN_UNIQ_DP, solid_search, 1, &dp, wdbp->dbip);
    for (int j = (int)BU_PTBL_LEN(&breps) - 1; j >= 0; j--) {
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(&breps, j);
	struct rt_db_internal solid_intern;

	rt_db_get_internal(&solid_intern, curr_dp, wdbp->dbip, bn_mat_identity, &rt_uniresource);
	RT_CK_DB_INTERNAL(&solid_intern);
	Object_To_STEP(curr_dp, &solid_intern, wdbp, sc);
	rt_db_free_internal(&solid_intern);
    }
    db_search_free(&breps);

    /* Find all combs that are not already wrappers, make instances of them, insert
     * them, and stick in *dp to STEPentity* map.
     *
     * NOTE: Once we have boolean evaluations, we will need to change this to search
     * for assemblies (i.e. combs with no regions above them) and regions, which
     * will become the "wrappers" for the evaluated brep solid below each region.
     * Again, some form of this logic will probably end up in AP214 */
    const char *comb_search = "-type comb";
    struct bu_ptbl combs = BU_PTBL_INIT_ZERO;

    (void)db_search(&combs, DB_SEARCH_RETURN_UNIQ_DP, comb_search, 1, &dp, wdbp->dbip);
    for (int j = (int)BU_PTBL_LEN(&combs) - 1; j >= 0; j--) {
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(&combs, j);
	int is_wrapper = !Comb_Is_Wrapper(curr_dp, wdbp);

	if (sc->comb_to_step->find(curr_dp) == sc->comb_to_step->end()) {
	    if (!is_wrapper) {
		STEPentity *comb_shape;
		STEPentity *comb_product;

		Comb_to_STEP(curr_dp, sc, &comb_shape, &comb_product);
		(*sc->comb_to_step)[curr_dp] = comb_product;
		(*sc->comb_to_step_shape)[curr_dp] = comb_shape;
		non_wrapper_combs.insert(curr_dp);
	    } else {
		struct rt_db_internal comb_intern;
		struct rt_comb_internal *comb;
		struct directory *child;
		union tree *curr_node;

		rt_db_get_internal(&comb_intern, curr_dp, wdbp->dbip, bn_mat_identity, &rt_uniresource);
		RT_CK_DB_INTERNAL(&comb_intern);
		comb = (struct rt_comb_internal *)(comb_intern.idb_ptr);
		child = Comb_Get_Only_Child(curr_dp, wdbp);

		curr_node = db_find_named_leaf(comb->tree, child->d_namep);
		if (curr_node && (sc->solid_to_step->find(child) != sc->solid_to_step->end())) {
		    std::ostringstream ss;
		    ss << "'" << curr_dp->d_namep << "'";
		    std::string str = ss.str();
		    (*sc->comb_to_step)[curr_dp] = sc->solid_to_step->find(child)->second;
		    (*sc->comb_to_step_shape)[curr_dp] = sc->solid_to_step_shape->find(child)->second;
		    //bu_log("Comb wrapper: %s\n", curr_dp->d_namep);
		    ((SdaiProduct_definition *)((*sc->comb_to_step)[curr_dp]))->formation_()->of_product_()->name_(str.c_str());
		}
	    }
	}
    }
    db_search_free(&combs);

    /* For each non-wrapper comb, get list of immediate children and call Assembly Product,
     * which will define the relationships between the comb and its children using
     * the appropriate step foo and the pointers in the map.*/

    /* TODO - need to figure out how to pull matrices, translate them into STEP, and
     * where to associate them.*/
    for (std::set<struct directory *>::iterator it=non_wrapper_combs.begin(); it != non_wrapper_combs.end(); ++it) {
	const char *comb_children_search = "-mindepth 1 -maxdepth 1";

	struct bu_ptbl comb_children = BU_PTBL_INIT_ZERO;
	struct directory *ccs_dp = (*it);

       	(void)db_search(&comb_children, DB_SEARCH_RETURN_UNIQ_DP, comb_children_search, 1, &ccs_dp, wdbp->dbip);
	Add_Assembly_Product((*it), wdbp->dbip, &comb_children, sc);
	db_search_free(&comb_children);
    }

    return toplevel_comb;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
