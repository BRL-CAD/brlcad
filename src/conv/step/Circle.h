/*                 Circle.h
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
/** @file step/Circle.h
 *
 * Class definition used to convert STEP "Circle" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CIRCLE_H_
#define CIRCLE_H_

#include "Conic.h"

class Circle : public Conic {
private:
	static string entityname;

protected:
	double radius;

	Circle();
	Circle(STEPWrapper *sw,int step_id);

public:
	virtual ~Circle();
	virtual curve_type CurveType() { return CONIC; };
	virtual conic_type ConicType() { return CIRCLE; };
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);
	virtual void SetParameterTrim(double start, double end);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* CIRCLE_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
