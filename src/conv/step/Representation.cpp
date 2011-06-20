/*                 Representation.cpp
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
/** @file step/Representation.cpp
 *
 * Routines to convert STEP "Representation" to BRL-CAD BREP
 * structures.
 *
 */

/* interface header */
#include "./Representation.h"

/* implemenation headers */
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

string Representation::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Representation::Create);


Representation::Representation() {
	step = NULL;
	id = 0;
}


Representation::Representation(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}


Representation::~Representation() {
	/*
	LIST_OF_REPRESENTATION_ITEMS::iterator i = items.begin();

	while(i != items.end()) {
		delete (*i);
		i = items.erase(i);
	}

	LIST_OF_REPRESENTATION_CONTEXT::iterator ic = context_of_items.begin();

	while(ic != context_of_items.end()) {
		delete (*ic);
		ic = context_of_items.erase(ic);
	}
	*/
	items.clear();
	if (context_of_items.size() > 1) {
		LIST_OF_REPRESENTATION_CONTEXT::iterator ic = context_of_items.begin();

		while(ic != context_of_items.end()) {
			delete (*ic);
			ic = context_of_items.erase(ic);
		}
	} else {
		context_of_items.clear();
	}
}


double
Representation::GetLengthConversionFactor() {
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
Representation::GetPlaneAngleConversionFactor() {
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
Representation::GetSolidAngleConversionFactor() {
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


bool
Representation::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	name = step->getStringAttribute(sse,"name");

	if (items.empty()) {
		LIST_OF_ENTITIES *l = step->getListOfEntities(sse,"items");
		LIST_OF_ENTITIES::iterator i;
		for(i=l->begin();i!=l->end();i++) {
			SCLP23(Application_instance) *entity = (*i);
			if (entity) {
				RepresentationItem *aRI = dynamic_cast<RepresentationItem *>(Factory::CreateObject(sw,entity));
				items.push_back(aRI);
			} else {
		std::cerr << CLASSNAME << ": Unhandled entity in attribute 'items'." << std::endl;;
				return false;
			}
		}
		l->clear();
		delete l;
	}

	if (context_of_items.empty()) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"context_of_items");
		if (entity->IsComplex()) {
			SCLP23(Application_instance) *sub_entity = step->getEntity(entity,"Geometric_Representation_Context");
			if (sub_entity) {
				GeometricRepresentationContext *aGRC = new GeometricRepresentationContext();

				context_of_items.push_back(aGRC);
				if (!aGRC->Load(step,sub_entity)) {
		    std::cout << CLASSNAME << ":Error loading GeometricRepresentationContext" << std::endl;;
					return false;
				}
			}

			sub_entity = step->getEntity(entity,"Global_Uncertainty_Assigned_Context");
			if (sub_entity) {
				GlobalUncertaintyAssignedContext *aGUAC = new GlobalUncertaintyAssignedContext();

				context_of_items.push_back(aGUAC);
				if (!aGUAC->Load(step,sub_entity)) {
		    std::cout << CLASSNAME << ":Error loading GlobalUncertaintyAssignedContext" << std::endl;;
					return false;
				}
			}

			sub_entity = step->getEntity(entity,"Global_Unit_Assigned_Context");
			if (sub_entity) {
				GlobalUnitAssignedContext *aGUAC = new GlobalUnitAssignedContext();

				context_of_items.push_back(aGUAC);
				if (!aGUAC->Load(step,sub_entity)) {
		    std::cout << CLASSNAME << ":Error loading GlobalUnitAssignedContext" << std::endl;;
					return false;
				}
			}

			sub_entity = step->getEntity(entity,"Parametric_Representation_Context");
			if (sub_entity) {
				ParametricRepresentationContext *aPRC = new ParametricRepresentationContext();

				context_of_items.push_back(aPRC);
				if (!aPRC->Load(step,sub_entity)) {
		    std::cout << CLASSNAME << ":Error loading ParametricRepresentationContext" << std::endl;;
					return false;
				}
			}
		} else {
			RepresentationContext *aRC = (RepresentationContext *)Factory::CreateObject(sw,entity);
			context_of_items.push_back(aRC);
		}
	}
	/*
	LIST_OF_STRINGS *attributes = step->getAttributes(id);

	LIST_OF_STRINGS::iterator i;
	for(i=attributes->begin(); i != attributes->end(); ++i) {
      std::string attrName = (*i);

		if (attrName.compare("name") == 0) {
			name = step->getStringAttribute(id,attrName.c_str());
		} else if (attrName.compare("items") == 0) {
			LIST_OF_ENTITIES *entities = step->getListOfEntities(id,attrName.c_str());

			LIST_OF_ENTITIES::iterator i;
			for(i=entities->begin();i!=entities->end();i++) {
				SCLP23(Application_instance) *entity = (*i);
				if (entity) {
				//if ((entity->STEPfile_id > 0 ) && ( entity->IsA(config_control_designe_manifold_solid_brep))) {
					//ManifoldSolidBrep *aMSB = new ManifoldSolidBrep();
					RepresentationItem *aRI = (RepresentationItem *)Factory::CreateRepresentationItemObject(sw,entity);

					items.push_back(aRI);

					//if ( !aMSB->Load(step,entity)) {
      //	std::cout << CLASSNAME << "::" << __func__ << "()" << ":Error loading ManifoldSolidBrep." << std::endl;;
					//	return false;
					//}
				} else {
      std::cout << CLASSNAME << "::" << __func__ << "()" << ":Unhandled entity in attribute 'items': " << entity->EntityName() << std::endl;;
					return false;
				}
			}

			entities->clear();
			delete entities;

		} else if (attrName.compare("context_of_items") == 0) {
			SCLP23(Application_instance) *ae = step->getEntityAttribute(sse,"context_of_items");

			int aeID = ae->STEPfile_id;
			SCLP23(Application_instance) *entity = step->getEntity(aeID,"Geometric_Representation_Context");
			if (entity) {
				GeometricRepresentationContext *aGRC = new GeometricRepresentationContext();

				context_of_items.push_back(aGRC);
				if (!aGRC->Load(step,entity)) {
      std::cout << CLASSNAME << ":Error loading GeometricRepresentationContext" << std::endl;;
					return false;
				}
			}

			entity = step->getEntity(aeID,"Global_Uncertainty_Assigned_Context");

			if (entity) {
				GlobalUncertaintyAssignedContext *aGUAC = new GlobalUncertaintyAssignedContext();

				context_of_items.push_back(aGUAC);
				if (!aGUAC->Load(step,entity)) {
      std::cout << CLASSNAME << ":Error loading GlobalUncertaintyAssignedContext" << std::endl;;
					return false;
				}
			}

			entity = step->getEntity(aeID,"Global_Unit_Assigned_Context");

			if (entity) {
				GlobalUnitAssignedContext *aGUAC = new GlobalUnitAssignedContext();

				context_of_items.push_back(aGUAC);
				if (!aGUAC->Load(step,entity)) {
      std::cout << CLASSNAME << ":Error loading GlobalUnitAssignedContext" << std::endl;;
					return false;
				}
			}

			entity = step->getEntity(aeID,"Parametric_Representation_Context");

			if (entity) {
				ParametricRepresentationContext *aPRC = new ParametricRepresentationContext();

				context_of_items.push_back(aPRC);
				if (!aPRC->Load(step,entity)) {
      std::cout << CLASSNAME << ":Error loading ParametricRepresentationContext" << std::endl;;
					return false;
				}
			}

		} else {
      std::cout << CLASSNAME << ":Unhandled AttrName :" << attrName << std::endl;;
		}
	}
	*/
	return true;
}


void
Representation::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;;

    TAB(level+1); std::cout << "items:" << std::endl;;
	LIST_OF_REPRESENTATION_ITEMS::iterator i;
	for(i=items.begin(); i != items.end(); ++i) {
		//ManifoldSolidBrep *msb = (ManifoldSolidBrep *)(*i);
		(*i)->Print(level+1);
		//(*i)->LoadONBrep((ON_Brep *)NULL);
		(*i)->Print(level+1);
	}

    TAB(level+1); std::cout << "context_of_items:" << std::endl;;
	LIST_OF_REPRESENTATION_CONTEXT::iterator ic;
	for(ic=context_of_items.begin(); ic != context_of_items.end(); ++ic) {
		(*ic)->Print(level+1);
	}
}


STEPEntity *
Representation::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		Representation *object = new Representation(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, sse)) {
	    std::cerr << CLASSNAME << ":Error loading class in ::Create() method." << std::endl;;
			delete object;
			return NULL;
		}
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
