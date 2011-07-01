/*                 SurfaceCurve.h
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
/** @file step/SurfaceCurve.h
 *
 * Class definition used to convert STEP "SurfaceCurve" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef SURFACECURVE_H_
#define SURFACECURVE_H_

#include "Curve.h"

class Surface;
class DefinitionalRepresentation;
class PCurveOrSurface;

typedef list<PCurveOrSurface *> LIST_OF_PCURVE_OR_SURFACE;

class SurfaceCurve : public Curve {
private:
	static string entityname;

protected:
	Curve *curve_3d;
	LIST_OF_PCURVE_OR_SURFACE associated_geometry;
	Preferred_surface_curve_representation master_representation;

public:
	SurfaceCurve();
	virtual ~SurfaceCurve();
	SurfaceCurve(STEPWrapper *sw,int step_id);
	//TODO: check for ANDOR types of SURFACE_CURVE
	virtual curve_type CurveType() { return SURFACE_CURVE; };
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual const double *PointAtEnd();
	virtual const double *PointAtStart();
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* SURFACECURVE_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
