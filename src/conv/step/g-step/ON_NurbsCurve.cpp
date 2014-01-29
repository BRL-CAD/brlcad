/*               O N _ N U R B S C U R V E . C P P
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
/** @file ON_NurbsCurve.cpp
 *
 * File for writing out an ON_NurbsCurve into the STEPcode containers
 *
 */

#include "AP203.h"
#include "ON_Brep.h"


void
ON_NurbsCurveCV_to_EntityAggregate(EntityAggregate *control_pnts, ON_NurbsCurve *incrv, ON_Brep_Info_AP203 *info) {
    ON_3dPoint cv_pnt;
    for (int i = 0; i < incrv->CVCount(); i++) {
	SdaiCartesian_point *step_cartesian = (SdaiCartesian_point *)info->registry->ObjCreate("CARTESIAN_POINT");
	step_cartesian->name_("''");
	info->cartesian_pnts.push_back((STEPentity *)step_cartesian);
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
    STEPattribute *attr;
    STEPcomplex *stepcomplex;
    const char *entNmArr[8] = {"bounded_curve", "b_spline_curve", "b_spline_curve_with_knots",
	"curve", "geometric_representation_item", "rational_b_spline_curve", "representation_item", "*"};
    STEPcomplex *complex_entity = new STEPcomplex(info->registry, (const char **)entNmArr, info->registry->GetEntityCnt() + 1);

    /* Set b_spline_curve data */
    stepcomplex = complex_entity->EntityPart("b_spline_curve");
    stepcomplex->ResetAttributes();
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	if (!bu_strcmp(attr->Name(), "degree")) {
	    attr->ptr.i = new SDAI_Integer(ncurve->Degree());
	}
	if (!bu_strcmp(attr->Name(), "control_points_list")) {
	    EntityAggregate *control_pnts= new EntityAggregate();
	    ON_NurbsCurveCV_to_EntityAggregate(control_pnts, ncurve, info);
	    attr->ptr.a = control_pnts;
	}
	if (!bu_strcmp(attr->Name(), "curve_form")) attr->ptr.e = new SdaiB_spline_curve_form_var(B_spline_curve_form__unspecified);
	if (!bu_strcmp(attr->Name(), "closed_curve")) attr->ptr.e = new SDAI_LOGICAL((Logical)(ncurve->IsClosed()));
	if (!bu_strcmp(attr->Name(), "self_intersect")) attr->ptr.e = new SDAI_LOGICAL(LFalse);
    }

    /* Set knots */
    stepcomplex = complex_entity->EntityPart("b_spline_curve_with_knots");
    stepcomplex->ResetAttributes();
    IntAggregate *knot_multiplicities = new IntAggregate();
    RealAggregate *knots = new RealAggregate();
    ON_NurbsCurveKnots_to_Aggregates(knot_multiplicities, knots, ncurve);
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	if (!bu_strcmp(attr->Name(), "knot_multiplicities")) {
	    attr->ptr.a = knot_multiplicities;
	}
	if (!bu_strcmp(attr->Name(), "knots")) {
	    attr->ptr.a = knots;
	}
	if (!bu_strcmp(attr->Name(), "knot_spec")) attr->ptr.e = new SdaiKnot_type_var(Knot_type__unspecified);
    }

    /* Set weights */
    stepcomplex = complex_entity->EntityPart("rational_b_spline_curve");
    stepcomplex->ResetAttributes();
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	//std::cout << "  " << attr->Name() << "," << attr->NonRefType() << "\n";
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
    while ((attr = stepcomplex->NextAttribute()) != NULL) {
	if (!bu_strcmp(attr->Name(), "name")) attr->StrToVal("''");
    }

    return (STEPentity *)complex_entity;
}

bool
ON_NurbsCurve_to_STEP(ON_NurbsCurve *n_curve, ON_Brep_Info_AP203 *info, int i)
{
    /* For rational curves, we need a composite type.  Otherwise, go with the BSpline curve with knots.*/
    if (n_curve->IsRational()) {
	info->three_dimensional_curves.at(i) = Create_Rational_Curve_Aggregate(n_curve, info);
    } else {
	SdaiB_spline_curve_with_knots *curr_curve = (SdaiB_spline_curve_with_knots *)info->registry->ObjCreate("B_SPLINE_CURVE_WITH_KNOTS");
	info->three_dimensional_curves.at(i) = (STEPentity *)curr_curve;
	ON_NurbsCurveCV_to_EntityAggregate(curr_curve->control_points_list_(), n_curve, info);
	ON_NurbsCurveKnots_to_Aggregates(curr_curve->knot_multiplicities_(), curr_curve->knots_(), n_curve);
	curr_curve->name_("''");
	curr_curve->degree_(n_curve->Degree());
	curr_curve->knot_spec_(Knot_type__unspecified);
	curr_curve->curve_form_(B_spline_curve_form__unspecified);
	curr_curve->closed_curve_(SDAI_LOGICAL(n_curve->IsClosed()));
	/* TODO: Assume we don't have self-intersecting curves for
	 * now - need some way to test this...*/
	curr_curve->self_intersect_(LFalse);
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
