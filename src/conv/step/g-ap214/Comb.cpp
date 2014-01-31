/*                      C O M B . C P P
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
 * File for writing out a combination object into the STEPcode
 * containers.
 *
 * Note that this *only* creates the set of entities that correspond
 * to the comb object itself, without relating it to its children.
 *
 */

#include "common.h"
#include "AP_Common.h"
#include "bu.h"
#include "raytrace.h"
#include "ON_Brep.h"
#include "Assembly_Product.h"
#include "Comb.h"
#include "Default_Geometric_Context.h"
#include "Shape_Representation.h"

void
Comb_to_STEP(struct directory *dp, AP203_Contents *sc, STEPentity **shape, STEPentity **product) {
    std::ostringstream ss;
    ss << "'" << dp->d_namep << "'";
    std::string str = ss.str();

    STEPcomplex *context = (STEPcomplex *)sc->default_context;

    // PRODUCT_DEFINITION
    SdaiProduct_definition *prod_def = (SdaiProduct_definition *)sc->registry->ObjCreate("PRODUCT_DEFINITION");
    sc->instance_list->Append((STEPentity *)prod_def, completeSE);
    prod_def->id_(str.c_str());
    prod_def->description_("''");

    // PRODUCT_DEFINITION_FORMATION
    SdaiProduct_definition_formation *prod_def_form = (SdaiProduct_definition_formation *)sc->registry->ObjCreate("PRODUCT_DEFINITION_FORMATION");
    sc->instance_list->Append((STEPentity *)prod_def_form, completeSE);
    prod_def->formation_(prod_def_form);
    prod_def_form->id_("''");
    prod_def_form->description_("''");

    // PRODUCT
    SdaiProduct *prod = (SdaiProduct *)sc->registry->ObjCreate("PRODUCT");
    sc->instance_list->Append((STEPentity *)prod, completeSE);
    prod_def_form->of_product_(prod);
    prod->id_(str.c_str());
    prod->name_(str.c_str());
    prod->description_(str.c_str());

    // PRODUCT_DEFINITION_SHAPE
    SdaiProduct_definition_shape *pshape = (SdaiProduct_definition_shape *)sc->registry->ObjCreate("PRODUCT_DEFINITION_SHAPE");
    sc->instance_list->Append((STEPentity *)pshape, completeSE);
    pshape->name_("''");
    pshape->description_("'Comb shape definition'");
    SdaiCharacterized_product_definition *cpd = new SdaiCharacterized_product_definition(prod_def);
    pshape->definition_(new SdaiCharacterized_definition(cpd));

    // SHAPE_DEFINITION_REPRESENTATION
    SdaiShape_definition_representation *shape_def_rep = (SdaiShape_definition_representation*)sc->registry->ObjCreate("SHAPE_DEFINITION_REPRESENTATION");
    sc->instance_list->Append((STEPentity *)shape_def_rep, completeSE);

    // SHAPE_REPRESENTATION
    SdaiShape_representation* shape_rep = (SdaiShape_representation *)Add_Shape_Representation(sc, (SdaiRepresentation_context *) context);
    sc->instance_list->Append((STEPentity *)shape_rep, completeSE);
    shape_def_rep->used_representation_(shape_rep);
    shape_rep->name_("''");

    (*product) = (STEPentity *)prod_def;
    (*shape) = (STEPentity *)shape_rep;
}

HIDDEN union tree *
_db_tree_get_child(union tree *tp) {
    union tree *ret;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {

	case OP_DB_LEAF:
	    return tp;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    ret = _db_tree_get_child(tp->tr_b.tb_left);
	    if (ret != TREE_NULL) return ret;
	    return _db_tree_get_child(tp->tr_b.tb_right);

	default:
	    bu_log("_db_tree_get_child: bad op %d\n", tp->tr_op);
	    bu_bomb("_db_tree_get_child\n");
    }
    return TREE_NULL;

}

/* Convenience function to get a comb child's directory pointer
 * when it the comb only has one child */
struct directory *
Comb_Get_Only_Child(struct directory *dp, struct rt_wdb *wdbp){
    struct rt_db_internal comb_intern;
    struct rt_comb_internal *comb;
    union tree *child;
    struct directory *child_dp;
    int node_count = 0;

    rt_db_get_internal(&comb_intern, dp, wdbp->dbip, bn_mat_identity, &rt_uniresource);
    RT_CK_DB_INTERNAL(&comb_intern);
    if (comb_intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_COMBINATION) {
	rt_db_free_internal(&comb_intern);
	return RT_DIR_NULL;
    }

    /* If this comb has more than one child, there exists no "only" child */
    comb = (struct rt_comb_internal *)comb_intern.idb_ptr;
    if (comb->tree) {
	node_count = db_count_tree_nodes(comb->tree, 0);
	if (node_count > 1) {
	    rt_db_free_internal(&comb_intern);
	    return RT_DIR_NULL;
	}
    } else {
	/* Empty comb - there exists no "only" child */
	return RT_DIR_NULL;
    }

    /* If the child doesn't exist, there exists no "only" child, so at
     * this point whatever db_lookup returns is fine */
    child = _db_tree_get_child(comb->tree);
    child_dp = db_lookup(wdbp->dbip, child->tr_l.tl_name, LOOKUP_QUIET);
    rt_db_free_internal(&comb_intern);
    return child_dp;
}

/* A "wrapping" combination is a combination that contains a single object
 * and does not apply a matrix to it */
int
Comb_Is_Wrapper(struct directory *dp, struct rt_wdb *wdbp){
    struct rt_db_internal comb_intern;
    struct rt_db_internal comb_child_intern;
    struct rt_comb_internal *comb;
    union tree *child;
    struct directory *child_dp;
    int node_count = 0;

    rt_db_get_internal(&comb_intern, dp, wdbp->dbip, bn_mat_identity, &rt_uniresource);
    RT_CK_DB_INTERNAL(&comb_intern);
    if (comb_intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_COMBINATION) {
	rt_db_free_internal(&comb_intern);
	return -1;
    }

    /* If this comb has more than one child, it isn't a wrapper */
    comb = (struct rt_comb_internal *)comb_intern.idb_ptr;
    if (comb->tree) {
	node_count = db_count_tree_nodes(comb->tree, 0);
	if (node_count > 1) {
	    rt_db_free_internal(&comb_intern);
	    return 1;
	}
    } else {
	/* Empty comb */
	return -2;
    }

    /* If the child doesn't exist, this isn't a wrapper */
    child = _db_tree_get_child(comb->tree);
    child_dp = db_lookup(wdbp->dbip, child->tr_l.tl_name, LOOKUP_QUIET);
    if (child_dp == RT_DIR_NULL) {
	rt_db_free_internal(&comb_intern);
	return 1;
    }

    /* If the child is a comb, this isn't a wrapper */
    rt_db_get_internal(&comb_child_intern, child_dp, wdbp->dbip, bn_mat_identity, &rt_uniresource);
    RT_CK_DB_INTERNAL(&comb_child_intern);
    if (comb_child_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	rt_db_free_internal(&comb_intern);
	rt_db_free_internal(&comb_child_intern);
	return 1;
    }

    /* If the child has a matrix over it, this isn't a wrapper */
    if (child->tr_l.tl_mat) {
	rt_db_free_internal(&comb_intern);
	rt_db_free_internal(&comb_child_intern);
	return 1;
    }

    /* If we made it here, we have a wrapper */
    rt_db_free_internal(&comb_intern);
    rt_db_free_internal(&comb_child_intern);
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
