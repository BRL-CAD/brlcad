/*                 Representation.cpp
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
/** @file step/Representation.cpp
 *
 * Routines to convert STEP "Representation" to BRL-CAD BREP
 * structures.
 *
 */

/* interface header */
#include "./Representation.h"

/* implementation headers */
#include "STEPWrapper.h"
#include "Factory.h"

#include "ManifoldSolidBrep.h"
#include "GeometricRepresentationContext.h"
#include "GlobalUncertaintyAssignedContext.h"
#include "GlobalUnitAssignedContext.h"
#include "ParametricRepresentationContext.h"


#define BUFSIZE 255 // used for buffer size that stringifies step ID
#define CLASSNAME "Representation"
#define ENTITYNAME "Representation"

string Representation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Representation::Create);


Representation::Representation()
{
    step = NULL;
    id = 0;
}


Representation::Representation(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}


Representation::~Representation()
{
    /*
      LIST_OF_REPRESENTATION_ITEMS::iterator i = items.begin();

      while (i != items.end()) {
      delete (*i);
      i = items.erase(i);
      }

      LIST_OF_REPRESENTATION_CONTEXT::iterator ic = context_of_items.begin();

      while (ic != context_of_items.end()) {
      delete (*ic);
      ic = context_of_items.erase(ic);
      }
    */
    items.clear();
    if (context_of_items.size() > 1) {
	LIST_OF_REPRESENTATION_CONTEXT::iterator ic = context_of_items.begin();

	while (ic != context_of_items.end()) {
	    delete(*ic);
	    ic = context_of_items.erase(ic);
	}
    } else {
	context_of_items.clear();
    }
}


double
Representation::GetLengthConversionFactor()
{
    LIST_OF_REPRESENTATION_CONTEXT::iterator i = context_of_items.begin();

    while (i != context_of_items.end()) {
	GlobalUnitAssignedContext *aGUAC = dynamic_cast<GlobalUnitAssignedContext *>(*i);
	if (aGUAC != NULL) {
	    return aGUAC->GetLengthConversionFactor();
	}
	i++;
    }
    return 1000.0; // assume base of meters
}


double
Representation::GetPlaneAngleConversionFactor()
{
    LIST_OF_REPRESENTATION_CONTEXT::iterator i = context_of_items.begin();

    while (i != context_of_items.end()) {
	GlobalUnitAssignedContext *aGUAC = dynamic_cast<GlobalUnitAssignedContext *>(*i);
	if (aGUAC != NULL) {
	    return aGUAC->GetPlaneAngleConversionFactor();
	}
	i++;
    }
    return 1.0; // assume base of radians
}


double
Representation::GetSolidAngleConversionFactor()
{
    LIST_OF_REPRESENTATION_CONTEXT::iterator i = context_of_items.begin();

    while (i != context_of_items.end()) {
	GlobalUnitAssignedContext *aGUAC = dynamic_cast<GlobalUnitAssignedContext *>(*i);
	if (aGUAC != NULL) {
	    return aGUAC->GetSolidAngleConversionFactor();
	}
	i++;
    }
    return 1.0; // assume base of steradians
}


bool Representation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    name = step->getStringAttribute(sse, "name");

    if (items.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "items");
	LIST_OF_ENTITIES::iterator i;
	for (i = l->begin(); i != l->end(); i++) {
	    SDAI_Application_instance *entity = (*i);
	    if (entity) {
		RepresentationItem *aRI = dynamic_cast<RepresentationItem *>(Factory::CreateObject(sw, entity));
		if (aRI != NULL) {
		    items.push_back(aRI);
		}
	    } else {
		std::cerr << CLASSNAME << ": Unhandled entity in attribute 'items'." << std::endl;
		l->clear();
		delete l;
		return false;
	    }
	}
	l->clear();
	delete l;
    }

    if (context_of_items.empty()) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "context_of_items");
	if (entity) {
	    if (entity->IsComplex()) {
		SDAI_Application_instance *sub_entity = step->getEntity(entity, "Geometric_Representation_Context");
		if (sub_entity) {
		    GeometricRepresentationContext *aGRC = new GeometricRepresentationContext();

		    context_of_items.push_back(aGRC);
		    if (!aGRC->Load(step, sub_entity)) {
			std::cout << CLASSNAME << ":Error loading GeometricRepresentationContext" << std::endl;
			return false;
		    }
		}

		sub_entity = step->getEntity(entity, "Global_Uncertainty_Assigned_Context");
		if (sub_entity) {
		    GlobalUncertaintyAssignedContext *aGUAC = new GlobalUncertaintyAssignedContext();

		    context_of_items.push_back(aGUAC);
		    if (!aGUAC->Load(step, sub_entity)) {
			std::cout << CLASSNAME << ":Error loading GlobalUncertaintyAssignedContext" << std::endl;
			return false;
		    }
		}

		sub_entity = step->getEntity(entity, "Global_Unit_Assigned_Context");
		if (sub_entity) {
		    GlobalUnitAssignedContext *aGUAC = new GlobalUnitAssignedContext();

		    context_of_items.push_back(aGUAC);
		    if (!aGUAC->Load(step, sub_entity)) {
			std::cout << CLASSNAME << ":Error loading GlobalUnitAssignedContext" << std::endl;
			return false;
		    }
		}

		sub_entity = step->getEntity(entity, "Parametric_Representation_Context");
		if (sub_entity) {
		    ParametricRepresentationContext *aPRC = new ParametricRepresentationContext();

		    context_of_items.push_back(aPRC);
		    if (!aPRC->Load(step, sub_entity)) {
			std::cout << CLASSNAME << ":Error loading ParametricRepresentationContext" << std::endl;
			return false;
		    }
		}
	    } else {
		RepresentationContext *aRC = dynamic_cast<RepresentationContext *>(Factory::CreateObject(sw, entity));
		if (aRC != NULL) {
		    context_of_items.push_back(aRC);
		} else {
		    std::cout << CLASSNAME << ":Error loading RepresentationContext" << std::endl;
		    return false;
		}
	    }
	} else {
	    std::cout << CLASSNAME << ":Error loading \"context_of_items\"" << std::endl;
	    return false;
	}
    }
    return true;
}


void
Representation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level + 1);
    std::cout << "items:" << std::endl;
    LIST_OF_REPRESENTATION_ITEMS::iterator i;
    for (i = items.begin(); i != items.end(); ++i) {
	//ManifoldSolidBrep *msb = (ManifoldSolidBrep *)(*i);
	(*i)->Print(level + 1);
	//(*i)->LoadONBrep((ON_Brep *)NULL);
	(*i)->Print(level + 1);
    }

    TAB(level + 1);
    std::cout << "context_of_items:" << std::endl;
    LIST_OF_REPRESENTATION_CONTEXT::iterator ic;
    for (ic = context_of_items.begin(); ic != context_of_items.end(); ++ic) {
	(*ic)->Print(level + 1);
    }
}

STEPEntity *
Representation::GetInstance(STEPWrapper *sw, int id)
{
    return new Representation(sw, id);
}

STEPEntity *
Representation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
