/*                 STEPWrapper.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2009 United States Government as represented by
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
/** @file STEPWrapper.cpp
 *
 * C++ wrapper to NIST STEP parser/database functions.
 *
 */
#include "STEPWrapper.h"

#include <cctype>
#include <string>
#include <algorithm>

STEPWrapper::STEPWrapper() {
	registry = NULL;
	sfile = NULL;
}

STEPWrapper::~STEPWrapper() {
	if (sfile)
		delete sfile;
	if (registry)
		delete registry;
}

bool
STEPWrapper::loadSTEP(string &stepfile) {
	registry = new Registry(SchemaInit);
	sfile = new STEPfile(*registry, instance_list);

	this->stepfile = stepfile;
	try {
		/* load STEP file */
		sfile->ReadExchangeFile(stepfile.c_str());

	} catch (std::exception& e) {
		cerr << e.what() << endl;
		return false;
	}
	return true;

}
void
STEPWrapper::printLoadStatistics() {

    int num_ents = instance_list.InstanceCount();
    int num_schma_ents = registry->GetEntityCnt();

    // "Reset" the Schema and Entity hash tables... this sets things up
    // so we can walk through the table using registry->NextEntity()

    registry->ResetSchemas();
    registry->ResetEntities();

    // Print out what schema we're running through.

    const SchemaDescriptor *schema = registry->NextSchema();

    // "Loop" through the schema, building one of each entity type.

    const EntityDescriptor *ent;   // needs to be declared const...
    string filler = "....................................................................................";
    cout << "Loaded " << num_ents << " instances from STEP file \"" << stepfile << "\"" << endl;

    int numEntitiesUsed=0;
    for (int i=0; i < num_schma_ents; i++)
    {
	ent = registry->NextEntity();

	int entCount = instance_list.EntityKeywordCount(ent->Name());
	// possibly use boost string formater if available
	if ( entCount > 0 ) {
		cout << "\t" << ent->Name() << filler.substr(0,filler.length() - ((string)ent->Name()).length()) << entCount << endl;
		numEntitiesUsed++;
	}
    }
    cout << "Used " << numEntitiesUsed << " entities of the available " << num_schma_ents << " in schema \"" << schema->Name() << endl;
}

void
STEPWrapper::printEntity(SCLP23(Application_instance) *se, int level) {
	for ( int i=0; i< level; i++)
		cout << "    ";
	cout << "Entity:" << se->EntityName() << "(" << se->STEPfile_id << ")" << endl;
	for ( int i=0; i< level; i++)
		cout << "    ";
	cout << "Description:" << se->eDesc->Description() << endl;
	for ( int i=0; i< level; i++)
		cout << "    ";
	cout << "Entity Type:" << se->eDesc->Type() << endl;
	for ( int i=0; i< level; i++)
		cout << "    ";
	cout << "Atributes:" << endl;

	STEPattribute *attr;
	se->ResetAttributes();
	while ( (attr = se->NextAttribute()) != NULL ) {
		SCLstring attrval;

		for ( int i=0; i<= level; i++)
			cout << "    ";
		cout << attr->Name() << ": " << attr->asStr(attrval) << " TypeName: " << attr->TypeName() << " Type: " << attr->Type() << endl;
		if ( attr->Type() == 256 ) {
			if (attr->IsDerived()) {
				for ( int i=0; i<= level; i++)
					cout << "    ";
				cout << "        ********* DERIVED *********" << endl;
			} else {
			    printEntity(*(attr->ptr.c),level+2);
			}
		} else if ((attr->Type() == SET_TYPE)||(attr->Type() == LIST_TYPE)) {
			STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);

			if ( attr->BaseType() == ENTITY_TYPE ) {
				printEntityAggregate(sa, level+2);
			}
		}

	}
}


void
STEPWrapper::printEntityAggregate(STEPaggregate *sa, int level) {
	SCLstring strVal;

	for ( int i=0; i< level; i++)
		cout << "    ";
	cout << "Aggregate:" << sa->asStr(strVal) << endl;

	EntityNode *sn = (EntityNode *)sa->GetHead();
	SCLP23(Application_instance) *sse;
	while ( sn != NULL) {
		sse = (SCLP23(Application_instance) *)sn->node;

		if (((sse->eDesc->Type() == SET_TYPE)||(sse->eDesc->Type() == LIST_TYPE))&&(sse->eDesc->BaseType() == ENTITY_TYPE)) {
			printEntityAggregate((STEPaggregate *)sse,level+2);
		} else if ( sse->eDesc->Type() == ENTITY_TYPE ) {
			printEntity(sse,level+2);
		} else {
			cout << "Instance Type not handled:" << endl;
		}
		sn = (EntityNode *)sn->NextNode();
	}
}

bool
STEPWrapper::convertSTEP(BRLCADWrapper *dotg) {
	this->dotg = dotg;

	int num_ents = instance_list.InstanceCount();

	for( int i=0; i < num_ents; i++){
		SCLP23(Application_instance) *se = instance_list.GetSTEPentity(i);
		std::string name = se->EntityName();
		std::transform(name.begin(),name.end(),name.begin(),(int(*)(int))std::tolower);

		// started looking at advanced BREP shape representation
		if ( se->IsA(config_control_designe_advanced_brep_shape_representation) ) {
			printEntity(se,1);
		}
	}

	return true;
}
