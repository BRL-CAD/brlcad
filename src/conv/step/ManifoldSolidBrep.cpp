/*                 ManifoldSolidBrep.cpp
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
/** @file step/ManifoldSolidBrep.cpp
 *
 * Routines to convert STEP "ManifoldSolidBrep" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ManifoldSolidBrep.h"
#include "ClosedShell.h"

#define CLASSNAME "ManifoldSolidBrep"
#define ENTITYNAME "Manifold_Solid_Brep"
string ManifoldSolidBrep::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)ManifoldSolidBrep::Create);

ManifoldSolidBrep::ManifoldSolidBrep() {
	step=NULL;
	id = 0;
	outer = NULL;
}

ManifoldSolidBrep::ManifoldSolidBrep(STEPWrapper *sw,int step_id) {
	step=sw;
	id = step_id;
	outer = NULL;
}

ManifoldSolidBrep::~ManifoldSolidBrep() {
	outer = NULL;
}

bool
ManifoldSolidBrep::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	// load base class attributes
	if ( !SolidModel::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::SolidModel." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (outer == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"outer");
		if (entity) {
			//outer = dynamic_cast<ClosedShell *>(Factory::CreateTopologicalObject(sw,entity));
			outer = dynamic_cast<ClosedShell *>(Factory::CreateObject(sw,entity));
		} else {
			std::cout << CLASSNAME << ":Error loading entity attribute 'outer'." << std::endl;
			return false;
		}
	}
	return true;
}

void
ManifoldSolidBrep::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "outer:" << std::endl;
	outer->Print(level+1);

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	SolidModel::Print(level+1);
}

STEPEntity *
ManifoldSolidBrep::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		ManifoldSolidBrep *object = new ManifoldSolidBrep(sw,sse->STEPfile_id);

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
ManifoldSolidBrep::LoadONBrep(ON_Brep *brep)
{
    if (!brep || !outer->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
