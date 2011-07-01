/*                 Curve.h
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
/** @file step/Curve.h
 *
 * Class definition used to convert STEP "Curve" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CURVE_H_
#define CURVE_H_

#include "GeometricRepresentationItem.h"
class Vertex;

class Curve : virtual public GeometricRepresentationItem {
public:
	enum curve_type {
		LINE,
		CONIC,
		PCURVE,
		SURFACE_CURVE,
		OFFSET_CURVE_2D,
		OFFSET_CURVE_3D,
		CURVE_REPLICA,
		UNKNOWN_CURVE
	};

private:
	static string entityname;

protected:
	bool trimmed;
	bool parameter_trim;
	double t,s;
	double trim_startpoint[3];
	double trim_endpoint[3];
	Vertex *start;
	Vertex *end;
	Curve(STEPWrapper *sw,int step_id);

public:
	Curve();
	virtual ~Curve();
	virtual curve_type CurveType() { return UNKNOWN_CURVE; };
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual const double *PointAtEnd();
	virtual const double *PointAtStart();
	virtual void Print(int level);
	virtual void SetParameterTrim(double start, double end);
	void SetPointTrim(const double *start, const double *end);
	void Start(Vertex *v);
	void End(Vertex *v);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* CURVE_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
