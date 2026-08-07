/*                   T R E E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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

#include "AP_Common.h"
#include "STEPGeneratedAPI.h"
#include "bu/log.h"
#include "ON_Brep.h"
#include "Assembly_Product.h"
#include "Comb.h"
#include "Trees.h"
#include "G_Objects.h"

#include <set>
#include <vector>

namespace {

/** Collect only the product/assembly side of a selected hierarchy.  A
 * combination already present in comb_to_step is a schema-native CSG shape
 * boundary prepared by the export host; its operands belong to that shape and
 * must not also become orphan STEP products. */
static bool
collect_ordinary_products(struct directory *dp, struct rt_wdb *wdbp,
    AP203_Contents *sc, std::set<struct directory *> &solids,
    std::set<struct directory *> &geometry_solids,
    std::set<struct directory *> &combinations,
    std::set<struct directory *> &active, bool geometry_only)
{
    if (!dp) return false;
    if (!(dp->d_flags & RT_DIR_COMB)) {
	if (geometry_only)
	    geometry_solids.insert(dp);
	else
	    solids.insert(dp);
	return true;
    }
    if (sc->comb_to_step->find(dp) != sc->comb_to_step->end())
	return true;
    if (!active.insert(dp).second) {
	bu_log("ERROR: combination cycle while planning STEP products at %s\n",
	    dp->d_namep);
	return false;
    }
    combinations.insert(dp);
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (rt_db_get_internal(&internal, dp, wdbp->dbip, bn_mat_identity) < 0) {
	active.erase(dp);
	return false;
    }
    const struct rt_comb_internal *combination =
	static_cast<const struct rt_comb_internal *>(internal.idb_ptr);
    bool valid = combination && combination->tree;
    std::vector<const union tree *> pending;
    size_t occurrence_number = 0;
    if (valid) pending.push_back(combination->tree);
    while (!pending.empty()) {
	const union tree *node = pending.back();
	pending.pop_back();
	if (!node) {
	    valid = false;
	    continue;
	}
	if (node->tr_op == OP_DB_LEAF) {
	    struct directory *child = db_lookup(wdbp->dbip,
		node->tr_l.tl_name, LOOKUP_QUIET);
	    const size_t ordinal = ++occurrence_number;
	    const bool representation_membership =
		sc->representation_memberships &&
		sc->representation_memberships->find(std::make_pair(dp, ordinal)) !=
		    sc->representation_memberships->end();
	    if (child == RT_DIR_NULL || !collect_ordinary_products(child, wdbp,
		    sc, solids, geometry_solids, combinations, active,
		    geometry_only || representation_membership))
		valid = false;
	    continue;
	}
	if (node->tr_op == OP_UNION || node->tr_op == OP_INTERSECT ||
		node->tr_op == OP_SUBTRACT || node->tr_op == OP_XOR) {
	    pending.push_back(node->tr_b.tb_right);
	    pending.push_back(node->tr_b.tb_left);
	    continue;
	}
	valid = false;
    }
    rt_db_free_internal(&internal);
    active.erase(dp);
    return valid;
}

} // namespace

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
    std::set<struct directory *> ordinary_solids;
    std::set<struct directory *> geometry_solids;
    std::set<struct directory *> ordinary_combinations;
    std::set<struct directory *> active;
    (void)collect_ordinary_products(dp, wdbp, sc, ordinary_solids,
	geometry_solids, ordinary_combinations, active, false);
    for (std::set<struct directory *>::const_iterator solid =
	    geometry_solids.begin(); solid != geometry_solids.end(); ++solid) {
	struct directory *curr_dp = *solid;
	struct rt_db_internal solid_intern;
	mat_t output_scale;
	MAT_IDN(output_scale);
	output_scale[15] = sc->length_unit_mm;

	rt_db_get_internal(&solid_intern, curr_dp, wdbp->dbip, output_scale);
	RT_CK_DB_INTERNAL(&solid_intern);
	Object_Geometry_To_STEP(curr_dp, &solid_intern, wdbp, sc);
	rt_db_free_internal(&solid_intern);
    }
    for (std::set<struct directory *>::const_iterator solid =
	    ordinary_solids.begin(); solid != ordinary_solids.end(); ++solid) {
	struct directory *curr_dp = *solid;
	struct rt_db_internal solid_intern;
	mat_t output_scale;
	MAT_IDN(output_scale);
	output_scale[15] = sc->length_unit_mm;

	rt_db_get_internal(&solid_intern, curr_dp, wdbp->dbip, output_scale);
	RT_CK_DB_INTERNAL(&solid_intern);
	Object_To_STEP(curr_dp, &solid_intern, wdbp, sc);
	rt_db_free_internal(&solid_intern);
    }

    /* Find all combs that are not already wrappers, make instances of them, insert
     * them, and stick in *dp to STEPentity* map.
     *
     * NOTE: Once we have boolean evaluations, we will need to change this to search
     * for assemblies (i.e. combs with no regions above them) and regions, which
     * will become the "wrappers" for the evaluated brep solid below each region.
     * Again, some form of this logic will probably end up in AP214 */
    for (std::set<struct directory *>::const_iterator combination =
	    ordinary_combinations.begin();
	 combination != ordinary_combinations.end(); ++combination) {
	struct directory *curr_dp = *combination;
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

		if (rt_db_get_internal(&comb_intern, curr_dp, wdbp->dbip,
			bn_mat_identity) < 0)
		    continue;
		RT_CK_DB_INTERNAL(&comb_intern);
		comb = (struct rt_comb_internal *)(comb_intern.idb_ptr);
		child = Comb_Get_Only_Child(curr_dp, wdbp);

		curr_node = child ? db_find_named_leaf(comb->tree, child->d_namep) : NULL;
		if (curr_node && (sc->solid_to_step->find(child) != sc->solid_to_step->end())) {
		    std::ostringstream ss;
		    ss << "'" << curr_dp->d_namep << "'";
		    std::string str = ss.str();
		    (*sc->comb_to_step)[curr_dp] = sc->solid_to_step->find(child)->second;
		    (*sc->comb_to_step_shape)[curr_dp] = sc->solid_to_step_shape->find(child)->second;
		    std::map<struct directory *, STEPentity *>::const_iterator manifold =
			sc->solid_to_step_manifold->find(child);
		    if (manifold != sc->solid_to_step_manifold->end())
			(*sc->comb_to_step_manifold)[curr_dp] = manifold->second;
		    //bu_log("Comb wrapper: %s\n", curr_dp->d_namep);
		    STEPentity *formation = brlcad::step::Entity(
			(*sc->comb_to_step)[curr_dp], "formation");
		    STEPentity *product = brlcad::step::Entity(formation,
			"of_product");
		    brlcad::step::SetString(product, "name", str.c_str());
		}
		rt_db_free_internal(&comb_intern);
	    }
	}
    }

    /* Products and shape representations must all exist before their assembly
     * relationships are emitted.  Walk the actual boolean trees in the second
     * pass: a unique-directory search loses repeated occurrences and cannot
     * associate the correct matrix with each occurrence. */
    for (std::set<struct directory *>::iterator it=non_wrapper_combs.begin(); it != non_wrapper_combs.end(); ++it) {
	(void)Add_Assembly_Product((*it), wdbp->dbip, sc);
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
