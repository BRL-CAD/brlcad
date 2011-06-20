/*                 Axis2Placement.h
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
/** @file step/Axis2Placement.h
 *
 * Class definition used to convert STEP "Axis2Placement" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef AXIS2PLACEMENT_H_
#define AXIS2PLACEMENT_H_

#include "STEPEntity.h"

class Placement;

enum axis2_placement_type {
	AXIS2_PLACEMENT_2D,
	AXIS2_PLACEMENT_3D
};

class Axis2Placement : public STEPEntity {
private:
	static string entityname;

protected:
	Placement *value;
	axis2_placement_type type;

public:
	Axis2Placement();
	virtual ~Axis2Placement();
	Axis2Placement(STEPWrapper *sw,int step_id);
	const double *GetOrigin();
	const double *GetNormal();
	const double *GetXAxis();
	const double *GetYAxis();
	bool Load(STEPWrapper *sw,SCLP23(Select) *sse);
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* AXIS2PLACEMENT_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
