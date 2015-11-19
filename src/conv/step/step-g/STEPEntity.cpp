/*                  S T E P E N T I T Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/STEPEntity.cpp
 *
 * STEPEntity.cpp
 *
 */

/* interface header */
#include "STEPEntity.h"
#include "Factory.h"


STEPEntity::STEPEntity()
{
    step = NULL;
    id = 0;
    ON_id = -1;
}


STEPEntity::~STEPEntity()
{
}


int
STEPEntity::STEPid()
{
    return id;
}


STEPWrapper *STEPEntity::Step()
{
    return step;
}


STEPEntity *
STEPEntity::CreateEntity(
    STEPWrapper *sw,
    SDAI_Application_instance *sse,
    EntityInstanceFunc Instance,
    const char *classname)
{
    Factory::OBJECTS::iterator i;

    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	STEPEntity *object = Instance(sw, sse->STEPfile_id);

	if (!object->Load(sw, sse)) {
	    std::cerr << classname << ":Error loading class in ::Create() method." << std::endl;
	    delete object;
	    return NULL;
	}

	Factory::AddObject(object);

	return static_cast<STEPEntity *>(object);
    } else {
	return (*i).second;
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
