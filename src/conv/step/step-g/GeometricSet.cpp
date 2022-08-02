/*                 GeometricSet.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2022 United States Government as represented by
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
/** @file GeometricSet.cpp
 *
 * Routines to convert STEP "GeometricSet" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "GeometricSet.h"
#include "GeometricSetSelect.h"

#define CLASSNAME "GeometricSet"
#define ENTITYNAME "Geometric_Set"
string GeometricSet::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) GeometricSet::Create);

GeometricSet::GeometricSet()
{
    step = NULL;
    id = 0;
}

GeometricSet::GeometricSet(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

GeometricSet::~GeometricSet()
{
    // elements created through factory will be deleted there.
    elements.clear();
}

bool GeometricSet::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!GeometricRepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::ManifoldSolidBrep Item." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (elements.empty()) {

	LIST_OF_SELECTS *select_list = step->getListOfSelects(sse, "elements");
	LIST_OF_SELECTS::iterator i;
	for (i = select_list->begin(); i != select_list->end(); i++) {
	    SDAI_Select *select = (*i);

	    if (select) {
		GeometricSetSelect *aGSS = new GeometricSetSelect();
		if (aGSS) {
		    elements.push_back(aGSS);

		    if (!aGSS->Load(step, (SDAI_Application_instance *)select)) {
			std::cout << CLASSNAME << ":Error loading select attribute list 'elements' as GeometricSetSelect from GeometricSet." << std::endl;
			select_list->clear();
			delete select_list;
			return false;
		    }
		}
	    } else {
		std::cerr << CLASSNAME << ": Non-select type in select list 'elements'." << std::endl;
		select_list->clear();
		delete select_list;
		return false;
	    }
	}
	select_list->clear();
	delete select_list;
    }

    return true;
}

void GeometricSet::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "elements:" << std::endl;
    LIST_OF_GEOMETRIC_SET_SELECT::iterator i;
    for (i = elements.begin(); i != elements.end(); ++i) {
	(*i)->Print(level + 1);
    }

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    GeometricRepresentationItem::Print(level + 1);
}

STEPEntity *
GeometricSet::GetInstance(STEPWrapper *sw, int id)
{
    return new GeometricSet(sw, id);
}

STEPEntity *
GeometricSet::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool GeometricSet::LoadONBrep(ON_Brep *brep)
{
    if (!GeometricRepresentationItem::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    LIST_OF_GEOMETRIC_SET_SELECT::iterator i;
    for (i = elements.begin(); i != elements.end(); ++i) {
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
