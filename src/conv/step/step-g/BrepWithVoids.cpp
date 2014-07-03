/*                 BrepWithVoids.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file BrepWithVoids.cpp
 *
 * Routines to convert STEP "BrepWithVoids" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "BrepWithVoids.h"
#include "OrientedClosedShell.h"

#define CLASSNAME "BrepWithVoids"
#define ENTITYNAME "Brep_With_Voids"
string BrepWithVoids::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) BrepWithVoids::Create);

BrepWithVoids::BrepWithVoids()
{
    step = NULL;
    id = 0;
}

BrepWithVoids::BrepWithVoids(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

BrepWithVoids::~BrepWithVoids()
{
    // elements created through factory will be deleted there.
    voids.clear();
}

bool BrepWithVoids::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ManifoldSolidBrep::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::ManifoldSolidBrep Item." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (voids.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "voids");
	LIST_OF_ENTITIES::iterator i;
	for (i = l->begin(); i != l->end(); i++) {
	    SDAI_Application_instance *entity = (*i);
	    if (entity) {
		OrientedClosedShell *aOCS = dynamic_cast<OrientedClosedShell *>(Factory::CreateObject(sw, entity)); //CreateSurfaceObject(sw,entity));

		if (aOCS != NULL) {
		    voids.push_back(aOCS);
		} else {
		    std::cerr << CLASSNAME << ": Unhandled entity in attribute 'voids'." << std::endl;
		    l->clear();
		    delete l;
		    return false;
		}
	    } else {
		std::cerr << CLASSNAME << ": Unhandled entity in attribute 'voids'." << std::endl;
		l->clear();
		delete l;
		return false;
	    }
	}
	l->clear();
	delete l;
    }

    return true;
}

void BrepWithVoids::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "voids:" << std::endl;
    LIST_OF_ORIENTED_CLOSED_SHELLS::iterator i;
    for (i = voids.begin(); i != voids.end(); ++i) {
	(*i)->Print(level + 1);
    }

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ManifoldSolidBrep::Print(level + 1);
}

STEPEntity *
BrepWithVoids::GetInstance(STEPWrapper *sw, int id)
{
    return new BrepWithVoids(sw, id);
}

STEPEntity *
BrepWithVoids::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool BrepWithVoids::LoadONBrep(ON_Brep *brep)
{
    if (!ManifoldSolidBrep::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    LIST_OF_ORIENTED_CLOSED_SHELLS::iterator i;
    for (i = voids.begin(); i != voids.end(); ++i) {
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
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
