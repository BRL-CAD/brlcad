/*        C O M P O U N D R E P R E S E N T A T I O N I T E M . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "CompoundRepresentationItem.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <set>

#define CLASSNAME "CompoundRepresentationItem"
#define ENTITYNAME "Compound_Representation_Item"

string CompoundRepresentationItem::entityname = Factory::RegisterClass(
    ENTITYNAME, (FactoryMethod)CompoundRepresentationItem::Create);

CompoundRepresentationItem::CompoundRepresentationItem()
{
    step = NULL;
    id = 0;
}

CompoundRepresentationItem::CompoundRepresentationItem(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

CompoundRepresentationItem::~CompoundRepresentationItem()
{
    /* Members are owned by STEPWrapper's entity cache. */
    elements.clear();
}

bool
CompoundRepresentationItem::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse ? sse->STEPfile_id : 0;
    if (!sse || !RepresentationItem::Load(sw, sse)) {
	if (id > 0) sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sse = step->getEntity(sse, ENTITYNAME);
    SDAI_Select *selected = step->getSelectAttribute(sse, "item_element");
    if (!selected || !selected->exists()) {
	sw->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
	    "COMPOUND_REPRESENTATION_ITEM", "item_element",
	    "missing LIST/SET representation item selection");
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    /* STEPcode does not expose aggregate-valued SELECT contents through its
     * base SDAI_Select API.  Its canonical STEPwrite is lossless here and
     * consists solely of a typed aggregate of entity references.  Resolve
     * those references through the already materialized instance manager. */
    std::string serialized;
    selected->STEPwrite(serialized);
    std::set<int> seen;
    for (const char *cursor = serialized.c_str(); *cursor;) {
	if (*cursor != '#') {
	    ++cursor;
	    continue;
	}
	++cursor;
	if (!std::isdigit(static_cast<unsigned char>(*cursor))) {
	    sw->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"COMPOUND_REPRESENTATION_ITEM", "item_element",
		"malformed entity reference in compound item selection");
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
	errno = 0;
	char *end = NULL;
	long value = std::strtol(cursor, &end, 10);
	if (errno || !end || end == cursor || value <= 0 || value > INT_MAX) {
	    sw->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"COMPOUND_REPRESENTATION_ITEM", "item_element",
		"out-of-range entity reference in compound item selection");
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
	cursor = end;
	if (!seen.insert(static_cast<int>(value)).second)
	    continue;
	SDAI_Application_instance *entity = step->getEntity(static_cast<int>(value));
	RepresentationItem *item = entity ? dynamic_cast<RepresentationItem *>(
	    Factory::CreateObject(sw, entity)) : NULL;
	if (!item) {
	    sw->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"COMPOUND_REPRESENTATION_ITEM", "item_element",
		"referenced representation item could not be materialized");
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
	elements.push_back(item);
    }

    if (elements.empty()) {
	sw->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
	    "COMPOUND_REPRESENTATION_ITEM", "item_element",
	    "empty compound representation item");
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
}

bool
CompoundRepresentationItem::LoadONBrep(ON_Brep *)
{
    /* A compound is a container; callers must convert each member exactly. */
    return false;
}

void
CompoundRepresentationItem::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << "(" << id << ")" << std::endl;
    for (std::vector<RepresentationItem *>::const_iterator item = elements.begin();
	 item != elements.end(); ++item) {
	if (*item) (*item)->Print(level + 1);
    }
}

STEPEntity *
CompoundRepresentationItem::GetInstance(STEPWrapper *sw, int step_id)
{
    return new CompoundRepresentationItem(sw, step_id);
}

STEPEntity *
CompoundRepresentationItem::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
