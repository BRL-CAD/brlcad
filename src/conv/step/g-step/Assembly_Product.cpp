/*            A S S E M B L Y _ P R O D U C T . C P P
 *
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
/** @file Assembly_Product.cpp
 *
 */

#include "AP_Common.h"
#include "STEPGeneratedAPI.h"
#include "Comb.h"
#include "Shape_Representation_Relationship.h"

/*
 * To associate multiple objects in STEP, we must define and relate product definitions
 * for both shapes (BReps) and assembly objects (combs) that will relate the shapes.
 * Below is an example of how the structural definitions relate - in this instance, one
 * BRep and two instances of that BRep moved and added to an assembly.
 *
 * // Representation context
 * #10=DIMENSIONAL_EXPONENTS(1.E0,0.E0,0.E0,0.E0,0.E0,0.E0,0.E0);
 * #11=(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT(.MILLI.,.METRE.));
 * #12=LENGTH_MEASURE_WITH_UNIT(LENGTH_MEASURE(1E1),#11);
 * #13=(CONVERSION_BASED_UNIT('INCH',#12)LENGTH_UNIT()NAMED_UNIT(#10));
 * #14=DIMENSIONAL_EXPONENTS(0.E0,0.E0,0.E0,0.E0,0.E0,0.E0,0.E0);
 * #15=(NAMED_UNIT(*)PLANE_ANGLE_UNIT()SI_UNIT($,.RADIAN.));
 * #16=PLANE_ANGLE_MEASURE_WITH_UNIT(PLANE_ANGLE_MEASURE(0.0174532925199433),#15);
 * #17=(CONVERSION_BASED_UNIT('DEGREE',#16)NAMED_UNIT(*)PLANE_ANGLE_UNIT());
 * #18=(NAMED_UNIT(*)SI_UNIT($,.STERADIAN.)SOLID_ANGLE_UNIT());
 * #19=UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(0.05),#13,'accuracy','Tolerance');
 * #20=(GEOMETRIC_REPRESENTATION_CONTEXT(3)GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT
 * ((#19))GLOBAL_UNIT_ASSIGNED_CONTEXT((#13,#17,#18))REPRESENTATION_CONTEXT('ID52','3'));
 *
 * #100=APPLICATION_CONTEXT('CONFIGURATION CONTROLLED 3D DESIGNS OF MECHANICAL PARTS AND ASSEMBLIES');
 * #1001=DESIGN_CONTEXT('',#100,'design');
 *
 * // seems to be a list of AXIS2_PLACEMENT_3D objects - does this put all the axis
 * // in a representation context?  Used by the REPRESENTATION_RELATIONSHIP objects.
 * #300=SHAPE_REPRESENTATION('',(...,#9004,#9013,...), #20);
 *
 * // Transformation definitions - I think these are similar to matrices over combs?
 *
 * #8001=CARTESIAN_POINT('',(0.E0,0.E0,0.E0));
 * #8002=DIRECTION('',(0.E0,0.E0,1.E0));
 * #8003=DIRECTION('',(1.E0,0.E0,0.E0));
 * #8004=AXIS2_PLACEMENT_3D('',#8001,#8002,#8003);
 *
 * #9001=CARTESIAN_POINT('',());
 * #9002=DIRECTION('',());
 * #9003=DIRECTION('',());
 * #9004=AXIS2_PLACEMENT_3D('',#9001,#9002,#9003);
 * #7001=ITEM_DEFINED_TRANSFORMATION('','',#8004,#9004);
 *
 * #9010=CARTESIAN_POINT('',());
 * #9011=DIRECTION('',());
 * #9012=DIRECTION('',());
 * #9013=AXIS2_PLACEMENT_3D('',#9010,#9011,#9012);
 * #7002=ITEM_DEFINED_TRANSFORMATION('','',#8004,#9013);
 *
 * // Solid shape representation  Looks like this is the bit that needs to be created
 * // for each solid - should be done by the SHAPE_DEFINITION_REPRESENTATION logic.
 * #200=ADVANCED_BREP_SHAPE_REPRESENTATION();
 * #1002=MECHANICAL_CONTEXT('',#100,'mechanical');
 * #1003=PRODUCT('Geometric_Shape_01','Geometric_Shape_01','NOT SPECIFIED',(#1002));
 * #1004=PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('1','LAST_VERSION',#1003,.MADE.);
 * #1005=PRODUCT_DEFINITION('design','',#1004,#1001);
 * #1006=PRODUCT_DEFINITION_SHAPE('','Shape For Geometric_Shape_01',#1005);
 * #1007=SHAPE_DEFINITION_REPRESENTATION(#1006,#200);
 *
 * // Assembly shape representation
 * #5001=MECHANICAL_CONTEXT('',#100,'mechanical');
 * #5002=PRODUCT('Comb_01','Comb_01','NOT SPECIFIED',(#5001));
 * #5003=PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('1','LAST_VERSION',#5002,.MADE.);
 * #5004=PRODUCT_DEFINITION('design','',#5003,#1001);
 *
 * // Assembly components
 *
 * #2001=NEXT_ASSEMBLY_USAGE_OCCURRENCE('01','Next assembly relationship','Geometric_Shape_01',#5004,#1005,$);
 * #2002=PRODUCT_DEFINITION_SHAPE('Placement #01','Placement of Geometric_Shape_01 with respect to Comb_01',#2001);
 *
 * #3001=NEXT_ASSEMBLY_USAGE_OCCURRENCE('02','Next assembly relationship','Geometric_Shape_01',#5004,#1005,$);
 * #3002=PRODUCT_DEFINITION_SHAPE('Placement #02','Placement of Geometric_Shape_01 with respect to Comb_01',#3001);
 *
 * #4001=(REPRESENTATION_RELATIONSHIP('','',#200,#300)REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION(#7002)SHAPE_REPRESENTATION_RELATIONSHIP());
 * #4002=CONTEXT_DEPENDENT_SHAPE_REPRESENTATION(#4001,#3002);
 *
 * #6001=(REPRESENTATION_RELATIONSHIP('','',#200,#300)REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION(#7001)SHAPE_REPRESENTATION_RELATIONSHIP());
 * #6002=CONTEXT_DEPENDENT_SHAPE_REPRESENTATION(#6001,#2002);
 *
 *
 * Mapping from BRL-CAD to STEP
 *
 * Roughly speaking, the STEP requirement that each shape representation have an associated product appears
 * to translate to BRL-CAD as each brep object having a comb above it.  To avoid introducing unnecessary combs,
 * it will be necessary to recognize when a brep referenced by one or more comb trees listed for export has
 * one and only one parent comb in the existing tree structure, which in turn does not have any other children.
 * Otherwise, repeated imports and exports will bury the brep below an ever deepening hierarchy of combs.  When
 * a brep does not have this wrapper already in place, one will need to be explicitly created for it.
 * Hopefully, search can be used to quickly identify sets of geometry that need to be handled differently.
 * It may be that the assumption of shared naming - a comb with the same root name as its child brep - can be used
 * as a quick check to determine if the comb is a parent or not, if we accept that as a convention.
 *
 * Because combs reference combs in BRL-CAD, the step product definitions associated with combs will need to be
 * fully created before the assembly usage occurrences can start to be assembled.  A search that collects all combs
 * will provide a convenient list for multiple passes that create and then assemble combs, as will a similar search
 * for solids (currently just breps...).  Should also make sure the tree is union only for AP203, where booleans are
 * not supported. Once boolean evaluation is in place, should be able to default to brep solids for combs with non
 * union booleans below them.
 *
 */

/* TODO - generic, move to the internal utility file */
STEPentity *
Create_AXIS2_PLACEMENT_3D(fastf_t pt_x, fastf_t pt_y, fastf_t pt_z,
	fastf_t d1_x, fastf_t d1_y, fastf_t d1_z,
	fastf_t d2_x, fastf_t d2_y, fastf_t d2_z,
	Registry *registry, InstMgr *instance_list) {
    STEPentity *pnt = brlcad::step::CreateEntity(registry, instance_list,
	    "CARTESIAN_POINT");
    brlcad::step::SetString(pnt, "name", "");
    XYZ_to_Cartesian_point(pt_x, pt_y, pt_z, pnt);
    STEPentity *axis = brlcad::step::CreateEntity(registry, instance_list,
	    "DIRECTION");
    brlcad::step::SetString(axis, "name", "");
    XYZ_to_Direction(d1_x, d1_y, d1_z, axis);
    STEPentity *ref = brlcad::step::CreateEntity(registry, instance_list,
	    "DIRECTION");
    brlcad::step::SetString(ref, "name", "");
    XYZ_to_Direction(d2_x, d2_y, d2_z, ref);
    STEPentity *placement = brlcad::step::CreateEntity(registry, instance_list,
	    "AXIS2_PLACEMENT_3D");
    brlcad::step::SetString(placement, "name", "");
    brlcad::step::SetEntity(placement, "location", pnt);
    brlcad::step::SetEntity(placement, "axis", axis);
    brlcad::step::SetEntity(placement, "ref_direction", ref);
    return placement;
}

STEPentity *
Identity_AXIS2_PLACEMENT_3D(Registry *registry, InstMgr *instance_list) {
    return Create_AXIS2_PLACEMENT_3D(0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, registry, instance_list);
}

STEPentity *
Create_CARTESIAN_TRANSFORMATION_OPERATOR_3D(
	fastf_t pt_x, fastf_t pt_y, fastf_t pt_z,
	fastf_t d1_x, fastf_t d1_y, fastf_t d1_z,
	fastf_t d2_x, fastf_t d2_y, fastf_t d2_z,
	fastf_t d3_x, fastf_t d3_y, fastf_t d3_z,
	fastf_t scale,
	Registry *registry, InstMgr *instance_list)
{
    STEPentity *op3d = brlcad::step::CreateEntity(registry, instance_list,
	    "CARTESIAN_TRANSFORMATION_OPERATOR_3D");
    STEPentity *axis1 = brlcad::step::CreateEntity(registry, instance_list,
	    "DIRECTION");
    STEPentity *axis2 = brlcad::step::CreateEntity(registry, instance_list,
	    "DIRECTION");
    STEPentity *axis3 = brlcad::step::CreateEntity(registry, instance_list,
	    "DIRECTION");
    STEPentity *local_origin = brlcad::step::CreateEntity(registry,
	    instance_list, "CARTESIAN_POINT");
    XYZ_to_Cartesian_point(pt_x, pt_y, pt_z, local_origin);
    brlcad::step::SetString(local_origin, "name", "local_origin");
    XYZ_to_Direction(d1_x, d1_y, d1_z, axis1);
    brlcad::step::SetString(axis1, "name", "axis1");
    XYZ_to_Direction(d2_x, d2_y, d2_z, axis2);
    brlcad::step::SetString(axis2, "name", "axis2");
    XYZ_to_Direction(d3_x, d3_y, d3_z, axis3);
    brlcad::step::SetString(axis3, "name", "axis3");

    brlcad::step::SetEntity(op3d, "local_origin", local_origin);
    brlcad::step::SetEntity(op3d, "axis1", axis1);
    brlcad::step::SetEntity(op3d, "axis2", axis2);
    brlcad::step::SetEntity(op3d, "axis3", axis3);
    brlcad::step::SetReal(op3d, "scale", scale);

    /* For whatever reason, we seem to a) have TWO attributes called "name"
     * that need to be set and b) a "description" attribute that doesn't respond
     * to setting via op3d->description_("''") - fall back on attribute list
     * access */
    op3d->ResetAttributes();
    STEPattribute * attr = op3d->NextAttribute();
    while ( attr != 0 ) {
	if (!bu_strcmp(attr->Name(), "name")) attr->StrToVal("''");
	if (!bu_strcmp(attr->Name(), "description")) attr->StrToVal("''");
	attr = op3d->NextAttribute();
    }
    return op3d;
}

// Returns either an AXIS2_PLACEMENT_3D, a CARTESIAN_TRANSFORMATION_OPERATOR_3D, or NULL
// If NULL, geometry is being deformed in a way not supported by AP203
//
// Note:  After some experimentation with CARTESIAN_TRANSFORMATION_OPERATOR_3D did not
// succeed in generating useful imports, it looks like the limitation expressed in the
// STEP PDM Schema usage guide may hold in this context:
// http://www.steptools.com/support/stdev_docs/express/pdm/pdmug_release4_3.pdf
//
// "With regard to the transformations in the context of assembly, a part is in
//  principle incorporated in the assembly only by rigid motion (i.e.,
//  translation and/or rotation) excluding mirroring and scaling."
//
// What this means is that any scaling operations in BRL-CAD's matrices are not
// going to be expressible in STEP, because it is an "in-principle" conflict with
// how STEP views assemblies.

STEPentity *
Mat_to_Rep(matp_t curr_matrix, double mm_to_length_unit,
    Registry *registry, InstMgr *instance_list)
{
    point_t origin, outorig;
    vect_t x_axis, y_axis, z_axis;
    vect_t outx, outy, outz;
    VSET(origin, 0, 0, 0);
    VSET(x_axis, 1, 0, 0);
    VSET(y_axis, 0, 1, 0);
    VSET(z_axis, 0, 0, 1);
    MAT4X3PNT(outorig, curr_matrix, origin);
    MAT4X3VEC(outx, curr_matrix, x_axis);
    MAT4X3VEC(outy, curr_matrix, y_axis);
    MAT4X3VEC(outz, curr_matrix, z_axis);
    VUNITIZE(outx);
    VUNITIZE(outy);
    VUNITIZE(outz);
    VSCALE(outorig, outorig, mm_to_length_unit);

    // If we aren't scaling, handle things with axis placement
    if (NEAR_ZERO(curr_matrix[15] - 1.0, VUNITIZE_TOL)) {
	return Create_AXIS2_PLACEMENT_3D(
		outorig[0], outorig[1], outorig[2],
		outz[0], outz[1], outz[2],
		outx[0], outx[1], outx[2],
		registry, instance_list);
    }

    // Have scaling, which STEP doesn't support in assemblies
    return NULL;
}

// Representation relationships are a complex type
STEPentity *
Build_Representation_Relationship(STEPentity *input_transformation, STEPentity *parent, STEPentity *child, Registry *registry, InstMgr *instance_list) {
    STEPattribute *attr;
    STEPcomplex *stepcomplex;
    const char *entNmArr[4] = {"representation_relationship", "representation_relationship_with_transformation", "shape_representation_relationship", "*"};
    STEPcomplex *complex_entity = new STEPcomplex(registry, (const char **)entNmArr, registry->GetEntityCnt() + 1);
    /* REPRESENTATION_RELATIONSHIP */
    stepcomplex = complex_entity->EntityPart("representation_relationship");
    stepcomplex->ResetAttributes();
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	//std::cout << "  " << attr->Name() << "," << attr->NonRefType() << "\n";
	if (!bu_strcmp(attr->Name(), "name")) attr->StrToVal("''");
	if (!bu_strcmp(attr->Name(), "description")) attr->StrToVal("''");
    }
    if (!Set_Representation_Relationship_Reference(stepcomplex, "rep_1",
	    parent, instance_list) ||
	    !Set_Representation_Relationship_Reference(stepcomplex, "rep_2",
	    child, instance_list)) {
	delete complex_entity;
	return NULL;
    }

    /* REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION */
    stepcomplex = complex_entity->EntityPart("representation_relationship_with_transformation");
    stepcomplex->ResetAttributes();
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	if (!bu_strcmp(attr->Name(), "transformation_operator")) {
	    SDAI_Select *transformation = attr->Select();
	    if (transformation) transformation->SetEntity(input_transformation);
	}
    }

    instance_list->Append((STEPentity *)complex_entity, completeSE);

    return complex_entity;
}

namespace {

static bool
union_only_tree(const union tree *node)
{
    if (!node) return false;
    switch (node->tr_op) {
	case OP_DB_LEAF:
	    return true;
	case OP_UNION:
	    return union_only_tree(node->tr_b.tb_left) &&
		union_only_tree(node->tr_b.tb_right);
	default:
	    return false;
    }
}

static STEPentity *
mapped_shape(struct directory *dp, AP203_Contents *sc)
{
    std::map<struct directory *, STEPentity *>::const_iterator comb =
	sc->comb_to_step_shape->find(dp);
    if (comb != sc->comb_to_step_shape->end() && comb->second)
	return comb->second;
    std::map<struct directory *, STEPentity *>::const_iterator solid =
	sc->solid_to_step_shape->find(dp);
    return solid == sc->solid_to_step_shape->end() ? NULL : solid->second;
}

static STEPentity *
mapped_product(struct directory *dp, AP203_Contents *sc)
{
    std::map<struct directory *, STEPentity *>::const_iterator comb =
	sc->comb_to_step->find(dp);
    if (comb != sc->comb_to_step->end() && comb->second)
	return comb->second;
    std::map<struct directory *, STEPentity *>::const_iterator solid =
	sc->solid_to_step->find(dp);
    return solid == sc->solid_to_step->end() ? NULL : solid->second;
}

static bool
emit_occurrence(struct directory *parent_dp, STEPentity *parent_shape,
    STEPentity *parent_product, const union tree *node,
    struct db_i *dbip, AP203_Contents *sc, size_t occurrence_ordinal,
    size_t &usage_number)
{
    struct directory *child_dp = db_lookup(dbip, node->tr_l.tl_name, LOOKUP_QUIET);
    if (child_dp == RT_DIR_NULL) {
	bu_log("ERROR: omitting assembly member %s/%s because the child does not exist\n",
	    parent_dp->d_namep, node->tr_l.tl_name);
	return false;
    }
    STEPentity *child_shape = mapped_shape(child_dp, sc);
    STEPentity *child_product = mapped_product(child_dp, sc);
    if (!child_shape || !child_product) {
	bu_log("ERROR: omitting assembly member %s/%s because the child has "
	    "no STEP representation\n", parent_dp->d_namep, child_dp->d_namep);
	return false;
    }

    STEPentity *curr_transform = NULL;
    matp_t curr_matrix = node->tr_l.tl_mat;
	if (curr_matrix) {
	    curr_transform = Mat_to_Rep(curr_matrix, sc->mm_to_length_unit,
		sc->registry, sc->instance_list);
	} else {
	    curr_transform = Identity_AXIS2_PLACEMENT_3D(sc->registry, sc->instance_list);
	}
    if (!curr_transform) {
	bu_log("\nA matrix with a scaling component is present in the following comb relationship:\n  %s/%s\nScaling is not supported by STEP in assembly structures - to export this structure, consider using\npush or xpush to remove the scaling matrices from the hierarchy.\n", parent_dp->d_namep, child_dp->d_namep);
	return false;
    }

    STEPentity *orig_transform =
	Identity_AXIS2_PLACEMENT_3D(sc->registry, sc->instance_list);
    STEPentity *item_transform = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "ITEM_DEFINED_TRANSFORMATION");
    brlcad::step::SetString(item_transform, "name", "");
    brlcad::step::SetString(item_transform, "description", "");
    brlcad::step::SetEntity(item_transform, "transform_item_1", curr_transform);
    brlcad::step::SetEntity(item_transform, "transform_item_2", orig_transform);

    STEPentity *usage = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "NEXT_ASSEMBLY_USAGE_OCCURRENCE");
    std::ostringstream occurrence_id;
    occurrence_id << ++usage_number;
    brlcad::step::SetString(usage, "id", occurrence_id.str().c_str());
    brlcad::step::SetString(usage, "name", "");
    brlcad::step::SetString(usage, "description", "");
    brlcad::step::SetString(usage, "reference_designator",
	    occurrence_id.str().c_str());
    brlcad::step::SetEntity(usage, "relating_product_definition", parent_product);
    brlcad::step::SetEntity(usage, "related_product_definition", child_product);
    if (sc->occurrence_to_step)
	(*sc->occurrence_to_step)[std::make_pair(parent_dp, occurrence_ordinal)] =
	    usage;

    STEPentity *pshape = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "PRODUCT_DEFINITION_SHAPE");
    brlcad::step::SetString(pshape, "name", "");
    brlcad::step::SetString(pshape, "description", "");
    brlcad::step::SetEntity(pshape, "definition", usage);
    STEPentity *rep_rel = Build_Representation_Relationship(item_transform,
	parent_shape, child_shape, sc->registry, sc->instance_list);
    if (!rep_rel) {
	bu_log("ERROR: could not create the representation relationship for %s/%s\n",
	    parent_dp->d_namep, child_dp->d_namep);
	return false;
    }
    STEPentity *cshape = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION");
    brlcad::step::SetEntity(cshape, "representation_relation", rep_rel);
    brlcad::step::SetEntity(cshape, "represented_product_relation", pshape);
    return true;
}

static bool
emit_representation_membership(struct directory *parent_dp,
    STEPentity *parent_shape, const union tree *node, struct db_i *dbip,
    AP203_Contents *sc)
{
    struct directory *child_dp = db_lookup(dbip, node->tr_l.tl_name,
	LOOKUP_QUIET);
    STEPentity *child_shape = child_dp == RT_DIR_NULL ? NULL :
	mapped_shape(child_dp, sc);
    if (!child_shape) {
	bu_log("ERROR: omitting product representation member %s/%s because "
	    "the child has no STEP shape representation\n", parent_dp->d_namep,
	    node->tr_l.tl_name);
	return false;
    }

    if (!node->tr_l.tl_mat || bn_mat_is_identity(node->tr_l.tl_mat))
	return Add_Shape_Representation_Relationship(sc, parent_shape,
	    child_shape) != NULL;

    STEPentity *member_axis = node->tr_l.tl_mat ?
	Mat_to_Rep(node->tr_l.tl_mat, sc->mm_to_length_unit,
	    sc->registry, sc->instance_list) :
	Identity_AXIS2_PLACEMENT_3D(sc->registry, sc->instance_list);
    STEPentity *representation_axis =
	Identity_AXIS2_PLACEMENT_3D(sc->registry, sc->instance_list);
    if (!member_axis || !representation_axis) {
	bu_log("ERROR: product representation member %s/%s has a transform "
	    "which STEP representation relationships cannot preserve\n",
	    parent_dp->d_namep, child_dp->d_namep);
	return false;
    }
    STEPentity *transformation = brlcad::step::CreateEntity(sc->registry,
	sc->instance_list, "ITEM_DEFINED_TRANSFORMATION");
    brlcad::step::SetString(transformation, "name", "");
    brlcad::step::SetString(transformation, "description", "");
    brlcad::step::SetEntity(transformation, "transform_item_1", member_axis);
    brlcad::step::SetEntity(transformation, "transform_item_2",
	representation_axis);
    return Build_Representation_Relationship(transformation, parent_shape,
	child_shape, sc->registry, sc->instance_list) != NULL;
}

static bool
emit_union_tree(struct directory *parent_dp, STEPentity *parent_shape,
    STEPentity *parent_product, const union tree *node,
    struct db_i *dbip, AP203_Contents *sc, size_t &occurrence_ordinal,
    size_t &usage_number)
{
    if (node->tr_op == OP_DB_LEAF) {
	const size_t ordinal = ++occurrence_ordinal;
	const bool representation_membership =
	    sc->representation_memberships &&
	    sc->representation_memberships->find(
		std::make_pair(parent_dp, ordinal)) !=
		sc->representation_memberships->end();
	if (representation_membership)
	    return emit_representation_membership(parent_dp, parent_shape, node,
		dbip, sc);
	return emit_occurrence(parent_dp, parent_shape, parent_product, node,
	    dbip, sc, ordinal, usage_number);
    }
    bool left = emit_union_tree(parent_dp, parent_shape, parent_product,
	node->tr_b.tb_left, dbip, sc, occurrence_ordinal, usage_number);
    bool right = emit_union_tree(parent_dp, parent_shape, parent_product,
	node->tr_b.tb_right, dbip, sc, occurrence_ordinal, usage_number);
    return left && right;
}

} // namespace

bool
Add_Assembly_Product(struct directory *dp, struct db_i *dbip, AP203_Contents *sc)
{
    std::map<struct directory *, STEPentity *>::const_iterator parent_entry =
	sc->comb_to_step_shape->find(dp);
    STEPentity *parent_product = mapped_product(dp, sc);
    if (parent_entry == sc->comb_to_step_shape->end() || !parent_entry->second ||
	!parent_product) {
	bu_log("ERROR: assembly %s has no STEP product or shape representation\n",
	    dp->d_namep);
	return false;
    }

    struct rt_db_internal comb_intern;
    if (rt_db_get_internal(&comb_intern, dp, dbip, bn_mat_identity) < 0)
	return false;
    RT_CK_DB_INTERNAL(&comb_intern);
    struct rt_comb_internal *comb =
	(struct rt_comb_internal *)(comb_intern.idb_ptr);
    if (!union_only_tree(comb->tree)) {
	bu_log("WARNING: assembly relationships for %s were omitted because its "
	    "boolean tree is not union-only\n", dp->d_namep);
	rt_db_free_internal(&comb_intern);
	return false;
    }
    size_t occurrence_ordinal = 0;
    size_t usage_number = 0;
    bool result = emit_union_tree(dp, parent_entry->second, parent_product,
	comb->tree, dbip, sc, occurrence_ordinal, usage_number);
    rt_db_free_internal(&comb_intern);
    return result;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
