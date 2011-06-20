/*                 OffsetSurface.h
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
/** @file step/OffsetSurface.h
 *
 * Class definition used to interface to STEP "OffsetSurface".
 *
 */

#ifndef OFFSET_SURFACE_H_
#define OFFSET_SURFACE_H_

#include "Surface.h"

class OffsetSurface: public Surface {
private:
	static string entityname;

protected:
	Surface* basis_surface;
	double distance;
	//TODO: Fix all references to YAYA
#ifdef YAYA
	Logical self_intersect;
#else
	int self_intersect;
#endif

public:
	OffsetSurface();
	virtual ~OffsetSurface();
	OffsetSurface(STEPWrapper *sw,int step_id);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* OFFSET_SURFACE_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
