/*                 SiUnit.cpp
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
/** @file step/SiUnit.cpp
 *
 * Routines to convert STEP "SiUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "DimensionalExponents.h"
#include "SiUnit.h"

#define CLASSNAME "SiUnit"
#define ENTITYNAME "Si_Unit"
string SiUnit::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)SiUnit::Create);


static const char *Si_prefix_string[] = {
	"exa",
	"peta",
	"tera",
	"giga",
	"mega",
	"kilo",
	"hecto",
	"deca",
	"deci",
	"centi",
	"milli",
	"micro",
	"nano",
	"pico",
	"femto",
	"atto",
	"unset"
};

static const char *Si_unit_name_string[] = {
	"metre",
	"gram",
	"second",
	"ampere",
	"kelvin",
	"mole",
	"candela",
	"radian",
	"steradian",
	"hertz",
	"newton",
	"pascal",
	"joule",
	"watt",
	"coulomb",
	"volt",
	"farad",
	"ohm",
	"siemens",
	"weber",
	"tesla",
	"henry",
	"degree_celsius",
	"lumen",
	"lux",
	"becquerel",
	"gray",
	"sievert",
	"unset"
};

SiUnit::SiUnit() {
	step = NULL;
	id = 0;
}

SiUnit::SiUnit(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}

SiUnit::~SiUnit() {
}

double
SiUnit::GetLengthConversionFactor() {
	if (name == Si_unit_name__metre) {
		double pf = GetPrefixFactor();
		return 1000.0*pf; // local units millmeters
	}
	return 1.e3; //SiUnit for length better be Si_unit_name__metre
}

double
SiUnit::GetPlaneAngleConversionFactor() {
	if (name == Si_unit_name__radian) {
		double pf = GetPrefixFactor();
		return pf; // local units radians
	}
	return 1.e0; //SiUnit for plane angle better be Si_unit_name__radian
}

double
SiUnit::GetSolidAngleConversionFactor() {
	if (name == Si_unit_name__steradian) {
		double pf = GetPrefixFactor();
		return pf; // local units radians
	}
	return 1.e0; //SiUnit for solid angle better be Si_unit_name__steradian
}

double
SiUnit::GetPrefixFactor() {
	switch (prefix) {
	case Si_prefix__exa:
		return 1.e18;
	case Si_prefix__peta:
		return 1.e15;
	case Si_prefix__tera:
		return 1.e12;
	case Si_prefix__giga:
		return 1.e9;
	case Si_prefix__mega:
		return 1.e6;
	case Si_prefix__kilo:
		return 1.e3;
	case Si_prefix__hecto:
		return 1.e2;
	case Si_prefix__deca:
		return 1.e1;
	case Si_prefix__deci:
		return 1.e-1;
	case Si_prefix__centi:
		return 1.e-2;
	case Si_prefix__milli:
		return 1.e-3;
	case Si_prefix__micro:
		return 1.e-6;
	case Si_prefix__nano:
		return 1.e-9;
	case Si_prefix__pico:
		return 1.e-12;
	case Si_prefix__femto:
		return 1.e-15;
	case Si_prefix__atto:
		return 1.e-18;
	default:
		return 1.e0; //assuming unknown is meters
	}
}

bool
SiUnit::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;


	// load base class attributes
	if ( !NamedUnit::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	prefix = (Si_prefix)step->getEnumAttribute(sse,"prefix");
	if (prefix > Si_prefix_unset)
		prefix = Si_prefix_unset;

	name = (Si_unit_name)step->getEnumAttribute(sse,"name");
	if (name > Si_unit_name_unset)
		name = Si_unit_name_unset;

	return true;
}

void
SiUnit::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Local Attributes:" << std::endl;
	TAB(level+1); std::cout << "prefix:" << Si_prefix_string[prefix]<< std::endl;
	TAB(level+1); std::cout << "name:" << Si_unit_name_string[name] << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	NamedUnit::Print(level+1);

}
STEPEntity *
SiUnit::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		SiUnit *object = new SiUnit(sw,sse->STEPfile_id);

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
