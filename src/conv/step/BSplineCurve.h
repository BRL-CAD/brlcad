/*                 BSplineCurve.h
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
/** @file step/BSplineCurve.h
 *
 * Class definition used to convert STEP "BSplineCurve" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef BSPLINECURVE_H_
#define BSPLINECURVE_H_

#include "BoundedCurve.h"

class CartesianPoint;
typedef list<CartesianPoint *> LIST_OF_POINTS;

class BSplineCurve : public BoundedCurve {
private:
	static string entityname;

protected:
	int degree;
	LIST_OF_POINTS control_points_list;
#ifdef YAYA
	B_spline_curve_form curve_form;
	Logical closed_curve;
	Logical self_intersect;
#else
	int curve_form;
	int closed_curve;
	int self_intersect;
#endif

public:
	BSplineCurve();
	virtual ~BSplineCurve();
	BSplineCurve(STEPWrapper *sw,int step_id);
	void AddPolyLine(ON_Brep *brep);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
/*TODO: remove
	virtual const double *PointAtEnd();
	virtual const double *PointAtStart();
*/
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* BSPLINECURVE_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
