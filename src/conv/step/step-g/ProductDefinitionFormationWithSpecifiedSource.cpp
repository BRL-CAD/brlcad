/*                 ProductDefinitionFormationWithSpecifiedSource.cpp
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
/** @file step/ProductDefinitionFormationWithSpecifiedSource.cpp
 *
 * Routines to convert STEP "ProductDefinitionFormationWithSpecifiedSource" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ProductDefinitionFormationWithSpecifiedSource.h"

#define CLASSNAME "ProductDefinitionFormationWithSpecifiedSource"
#define ENTITYNAME "Product_Definition_Formation_With_Specified_Source"
string ProductDefinitionFormationWithSpecifiedSource::entityname = Factory::RegisterClass(ENTITYNAME,
	(FactoryMethod) ProductDefinitionFormationWithSpecifiedSource::Create);

ProductDefinitionFormationWithSpecifiedSource::ProductDefinitionFormationWithSpecifiedSource()
{
    step = NULL;
    id = 0;
    make_or_buy = Source_unset;
}

ProductDefinitionFormationWithSpecifiedSource::ProductDefinitionFormationWithSpecifiedSource(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    make_or_buy = Source_unset;
}

ProductDefinitionFormationWithSpecifiedSource::~ProductDefinitionFormationWithSpecifiedSource()
{
}

string ProductDefinitionFormationWithSpecifiedSource::ClassName()
{
    return entityname;
}

string ProductDefinitionFormationWithSpecifiedSource::SourceString()
{
    string sourcestring;
    switch (make_or_buy) {
	case Source__made:
	    sourcestring = ".MADE.";
	    break;
	case Source__bought:
	    sourcestring = ".BOUGHT.";
	    break;
	case Source__not_known:
	    sourcestring = ".NOT_KNOWN.";
	    break;
	case Source_unset:
	default:
	    sourcestring = ".UNSET.";
    }
    return sourcestring;
}

bool ProductDefinitionFormationWithSpecifiedSource::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ProductDefinitionFormation::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::ProductDefinitionFormation." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    make_or_buy = (Source) step->getEnumAttribute(sse, "make_or_buy");
    if (make_or_buy > Source_unset) {
	make_or_buy = Source_unset;
    }

    return true;
}

void ProductDefinitionFormationWithSpecifiedSource::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "make_or_buy:" << SourceString() << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ProductDefinitionFormation::Print(level + 1);
}

STEPEntity *
ProductDefinitionFormationWithSpecifiedSource::GetInstance(STEPWrapper *sw, int id)
{
    return new ProductDefinitionFormationWithSpecifiedSource(sw, id);
}

STEPEntity *
ProductDefinitionFormationWithSpecifiedSource::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ProductDefinitionFormationWithSpecifiedSource::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
