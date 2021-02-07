/*                 I F C W R A P P E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2021 United States Government as represented by
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
/** @file step/ifc-g/IFCWrapper.cpp
 *
 * C++ wrapper to NIST STEP parser/database functions.
 *
 */

#include "common.h"
#include "bu/str.h"
#include <cctype>
#include <algorithm>

#include <iostream>
#include <fstream>

/* interface header */
#include "./IFCWrapper.h"

IFCWrapper::IFCWrapper()
    : registry(NULL), sfile(NULL), verbose(false)
{
    int ownsInstanceMemory = 1;
    instance_list = new InstMgr(ownsInstanceMemory);
}


IFCWrapper::~IFCWrapper()
{
    delete instance_list;
    delete sfile;
    delete registry;
}


bool
IFCWrapper::load(std::string &ifc_file)
{
    registry = new Registry(SchemaInit);
    sfile = new STEPfile(*registry, *instance_list);

    ifcfile = ifc_file;
    try {
	/* load STEP file */
	sfile->ReadExchangeFile(ifcfile.c_str());

    } catch (std::exception &e) {
	std::cerr << e.what() << std::endl;
	return false;
    }
    return true;

}

void
IFCWrapper::printLoadStatistics()
{
    int num_ents = instance_list->InstanceCount();
    int num_schma_ents = registry->GetEntityCnt();

    // "Reset" the Schema and Entity hash tables... this sets things up
    // so we can walk through the table using registry->NextEntity()

    registry->ResetSchemas();
    registry->ResetEntities();

    // Print out what schema we're running through.

    const SchemaDescriptor *schema = registry->NextSchema();

    // "Loop" through the schema, building one of each entity type.

    const EntityDescriptor *ent;   // needs to be declared const...
    std::string filler = ".....................................................................";
    std::cout << "Loaded " << num_ents << " instances from ";
    if (BU_STR_EQUAL(ifcfile.c_str(), "-")) {
	std::cout << "standard input" << std::endl;
    } else {
	std::cout << "STEP file \"" << ifcfile << "\"" << std::endl;
    }

    int numEntitiesUsed = 0;
    for (int i = 0; i < num_schma_ents; i++) {
	ent = registry->NextEntity();

	int entCount = instance_list->EntityKeywordCount(ent->Name());
	// fix below with boost string formatter when available
	if (entCount > 0) {
	    std::cout << "\t" << ent->Name() << filler.substr(0, filler.length() - ((std::string)ent->Name()).length()) << entCount << std::endl;
	    numEntitiesUsed++;
	}
    }
    std::cout << "Used " << numEntitiesUsed << " entities of the available " << num_schma_ents << " in schema \"" << schema->Name() << std::endl;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
