/*               O N _ N U R B S C U R V E . C P P
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
/** @file ON_NurbsCurve.cpp
 *
 * File for writing out an ON_NurbsCurve into the STEPcode containers
 *
 */

#include "AP_Common.h"
#include "ON_Brep.h"
#include "STEPGeneratedAPI.h"


void
ON_NurbsCurveCV_to_EntityAggregate(STEPaggregate *control_pnts, ON_NurbsCurve *incrv,
    ON_Brep_Info_AP203 *info) {
    ON_3dPoint cv_pnt;
    for (int i = 0; i < incrv->CVCount(); i++) {
	STEPentity *step_cartesian = info->registry->ObjCreate("CARTESIAN_POINT");
	brlcad::step::SetString(step_cartesian, "name", "");
	info->cartesian_pnts.push_back(step_cartesian);
	incrv->GetCV(i, cv_pnt);
	ON_3dPoint_to_Cartesian_point(&(cv_pnt), step_cartesian);
	control_pnts->AddNode(new EntityNode((SDAI_Application_instance *)step_cartesian));
    }
}


void
ON_NurbsCurveKnots_to_Aggregates(IntAggregate *knot_multiplicities, RealAggregate *knots, ON_NurbsCurve *incrv)
{
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
}


/* For a rational B-Spline curve with weights, we need to create an aggregate type */

STEPentity *
Create_Rational_Curve_Aggregate(ON_NurbsCurve *ncurve, ON_Brep_Info_AP203 *info) {
    STEPcomplex *stepcomplex;
    const char *entNmArr[8] = {"bounded_curve", "b_spline_curve", "b_spline_curve_with_knots",
	"curve", "geometric_representation_item", "rational_b_spline_curve", "representation_item", "*"};
    STEPcomplex *complex_entity = new STEPcomplex(info->registry, (const char **)entNmArr, info->registry->GetEntityCnt() + 1);

    /* Set b_spline_curve data */
    stepcomplex = complex_entity->EntityPart("b_spline_curve");
    brlcad::step::SetInteger(stepcomplex, "degree", ncurve->Degree());
    ON_NurbsCurveCV_to_EntityAggregate(
	brlcad::step::Aggregate(stepcomplex, "control_points_list"), ncurve, info);
    brlcad::step::SetEnum(stepcomplex, "curve_form", "UNSPECIFIED");
    brlcad::step::SetLogical(stepcomplex, "closed_curve",
	ncurve->IsClosed() ? LTrue : LFalse);
    brlcad::step::SetLogical(stepcomplex, "self_intersect", LFalse);

    /* Set knots */
    stepcomplex = complex_entity->EntityPart("b_spline_curve_with_knots");
    IntAggregate *knot_multiplicities = dynamic_cast<IntAggregate *>(
	brlcad::step::Aggregate(stepcomplex, "knot_multiplicities"));
    RealAggregate *knots = dynamic_cast<RealAggregate *>(
	brlcad::step::Aggregate(stepcomplex, "knots"));
    if (!knot_multiplicities || !knots) {
	delete complex_entity;
	return NULL;
    }
    ON_NurbsCurveKnots_to_Aggregates(knot_multiplicities, knots, ncurve);
    brlcad::step::SetEnum(stepcomplex, "knot_spec", "UNSPECIFIED");

    /* Set weights */
    stepcomplex = complex_entity->EntityPart("rational_b_spline_curve");
    RealAggregate *weights = dynamic_cast<RealAggregate *>(
	brlcad::step::Aggregate(stepcomplex, "weights_data"));
    if (!weights) {
	delete complex_entity;
	return NULL;
    }
    for (int i = 0; i < ncurve->CVCount(); i++)
	weights->AddNode(new RealNode(ncurve->Weight(i)));

    /* Representation item */
    stepcomplex = complex_entity->EntityPart("representation_item");
    brlcad::step::SetString(stepcomplex, "name", "");

    return (STEPentity *)complex_entity;
}

bool
ON_NurbsCurve_to_STEP(ON_NurbsCurve *n_curve, ON_Brep_Info_AP203 *info, int i)
{
    /* For rational curves, we need a composite type.  Otherwise, go with the BSpline curve with knots.*/
    if (n_curve->IsRational()) {
	info->three_dimensional_curves.at(i) = Create_Rational_Curve_Aggregate(n_curve, info);
    } else {
	STEPentity *curr_curve = info->registry->ObjCreate("B_SPLINE_CURVE_WITH_KNOTS");
	info->three_dimensional_curves.at(i) = curr_curve;
	ON_NurbsCurveCV_to_EntityAggregate(
	    brlcad::step::Aggregate(curr_curve, "control_points_list"), n_curve, info);
	ON_NurbsCurveKnots_to_Aggregates(
	    dynamic_cast<IntAggregate *>(brlcad::step::Aggregate(curr_curve,
		"knot_multiplicities")),
	    dynamic_cast<RealAggregate *>(brlcad::step::Aggregate(curr_curve, "knots")),
	    n_curve);
	brlcad::step::SetString(curr_curve, "name", "");
	brlcad::step::SetInteger(curr_curve, "degree", n_curve->Degree());
	brlcad::step::SetEnum(curr_curve, "knot_spec", "UNSPECIFIED");
	brlcad::step::SetEnum(curr_curve, "curve_form", "UNSPECIFIED");
	brlcad::step::SetLogical(curr_curve, "closed_curve",
	    n_curve->IsClosed() ? LTrue : LFalse);
	/* TODO: Assume we don't have self-intersecting curves for
	 * now - need some way to test this...*/
	brlcad::step::SetLogical(curr_curve, "self_intersect", LFalse);
    }

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
