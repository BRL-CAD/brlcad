/*                  S T E P E N T I T Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009-2026 United States Government as represented by
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

namespace {
thread_local STEPEntity::ONStateMap *active_on_state = NULL;
}


STEPEntity::STEPEntity()
{
    step = NULL;
    id = 0;
    ON_id = -1;
}


STEPEntity::~STEPEntity()
{
}


STEPEntity::ONStateScope::ONStateScope(ONStateMap *state)
    : previous(active_on_state)
{
    active_on_state = state;
}


STEPEntity::ONStateScope::~ONStateScope()
{
    active_on_state = previous;
}


int
STEPEntity::GetONId() const
{
    if (!active_on_state)
	return ON_id;
    ONStateMap::const_iterator found = active_on_state->find(this);
    return found == active_on_state->end() ? -1 : found->second;
}


void
STEPEntity::SetONId(int on_id)
{
    if (active_on_state)
	(*active_on_state)[this] = on_id;
    else
	ON_id = on_id;
}


void
STEPEntity::ResetONState()
{
    if (active_on_state)
	active_on_state->erase(this);
    else
	ON_id = -1;
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
    STEPEntity *cached = sw->FindObject(sse->STEPfile_id);
    if (!cached) {
	STEPEntity *object = Instance(sw, sse->STEPfile_id);

	if (!object->Load(sw, sse)) {
	    std::cerr << classname << ":Error loading class in ::Create() method." << std::endl;
	    delete object;
	    return NULL;
	}

	sw->AddObject(object);

	return static_cast<STEPEntity *>(object);
    } else {
	return cached;
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
