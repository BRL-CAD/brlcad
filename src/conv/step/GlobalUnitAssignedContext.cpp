/*                 GlobalUnitAssignedContext.cpp
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
/** @file step/GlobalUnitAssignedContext.cpp
 *
 * Routines to convert STEP "GlobalUnitAssignedContext" to BRL-CAD BREP
 * structures.
 *
 */

//#include "SdaiCONFIG_CONTROL_DESIGN.h"
#include "STEPWrapper.h"
#include "Factory.h"

#include "Unit.h"
#include "NamedUnit.h"
#include "SiUnit.h"
#include "MeasureValue.h"
#include "Unit.h"
#include "MeasureWithUnit.h"
#include "LengthSiUnit.h"
#include "LengthConversionBasedUnit.h"
#include "LengthContextDependentUnit.h"
#include "PlaneAngleSiUnit.h"
#include "PlaneAngleConversionBasedUnit.h"
#include "PlaneAngleContextDependentUnit.h"
#include "SolidAngleSiUnit.h"
#include "SolidAngleConversionBasedUnit.h"
#include "SolidAngleContextDependentUnit.h"


//#include "DerivedUnit.h"
#include "GlobalUnitAssignedContext.h"

#define CLASSNAME "GlobalUnitAssignedContext"
#define ENTITYNAME "Global_Unit_Assigned_Context"
string GlobalUnitAssignedContext::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)GlobalUnitAssignedContext::Create);


GlobalUnitAssignedContext::GlobalUnitAssignedContext() {
    step = NULL;
    id = 0;
}

GlobalUnitAssignedContext::GlobalUnitAssignedContext(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
}

GlobalUnitAssignedContext::~GlobalUnitAssignedContext() {
    /*
      LIST_OF_UNITS::iterator i = units.begin();

      while(i!=units.end()) {
      delete (*i);
      i = units.erase(i);
      }
    */
    units.clear();
}

double
GlobalUnitAssignedContext::GetLengthConversionFactor() {
    LIST_OF_UNITS::iterator i = units.begin();

    while(i != units.end()) {
	LengthSiUnit *si = dynamic_cast<LengthSiUnit *>(*i);
	if (si != NULL) {
	    //found SI length unit
	    return  si->GetLengthConversionFactor();
	}
	LengthConversionBasedUnit *cb = dynamic_cast<LengthConversionBasedUnit *>(*i);
	if (cb != NULL) {
	    //found conversion based length unit
	    return cb->GetLengthConversionFactor();
	}
	LengthContextDependentUnit *cd = dynamic_cast<LengthContextDependentUnit *>(*i);
	if (cd != NULL) {
	    //found conversion based length unit
	    std::cerr << "found context dependent length unit" << std::endl;
	    return 1.0;
	}
	i++;
    }
    return 1.0; //assume no conversion factor
}

double
GlobalUnitAssignedContext::GetPlaneAngleConversionFactor() {
    LIST_OF_UNITS::iterator i = units.begin();

    while(i != units.end()) {
	PlaneAngleSiUnit *si = dynamic_cast<PlaneAngleSiUnit *>(*i);
	if (si != NULL) {
	    //found SI length unit
	    return  si->GetPlaneAngleConversionFactor();
	}
	PlaneAngleConversionBasedUnit *cb = dynamic_cast<PlaneAngleConversionBasedUnit *>(*i);
	if (cb != NULL) {
	    //found conversion based length unit
	    return cb->GetPlaneAngleConversionFactor();
	}
	PlaneAngleContextDependentUnit *cd = dynamic_cast<PlaneAngleContextDependentUnit *>(*i);
	if (cd != NULL) {
	    //found conversion based length unit
	    std::cerr << "found context dependent length unit" << std::endl;
	    return 1.0;
	}
	i++;
    }
    return 1.0; //assume no conversion factor
}

double
GlobalUnitAssignedContext::GetSolidAngleConversionFactor() {
    LIST_OF_UNITS::iterator i = units.begin();

    while(i != units.end()) {
	SolidAngleSiUnit *si = dynamic_cast<SolidAngleSiUnit *>(*i);
	if (si != NULL) {
	    //found SI length unit
	    return  si->GetSolidAngleConversionFactor();
	}
	SolidAngleConversionBasedUnit *cb = dynamic_cast<SolidAngleConversionBasedUnit *>(*i);
	if (cb != NULL) {
	    //found conversion based length unit
	    std::cerr << "found SI length unit" << std::endl;
	    return 1.0;
	}
	SolidAngleContextDependentUnit *cd = dynamic_cast<SolidAngleContextDependentUnit *>(*i);
	if (cd != NULL) {
	    //found conversion based length unit
	    std::cerr << "found context dependent length unit" << std::endl;
	    return 1.0;
	}
	i++;
    }
    return 1.0; //assume no conversion factor
}

bool
GlobalUnitAssignedContext::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if ( !RepresentationContext::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::RepresentationContext." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (units.empty()) {
	SdaiUnit *unit;
	LIST_OF_SELECTS *l = step->getListOfSelects(sse,"units");
	LIST_OF_SELECTS::iterator i;
	for(i=l->begin();i!=l->end();i++) {
	    unit=(SdaiUnit *)(*i);
	    if (unit->IsNamed_unit()){
		SdaiNamed_unit *snu = *unit;
		NamedUnit *nu = (NamedUnit*)Factory::CreateObject(sw,(SCLP23(Application_instance) *)snu);
		units.push_back(nu);
#ifdef AP203e2
	    } else if (unit->IsDerived_unit()) { 		//TODO: derived_unit
		SdaiDerived_unit *sdu = *unit;
		NamedUnit *nu = (NamedUnit*)
		    Unit *du = (Unit *)Factory::CreateObject(sw,(SCLP23(Application_instance) *)sdu);
		units.push_back(du);
#endif
	    } else {
		l->clear();
		delete l;
		std::cout << CLASSNAME << ":Error unhandled unit type in units list." << std::endl;
		return false;
	    }
	}

	l->clear();
	delete l;
    }
    return true;
}

void
GlobalUnitAssignedContext::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    TAB(level+1); std::cout << "units(list):" << std::endl;
    LIST_OF_UNITS::iterator i;
    for(i=units.begin();i!=units.end();i++) {
	(*i)->Print(level+1);
	std::cout << std::endl;
    }

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    RepresentationContext::Print(level+1);
}
STEPEntity *
GlobalUnitAssignedContext::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	GlobalUnitAssignedContext *object = new GlobalUnitAssignedContext(sw,sse->STEPfile_id);

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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
