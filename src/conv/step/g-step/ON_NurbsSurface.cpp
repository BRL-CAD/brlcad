/*            O N _ N U R B S S U R F A C E . C P P
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
/** @file ON_NurbsSurface.cpp
 *
 * File for writing out an ON_NurbsSurface structure into the STEPcode containers
 *
 */

#include "AP_Common.h"
#include "ON_Brep.h"
#include "STEPGeneratedAPI.h"

/* Unlike most of the structures we're working with, GenericAggregate seems to require that we manually
 * build its final string with the step file id numbers that identify each control point.  To allow for
 * delayed instance manager population, we build a temporary map of nested vectors to hold the information
 * in the proper form until we are ready for it.*/
void
ON_NurbsSurfaceCV_Initialize(ON_NurbsSurface *insrf, STEPentity *step_srf, ON_Brep_Info_AP203 *info) {
    ON_3dPoint cv_pnt;
    std::vector<std::vector<STEPentity *> > i_array;
    for (int i = 0; i < insrf->CVCount(0); i++) {
	std::vector<STEPentity *> j_array;
	for (int j = 0; j < insrf->CVCount(1); j++) {
	    STEPentity *step_cartesian = info->registry->ObjCreate("CARTESIAN_POINT");
	    brlcad::step::SetString(step_cartesian, "name", "");
	    insrf->GetCV(i, j, cv_pnt);
	    ON_3dPoint_to_Cartesian_point(&(cv_pnt), step_cartesian);
	    j_array.push_back((STEPentity *)step_cartesian);
	}
	i_array.push_back(j_array);
    }
    info->surface_cv[(STEPentity*)step_srf] = i_array;
}


void
ON_NurbsSurfaceKnots_to_Aggregates(
	IntAggregate *u_knot_multiplicities,
	IntAggregate *v_knot_multiplicities,
	RealAggregate *u_knots,
	RealAggregate *v_knots,
	ON_NurbsSurface *insrf)
{

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
}


/* For a rational B-Spline surface with weights, we need to create an aggregate type */

STEPentity *
Create_Rational_Surface_Aggregate(ON_NurbsSurface *nsurface, ON_Brep_Info_AP203 *info) {
    STEPcomplex *stepcomplex;
    const char *entNmArr[8] = {"bounded_surface", "b_spline_surface", "b_spline_surface_with_knots",
	"surface", "geometric_representation_item", "rational_b_spline_surface", "representation_item", "*"};
    STEPcomplex *complex_entity = new STEPcomplex(info->registry, (const char **)entNmArr, info->registry->GetEntityCnt() + 1);
/*
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
*/
    /* Set b_spline_surface data */
    stepcomplex = complex_entity->EntityPart("b_spline_surface");
    brlcad::step::SetInteger(stepcomplex, "u_degree", nsurface->Degree(0));
    brlcad::step::SetInteger(stepcomplex, "v_degree", nsurface->Degree(1));
    GenericAggregate *control_pnts = dynamic_cast<GenericAggregate *>(
	brlcad::step::Aggregate(stepcomplex, "control_points_list"));
    if (!control_pnts) {
	delete complex_entity;
	return NULL;
    }
    ON_NurbsSurfaceCV_Initialize(nsurface, complex_entity, info);
    info->surf_genagg[complex_entity] = control_pnts;
    brlcad::step::SetEnum(stepcomplex, "surface_form", "UNSPECIFIED");
    brlcad::step::SetLogical(stepcomplex, "u_closed",
	nsurface->IsClosed(0) ? LTrue : LFalse);
    brlcad::step::SetLogical(stepcomplex, "v_closed",
	nsurface->IsClosed(1) ? LTrue : LFalse);
    brlcad::step::SetLogical(stepcomplex, "self_intersect", LFalse);

    /* Set knots */
    stepcomplex = complex_entity->EntityPart("b_spline_surface_with_knots");
    IntAggregate *u_multiplicities = dynamic_cast<IntAggregate *>(
	brlcad::step::Aggregate(stepcomplex, "u_multiplicities"));
    IntAggregate *v_multiplicities = dynamic_cast<IntAggregate *>(
	brlcad::step::Aggregate(stepcomplex, "v_multiplicities"));
    RealAggregate *u_knots = dynamic_cast<RealAggregate *>(
	brlcad::step::Aggregate(stepcomplex, "u_knots"));
    RealAggregate *v_knots = dynamic_cast<RealAggregate *>(
	brlcad::step::Aggregate(stepcomplex, "v_knots"));
    if (!u_multiplicities || !v_multiplicities || !u_knots || !v_knots) {
	delete complex_entity;
	return NULL;
    }

    ON_NurbsSurfaceKnots_to_Aggregates(u_multiplicities, v_multiplicities, u_knots, v_knots, nsurface);

    brlcad::step::SetEnum(stepcomplex, "knot_spec", "UNSPECIFIED");

    /* Set weights */
    stepcomplex = complex_entity->EntityPart("rational_b_spline_surface");
    GenericAggregate *weights = dynamic_cast<GenericAggregate *>(
	brlcad::step::Aggregate(stepcomplex, "weights_data"));
    if (!weights) {
	delete complex_entity;
	return NULL;
    }
    for (int i = 0; i < nsurface->CVCount(0); i++) {
	std::ostringstream ss;
	ss << "(";
	for (int j = 0; j < nsurface->CVCount(1); j++) {
	    if (j != 0) ss << ", ";
	    ss << nsurface->Weight(i,j);
	}
	ss << ")";
	weights->AddNode(new GenericAggrNode(ss.str().c_str()));
    }

    /* Representation item */
    stepcomplex = complex_entity->EntityPart("representation_item");
    brlcad::step::SetString(stepcomplex, "name", "");

    return (STEPentity *)complex_entity;
}


// Call this function after all cartesian points have an instance manager instance,
// (and hence a StepFileID) to populate the surface GenericAggregate control point
// slots.  Must be run *after* ON_NurbsSurfaceCV_Initialize has been run on *all*
// surfaces.
void
ON_NurbsSurfaceCV_Finalize_GenericAggregates(ON_Brep_Info_AP203 *info)
{
    std::map<STEPentity*, std::vector<std::vector<STEPentity *> > >::iterator scv_it;
    std::vector<std::vector<STEPentity *> >::iterator outer_it;
    std::vector<STEPentity *>::iterator inner_it;
    for (scv_it = info->surface_cv.begin(); scv_it != info->surface_cv.end(); ++scv_it) {
	GenericAggregate *control_pnts_lists = info->surf_genagg.find(scv_it->first)->second;
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

bool
ON_NurbsSurface_to_STEP(ON_NurbsSurface *n_surface, ON_Brep_Info_AP203 *info, int i)
{
    bool surface_converted = true;
    if (n_surface->IsRational()) {
	info->surfaces.at(i) = Create_Rational_Surface_Aggregate(n_surface, info);
    } else {
	info->surfaces.at(i) = info->registry->ObjCreate("B_SPLINE_SURFACE_WITH_KNOTS");

	STEPentity *curr_surface = info->surfaces.at(i);
	brlcad::step::SetString(curr_surface, "name", "");
	brlcad::step::SetInteger(curr_surface, "u_degree", n_surface->Degree(0));
	brlcad::step::SetInteger(curr_surface, "v_degree", n_surface->Degree(1));
	ON_NurbsSurfaceCV_Initialize(n_surface, curr_surface, info);
	info->surf_genagg[curr_surface] = dynamic_cast<GenericAggregate *>(
	    brlcad::step::Aggregate(curr_surface, "control_points_list"));

	ON_NurbsSurfaceKnots_to_Aggregates(
	    dynamic_cast<IntAggregate *>(brlcad::step::Aggregate(curr_surface,
		"u_multiplicities")),
	    dynamic_cast<IntAggregate *>(brlcad::step::Aggregate(curr_surface,
		"v_multiplicities")),
	    dynamic_cast<RealAggregate *>(brlcad::step::Aggregate(curr_surface, "u_knots")),
	    dynamic_cast<RealAggregate *>(brlcad::step::Aggregate(curr_surface, "v_knots")),
	    n_surface);

	brlcad::step::SetEnum(curr_surface, "surface_form", "UNSPECIFIED");
	brlcad::step::SetEnum(curr_surface, "knot_spec", "UNSPECIFIED");
	/* TODO - for now, assume the surfaces don't self-intersect - need to figure out how to test this */
	brlcad::step::SetLogical(curr_surface, "self_intersect", LFalse);
	brlcad::step::SetLogical(curr_surface, "u_closed",
	    n_surface->IsClosed(0) ? LTrue : LFalse);
	brlcad::step::SetLogical(curr_surface, "v_closed",
	    n_surface->IsClosed(1) ? LTrue : LFalse);
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
