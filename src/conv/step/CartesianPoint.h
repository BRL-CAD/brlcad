/*                 CartesianPoint.h
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
/** @file step/CartesianPoint.h
 *
 * Class definition used to interface to STEP "CartesianPoint".
 *
 */

#ifndef CARTESIANPOINT_H_
#define CARTESIANPOINT_H_

#include <Point.h>

class CartesianPoint: public Point {
private:
	static string entityname;
	int vertex_index;

protected:
	double coordinates[3];

public:
	CartesianPoint();
	virtual ~CartesianPoint();
	CartesianPoint(STEPWrapper *sw,int step_id);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void AddVertex(ON_Brep *brep);
	virtual const double *Point3d() { return coordinates; };
	virtual void Print(int level);
	double X() { return coordinates[0]; };
	double Y() { return coordinates[1]; };
	double Z() { return coordinates[2]; };
	const double *Coordinates() { return coordinates; };

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* CARTESIANPOINT_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
