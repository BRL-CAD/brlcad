/*                 BSplineCurveWithKnots.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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

#ifndef BSPLINECURVEWITHKNOTS_H_
#define BSPLINECURVEWITHKNOTS_H_

#include "BSplineCurve.h"

typedef list<int> LIST_OF_INTEGERS;
typedef list<double> LIST_OF_REALS;

class BSplineCurveWithKnots : virtual public BSplineCurve {
private:
	static string entityname;

protected:
	LIST_OF_INTEGERS knot_multiplicities;
	LIST_OF_REALS knots;
	//TODO: fix YAYA references
#ifdef YAYA
    Knot_type knot_spec;
#else
    int knot_spec;
#endif

public:
	BSplineCurveWithKnots();
	virtual ~BSplineCurveWithKnots();
	BSplineCurveWithKnots(STEPWrapper *sw,int step_id);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* BSPLINECURVEWITHKNOTS_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
