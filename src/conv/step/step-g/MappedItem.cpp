/*                 MappedItem.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file step/MappedItem.cpp
 *
 * Routines to convert STEP "MappedItem" to BRL-CAD structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "MappedItem.h"
#include "RepresentationMap.h"
#include "Axis2Placement3D.h"

#define CLASSNAME "MappedItem"
#define ENTITYNAME "Mapped_Item"
string MappedItem::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)MappedItem::Create);

MappedItem::MappedItem()
{
    step = NULL;
    id = 0;
    mapping_source = NULL;
    mapping_target = NULL;
}

MappedItem::MappedItem(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    mapping_source = NULL;
    mapping_target = NULL;
}

MappedItem::~MappedItem()
{
    // created through factory will be deleted there.
    mapping_source = NULL;
    mapping_target = NULL;
}

bool
MappedItem::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes (name)
    if (!RepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::RepresentationItem." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (mapping_source == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "mapping_source");
	if (entity) {
	    mapping_source = dynamic_cast<RepresentationMap *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'mapping_source'." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    if (mapping_target == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "mapping_target");
	if (entity) {
	    // mapping_target is a representation_item; for placement it is an
	    // axis2_placement_3d.  Other kinds are tolerated (stays NULL ->
	    // identity placement).
	    mapping_target = dynamic_cast<Axis2Placement3D *>(Factory::CreateObject(sw, entity));
	}
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void
MappedItem::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    if (mapping_source) {
	TAB(level + 1);
	std::cout << "mapping_source:" << std::endl;
	mapping_source->Print(level + 2);
    }
    if (mapping_target) {
	TAB(level + 1);
	std::cout << "mapping_target:" << std::endl;
	mapping_target->Print(level + 2);
    }
}

STEPEntity *
MappedItem::GetInstance(STEPWrapper *sw, int id)
{
    return new MappedItem(sw, id);
}

STEPEntity *
MappedItem::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
MappedItem::LoadONBrep(ON_Brep *UNUSED(brep))
{
    /* instancing is resolved in STEPWrapper::convert() as a comb membership
     * with a placement transform, not as direct BRep geometry.
     */
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
