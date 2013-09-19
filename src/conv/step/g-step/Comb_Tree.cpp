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

#include <map>
#include "bu.h"
#include "raytrace.h"
#include "STEPWrapper.h"
#include "ON_Brep.h"

STEPentity *
Comb_to_STEP(struct directory *dp, Registry *registry, InstMgr *instance_list) {
    // MECHANICAL_CONTEXT
    SdaiMechanical_context *mech_context = (SdaiMechanical_context *)registry->ObjCreate("MECHANICAL_CONTEXT");
    instance_list->Append((STEPentity *)mech_context, completeSE);
    mech_context->name_("''");
    mech_context->discipline_type_("''");

    // APPLICATION_CONTEXT - TODO, should be one of these per file?
    SdaiApplication_context *app_context = (SdaiApplication_context *)registry->ObjCreate("APPLICATION_CONTEXT");
    instance_list->Append((STEPentity *)app_context, completeSE);
    mech_context->frame_of_reference_(app_context);
    app_context->application_("''");

    // PRODUCT_DEFINITION
    SdaiProduct_definition *prod_def = (SdaiProduct_definition *)registry->ObjCreate("PRODUCT_DEFINITION");
    instance_list->Append((STEPentity *)prod_def, completeSE);
    prod_def->id_("''");
    prod_def->description_("''");

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
    prod->name_(dp->d_namep);
    prod->description_("''");
    prod->frame_of_reference_()->AddNode(new EntityNode((SDAI_Application_instance *)mech_context));

    return (STEPentity *)prod_def;
}

STEPentity *
Comb_Tree_to_STEP(struct directory *dp, struct rt_wdb *wdbp, struct rt_db_internal *intern, Registry *registry, InstMgr *instance_list)
{
    STEPentity *toplevel_comb = NULL;

    std::map<struct directory *, STEPentity *> brep_to_step;
    std::map<struct directory *, STEPentity *> comb_to_step;

    struct rt_comb_internal *comb = (struct rt_comb_internal *)(intern->idb_ptr);

    /* Find all solids, make instances of them, insert them, and stick in *dp to STEPentity* map */
    const char *brep_search = "-type brep";
    struct bu_ptbl *breps = db_search_path_obj(brep_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(breps) - 1; j >= 0; j--){
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(breps, j);
	struct rt_db_internal brep_intern;
	rt_db_get_internal(&brep_intern, curr_dp, wdbp->dbip, bn_mat_identity, &rt_uniresource);
	RT_CK_DB_INTERNAL(&brep_intern);
	brep_to_step[curr_dp] = ON_BRep_to_STEP(curr_dp, &brep_intern, registry, instance_list);
	bu_log("Brep: %s\n", curr_dp->d_namep);
    }

    /* Find all combs that are not already brep wrappers, make instances of them, insert them, and stick in *dp to STEPentity* map */
    const char *comb_search = "-type comb ! ( -nnodes 1 -below=1 -type brep )";
    struct bu_ptbl *combs = db_search_path_obj(comb_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(combs) - 1; j >= 0; j--){
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(combs, j);
	comb_to_step[curr_dp] = Comb_to_STEP(curr_dp, registry, instance_list);
	bu_log("Comb non-wrapper: %s\n", curr_dp->d_namep);
    }

    /* Find the combs that *are* wrappers, and point them to the product associated with their brep.
     * Change the name of the product, if appropriate */
    const char *comb_wrapper_search = "-type comb -nnodes 1 -below=1 -type brep";
    const char *comb_child_search = "-type comb -nnodes 1 -below=1 -type brep";
    struct bu_ptbl *comb_wrappers = db_search_path_obj(comb_wrapper_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(comb_wrappers) - 1; j >= 0; j--){
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(comb_wrappers, j);
	struct bu_ptbl *comb_child = db_search_path_obj(comb_child_search, curr_dp, wdbp);
	struct directory *child = (struct directory *)BU_PTBL_GET(comb_child, 0);
	bu_ptbl_free(comb_child);
	bu_free(comb_child, "free search result");
	comb_to_step[curr_dp] = brep_to_step.find(child)->second;
	bu_log("Comb wrapper: %s\n", curr_dp->d_namep);
    }

    /* For each non-wrapper comb, get list of immediate children and call Assembly Product,
     * which will define the relationships between the comb and its children using
     * the appropriate step foo and the pointers in the map.*/

    /* TODO - need to figure out how to pull matrices, translate them into STEP, and
     * where to associate them.*/
    struct bu_ptbl *comb_children = db_search_path_obj(comb_child_search, dp, wdbp);
    for (int j = (int)BU_PTBL_LEN(comb_children) - 1; j >= 0; j--){
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(comb_children, j);
	bu_log("%s under %s: ", curr_dp->d_namep, dp->d_namep);
	union tree *curr_node = db_find_named_leaf(comb->tree, curr_dp->d_namep);
	matp_t curr_matrix = curr_node->tr_l.tl_mat;
	if(curr_matrix) {
	    bn_mat_print(curr_dp->d_namep, curr_matrix);
	} else {
	    bu_log("identity matrix\n");
	}
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
