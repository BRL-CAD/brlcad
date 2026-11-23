/*                 BSplineCurveWithKnots.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/BSplineCurveWithKnots.h
 *
 * Class definition used to convert STEP "BSplineCurveWithKnots" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_BSPLINECURVEWITHKNOTS_H
#define CONV_STEP_STEP_G_BSPLINECURVEWITHKNOTS_H

#include <memory>

#include "BSplineCurve.h"

class ON_NurbsCurve;

typedef list<int> LIST_OF_INTEGERS;
typedef list<double> LIST_OF_REALS;

class BSplineCurveWithKnots : virtual public BSplineCurve
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;
    std::shared_ptr<const ON_NurbsCurve> exact_curve_cache;
    double cached_length_factor = 0.0;
    bool BuildCurve(ON_NurbsCurve *curve, int dimension, double length_factor,
	bool canonicalize, size_t *repeated_knots, std::string *failure) const;

protected:
    LIST_OF_INTEGERS knot_multiplicities;
    LIST_OF_REALS knots;


    int knot_spec;

public:
    BSplineCurveWithKnots();
    virtual ~BSplineCurveWithKnots();
    BSplineCurveWithKnots(STEPWrapper *sw, int step_id);
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    bool BuildParameterCurve(ON_NurbsCurve *curve) const;
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_BSPLINECURVEWITHKNOTS_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
