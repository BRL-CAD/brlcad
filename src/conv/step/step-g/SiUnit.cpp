/*                 SiUnit.cpp
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
string SiUnit::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)SiUnit::Create);


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

enum StepSiPrefixValue {
    STEP_SI_EXA,
    STEP_SI_PETA,
    STEP_SI_TERA,
    STEP_SI_GIGA,
    STEP_SI_MEGA,
    STEP_SI_KILO,
    STEP_SI_HECTO,
    STEP_SI_DECA,
    STEP_SI_DECI,
    STEP_SI_CENTI,
    STEP_SI_MILLI,
    STEP_SI_MICRO,
    STEP_SI_NANO,
    STEP_SI_PICO,
    STEP_SI_FEMTO,
    STEP_SI_ATTO,
    STEP_SI_PREFIX_UNSET
};

enum StepSiUnitNameValue {
    STEP_SI_METRE,
    STEP_SI_GRAM,
    STEP_SI_SECOND,
    STEP_SI_AMPERE,
    STEP_SI_KELVIN,
    STEP_SI_MOLE,
    STEP_SI_CANDELA,
    STEP_SI_RADIAN,
    STEP_SI_STERADIAN,
    STEP_SI_HERTZ,
    STEP_SI_NEWTON,
    STEP_SI_PASCAL,
    STEP_SI_JOULE,
    STEP_SI_WATT,
    STEP_SI_COULOMB,
    STEP_SI_VOLT,
    STEP_SI_FARAD,
    STEP_SI_OHM,
    STEP_SI_SIEMENS,
    STEP_SI_WEBER,
    STEP_SI_TESLA,
    STEP_SI_HENRY,
    STEP_SI_DEGREE_CELSIUS,
    STEP_SI_LUMEN,
    STEP_SI_LUX,
    STEP_SI_BECQUEREL,
    STEP_SI_GRAY,
    STEP_SI_SIEVERT,
    STEP_SI_UNIT_NAME_UNSET
};

SiUnit::SiUnit()
{
    step = NULL;
    id = 0;
    prefix = STEP_SI_PREFIX_UNSET;
    name = STEP_SI_UNIT_NAME_UNSET;
}

SiUnit::SiUnit(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    prefix = STEP_SI_PREFIX_UNSET;
    name = STEP_SI_UNIT_NAME_UNSET;
}

SiUnit::~SiUnit()
{
}

double
SiUnit::GetLengthConversionFactor()
{
    if (name == STEP_SI_METRE) {
	double pf = GetPrefixFactor();
	return 1000.0 * pf; // local units millimeters
    }
    return 1.e3; //SiUnit for length better be Si_unit_name__metre
}

double
SiUnit::GetPlaneAngleConversionFactor()
{
    if (name == STEP_SI_RADIAN) {
	double pf = GetPrefixFactor();
	return pf; // local units radians
    }
    return 1.e0; //SiUnit for plane angle better be Si_unit_name__radian
}

double
SiUnit::GetSolidAngleConversionFactor()
{
    if (name == STEP_SI_STERADIAN) {
	double pf = GetPrefixFactor();
	return pf; // local units radians
    }
    return 1.e0; //SiUnit for solid angle better be Si_unit_name__steradian
}

double
SiUnit::GetPrefixFactor()
{
    switch (prefix) {
	case STEP_SI_EXA:
	    return 1.e18;
	case STEP_SI_PETA:
	    return 1.e15;
	case STEP_SI_TERA:
	    return 1.e12;
	case STEP_SI_GIGA:
	    return 1.e9;
	case STEP_SI_MEGA:
	    return 1.e6;
	case STEP_SI_KILO:
	    return 1.e3;
	case STEP_SI_HECTO:
	    return 1.e2;
	case STEP_SI_DECA:
	    return 1.e1;
	case STEP_SI_DECI:
	    return 1.e-1;
	case STEP_SI_CENTI:
	    return 1.e-2;
	case STEP_SI_MILLI:
	    return 1.e-3;
	case STEP_SI_MICRO:
	    return 1.e-6;
	case STEP_SI_NANO:
	    return 1.e-9;
	case STEP_SI_PICO:
	    return 1.e-12;
	case STEP_SI_FEMTO:
	    return 1.e-15;
	case STEP_SI_ATTO:
	    return 1.e-18;
	default:
	    return 1.e0; //assuming unknown is meters
    }
}

bool
SiUnit::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;


    // load base class attributes
    if (!NamedUnit::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    prefix = step->getEnumAttributeIndex(sse, "prefix", Si_prefix_string,
	sizeof(Si_prefix_string) / sizeof(Si_prefix_string[0]),
	STEP_SI_PREFIX_UNSET);

    name = step->getEnumAttributeIndex(sse, "name", Si_unit_name_string,
	sizeof(Si_unit_name_string) / sizeof(Si_unit_name_string[0]),
	STEP_SI_UNIT_NAME_UNSET);

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
SiUnit::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Local Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "prefix:" << Si_prefix_string[prefix] << std::endl;
    TAB(level + 1);
    std::cout << "name:" << Si_unit_name_string[name] << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    NamedUnit::Print(level + 1);

}

STEPEntity *
SiUnit::GetInstance(STEPWrapper *sw, int id)
{
    return new SiUnit(sw, id);
}

STEPEntity *
SiUnit::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
