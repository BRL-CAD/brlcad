/*                 Parabola.h
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
/** @file step/Parabola.h
 *
 * Class definition used to convert STEP "Parabola" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef PARABOLA_H_
#define PARABOLA_H_

#include "Conic.h"

class Parabola : public Conic {
private:
	static string entityname;

protected:
	double focal_dist;

	Parabola();
	Parabola(STEPWrapper *sw,int step_id);

public:
	virtual ~Parabola();
	virtual curve_type CurveType() { return CONIC; };
	virtual conic_type ConicType() { return PARABOLA; };
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);
	virtual void SetParameterTrim(double start, double end);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* PARABOLA_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
