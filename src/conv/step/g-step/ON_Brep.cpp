/*                     O N _ B R E P . C P P
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
/** @file ON_Brep.cpp
 *
 * File for writing out an ON_Brep structure into the STEPcode containers
 *
 */


// Make entity arrays for each of the m_V, m_S, etc arrays and create
// step instances of them, starting with the basic ones.
//
// The array indices in the ON_Brep will correspond to the step entity
// array locations that hold the step version of each entity.
//
// then, need to map the ON_Brep hierarchy to the corresponding STEP
// hierarchy
//
// brep -> advanced_brep_shape_representation
//         manifold_solid_brep
// faces-> closed_shell
//         advanced_face
//
// surface-> bspline_surface_with_knots
//           cartesian_point
//
// outer 3d trim loop ->  face_outer_bound    ->    SdaiFace_outer_bound -> SdaiFace_bound
//                        edge_loop           ->    SdaiEdge_loop
//                        oriented_edge       ->    SdaiOriented_edge
//                        edge_curve          ->    SdaiEdge_curve
//                        bspline_curve_with_knots
//                        vertex_point
//                        cartesian_point
//
// 2d points -> point_on_surface
// 1d points to bound curve -> point_on_curve
//
// 2d trimming curves -> pcurve using point_on_surface? almost doesn't
// look as if there is a good AP203 way to represent 2d trimming
// curves...
//
//
// Note that STEPentity is the same thing as
// SDAI_Application_instance... see src/clstepcore/sdai.h line 220
//

#include "common.h"

#include <sstream>

#include "STEPWrapper.h"

void
ON_3dPoint_to_Cartesian_point(ON_3dPoint *inpnt, SdaiCartesian_point *step_pnt) {
    RealAggregate_ptr coord_vals = step_pnt->coordinates_();
    RealNode *xnode = new RealNode();
    xnode->value = inpnt->x;
    coord_vals->AddNode(xnode);
    RealNode *ynode = new RealNode();
    ynode->value = inpnt->y;
    coord_vals->AddNode(ynode);
    RealNode *znode = new RealNode();
    znode->value = inpnt->z;
    coord_vals->AddNode(znode);
}


void
ON_3dVector_to_Direction(ON_3dVector *invect, SdaiDirection *step_direction) {
    invect->Unitize();
    RealAggregate_ptr coord_vals = step_direction->direction_ratios_();
    RealNode *xnode = new RealNode();
    xnode->value = invect->x;
    coord_vals->AddNode(xnode);
    RealNode *ynode = new RealNode();
    ynode->value = invect->y;
    coord_vals->AddNode(ynode);
    RealNode *znode = new RealNode();
    znode->value = invect->z;
    coord_vals->AddNode(znode);
}


void
ON_NurbsCurveCV_to_EntityAggregate(ON_NurbsCurve *incrv, SdaiB_spline_curve *step_crv, Registry *registry, InstMgr *instance_list) {
    EntityAggregate *control_pnts = step_crv->control_points_list_();
    ON_3dPoint cv_pnt;
    for (int i = 0; i < incrv->CVCount(); i++) {
	SdaiCartesian_point *step_cartesian = (SdaiCartesian_point *)registry->ObjCreate("CARTESIAN_POINT");
	step_cartesian->name_("''");
	instance_list->Append(step_cartesian, completeSE);
	incrv->GetCV(i, cv_pnt);
	ON_3dPoint_to_Cartesian_point(&(cv_pnt), step_cartesian);
	control_pnts->AddNode(new EntityNode((SDAI_Application_instance *)step_cartesian));
    }
}


void
ON_NurbsSurfaceCV_to_GenericAggregate(ON_NurbsSurface *insrf, SdaiB_spline_surface *step_srf, Registry *registry, InstMgr *instance_list) {
    GenericAggregate *control_pnts_lists = step_srf->control_points_list_();
    ON_3dPoint cv_pnt;
    for (int i = 0; i < insrf->CVCount(0); i++) {
	std::ostringstream ss;
	ss << "(";
	for (int j = 0; j < insrf->CVCount(1); j++) {
	    SdaiCartesian_point *step_cartesian = (SdaiCartesian_point *)registry->ObjCreate("CARTESIAN_POINT");
	    step_cartesian->name_("''");
	    instance_list->Append(step_cartesian, completeSE);
	    insrf->GetCV(i, j, cv_pnt);
	    ON_3dPoint_to_Cartesian_point(&(cv_pnt), step_cartesian);
	    if (j != 0) ss << ", ";
	    ss << "#" << ((SDAI_Application_instance *)step_cartesian)->StepFileId();
	}
	ss << ")";
	std::string str = ss.str();
	control_pnts_lists->AddNode(new GenericAggrNode(str.c_str()));
    }
}


void
ON_NurbsCurveKnots_to_Aggregates(ON_NurbsCurve *incrv, SdaiB_spline_curve_with_knots *step_crv)
{
    IntAggregate_ptr knot_multiplicities = step_crv->knot_multiplicities_();
    RealAggregate_ptr knots = step_crv->knots_();
    int i = 0;
    while (i < incrv->KnotCount()) {
	int multiplicity_val = incrv->KnotMultiplicity(i);
	/* Add knot */
	RealNode *knot = new RealNode();
	knot->value = incrv->Knot(i);
	knots->AddNode(knot);

	/* OpenNURBS and STEP have different notions of end knot
	 * multiplicity - see:
	 * http://wiki.mcneel.com/developer/onsuperfluousknot
	 */
	if ((i == 0) || (i == (incrv->KnotCount() - incrv->KnotMultiplicity(0)))) multiplicity_val++;
	/* Set Multiplicity */
	IntNode *multiplicity = new IntNode();
	multiplicity->value = multiplicity_val;
	knot_multiplicities->AddNode(multiplicity);
	i += incrv->KnotMultiplicity(i);
    }
    step_crv->knot_spec_(Knot_type__unspecified);
}


void
ON_NurbsSurfaceKnots_to_Aggregates(ON_NurbsSurface *insrf, SdaiB_spline_surface_with_knots *step_srf)
{
    IntAggregate_ptr u_knot_multiplicities = step_srf->u_multiplicities_();
    IntAggregate_ptr v_knot_multiplicities = step_srf->v_multiplicities_();
    RealAggregate_ptr u_knots = step_srf->u_knots_();
    RealAggregate_ptr v_knots = step_srf->v_knots_();

    /* u knots */
    int i = 0;
    while (i < insrf->KnotCount(0)) {
	int multiplicity_val = insrf->KnotMultiplicity(0, i);
	/* Add knot */
	RealNode *knot = new RealNode();
	knot->value = insrf->Knot(0, i);
	u_knots->AddNode(knot);

	/* OpenNURBS and STEP have different notions of end knot
	 * multiplicity - see:
	 * http://wiki.mcneel.com/developer/onsuperfluousknot
	 */
	if ((i == 0) || (i == (insrf->KnotCount(0) - insrf->KnotMultiplicity(0, 0)))) multiplicity_val++;

	/* Set Multiplicity */
	IntNode *multiplicity = new IntNode();
	multiplicity->value = multiplicity_val;
	u_knot_multiplicities->AddNode(multiplicity);
	i += insrf->KnotMultiplicity(0, i);
    }

    /* v knots */
    i = 0;
    while (i < insrf->KnotCount(1)) {
	int multiplicity_val = insrf->KnotMultiplicity(1, i);
	/* Add knot */
	RealNode *knot = new RealNode();
	knot->value = insrf->Knot(1, i);
	v_knots->AddNode(knot);

	/* OpenNURBS and STEP have different notions of end knot multiplicity -
	 * see http://wiki.mcneel.com/developer/onsuperfluousknot */
	if ((i == 0) || (i == (insrf->KnotCount(1) - insrf->KnotMultiplicity(1, 0)))) multiplicity_val++;

	/* Set Multiplicity */
	IntNode *multiplicity = new IntNode();
	multiplicity->value = multiplicity_val;
	v_knot_multiplicities->AddNode(multiplicity);
	i += insrf->KnotMultiplicity(1, i);
    }
    step_srf->knot_spec_(Knot_type__unspecified);
}


// STEP needs explicit edges corresponding to what in OpenNURBS are the UV space trimming curves
int Add_Edge(ON_BrepTrim *trim, Registry *registry, InstMgr *instance_list, std::vector<STEPentity *> *oriented_edges, std::vector<STEPentity *> *edge_curves, std::vector<STEPentity *> *vertex_pnts) {
    ON_BrepEdge *edge = trim->Edge();
    int i = -1;
    if (edge) {
	std::cout << "Trim " << trim->m_trim_index << " curve: " << edge->EdgeCurveIndexOf() << "\n";
	STEPentity *new_oriented_edge = registry->ObjCreate("ORIENTED_EDGE");
	SdaiOriented_edge *oriented_edge = (SdaiOriented_edge *)new_oriented_edge;
	oriented_edge->name_("''");
	SdaiEdge_curve *e_curve = (SdaiEdge_curve *)edge_curves->at(edge->EdgeCurveIndexOf());
	oriented_edge->edge_element_((SdaiEdge *)e_curve);
	if (trim->m_bRev3d) {
	    oriented_edge->edge_start_(((SdaiVertex *)vertex_pnts->at(edge->Vertex(1)->m_vertex_index)));
	    oriented_edge->edge_end_(((SdaiVertex *)vertex_pnts->at(edge->Vertex(0)->m_vertex_index)));
	    std::cout << "Verts " << edge->Vertex(1)->m_vertex_index << ", " << edge->Vertex(0)->m_vertex_index << "\n";
	} else {
	    oriented_edge->edge_start_(((SdaiVertex *)vertex_pnts->at(edge->Vertex(0)->m_vertex_index)));
	    oriented_edge->edge_end_(((SdaiVertex *)vertex_pnts->at(edge->Vertex(1)->m_vertex_index)));
	    std::cout << "Verts " << edge->Vertex(0)->m_vertex_index << ", " << edge->Vertex(1)->m_vertex_index << "\n";
	}
	oriented_edge->orientation_((Boolean)!trim->m_bRev3d);
	instance_list->Append(new_oriented_edge, completeSE);
	oriented_edges->push_back(new_oriented_edge);
	i = oriented_edges->size() - 1;
    }
    return i;
}


STEPcomplex *
Add_Default_Geometric_Context(Registry *registry, InstMgr *instance_list)
{

    int instance_cnt = 0;
    STEPattribute *attr;
    STEPcomplex *stepcomplex;

    /* Uncertainty measure with unit */
    SdaiUncertainty_measure_with_unit *uncertainty = (SdaiUncertainty_measure_with_unit *)registry->ObjCreate("UNCERTAINTY_MEASURE_WITH_UNIT");
    uncertainty->name_("'DISTANCE_ACCURACY_VALUE'");
    uncertainty->description_("'Threshold below which geometry imperfections (such as overlaps) are not considered errors.'");
    instance_list->Append(uncertainty, completeSE);
    instance_cnt++;

    /** unit component of uncertainty measure with unit */
    const char *unitNmArr[4] = {"length_unit", "named_unit", "si_unit", "*"};
    STEPcomplex *unit_complex = new STEPcomplex(registry, (const char **)unitNmArr, instance_cnt);
    instance_list->Append((STEPentity *)unit_complex, completeSE);
    instance_cnt++;
    stepcomplex = unit_complex->head;
    while (stepcomplex) {
	if (!bu_strcmp(stepcomplex->EntityName(), "Si_Unit")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "prefix")) attr->ptr.e = new SdaiSi_prefix_var(Si_prefix__milli);
		if (!bu_strcmp(attr->Name(), "name")) attr->ptr.e = new SdaiSi_unit_name_var(Si_unit_name__metre);
	    }
	}
	stepcomplex = stepcomplex->sc;
    }

    SdaiUnit *new_unit = new SdaiUnit((SdaiNamed_unit *)unit_complex);
    uncertainty->ResetAttributes();
    {
	while ((attr = uncertainty->NextAttribute()) != NULL) {
	    if (!bu_strcmp(attr->Name(), "unit_component")) attr->ptr.sh = new_unit;
	    if (!bu_strcmp(attr->Name(), "value_component")) attr->StrToVal("0.05");
	}
    }

    /* Global Unit Assigned Context */
    const char *ua_entry_1_types[4] = {"named_unit", "si_unit", "solid_angle_unit", "*"};
    STEPcomplex *ua_entry_1 = new STEPcomplex(registry, (const char **)ua_entry_1_types, instance_cnt);
    stepcomplex = ua_entry_1->head;
    while (stepcomplex) {
	if (!bu_strcmp(stepcomplex->EntityName(), "Si_Unit")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "name")) attr->ptr.e = new SdaiSi_unit_name_var(Si_unit_name__steradian);
	    }
	}
	stepcomplex = stepcomplex->sc;
    }
    instance_list->Append((STEPentity *)ua_entry_1, completeSE);
    instance_cnt++;

    const char *ua_entry_3_types[4] = {"named_unit", "plane_angle_unit", "si_unit", "*"};
    STEPcomplex *ua_entry_3 = new STEPcomplex(registry, (const char **)ua_entry_3_types, instance_cnt);
    stepcomplex = ua_entry_3->head;
    while (stepcomplex) {
	if (!bu_strcmp(stepcomplex->EntityName(), "Si_Unit")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "name")) attr->ptr.e = new SdaiSi_unit_name_var(Si_unit_name__radian);
	    }
	}
	stepcomplex = stepcomplex->sc;
    }
    instance_list->Append((STEPentity *)ua_entry_3, completeSE);
    instance_cnt++;

    /* Plane Angle Measure */
    SdaiPlane_angle_measure_with_unit *p_ang_measure_with_unit = new SdaiPlane_angle_measure_with_unit();
    SdaiMeasure_value * p_ang_measure_value = new SdaiMeasure_value(DEG2RAD, config_control_design::t_measure_value);
    p_ang_measure_value->SetUnderlyingType(config_control_design::t_plane_angle_measure);
    p_ang_measure_with_unit->value_component_(p_ang_measure_value);
    SdaiUnit *p_ang_unit = new SdaiUnit((SdaiNamed_unit *)ua_entry_3);
    p_ang_measure_with_unit->unit_component_(p_ang_unit);
    instance_list->Append((STEPentity *)p_ang_measure_with_unit, completeSE);
    instance_cnt++;

    /* Conversion based unit */
    const char *ua_entry_2_types[4] = {"conversion_based_unit", "named_unit", "plane_angle_unit", "*"};
    STEPcomplex *ua_entry_2 = new STEPcomplex(registry, (const char **)ua_entry_2_types, instance_cnt);

    /** dimensional exponents **/
    SdaiDimensional_exponents *dimensional_exp = new SdaiDimensional_exponents();
    dimensional_exp->length_exponent_(0.0);
    dimensional_exp->mass_exponent_(0.0);
    dimensional_exp->time_exponent_(0.0);
    dimensional_exp->electric_current_exponent_(0.0);
    dimensional_exp->thermodynamic_temperature_exponent_(0.0);
    dimensional_exp->amount_of_substance_exponent_(0.0);
    dimensional_exp->luminous_intensity_exponent_(0.0);
    instance_list->Append((STEPentity *)dimensional_exp, completeSE);

    stepcomplex = ua_entry_2->head;
    while (stepcomplex) {
	if (!bu_strcmp(stepcomplex->EntityName(), "Conversion_Based_Unit")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "name")) attr->StrToVal("'DEGREES'");
		if (!bu_strcmp(attr->Name(), "conversion_factor")) {
		    attr->ptr.c = new (STEPentity *);
		    *(attr->ptr.c) = (STEPentity *)(p_ang_measure_with_unit);
		}
	    }
	}
	if (!bu_strcmp(stepcomplex->EntityName(), "Named_Unit")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "dimensions")) {
		    attr->ptr.c = new (STEPentity *);
		    *(attr->ptr.c) = (STEPentity *)(dimensional_exp);
		}
	    }
	}
	stepcomplex = stepcomplex->sc;
    }

    instance_list->Append((STEPentity *)ua_entry_2, completeSE);
    instance_cnt++;

    /*
     * Now that we have the pieces, build the final complex type from four other types:
     */
    const char *entNmArr[5] = {"geometric_representation_context", "global_uncertainty_assigned_context",
			       "global_unit_assigned_context", "representation_context", "*"};
    STEPcomplex *complex_entity = new STEPcomplex(registry, (const char **)entNmArr, instance_cnt);
    stepcomplex = complex_entity->head;

    while (stepcomplex) {

	if (!bu_strcmp(stepcomplex->EntityName(), "Geometric_Representation_Context")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "coordinate_space_dimension")) attr->StrToVal("3");
	    }
	}

	if (!bu_strcmp(stepcomplex->EntityName(), "Global_Uncertainty_Assigned_Context")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "uncertainty")) {
		    EntityAggregate *unc_agg = new EntityAggregate();
		    unc_agg->AddNode(new EntityNode((SDAI_Application_instance *)uncertainty));
		    attr->ptr.a = unc_agg;
		}
	    }

	}

	if (!bu_strcmp(stepcomplex->EntityName(), "Global_Unit_Assigned_Context")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		std::string attrval;
		if (!bu_strcmp(attr->Name(), "units")) {
		    EntityAggregate *unit_assigned_agg = new EntityAggregate();
		    unit_assigned_agg->AddNode(new EntityNode((SDAI_Application_instance *)unit_complex));
		    unit_assigned_agg->AddNode(new EntityNode((SDAI_Application_instance *)ua_entry_2));
		    unit_assigned_agg->AddNode(new EntityNode((SDAI_Application_instance *)ua_entry_3));
		    attr->ptr.a = unit_assigned_agg;
		}
	    }
	}

	if (!bu_strcmp(stepcomplex->EntityName(), "Representation_Context")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "context_identifier")) attr->StrToVal("'STANDARD'");
		if (!bu_strcmp(attr->Name(), "context_type")) attr->StrToVal("'3D'");
	    }
	}
	stepcomplex = stepcomplex->sc;
    }

    instance_list->Append((STEPentity *)complex_entity, completeSE);

    return complex_entity;
}


/* Defining a shape is necessary for at least some systems, but how
 * this is done does not appear to be at all uniform between the
 * outputs from various systems. Rhino outputs a shape definition representation
 * that does not directly reference the Brep, and instead uses a shape
 * representation relationship.  Creo, on the other hand, appears to reference
 * the Brep directly in the shape definition representation and does not have
 * the shape representation relationship defined.  TODO - figure out
 * which way is 'better' practice.
 * */

/* #1: Shape Definition Representation
 *
 * SHAPE_DEFINITION_REPRESENTATION (SdaiShape_definition_representation -> SdaiProperty_definition_representation)
 * PRODUCT_DEFINITION_SHAPE (SdaiProduct_definition_shape -> SdaiProperty_definition)
 * PRODUCT_DEFINITION (SdaiProduct_definition)
 * PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE (SdaiProduct_definition_formation_with_specified_source -> SdaiProduct_definition_formation) Can we just use PRODUCT_DEFINITION_FORMATION here?
 * PRODUCT (SdaiProduct)
 * MECHANICAL_CONTEXT (SdaiMechanical_context -> SdaiProduct_context -> SdaiApplication_context_element)
 * APPLICATION_CONTEXT (SdaiApplication_context)
 * DESIGN_CONTEXT (SdaiDesign_context -> SdaiProduct_definition_context -> SdaiApplication_context_element)
 *
 */
STEPentity *
Add_Shape_Definition_Representation(Registry *registry, InstMgr *instance_list, SdaiRepresentation *sdairep)
{
    // SHAPE_DEFINITION_REPRESENTATION
    STEPentity *ret_entity = registry->ObjCreate("SHAPE_DEFINITION_REPRESENTATION");
    instance_list->Append(ret_entity, completeSE);
    SdaiShape_definition_representation *shape_def_rep = (SdaiShape_definition_representation *)ret_entity;
    shape_def_rep->used_representation_(sdairep);

    // PRODUCT_DEFINITION_SHAPE
    SdaiProduct_definition_shape *prod_def_shape = (SdaiProduct_definition_shape *)registry->ObjCreate("PRODUCT_DEFINITION_SHAPE");
    instance_list->Append((STEPentity *)prod_def_shape, completeSE);
    prod_def_shape->name_("''");
    prod_def_shape->description_("''");
    shape_def_rep->definition_(prod_def_shape);

    // PRODUCT_DEFINITION
    SdaiProduct_definition *prod_def = (SdaiProduct_definition *)registry->ObjCreate("PRODUCT_DEFINITION");
    instance_list->Append((STEPentity *)prod_def, completeSE);
    SdaiCharacterized_product_definition *char_def_prod = new SdaiCharacterized_product_definition(prod_def);
    SdaiCharacterized_definition *char_def= new SdaiCharacterized_definition(char_def_prod);
    prod_def_shape->definition_(char_def);
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
    prod->name_("''");
    prod->description_("''");

    // MECHANICAL_CONTEXT
    SdaiMechanical_context *mech_context = (SdaiMechanical_context *)registry->ObjCreate("MECHANICAL_CONTEXT");
    instance_list->Append((STEPentity *)mech_context, completeSE);
    prod->frame_of_reference_()->AddNode(new EntityNode((SDAI_Application_instance *)mech_context));
    mech_context->name_("''");
    mech_context->discipline_type_("''");

    // APPLICATION_CONTEXT
    SdaiApplication_context *app_context = (SdaiApplication_context *)registry->ObjCreate("APPLICATION_CONTEXT");
    instance_list->Append((STEPentity *)app_context, completeSE);
    mech_context->frame_of_reference_(app_context);
    app_context->application_("''");

    // DESIGN_CONTEXT
    SdaiDesign_context *design_context = (SdaiDesign_context *)registry->ObjCreate("DESIGN_CONTEXT");
    instance_list->Append((STEPentity *)design_context, completeSE);
    prod_def->frame_of_reference_(design_context);
    design_context->name_("''");
    design_context->life_cycle_stage_("Design");
    design_context->frame_of_reference_(app_context);

    return ret_entity;
}

/* #2: Shape Representation Relationship
 *
 * SHAPE_REPRESENTATION_RELATIONSHIP (SdaiShape_representation_relationship -> SdaiRepresentation_relationship)
 * SHAPE_REPRESENTATION (SdaiShape_representation -> SdaiRepresentation
 * AXIS2_PLACEMENT_3D (SdaiAxis2_placement_3d -> SdaiPlacement)
 * DIRECTION (two of these) (SdaiDirection -> SdaiGeometric_representation_item -> SdaiRepresentation_item)
 * CARTESIAN_POINT (SdaiCartesian_point)
 *
 *
 */
STEPentity *
Add_Shape_Representation_Relationship(Registry *registry, InstMgr *instance_list, SdaiRepresentation *shape_rep, SdaiRepresentation *manifold_shape)
{
    STEPentity *ret_entity = registry->ObjCreate("SHAPE_REPRESENTATION_RELATIONSHIP");
    instance_list->Append(ret_entity, completeSE);

    SdaiShape_representation_relationship *shape_rep_rel = (SdaiShape_representation_relationship *) ret_entity;
    shape_rep_rel->name_("''");
    shape_rep_rel->description_("''");
    shape_rep_rel->rep_1_(shape_rep);
    shape_rep_rel->rep_2_(manifold_shape);

    return ret_entity;
}

SdaiRepresentation *
Add_Shape_Representation(Registry *registry, InstMgr *instance_list, SdaiRepresentation_context *context)
{

    SdaiShape_representation *shape_rep = (SdaiShape_representation *)registry->ObjCreate("SHAPE_REPRESENTATION");
    instance_list->Append((STEPentity *)shape_rep, completeSE);
    shape_rep->name_("''");
    shape_rep->context_of_items_(context);

    EntityAggregate *axis_items = shape_rep->items_();

    /* create an axis */

    SdaiAxis2_placement_3d *axis3d = (SdaiAxis2_placement_3d *)registry->ObjCreate("AXIS2_PLACEMENT_3D");
    instance_list->Append((STEPentity *)axis3d, completeSE);
    axis3d->name_("''");

    /* set the axis origin */

    SdaiCartesian_point *origin= (SdaiCartesian_point *)registry->ObjCreate("CARTESIAN_POINT");
    instance_list->Append((STEPentity *)origin, completeSE);

    RealNode *xnode = new RealNode();
    xnode->value = 0.0;
    RealNode *ynode= new RealNode();
    ynode->value = 0.0;
    RealNode *znode= new RealNode();
    znode->value = 0.0;
    origin->coordinates_()->AddNode(xnode);
    origin->coordinates_()->AddNode(ynode);
    origin->coordinates_()->AddNode(znode);
    origin->name_("''");
    axis3d->location_(origin);

    /* set the axis up direction (i-vector) */

    SdaiDirection *axis = (SdaiDirection *)registry->ObjCreate("DIRECTION");
    instance_list->Append((STEPentity *)axis, completeSE);

    RealNode *axis_xnode = new RealNode();
    axis_xnode->value = 0.0;
    RealNode *axis_ynode= new RealNode();
    axis_ynode->value = 0.0;
    RealNode *axis_znode= new RealNode();
    axis_znode->value = 1.0;
    axis->direction_ratios_()->AddNode(axis_xnode);
    axis->direction_ratios_()->AddNode(axis_ynode);
    axis->direction_ratios_()->AddNode(axis_znode);
    axis->name_("''");
    axis3d->axis_(axis);

    /* add the axis front direction (j-vector) */

    SdaiDirection *ref_dir = (SdaiDirection *)registry->ObjCreate("DIRECTION");
    instance_list->Append((STEPentity *)ref_dir, completeSE);

    RealNode *ref_dir_xnode = new RealNode();
    ref_dir_xnode->value = 1.0;
    RealNode *ref_dir_ynode= new RealNode();
    ref_dir_ynode->value = 0.0;
    RealNode *ref_dir_znode= new RealNode();
    ref_dir_znode->value = 0.0;
    ref_dir->direction_ratios_()->AddNode(ref_dir_xnode);
    ref_dir->direction_ratios_()->AddNode(ref_dir_ynode);
    ref_dir->direction_ratios_()->AddNode(ref_dir_znode);
    ref_dir->name_("''");
    axis3d->ref_direction_(ref_dir);

    /* add the axis to the shape definition */

    axis_items->AddNode(new EntityNode((SDAI_Application_instance *)axis3d));

    return (SdaiRepresentation *)shape_rep;
}


#if 0
void
ON_RationalNurbsCurve_to_EntityAggregate(ON_NurbsCurve *incrv, SdaiRational_B_spline_curve *step_crv) {
}
#endif


bool
ON_BRep_to_STEP(ON_Brep *brep, Registry *registry, InstMgr *instance_list)
{
    std::vector<STEPentity *> cartesian_pnts(brep->m_V.Count(), (STEPentity *)0);
    std::vector<STEPentity *> vertex_pnts(brep->m_V.Count(), (STEPentity *)0);
    std::vector<STEPentity *> three_dimensional_curves(brep->m_C3.Count(), (STEPentity *)0);
    std::vector<STEPentity *> edge_curves(brep->m_E.Count(), (STEPentity *)0);
    std::vector<STEPentity *> oriented_edges;
    std::vector<STEPentity *> edge_loops(brep->m_L.Count(), (STEPentity *)0);
    std::vector<STEPentity *> outer_bounds(brep->m_F.Count(), (STEPentity *)0);
    std::vector<STEPentity *> surfaces(brep->m_S.Count(), (STEPentity *)0);
    std::vector<STEPentity *> faces(brep->m_F.Count(), (STEPentity *)0);

    /* The BRep needs a context - TODO: this can probably be used once for the whole step file... */
    STEPcomplex *context = Add_Default_Geometric_Context(registry, instance_list);

    // Set up vertices and associated cartesian points
    for (int i = 0; i < brep->m_V.Count(); ++i) {
	// Cartesian points (actual 3D geometry)
	cartesian_pnts.at(i) = registry->ObjCreate("CARTESIAN_POINT");
	((SdaiCartesian_point *)cartesian_pnts.at(i))->name_("''");
	instance_list->Append(cartesian_pnts.at(i), completeSE);
	ON_3dPoint v_pnt = brep->m_V[i].Point();
	ON_3dPoint_to_Cartesian_point(&(v_pnt), (SdaiCartesian_point *)cartesian_pnts.at(i));

	// Vertex points (topological, references actual 3D geometry)
	vertex_pnts.at(i) = registry->ObjCreate("VERTEX_POINT");
	((SdaiVertex_point *)vertex_pnts.at(i))->name_("''");
	((SdaiVertex_point *)vertex_pnts.at(i))->vertex_geometry_((const SdaiPoint_ptr)cartesian_pnts.at(i));
	instance_list->Append(vertex_pnts.at(i), completeSE);
    }

    // 3D curves
    std::cout << "Have " << brep->m_C3.Count() << " curves\n";
    for (int i = 0; i < brep->m_C3.Count(); ++i) {
	int curve_converted = 0;
	ON_Curve* curve = brep->m_C3[i];
	/* Supported curve types */
	ON_ArcCurve *a_curve = ON_ArcCurve::Cast(curve);
	ON_LineCurve *l_curve = ON_LineCurve::Cast(curve);
	ON_NurbsCurve *n_curve = ON_NurbsCurve::Cast(curve);
	ON_PolyCurve *p_curve = ON_PolyCurve::Cast(curve);

	if (a_curve && !curve_converted) {
	    std::cout << "Have ArcCurve\n";
	}

	if (l_curve && !curve_converted) {
	    std::cout << "Have LineCurve\n";
	    ON_Line *m_line = &(l_curve->m_line);

	    /* In STEP, a line consists of a cartesian point and a 3D
	     * vector.  Since it does not appear that OpenNURBS data
	     * structures reference m_V points for these constructs,
	     * create our own
	     */

	    three_dimensional_curves.at(i) = registry->ObjCreate("LINE");

	    SdaiLine *curr_line = (SdaiLine *)three_dimensional_curves.at(i);
	    curr_line->pnt_((SdaiCartesian_point *)registry->ObjCreate("CARTESIAN_POINT"));
	    ON_3dPoint_to_Cartesian_point(&(m_line->from), curr_line->pnt_());
	    curr_line->dir_((SdaiVector *)registry->ObjCreate("VECTOR"));
	    SdaiVector *curr_dir = curr_line->dir_();
	    curr_dir->orientation_((SdaiDirection *)registry->ObjCreate("DIRECTION"));
	    ON_3dVector on_dir = m_line->Direction();
	    ON_3dVector_to_Direction(&(on_dir), curr_line->dir_()->orientation_());
	    curr_line->dir_()->magnitude_(m_line->Length());
	    curr_line->pnt_()->name_("''");
	    curr_dir->orientation_()->name_("''");
	    curr_line->dir_()->name_("''");
	    curr_line->name_("''");

	    instance_list->Append(curr_line->pnt_(), completeSE);
	    instance_list->Append(curr_dir->orientation_(), completeSE);
	    instance_list->Append(curr_line->dir_(), completeSE);
	    instance_list->Append(three_dimensional_curves.at(i), completeSE);
	    curve_converted = 1;
	}

	if (p_curve && !curve_converted) {
	    std::cout << "Have PolyCurve\n";
	}

	if (n_curve && !curve_converted) {
	    std::cout << "Have NurbsCurve\n";
	    if (n_curve->IsRational()) {
		std::cout << "TODO - Have Rational NurbsCurve\n";
		three_dimensional_curves.at(i) = registry->ObjCreate("RATIONAL_B_SPLINE_CURVE");
	    } else {
		three_dimensional_curves.at(i) = registry->ObjCreate("B_SPLINE_CURVE_WITH_KNOTS");
		SdaiB_spline_curve *curr_curve = (SdaiB_spline_curve *)three_dimensional_curves.at(i);
		curr_curve->degree_(n_curve->Degree());
		ON_NurbsCurveCV_to_EntityAggregate(n_curve, curr_curve, registry, instance_list);
		SdaiB_spline_curve_with_knots *curve_knots = (SdaiB_spline_curve_with_knots *)three_dimensional_curves.at(i);
		ON_NurbsCurveKnots_to_Aggregates(n_curve, curve_knots);
	    }

	    ((SdaiB_spline_curve *)three_dimensional_curves.at(i))->curve_form_(B_spline_curve_form__unspecified);
	    ((SdaiB_spline_curve *)three_dimensional_curves.at(i))->closed_curve_(SDAI_LOGICAL(n_curve->IsClosed()));

	    /* TODO: Assume we don't have self-intersecting curves for
	     * now - need some way to test this...
	     */
	    ((SdaiB_spline_curve *)three_dimensional_curves.at(i))->self_intersect_(LFalse);
	    ((SdaiB_spline_curve *)three_dimensional_curves.at(i))->name_("''");
	    instance_list->Append(three_dimensional_curves.at(i), completeSE);
	    curve_converted = 1;
	}

	/* Whatever this is, if it's not a supported type and it does
	 * have a NURBS form, use that
	 */
	if (!curve_converted) std::cout << "Curve not converted! " << i << "\n";

    }

    // edge topology - ON_BrepEdge -> edge curves and oriented edges
    for (int i = 0; i < brep->m_E.Count(); ++i) {
	ON_BrepEdge *edge = &(brep->m_E[i]);
	edge_curves.at(i) = registry->ObjCreate("EDGE_CURVE");
	instance_list->Append(edge_curves.at(i), completeSE);

	SdaiEdge_curve *e_curve = (SdaiEdge_curve *)edge_curves.at(i);
	e_curve->name_("''");
	e_curve->edge_geometry_(((SdaiCurve *)three_dimensional_curves.at(edge->EdgeCurveIndexOf())));
	e_curve->same_sense_(BTrue);
	e_curve->edge_start_(((SdaiVertex *)vertex_pnts.at(edge->Vertex(0)->m_vertex_index)));
	e_curve->edge_end_(((SdaiVertex *)vertex_pnts.at(edge->Vertex(1)->m_vertex_index)));
    }

    // loop topology.  STEP defines loops with 3D edge curves, but
    // OpenNURBS describes ON_BrepLoops with 2d trim curves.  So for a
    // given loop, we need to iterate over the trims, for each trim
    // get the index of its corresponding edge, and add that edge to
    // the _edge_list for the loop.
    for (int i = 0; i < brep->m_L.Count(); ++i) {
	ON_BrepLoop *loop= &(brep->m_L[i]);
	std::cout << "Loop " << i << "\n";
	edge_loops.at(i) = registry->ObjCreate("EDGE_LOOP");
	instance_list->Append(edge_loops.at(i), completeSE);
	((SdaiEdge_loop *)edge_loops.at(i))->name_("''");

	// Why doesn't SdaiEdge_loop's edge_list_() function give use
	// the edge_list from the SdaiPath??  Initialized to NULL and
	// crashes - what good is it?  Have to get at the internal
	// SdaiPath directly to build something that STEPwrite will
	// output.
	SdaiPath *e_loop_path = (SdaiPath *)edge_loops.at(i)->GetNextMiEntity();
	for (int l = 0; l < loop->TrimCount(); ++l) {
	    int trim_edge = Add_Edge(loop->Trim(l), registry, instance_list, &oriented_edges, &edge_curves, &vertex_pnts);
	    if (trim_edge >= 0)
		e_loop_path->edge_list_()->AddNode(new EntityNode((SDAI_Application_instance *)(oriented_edges.at(trim_edge))));
	}
    }

    // surfaces - TODO - need to handle cylindrical, conical,
    // toroidal, etc. types that are enumerated
    std::cout << "Have " << brep->m_S.Count() << " surfaces\n";
    for (int i = 0; i < brep->m_S.Count(); ++i) {
	int surface_converted = 0;
	ON_Surface* surface = brep->m_S[i];
	/* Supported surface types */
	ON_OffsetSurface *o_surface = ON_OffsetSurface::Cast(surface);
	ON_PlaneSurface *p_surface = ON_PlaneSurface::Cast(surface);
	ON_ClippingPlaneSurface *pc_surface = ON_ClippingPlaneSurface::Cast(surface);
	ON_NurbsSurface *n_surface = ON_NurbsSurface::Cast(surface);
	ON_RevSurface *rev_surface = ON_RevSurface::Cast(surface);
	ON_SumSurface *sum_surface = ON_SumSurface::Cast(surface);
	ON_SurfaceProxy *surface_proxy = ON_SurfaceProxy::Cast(surface);

	if (o_surface && !surface_converted) {
	    std::cout << "Have OffsetSurface\n";
	}

	if (p_surface && !surface_converted) {
	    std::cout << "Have PlaneSurface\n";

	    ON_NurbsSurface p_nurb;
	    p_surface->GetNurbForm(p_nurb);
	    surfaces.at(i) = registry->ObjCreate("B_SPLINE_SURFACE_WITH_KNOTS");

	    SdaiB_spline_surface *curr_surface = (SdaiB_spline_surface *)surfaces.at(i);
	    curr_surface->name_("''");
	    curr_surface->u_degree_(p_nurb.Degree(0));
	    curr_surface->v_degree_(p_nurb.Degree(1));
	    ON_NurbsSurfaceCV_to_GenericAggregate(&p_nurb, curr_surface, registry, instance_list);

	    SdaiB_spline_surface_with_knots *surface_knots = (SdaiB_spline_surface_with_knots *)surfaces.at(i);
	    ON_NurbsSurfaceKnots_to_Aggregates(&p_nurb, surface_knots);
	    curr_surface->surface_form_(B_spline_surface_form__plane_surf);
	    /* Planes don't self-intersect */
	    curr_surface->self_intersect_(LFalse);
	    /* TODO - need to recognize when these should be true */
	    curr_surface->u_closed_(LFalse);
	    curr_surface->v_closed_(LFalse);
	    instance_list->Append(surfaces.at(i), completeSE);
	    surface_converted = 1;
	}

	if (pc_surface && !surface_converted) {
	    std::cout << "Have CuttingPlaneSurface\n";
	}

	if (n_surface && !surface_converted) {
	    std::cout << "Have NurbsSurface\n";
	    surfaces.at(i) = registry->ObjCreate("B_SPLINE_SURFACE_WITH_KNOTS");

	    SdaiB_spline_surface *curr_surface = (SdaiB_spline_surface *)surfaces.at(i);
	    curr_surface->name_("''");
	    curr_surface->u_degree_(n_surface->Degree(0));
	    curr_surface->v_degree_(n_surface->Degree(1));
	    ON_NurbsSurfaceCV_to_GenericAggregate(n_surface, curr_surface, registry, instance_list);

	    SdaiB_spline_surface_with_knots *surface_knots = (SdaiB_spline_surface_with_knots *)surfaces.at(i);
	    ON_NurbsSurfaceKnots_to_Aggregates(n_surface, surface_knots);
	    curr_surface->surface_form_(B_spline_surface_form__unspecified);
	    /* TODO - for now, assume the surfaces don't self-intersect - need to figure out how to test this */
	    curr_surface->self_intersect_(LFalse);
	    /* TODO - need to recognize when these should be true */
	    curr_surface->u_closed_(LFalse);
	    curr_surface->v_closed_(LFalse);
	    instance_list->Append(surfaces.at(i), completeSE);
	    surface_converted = 1;
	}

	if (rev_surface && !surface_converted) {
	    std::cout << "Have RevSurface\n";
	}

	if (sum_surface && !surface_converted) {
	    std::cout << "Have SumSurface\n";

	    ON_NurbsSurface sum_nurb;
	    sum_surface->GetNurbForm(sum_nurb);
	    surfaces.at(i) = registry->ObjCreate("B_SPLINE_SURFACE_WITH_KNOTS");

	    SdaiB_spline_surface *curr_surface = (SdaiB_spline_surface *)surfaces.at(i);
	    curr_surface->name_("''");
	    curr_surface->u_degree_(sum_nurb.Degree(0));
	    curr_surface->v_degree_(sum_nurb.Degree(1));
	    ON_NurbsSurfaceCV_to_GenericAggregate(&sum_nurb, curr_surface, registry, instance_list);

	    SdaiB_spline_surface_with_knots *surface_knots = (SdaiB_spline_surface_with_knots *)surfaces.at(i);
	    ON_NurbsSurfaceKnots_to_Aggregates(&sum_nurb, surface_knots);
	    curr_surface->surface_form_(B_spline_surface_form__plane_surf);
	    /* TODO - for now, assume non-self-intersecting */
	    curr_surface->self_intersect_(LFalse);
	    /* TODO - need to recognize when these should be true */
	    curr_surface->u_closed_(LFalse);
	    curr_surface->v_closed_(LFalse);
	    instance_list->Append(surfaces.at(i), completeSE);
	    surface_converted = 1;
	}

	if (surface_proxy && !surface_converted) {
	    std::cout << "Have SurfaceProxy\n";
	}

    }

    // faces
    for (int i = 0; i < brep->m_F.Count(); ++i) {
	ON_BrepFace* face = &(brep->m_F[i]);
	faces.at(i) = registry->ObjCreate("ADVANCED_FACE");

	SdaiAdvanced_face *step_face = (SdaiAdvanced_face *)faces.at(i);
	step_face->name_("''");
	step_face->face_geometry_((SdaiSurface *)surfaces.at(face->SurfaceIndexOf()));
	// TODO - is m_bRev the same thing as same_sense?
	step_face->same_sense_((const Boolean)(face->m_bRev));

	EntityAggregate *bounds = step_face->bounds_();

	for (int j = 0; j < face->LoopCount(); ++j) {
	    ON_BrepLoop *curr_loop = face->Loop(j);
	    if (curr_loop == face->OuterLoop()) {
		SdaiFace_outer_bound *outer_bound = (SdaiFace_outer_bound *)registry->ObjCreate("FACE_OUTER_BOUND");
		outer_bound->name_("''");
		instance_list->Append(outer_bound, completeSE);
		outer_bound->bound_((SdaiLoop *)edge_loops.at(curr_loop->m_loop_index));
		// TODO - When should this be false?
		outer_bound->orientation_(BTrue);
		bounds->AddNode(new EntityNode((SDAI_Application_instance *)outer_bound));
	    } else {
		SdaiFace_bound *inner_bound = (SdaiFace_bound *)registry->ObjCreate("FACE_BOUND");
		inner_bound->name_("''");
		instance_list->Append(inner_bound, completeSE);
		inner_bound->bound_((SdaiLoop *)edge_loops.at(curr_loop->m_loop_index));
		// TODO - When should this be false?
		inner_bound->orientation_(BTrue);
		bounds->AddNode(new EntityNode((SDAI_Application_instance *)inner_bound));
	    }
	}
	instance_list->Append(step_face, completeSE);
    }

    // Closed shell that assembles the faces
    SdaiClosed_shell *closed_shell = (SdaiClosed_shell *)registry->ObjCreate("CLOSED_SHELL");
    closed_shell->name_("''");
    instance_list->Append(closed_shell, completeSE);

    EntityAggregate *shell_faces = closed_shell->cfs_faces_();
    for (int i = 0; i < brep->m_F.Count(); ++i) {
	shell_faces->AddNode(new EntityNode((SDAI_Application_instance *)faces.at(i)));
    }

    // Solid manifold BRep
    SdaiManifold_solid_brep *manifold_solid_brep = (SdaiManifold_solid_brep *)registry->ObjCreate("MANIFOLD_SOLID_BREP");
    instance_list->Append(manifold_solid_brep, completeSE);
    manifold_solid_brep->outer_(closed_shell);
    manifold_solid_brep->name_("''");

    // Advanced BRep shape representation - this is the object step-g will look for
    SdaiAdvanced_brep_shape_representation *advanced_brep= (SdaiAdvanced_brep_shape_representation *)registry->ObjCreate("ADVANCED_BREP_SHAPE_REPRESENTATION");
    advanced_brep->name_("'brep.s'");
    instance_list->Append(advanced_brep, completeSE);
    EntityAggregate *items = advanced_brep->items_();
    items->AddNode(new EntityNode((SDAI_Application_instance *)manifold_solid_brep));
    advanced_brep->context_of_items_((SdaiRepresentation_context *) context);

    // Top level structures
    SdaiRepresentation *shape_rep = Add_Shape_Representation(registry, instance_list, (SdaiRepresentation_context *)context);
    (void *)Add_Shape_Representation_Relationship(registry, instance_list, shape_rep, (SdaiRepresentation *)advanced_brep);
    (void *)Add_Shape_Definition_Representation(registry, instance_list, (SdaiRepresentation *)shape_rep);

    return true;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
