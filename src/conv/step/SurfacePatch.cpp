/*                 SurfacePatch.cpp
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
/** @file step/SurfacePatch.cpp
 *
 * Routines to convert STEP "SurfacePatch" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "BoundedSurface.h"

#include "SurfacePatch.h"

#define CLASSNAME "SurfacePatch"
#define ENTITYNAME "Surface_Patch"
string SurfacePatch::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)SurfacePatch::Create);

static const char *Transition_code_string[] = {
		"discontinuous",
		"continuous",
		"cont_same_gradient",
		"cont_same_gradient_same_curvature",
		"unset"
};

SurfacePatch::SurfacePatch() {
	step = NULL;
	id = 0;
	parent_surface = NULL;
}

SurfacePatch::SurfacePatch(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
	parent_surface = NULL;
}

SurfacePatch::~SurfacePatch() {
}

bool
SurfacePatch::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !FoundedItem::Load(sw,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (parent_surface == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"parent_surface");
		parent_surface = dynamic_cast<BoundedSurface *>(Factory::CreateObject(sw,entity)); //CreateCurveObject(sw,entity));
	}

	u_transition = (Transition_code)step->getEnumAttribute(sse,"u_transition");
	if (u_transition > Transition_code_unset)
		u_transition = Transition_code_unset;
	v_transition = (Transition_code)step->getEnumAttribute(sse,"v_transition");
	if (v_transition > Transition_code_unset)
		v_transition = Transition_code_unset;

	u_sense = step->getBooleanAttribute(sse,"u_sense");
	v_sense = step->getBooleanAttribute(sse,"v_sense");

	return true;
}

void
SurfacePatch::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "parent_surface:" << std::endl;
	parent_surface->Print(level+1);
	TAB(level+1); std::cout << "u_transition:" << Transition_code_string[u_transition] << std::endl;
	TAB(level+1); std::cout << "v_transition:" << Transition_code_string[v_transition] << std::endl;
	TAB(level+1); std::cout << "u_sense:" << step->getBooleanString(u_sense) << std::endl;
	TAB(level+1); std::cout << "v_sense:" << step->getBooleanString(v_sense) << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	FoundedItem::Print(level+1);
}

STEPEntity *
SurfacePatch::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		SurfacePatch *object = new SurfacePatch(sw,sse->STEPfile_id);

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

bool
SurfacePatch::LoadONBrep(ON_Brep *brep)
{
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
	return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
