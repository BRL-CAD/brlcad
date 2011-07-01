/*                 BSplineSurface.h
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
/** @file step/BSplineSurface.h
 *
 * Class definition used to interface to STEP "BSplineSurface".
 *
 */

#ifndef BSPLINESURFACE_H_
#define BSPLINESURFACE_H_

#include <list>

#include "BoundedSurface.h"

// forward declaration of class
class CartesianPoint;
typedef list<CartesianPoint *> LIST_OF_POINTS;
typedef list<LIST_OF_POINTS *> LIST_OF_LIST_OF_POINTS;

class BSplineSurface: public BoundedSurface {
private:
	static string entityname;

protected:
	int u_degree;
	int v_degree;
	LIST_OF_LIST_OF_POINTS *control_points_list;
#ifdef YAYA
	B_spline_surface_form surface_form;
	Logical u_closed;
	Logical v_closed;
	Logical self_intersect;
#else
	int surface_form;
	int u_closed;
	int v_closed;
	int self_intersect;
#endif

public:
	BSplineSurface();
	virtual ~BSplineSurface();
	BSplineSurface(STEPWrapper *sw,int step_id);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);
	string Form();

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* BSPLINESURFACE_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
