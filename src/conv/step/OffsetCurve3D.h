/*                 OffsetCurve3D.h
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
/** @file step/OffsetCurve3D.h
 *
 * Class definition used to convert STEP "OffsetCurve3D" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef OFFSETCURVE3D_H_
#define OFFSETCURVE3D_H_

#include "Curve.h"

class Direction;

class CartesianTransformationOperator;

class OffsetCurve3D : public Curve {
private:
	static string entityname;

protected:
	Curve *basis_curve;
	double distance;
	Logical self_intersect;
	Direction *ref_direction;

public:
	OffsetCurve3D();
	virtual ~OffsetCurve3D();
	OffsetCurve3D(STEPWrapper *sw,int step_id);
	virtual curve_type CurveType() { return OFFSET_CURVE_3D; };
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual const double *PointAtEnd();
	virtual const double *PointAtStart();
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* OFFSETCURVE3D_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
