/*                   C O M B _ T R E E . C P P
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

#include "common.h"
#include <sstream>
#include <map>
#include <set>
#include "bu.h"
#include "raytrace.h"
#include "AP203.h"
#include "STEPWrapper.h"
#include "ON_Brep.h"
#include "Assembly_Product.h"
#include "Comb.h"
#include "Trees.h"

STEPentity *
Comb_Tree_to_STEP(struct directory *dp, struct rt_wdb *wdbp, Registry *registry, InstMgr *instance_list)
{
    STEPentity *toplevel_comb = NULL;
    struct comb_maps *maps = new comb_maps;

    std::set<struct directory *> non_wrapper_combs;


    /* Find all brep solids, make instances of them, insert them, and stick in *dp to STEPentity* map */
    const char *brep_search = "-type brep";
    struct bu_ptbl *breps = db_search_path_obj(brep_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(breps) - 1; j >= 0; j--){
	STEPentity *brep_shape;
	STEPentity *brep_product;
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(breps, j);
	struct rt_db_internal brep_intern;
	rt_db_get_internal(&brep_intern, curr_dp, wdbp->dbip, bn_mat_identity, &rt_uniresource);
	RT_CK_DB_INTERNAL(&brep_intern);
	struct rt_brep_internal *bi = (struct rt_brep_internal*)(brep_intern.idb_ptr);
	RT_BREP_TEST_MAGIC(bi);
	ON_Brep *brep = bi->brep;
	ON_BRep_to_STEP(curr_dp, brep, registry, instance_list, &brep_shape, &brep_product);
	maps->brep_to_step[curr_dp] = brep_product;
	maps->brep_to_step_shape[curr_dp] = brep_shape;
	bu_log("Brep: %s\n", curr_dp->d_namep);
    }

    /* Find all solids that aren't already breps, convert them, insert them, and stick in dp->STEPentity map */
    /* NOTE: this is a temporary measure until evaluated booleans are working - after that, it will be needed
     * only for unevaluated boolean export via more advanced forms of STEP */
    const char *non_brep_search = "! -type comb ! -type brep";
    struct bu_ptbl *non_breps = db_search_path_obj(non_brep_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(non_breps) - 1; j >= 0; j--){
	STEPentity *brep_shape;
	STEPentity *brep_product;
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(non_breps, j);
	// Get brep form of solid
	// if that fails, try something else like oriented bounding box as a last resort?
	// triangle meshes probably should go as is?
	struct bn_tol tol;
	tol.magic = BN_TOL_MAGIC;
	tol.dist = BN_TOL_DIST;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = SMALL_FASTF;
	tol.para = 1.0 - tol.perp;
	struct rt_db_internal solid_intern;
	rt_db_get_internal(&solid_intern, curr_dp, wdbp->dbip, bn_mat_identity, &rt_uniresource);
	RT_CK_DB_INTERNAL(&solid_intern);
	if (solid_intern.idb_meth->ft_brep != NULL) {
	    ON_Brep *brep_obj = ON_Brep::New();
	    ON_Brep **brep = &brep_obj;
	    solid_intern.idb_meth->ft_brep(brep, &solid_intern, &tol);
	    ON_BRep_to_STEP(curr_dp, *brep, registry, instance_list, &brep_shape, &brep_product);
	    maps->brep_to_step[curr_dp] = brep_product;
	    maps->brep_to_step_shape[curr_dp] = brep_shape;
	    bu_log("solid to Brep: %s\n", curr_dp->d_namep);
	} else {
	    bu_log("WARNING: No Brep representation for solid %s!\n", curr_dp->d_namep);
	}
    }


    /* Find all combs that are not already wrappers, make instances of them, insert them, and stick in *dp to STEPentity* map */
    const char *comb_search = "-type comb ! ( -nnodes 1 -below=1 ! -type comb )";
    struct bu_ptbl *combs = db_search_path_obj(comb_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(combs) - 1; j >= 0; j--){
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(combs, j);
	STEPentity *comb_shape;
	STEPentity *comb_product;
	if (!Comb_Is_Wrapper(curr_dp, wdbp)) bu_log("Awk! %s is a wrapper!\n", curr_dp->d_namep);
	Comb_to_STEP(curr_dp, registry, instance_list, &comb_shape, &comb_product);
	maps->comb_to_step[curr_dp] = comb_product;
	maps->comb_to_step_shape[curr_dp] = comb_shape;
	non_wrapper_combs.insert(curr_dp);
	bu_log("Comb non-wrapper: %s\n", curr_dp->d_namep);
    }

    /* Find the combs that *are* wrappers, and point them to the product associated with their brep.
     * Change the name of the product, if appropriate */
    const char *comb_wrapper_search = "-type comb -nnodes 1 ! ( -below=1 -type comb )";
    const char *comb_child_search = "-mindepth 1 ! -type comb";
    struct bu_ptbl *comb_wrappers = db_search_path_obj(comb_wrapper_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(comb_wrappers) - 1; j >= 0; j--){
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(comb_wrappers, j);
	if (Comb_Is_Wrapper(curr_dp, wdbp)) bu_log("Awk! %s isn't a wrapper!\n", curr_dp->d_namep);
	/* Satisfying the search pattern isn't enough to qualify as a wrapping comb - there
	 * must also be no matrix hanging over the primitive */
	struct rt_db_internal comb_intern;
	rt_db_get_internal(&comb_intern, curr_dp, wdbp->dbip, bn_mat_identity, &rt_uniresource);
	RT_CK_DB_INTERNAL(&comb_intern);
	struct rt_comb_internal *comb = (struct rt_comb_internal *)(comb_intern.idb_ptr);
	struct bu_ptbl *comb_child = db_search_path_obj(comb_child_search, curr_dp, wdbp);
	struct directory *child = (struct directory *)BU_PTBL_GET(comb_child, 0);
	bu_ptbl_free(comb_child);
	bu_free(comb_child, "free search result");
	union tree *curr_node = db_find_named_leaf(comb->tree, child->d_namep);
	if (curr_node) {
	    if (!(curr_node->tr_l.tl_mat)) {
		std::ostringstream ss;
		ss << "'" << curr_dp->d_namep << "'";
		std::string str = ss.str();
		maps->comb_to_step[curr_dp] = maps->brep_to_step.find(child)->second;
		maps->comb_to_step_shape[curr_dp] = maps->brep_to_step_shape.find(child)->second;
		bu_log("Comb wrapper: %s\n", curr_dp->d_namep);
		((SdaiProduct_definition *)(maps->comb_to_step[curr_dp]))->formation_()->of_product_()->name_(str.c_str());
	    } else {
		STEPentity *comb_shape;
		STEPentity *comb_product;
		Comb_to_STEP(curr_dp, registry, instance_list, &comb_shape, &comb_product);
		maps->comb_to_step[curr_dp] = comb_product;
		maps->comb_to_step_shape[curr_dp] = comb_shape;
		non_wrapper_combs.insert(curr_dp);
		bu_log("Comb non-wrapper (matrix over primitive): %s\n", curr_dp->d_namep);
	    }
	} else {
	    bu_log("WARNING: db_find_named_leaf could not find %s in the tree of %s\n", child->d_namep, curr_dp->d_namep);
	}
	rt_db_free_internal(&comb_intern);
    }

    /* For each non-wrapper comb, get list of immediate children and call Assembly Product,
     * which will define the relationships between the comb and its children using
     * the appropriate step foo and the pointers in the map.*/

    /* TODO - need to figure out how to pull matrices, translate them into STEP, and
     * where to associate them.*/
    const char *comb_children_search = "-mindepth 1 -maxdepth 1";
    for (std::set<struct directory *>::iterator it=non_wrapper_combs.begin(); it != non_wrapper_combs.end(); ++it) {
	bu_log("look for matrices in %s\n", (*it)->d_namep);
	struct bu_ptbl *comb_children = db_search_path_obj(comb_children_search, (*it), wdbp);
	Add_Assembly_Product((*it), wdbp->dbip, comb_children, maps, registry, instance_list);
	bu_ptbl_free(comb_children);
	bu_free(comb_children, "free search result");
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
