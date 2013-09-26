/*                   C O M B _ T R E E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include <sstream>
#include <map>
#include <set>
#include "bu.h"
#include "raytrace.h"
#include "G_STEP_internal.h"
#include "STEPWrapper.h"
#include "ON_Brep.h"
#include "Assembly_Product.h"


STEPentity *
Comb_to_STEP(struct directory *dp, Registry *registry, InstMgr *instance_list) {
    std::ostringstream ss;
    ss << "'" << dp->d_namep << "'";
    std::string str = ss.str();

    // MECHANICAL_CONTEXT
    SdaiMechanical_context *mech_context = (SdaiMechanical_context *)registry->ObjCreate("MECHANICAL_CONTEXT");
    instance_list->Append((STEPentity *)mech_context, completeSE);
    mech_context->name_("''");
    mech_context->discipline_type_("''");

    // APPLICATION_CONTEXT - TODO, should be one of these per file?
    SdaiApplication_context *app_context = (SdaiApplication_context *)registry->ObjCreate("APPLICATION_CONTEXT");
    instance_list->Append((STEPentity *)app_context, completeSE);
    mech_context->frame_of_reference_(app_context);
    app_context->application_("'CONFIGURATION CONTROLLED 3D DESIGNS OF MECHANICAL PARTS AND ASSEMBLIES'");

    // DESIGN_CONTEXT - TODO, should be one of these per file?
    SdaiDesign_context *design_context = (SdaiDesign_context *)registry->ObjCreate("DESIGN_CONTEXT");
    instance_list->Append((STEPentity *)design_context, completeSE);
    design_context->name_("''");
    design_context->life_cycle_stage_("'design'");
    design_context->frame_of_reference_(app_context);

    // PRODUCT_DEFINITION
    SdaiProduct_definition *prod_def = (SdaiProduct_definition *)registry->ObjCreate("PRODUCT_DEFINITION");
    instance_list->Append((STEPentity *)prod_def, completeSE);
    prod_def->id_("''");
    prod_def->description_("''");
    prod_def->frame_of_reference_(design_context);

    // PRODUCT_DEFINITION_FORMATION
    SdaiProduct_definition_formation *prod_def_form = (SdaiProduct_definition_formation *)registry->ObjCreate("PRODUCT_DEFINITION_FORMATION");
    instance_list->Append((STEPentity *)prod_def_form, completeSE);
    prod_def->formation_(prod_def_form);
    prod_def_form->id_("''");
    prod_def_form->description_("''");

    // PRODUCT
    SdaiProduct *prod = (SdaiProduct *)registry->ObjCreate("PRODUCT");
    instance_list->Append((STEPentity *)prod, completeSE);
    prod_def_form->of_product_(prod);
    prod->id_("''");
    prod->name_(str.c_str());
    prod->description_("''");
    prod->frame_of_reference_()->AddNode(new EntityNode((SDAI_Application_instance *)mech_context));

    return (STEPentity *)prod_def;
}

STEPentity *
Comb_Tree_to_STEP(struct directory *dp, struct rt_wdb *wdbp, Registry *registry, InstMgr *instance_list)
{
    STEPentity *toplevel_comb = NULL;

    std::map<struct directory *, STEPentity *> *brep_to_step = new std::map<struct directory *, STEPentity *>;
    std::map<struct directory *, STEPentity *> *comb_to_step = new std::map<struct directory *, STEPentity *>;
    std::set<struct directory *> non_wrapper_combs;


    /* Find all solids, make instances of them, insert them, and stick in *dp to STEPentity* map */
    const char *brep_search = "-type brep";
    struct bu_ptbl *breps = db_search_path_obj(brep_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(breps) - 1; j >= 0; j--){
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(breps, j);
	struct rt_db_internal brep_intern;
	rt_db_get_internal(&brep_intern, curr_dp, wdbp->dbip, bn_mat_identity, &rt_uniresource);
	RT_CK_DB_INTERNAL(&brep_intern);
	(*brep_to_step)[curr_dp] = ON_BRep_to_STEP(curr_dp, &brep_intern, registry, instance_list);
	bu_log("Brep: %s\n", curr_dp->d_namep);
    }

    /* Find all combs that are not already brep wrappers, make instances of them, insert them, and stick in *dp to STEPentity* map */
    const char *comb_search = "-type comb ! ( -nnodes 1 -below=1 -type brep )";
    struct bu_ptbl *combs = db_search_path_obj(comb_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(combs) - 1; j >= 0; j--){
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(combs, j);
	(*comb_to_step)[curr_dp] = Comb_to_STEP(curr_dp, registry, instance_list);
	non_wrapper_combs.insert(curr_dp);
	bu_log("Comb non-wrapper: %s\n", curr_dp->d_namep);
    }

    /* Find the combs that *are* wrappers, and point them to the product associated with their brep.
     * Change the name of the product, if appropriate */
    const char *comb_wrapper_search = "-type comb -nnodes 1 -below=1 -type brep";
    const char *comb_child_search = "-mindepth 1 -type brep";
    struct bu_ptbl *comb_wrappers = db_search_path_obj(comb_wrapper_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(comb_wrappers) - 1; j >= 0; j--){
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(comb_wrappers, j);
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
	if (!(curr_node->tr_l.tl_mat)) {
	    std::ostringstream ss;
	    ss << "'" << curr_dp->d_namep << "'";
	    std::string str = ss.str();
	    (*comb_to_step)[curr_dp] = brep_to_step->find(child)->second;
	    bu_log("Comb wrapper: %s\n", curr_dp->d_namep);
	    ((SdaiProduct_definition *)((*comb_to_step)[curr_dp]))->formation_()->of_product_()->name_(str.c_str());
	}else{
	    (*comb_to_step)[curr_dp] = Comb_to_STEP(curr_dp, registry, instance_list);
	    non_wrapper_combs.insert(curr_dp);
	    bu_log("Comb non-wrapper (matrix over primitive): %s\n", curr_dp->d_namep);
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
	Add_Assembly_Product((*it), wdbp->dbip, comb_children, comb_to_step, brep_to_step, registry, instance_list);
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
