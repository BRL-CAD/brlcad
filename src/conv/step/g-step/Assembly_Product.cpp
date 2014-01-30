/*            A S S E M B L Y _ P R O D U C T . C P P
 *
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
/** @file Assembly_Product.cpp
 *
 */

#include "AP203.h"
#include "Comb.h"
#include "Trees.h"
#include "ON_Brep.h"

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
    SdaiCartesian_point *pnt = (SdaiCartesian_point *)registry->ObjCreate("CARTESIAN_POINT");
    pnt->name_("''");
    XYZ_to_Cartesian_point(pt_x, pt_y, pt_z, pnt);
    SdaiDirection *axis = (SdaiDirection *)registry->ObjCreate("DIRECTION");
    axis->name_("''");
    XYZ_to_Direction(d1_x, d1_y, d1_z, axis);
    SdaiDirection *ref = (SdaiDirection *)registry->ObjCreate("DIRECTION");
    ref->name_("''");
    XYZ_to_Direction(d2_x, d2_y, d2_z, ref);
    SdaiAxis2_placement_3d *placement = (SdaiAxis2_placement_3d *)registry->ObjCreate("AXIS2_PLACEMENT_3D");
    placement->name_("''");
    placement->location_(pnt);
    placement->axis_(axis);
    placement->ref_direction_(ref);
    instance_list->Append((STEPentity *)pnt, completeSE);
    instance_list->Append((STEPentity *)axis, completeSE);
    instance_list->Append((STEPentity *)ref, completeSE);
    instance_list->Append((STEPentity *)placement, completeSE);
    return (STEPentity *)placement;
}

STEPentity *
Identity_AXIS2_PLACEMENT_3D(Registry *registry, InstMgr *instance_list) {
    return Create_AXIS2_PLACEMENT_3D(0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, registry, instance_list);
}

// Returns either an AXIS2_PLACEMENT_3D, a CARTESIAN_TRANSFORMATION_OPERATOR_3D, or NULL
// If NULL, geometry is being deformed in a way not supported by AP203
STEPentity *
Mat_to_Rep(matp_t curr_matrix, Registry *registry, InstMgr *instance_list)
{
    // TODO - investigate CARTESIAN_TRANSFORMATION_OPERATOR_3D, which seems to
    // allow uniform scaling as well and appears to be compatible with the
    // same slot used for AXIS2_PLACEMENT_3D - may be able to use the latter
    // for simple cases without scaling (i.e. VUNITIZE doesn't change the length
    // of X, Y or Z unit vectors after the matrix is applied), the former when
    // the there is uniform scaling (X, Y and Z lengths all change by the same
    // amount) and in the worst case (non-uniform scaling) will have to create
    // a new Brep and abandon the attempt to preserve the comb-level operation.

    point_t origin, outorig;
    vect_t x_axis, y_axis, z_axis;
    vect_t outx, outy, outz;
    fastf_t xm, ym, zm;
    VSET(origin, 0, 0, 0);
    VSET(x_axis, 1, 0, 0);
    VSET(y_axis, 0, 1, 0);
    VSET(z_axis, 0, 0, 1);
    MAT4X3PNT(outorig, curr_matrix, origin);
    MAT4X3VEC(outx, curr_matrix, x_axis);
    MAT4X3VEC(outy, curr_matrix, y_axis);
    MAT4X3VEC(outz, curr_matrix, z_axis);
    xm = MAGNITUDE(outx);
    ym = MAGNITUDE(outy);
    zm = MAGNITUDE(outz);
    VUNITIZE(outx);
    VUNITIZE(outy);
    VUNITIZE(outz);
    if (NEAR_ZERO(xm - 1.0, RT_LEN_TOL) && NEAR_ZERO(ym - 1.0, RT_LEN_TOL) && NEAR_ZERO(zm - 1.0, RT_LEN_TOL)) {
	bu_log("AXIS2_PLACEMENT_3D cartesian_point: %f, %f, %f\n", -outorig[0], -outorig[1], -outorig[2]);
	bu_log("AXIS2_PLACEMENT_3D axis: %f, %f, %f\n", outz[0], outz[1], outz[2]);
	bu_log("AXIS2_PLACEMENT_3D ref axis: %f, %f, %f\n", outx[0], outx[1], outx[2]);
	return Create_AXIS2_PLACEMENT_3D(-outorig[0], -outorig[1], -outorig[2],
		outz[0], outz[1], outz[2], outx[0], outx[1], outx[2], registry, instance_list);
    } else {
	if (NEAR_ZERO(xm - ym, RT_LEN_TOL) && NEAR_ZERO(xm - zm, RT_LEN_TOL)) {
	    bu_log("CARTESIAN_TRANSFORMATION_OPERATOR_3D local_origin: %f, %f, %f\n", outorig[0], outorig[1], outorig[2]);
	    bu_log("CARTESIAN_TRANSFORMATION_OPERATOR_3D axis1: %f, %f, %f\n", outx[0], outx[1], outx[2]);
	    bu_log("CARTESIAN_TRANSFORMATION_OPERATOR_3D axis2: %f, %f, %f\n", outy[0], outy[1], outy[2]);
	    bu_log("CARTESIAN_TRANSFORMATION_OPERATOR_3D axis3: %f, %f, %f\n", outz[0], outz[1], outz[2]);
	    bu_log("Scaling: %f\n", xm);
	    return NULL;
	} else {
	    return NULL;
	}
    }
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
	if (!bu_strcmp(attr->Name(), "rep_1")) {
	    attr->ptr.c = new (STEPentity *);
	    *(attr->ptr.c) = parent;
	}
	if (!bu_strcmp(attr->Name(), "rep_2")) {
	    attr->ptr.c = new (STEPentity *);
	    *(attr->ptr.c) = child;
	}
    }

    /* REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION */
    stepcomplex = complex_entity->EntityPart("representation_relationship_with_transformation");
    stepcomplex->ResetAttributes();
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	if (!bu_strcmp(attr->Name(), "transformation_operator")) {
	    SdaiTransformation *transformation = new SdaiTransformation((SdaiItem_defined_transformation *)input_transformation);
	    attr->ptr.sh = transformation;
	}
    }

    instance_list->Append((STEPentity *)complex_entity, completeSE);

    return complex_entity;
}

void
Add_Assembly_Product(struct directory *dp, struct db_i *dbip, struct bu_ptbl *children,
	AP203_Contents *sc)
{
    struct rt_db_internal comb_intern;
    STEPentity *parent_shape = sc->comb_to_step_shape.find(dp)->second;
    rt_db_get_internal(&comb_intern, dp, dbip, bn_mat_identity, &rt_uniresource);
    RT_CK_DB_INTERNAL(&comb_intern);
    struct rt_comb_internal *comb = (struct rt_comb_internal *)(comb_intern.idb_ptr);
    for (int j = (int)BU_PTBL_LEN(children) - 1; j >= 0; j--){
	STEPentity *orig_transform = Identity_AXIS2_PLACEMENT_3D(sc->registry, sc->instance_list);
	STEPentity *curr_transform = NULL;
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(children, j);
	STEPentity *child_shape = sc->comb_to_step_shape.find(curr_dp)->second;
	if (!child_shape)
	    child_shape = sc->solid_to_step_shape.find(curr_dp)->second;
	union tree *curr_node = db_find_named_leaf(comb->tree, curr_dp->d_namep);
	matp_t curr_matrix = curr_node->tr_l.tl_mat;
	if(curr_matrix) {
	    bu_log("%s under %s: ", curr_dp->d_namep, dp->d_namep);
	    bu_log(" - found matrix over %s in %s\n", curr_dp->d_namep, dp->d_namep);
	    bn_mat_print(curr_dp->d_namep, curr_matrix);
	    curr_transform = Mat_to_Rep(curr_matrix, sc->registry, sc->instance_list);
	} else {
	    curr_transform = Identity_AXIS2_PLACEMENT_3D(sc->registry, sc->instance_list);
	    //bu_log("identity matrix\n");
	}
	if (curr_transform) {
	    SdaiItem_defined_transformation *item_transform = (SdaiItem_defined_transformation *)sc->registry->ObjCreate("ITEM_DEFINED_TRANSFORMATION");
	    item_transform->name_("''");
	    item_transform->description_("''");
	    item_transform->transform_item_1_((SdaiRepresentation_item_ptr)orig_transform);
	    item_transform->transform_item_2_((SdaiRepresentation_item_ptr)curr_transform);
	    sc->instance_list->Append((STEPentity *)item_transform, completeSE);
	    SdaiNext_assembly_usage_occurrence *usage = (SdaiNext_assembly_usage_occurrence *)sc->registry->ObjCreate("NEXT_ASSEMBLY_USAGE_OCCURRENCE");
	    usage->id_("''");
	    usage->name_("''");
	    usage->description_("''");
	    usage->reference_designator_("''");
	    usage->relating_product_definition_((SdaiProduct_definition *)sc->comb_to_step.find(dp)->second);
	    SdaiProduct_definition *child_def = (SdaiProduct_definition *)sc->comb_to_step.find(curr_dp)->second;
	    if (!child_def)
		child_def = (SdaiProduct_definition *)sc->solid_to_step.find(curr_dp)->second;
	    usage->related_product_definition_(child_def);
	    sc->instance_list->Append((STEPentity *)usage, completeSE);
	    SdaiProduct_definition_shape *pshape = (SdaiProduct_definition_shape *)sc->registry->ObjCreate("PRODUCT_DEFINITION_SHAPE");
	    pshape->name_("''");
	    pshape->description_("''");
	    SdaiCharacterized_product_definition *cpd = new SdaiCharacterized_product_definition(usage);
	    pshape->definition_(new SdaiCharacterized_definition(cpd));
	    sc->instance_list->Append((STEPentity *)pshape, completeSE);
	    STEPentity *rep_rel = Build_Representation_Relationship(item_transform, parent_shape, child_shape, sc->registry, sc->instance_list);
	    SdaiContext_dependent_shape_representation *cshape = (SdaiContext_dependent_shape_representation *)sc->registry->ObjCreate("CONTEXT_DEPENDENT_SHAPE_REPRESENTATION");
	    cshape->representation_relation_((SdaiShape_representation_relationship *)rep_rel);
	    cshape->represented_product_relation_(pshape);
	    sc->instance_list->Append((STEPentity *)cshape, completeSE);
	} else {
	    bu_log("non-uniform scaling detected: %s/%s\n", dp->d_namep, curr_dp->d_namep);
	}
    }
    rt_db_free_internal(&comb_intern);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
