/*                 BSplineSurface.cpp
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
/** @file step/BSplineSurface.cpp
 *
 * Routines to interface to STEP "BSplineSurface".
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "CartesianPoint.h"
#include "BSplineSurface.h"

#define CLASSNAME "BSplineSurface"
#define ENTITYNAME "B_Spline_Surface"
string BSplineSurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)BSplineSurface::Create);

static const char *B_spline_surface_form_string[] = {
	"plane_surf",
	"cylindrical_surf",
	"conical_surf",
	"spherical_surf",
	"toroidal_surf",
	"surf_of_revolution",
	"ruled_surf",
	"generalised_cone",
	"quadric_surf",
	"surf_of_linear_extrusion",
	"unspecified",
	"unset"
};

BSplineSurface::BSplineSurface() {
	step=NULL;
	id = 0;
	control_points_list = NULL;
}

BSplineSurface::BSplineSurface(STEPWrapper *sw,int step_id) {
	step=sw;
	id = step_id;
	control_points_list = NULL;
}

BSplineSurface::~BSplineSurface() {

	LIST_OF_LIST_OF_POINTS::iterator i = control_points_list->begin();

	while(i != control_points_list->end()) {
		(*i)->clear();
		delete (*i);
		i = control_points_list->erase(i);
	}
	control_points_list->clear();
	delete control_points_list;
}

bool
BSplineSurface::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {

	step=sw;
	id = sse->STEPfile_id;

	if ( !BoundedSurface::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::BoundedSurface." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	u_degree = step->getIntegerAttribute(sse,"u_degree");
	v_degree = step->getIntegerAttribute(sse,"v_degree");
	if (control_points_list == NULL)
		control_points_list = step->getListOfListOfPoints(sse,"control_points_list");
	surface_form = (B_spline_surface_form)step->getEnumAttribute(sse,"surface_form");
	if (surface_form > B_spline_surface_form_unset)
		surface_form = B_spline_surface_form_unset;
	u_closed = step->getLogicalAttribute(sse,"u_closed");
	v_closed = step->getLogicalAttribute(sse,"v_closed");
	self_intersect = step->getLogicalAttribute(sse,"self_intersect");

	return true;
}

void
BSplineSurface::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "control_points:" << std::endl;
	LIST_OF_LIST_OF_POINTS::iterator i;
	int cnt=0;
	for(i=control_points_list->begin(); i != control_points_list->end(); ++i) {
		LIST_OF_POINTS::iterator j;
		LIST_OF_POINTS *p = *i;
		TAB(level+1); std::cout << "line " << cnt++ << ":" << std::endl;
		for(j=p->begin(); j != p->end(); ++j) {
			(*j)->Print(level+1);
		}
	}

	TAB(level+1); std::cout << "u_degree:" << u_degree << std::endl;
	TAB(level+1); std::cout << "v_degree:" << v_degree << std::endl;

	TAB(level+1); std::cout << "u_closed:" << step->getLogicalString((Logical)u_closed) << std::endl;
	TAB(level+1); std::cout << "v_closed:" << step->getLogicalString((Logical)v_closed) << std::endl;
	TAB(level+1); std::cout << "self_intersect:" << step->getLogicalString((Logical)self_intersect) << std::endl;
	TAB(level+1); std::cout << "surface_form:" << B_spline_surface_form_string[surface_form] << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	BoundedSurface::Print(level+1);
}

STEPEntity *
BSplineSurface::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		BSplineSurface *object = new BSplineSurface(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, sse)) {
			std::cerr << CLASSNAME << ":Error loading class in ::Create() method." << std::endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
	}
}

string
BSplineSurface::Form() {
	return B_spline_surface_form_string[surface_form];
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
