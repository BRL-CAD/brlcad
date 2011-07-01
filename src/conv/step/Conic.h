/*                 Conic.h
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
/** @file step/Conic.h
 *
 * Class definition used to convert STEP "Conic" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONIC_H_
#define CONIC_H_

#include "Curve.h"

class Axis2Placement;


class Conic : public Curve {
public:
	enum conic_type {
	CIRCLE,
	ELLIPSE,
	HYPERBOLA,
	PARABOLA,
	UNKNOWN_CONIC
	};

private:
	static string entityname;

protected:
	Axis2Placement *position;

	Conic();
	Conic(STEPWrapper *sw,int step_id);

public:
	virtual ~Conic();
	const double *GetOrigin();
	const double *GetNormal();
	const double *GetXAxis();
	const double *GetYAxis();
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);
	virtual curve_type CurveType() { return CONIC; };
	virtual conic_type ConicType() { return UNKNOWN_CONIC; };

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* CONIC_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
