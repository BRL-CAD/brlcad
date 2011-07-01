/*                 Axis2Placement2D.h
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
/** @file step/Axis2Placement2D.h
 *
 * Class definition used to convert STEP "Axis2Placement2D" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef AXIS2_PLACEMENT_2D_H_
#define AXIS2_PLACEMENT_2D_H_

#include "Placement.h"

class Direction;
class ON_Brep;

class Axis2Placement2D : public Placement {
private:
	static string entityname;
	double p[3][3];

	void BuildAxis();
	void OrthogonalComplement(double *ortho, double *vec);

protected:
	Direction *ref_direction;

public:
	Axis2Placement2D();
	virtual ~Axis2Placement2D();
	Axis2Placement2D(STEPWrapper *sw,int step_id);
	const double *GetAxis(int i);
	virtual const double *GetOrigin();
	virtual const double *GetNormal();
	virtual const double *GetXAxis();
	virtual const double *GetYAxis();
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* AXIS2_PLACEMENT_2D_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
