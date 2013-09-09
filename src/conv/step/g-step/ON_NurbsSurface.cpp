/*            O N _ N U R B S S U R F A C E . C P P
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
/** @file ON_NurbsSurface.cpp
 *
 * File for writing out an ON_NurbsSurface structure into the STEPcode containers
 *
 */

#include "common.h"

#include <sstream>
#include <map>

#include "G_STEP_internal.h"
#include "ON_Brep.h"

/* For a rational B-Spline surface with weights, we need to create an aggregate type */

STEPentity *
Create_Rational_Surface_Aggregate(ON_NurbsSurface *nsurface, Exporter_Info_AP203 *info) {
    STEPattribute *attr;
    STEPcomplex *stepcomplex;
    const char *entNmArr[8] = {"bounded_surface", "b_spline_surface", "b_spline_surface_with_knots",
	"surface", "geometric_representation_item", "rational_b_spline_surface", "representation_item", "*"};
    STEPcomplex *complex_entity = new STEPcomplex(info->registry, (const char **)entNmArr, info->registry->GetEntityCnt() + 1);

    stepcomplex = complex_entity->head;
    stepcomplex->ResetAttributes();
    while (stepcomplex) {
	std::cout << stepcomplex->EntityName() << "\n";
	while ((attr = stepcomplex->NextAttribute()) != NULL) {
	    std::cout << "  " << attr->Name() << "," << attr->NonRefType() << "\n";
	}
	stepcomplex = stepcomplex->sc;
	stepcomplex->ResetAttributes();
    }
#if 0
    /* Set b_spline_curve data */
    stepcomplex = complex_entity->EntityPart("b_spline_surface");
    stepcomplex->ResetAttributes();
    std::cout << "b_spline_surface\n";
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	if (!bu_strcmp(attr->Name(), "degree")) {
	    attr->ptr.i = new SDAI_Integer(ncurve->Degree());
	}
	if (!bu_strcmp(attr->Name(), "control_points_list")) {
	    EntityAggregate *control_pnts= new EntityAggregate();
	    ON_4dPoint cv_pnt;
	    for (int i = 0; i < ncurve->CVCount(); i++) {
		double w = ncurve->Weight(i);
		SdaiCartesian_point *step_cartesian = (SdaiCartesian_point *)info->registry->ObjCreate("CARTESIAN_POINT");
		step_cartesian->name_("''");
		info->cartesian_pnts.push_back((STEPentity *)step_cartesian);
		ncurve->GetCV(i, cv_pnt);
		std::cout << "4d point: " << cv_pnt.x << "," << cv_pnt.y << "," << cv_pnt.z << "," << ncurve->Weight(i) << "\n";
		XYZ_to_Cartesian_point(cv_pnt.x/w, cv_pnt.y/w, cv_pnt.z/w, step_cartesian);
		control_pnts->AddNode(new EntityNode((SDAI_Application_instance *)step_cartesian));
	    }
	    attr->ptr.a = control_pnts;
	}
	if (!bu_strcmp(attr->Name(), "curve_form")) attr->ptr.e = new SdaiB_spline_curve_form_var(B_spline_curve_form__unspecified);
	if (!bu_strcmp(attr->Name(), "closed_curve")) attr->ptr.e = new SDAI_LOGICAL((Logical)(ncurve->IsClosed()));
	if (!bu_strcmp(attr->Name(), "self_intersect")) attr->ptr.e = new SDAI_LOGICAL(LFalse);
    }

    /* Set knots */
    stepcomplex = complex_entity->EntityPart("b_spline_curve_with_knots");
    stepcomplex->ResetAttributes();
    std::cout << "b_spline_curve_with_knots\n";
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	if (!bu_strcmp(attr->Name(), "knot_multiplicities")) {
	    IntAggregate *knot_multiplicities = new IntAggregate();
	    int i = 0;
	    while (i < ncurve->KnotCount()) {
		int multiplicity_val = ncurve->KnotMultiplicity(i);
		/* OpenNURBS and STEP have different notions of end knot
		 * multiplicity - see:
		 * http://wiki.mcneel.com/developer/onsuperfluousknot
		 *
		 * TODO - might have a problem here if curve is closed (and in the non-rational case too)
		 */
		if ((i == 0) || (i == (ncurve->KnotCount() - ncurve->KnotMultiplicity(0)))) multiplicity_val++;
		/* Set Multiplicity */
		IntNode *multiplicity = new IntNode();
		multiplicity->value = multiplicity_val;
		knot_multiplicities->AddNode(multiplicity);
		i += ncurve->KnotMultiplicity(i);
	    }
	    attr->ptr.a = knot_multiplicities;
	}
	if (!bu_strcmp(attr->Name(), "knots")) {
	    RealAggregate *knots = new RealAggregate();
	    int i = 0;
	    while (i < ncurve->KnotCount()) {
		/* Add knot */
		RealNode *knot = new RealNode();
		knot->value = ncurve->Knot(i);
		knots->AddNode(knot);
		i += ncurve->KnotMultiplicity(i);
	    }
	    attr->ptr.a = knots;
	}
	if (!bu_strcmp(attr->Name(), "knot_spec")) attr->ptr.e = new SdaiKnot_type_var(Knot_type__unspecified);
    }

    /* Set weights */
    stepcomplex = complex_entity->EntityPart("rational_b_spline_curve");
    stepcomplex->ResetAttributes();
    std::cout << "rational_b_spline_curve\n";
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	std::cout << "  " << attr->Name() << "," << attr->NonRefType() << "\n";
	RealAggregate *weights = new RealAggregate();
	for (int i = 0; i < ncurve->CVCount(); i++) {
	    RealNode *wnode = new RealNode();
	    wnode->value = ncurve->Weight(i);
	    weights->AddNode(wnode);
	}
	attr->ptr.a = weights;
    }

    /* Representation item */
    stepcomplex = complex_entity->EntityPart("representation_item");
    stepcomplex->ResetAttributes();
    std::cout << "representation_item\n";
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	std::cout << "  " << attr->Name() << "," << attr->NonRefType() << "\n";
	if (!bu_strcmp(attr->Name(), "name")) attr->StrToVal("''");
    }

#endif
    return (STEPentity *)complex_entity;
}


/* Unlike most of the structures we're working with, GenericAggregate seems to require that we manually
 * build its final string with the step file id numbers that identify each control point.  To allow for
 * delayed instance manager population, we build a temporary map of nested vectors to hold the information
 * inthe proper form until we are ready for it.*/
void
ON_NurbsSurfaceCV_Initialize(ON_NurbsSurface *insrf, SdaiB_spline_surface *step_srf, Exporter_Info_AP203 *info) {
    ON_3dPoint cv_pnt;
    std::vector<std::vector<STEPentity *> > i_array;
    for (int i = 0; i < insrf->CVCount(0); i++) {
	std::vector<STEPentity *> j_array;
	for (int j = 0; j < insrf->CVCount(1); j++) {
	    SdaiCartesian_point *step_cartesian = (SdaiCartesian_point *)info->registry->ObjCreate("CARTESIAN_POINT");
	    step_cartesian->name_("''");
	    insrf->GetCV(i, j, cv_pnt);
	    ON_3dPoint_to_Cartesian_point(&(cv_pnt), step_cartesian);
	    j_array.push_back((STEPentity *)step_cartesian);
	}
	i_array.push_back(j_array);
    }
    info->surface_cv[(STEPentity*)step_srf] = i_array;
}

// Call this function after all cartesian points have an instance manager instance,
// (and hence a StepFileID) to populate the surface GenericAggregate control point
// slots.  Must be run *after* ON_NurbsSurfaceCV_Initialize has been run on *all*
// surfaces.
void
ON_NurbsSurfaceCV_Finalize_GenericAggregates(Exporter_Info_AP203 *info) 
{
    std::map<STEPentity*, std::vector<std::vector<STEPentity *> > >::iterator scv_it;
    std::vector<std::vector<STEPentity *> >::iterator outer_it;
    std::vector<STEPentity *>::iterator inner_it;
    for(scv_it = info->surface_cv.begin(); scv_it != info->surface_cv.end(); ++scv_it) {
        SdaiB_spline_surface *step_srf = (SdaiB_spline_surface *)scv_it->first;
	GenericAggregate *control_pnts_lists = step_srf->control_points_list_();
	for (outer_it = scv_it->second.begin(); outer_it != scv_it->second.end(); ++outer_it) {
	    std::ostringstream ss;
	    ss << "(";
	    for (inner_it = (*outer_it).begin(); inner_it != (*outer_it).end(); ++inner_it) {
		info->instance_list->Append((STEPentity *)(*inner_it), completeSE);
		if (inner_it != (*outer_it).begin()) ss << ", ";
		ss << "#" << ((STEPentity *)(*inner_it))->StepFileId();
	    }
	    ss << ")";
	    std::string str = ss.str();
	    control_pnts_lists->AddNode(new GenericAggrNode(str.c_str()));
	}
    }
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

bool
ON_NurbsSurface_to_STEP(ON_NurbsSurface *n_surface, Exporter_Info_AP203 *info, int i)
{
    bool surface_converted = true;
    std::cout << "Have NurbsSurface\n";
    if (n_surface->IsRational()) {
	std::cout << "Have Rational NurbsSurface\n";
	info->surfaces.at(i) = Create_Rational_Surface_Aggregate(n_surface, info);
    } else {
	info->surfaces.at(i) = info->registry->ObjCreate("B_SPLINE_SURFACE_WITH_KNOTS");

	SdaiB_spline_surface *curr_surface = (SdaiB_spline_surface *)info->surfaces.at(i);
	curr_surface->name_("''");
	curr_surface->u_degree_(n_surface->Degree(0));
	curr_surface->v_degree_(n_surface->Degree(1));
	ON_NurbsSurfaceCV_Initialize(n_surface, curr_surface, info);

	SdaiB_spline_surface_with_knots *surface_knots = (SdaiB_spline_surface_with_knots *)info->surfaces.at(i);
	ON_NurbsSurfaceKnots_to_Aggregates(n_surface, surface_knots);
	curr_surface->surface_form_(B_spline_surface_form__unspecified);
	/* TODO - for now, assume the surfaces don't self-intersect - need to figure out how to test this */
	curr_surface->self_intersect_(LFalse);
	curr_surface->u_closed_((Logical)n_surface->IsClosed(0));
	curr_surface->v_closed_((Logical)n_surface->IsClosed(1));
    }
    return surface_converted;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
