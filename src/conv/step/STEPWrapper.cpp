/*                 STEPWrapper.cpp
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
/** @file step/STEPWrapper.cpp
 *
 * C++ wrapper to NIST STEP parser/database functions.
 *
 */

/* inteface header */
#include "./STEPWrapper.h"

/* implemenation headers */
#include "AdvancedBrepShapeRepresentation.h"
#include "CartesianPoint.h"
#include "VertexPoint.h"
#include "SurfacePatch.h"
#include "LocalUnits.h"

#include <cctype>
#include <algorithm>


STEPWrapper::STEPWrapper()
{
	registry = NULL;
	sfile = NULL;
}


STEPWrapper::~STEPWrapper()
{
	if (sfile)
		delete sfile;
	if (registry)
		delete registry;
}


bool STEPWrapper::convert(BRLCADWrapper *dot_g)
{
    this->dotg = dot_g;

	int num_ents = instance_list.InstanceCount();
    //std::cout << "Loaded " << num_ents << "instances from STEP file \"" << stepfile << "\"" << std::endl;
	for( int i=0; i < num_ents; i++){
		SCLP23(Application_instance) *sse = instance_list.GetSTEPentity(i);
		if (sse == NULL)
			continue;
		std::string name = sse->EntityName();
		std::transform(name.begin(),name.end(),name.begin(),(int(*)(int))std::tolower);

		if ((sse->STEPfile_id > 0 ) && ( sse->IsA(config_control_designe_advanced_brep_shape_representation))) {
			AdvancedBrepShapeRepresentation *aBrep = new AdvancedBrepShapeRepresentation();

			if ( aBrep->Load(this,sse) ) {
		name = aBrep->Name();
				//aBrep->Print(0);

				LocalUnits::length = aBrep->GetLengthConversionFactor();
				LocalUnits::planeangle = aBrep->GetPlaneAngleConversionFactor();
				LocalUnits::solidangle = aBrep->GetSolidAngleConversionFactor();
//TODO: remove debugging (used to turn off unit scaling)
//				LocalUnits::length = 1.0;
//				LocalUnits::planeangle = 1.0;
//				LocalUnits::solidangle = 1.0;
				ON_Brep *onBrep = aBrep->GetONBrep();

				ON_TextLog tl;
				onBrep->IsValid(&tl);

				//onBrep->SpSplitClosedFaces();
				//ON_Brep *tbrep = TightenBrep( onBrep );

				dotg->WriteBrep(name,onBrep);

				delete onBrep;
				delete aBrep;
			}
		}
	}

	return true;
}


SCLP23(Application_instance) *
STEPWrapper::getEntity(int STEPid)
{
	return instance_list.FindFileId(STEPid)->GetSTEPentity();
}


SCLP23(Application_instance) *
STEPWrapper::getEntity(int STEPid, const char *name)
{
	SCLP23(Application_instance) *se = getEntity(STEPid);

	if (se->IsComplex()) {
		se = getSuperType(STEPid,name);
	}
	return se;
}


SCLP23(Application_instance) *
STEPWrapper::getEntity(SCLP23(Application_instance) *sse, const char *name)
{
	if (sse->IsComplex()) {
		sse = getSuperType(sse,name);
	}
	return sse;
}


STEPattribute *
STEPWrapper::getAttribute(int STEPid, const char *name)
{
	STEPattribute *retValue = NULL;
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();
		if (attrname.compare(name) == 0) {
			retValue = attr;
			break;
		}
	}

	return retValue;
}


LIST_OF_STRINGS *
STEPWrapper::getAttributes(int STEPid)
{
	LIST_OF_STRINGS *l = new LIST_OF_STRINGS;
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string name = attr->Name();

		l->push_back(name);
	}

	return l;
}


Boolean
STEPWrapper::getBooleanAttribute(int STEPid, const char *name)
{
    Boolean retValue = BUnset;
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
	    retValue = (Boolean)(*attr->ptr.e).asInt();
			if (retValue > BUnset)
				retValue = BUnset;
			break;
		}
	}
	return retValue;
}


int
STEPWrapper::getEnumAttribute(int STEPid, const char *name)
{
	int retValue = 0;
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = (*attr->ptr.e).asInt();
			break;
		}
	}
	return retValue;
}


Logical
STEPWrapper::getLogicalAttribute(int STEPid, const char *name)
{
	Logical retValue = LUnknown;
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = (Logical)(*attr->ptr.e).asInt();
			if (retValue > LUnknown)
				retValue = LUnknown;
			break;
		}
	}
	return retValue;
}


std::string
STEPWrapper::getLogicalString(Logical v)
{
    std::string retValue = "Unknown";

	switch (v) {
	case LFalse:
		retValue = "LFalse";
		break;
	case LTrue:
		retValue = "LTrue";
		break;
	case LUnset:
		retValue = "LUnset";
		break;
	case LUnknown:
		retValue = "LUnknown";
		break;
	}
	return retValue;
}


std::string
STEPWrapper::getBooleanString(Boolean v)
{
    std::string retValue = "Unknown";

	switch (v) {
	case BFalse:
		retValue = "BFalse";
		break;
	case BTrue:
		retValue = "BTrue";
		break;
	case BUnset:
		retValue = "BUnset";
		break;
	}
	return retValue;
}


SCLP23(Application_instance) *
STEPWrapper::getEntityAttribute(int STEPid, const char *name)
{
	SCLP23(Application_instance) *retValue = NULL;
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = (SCLP23(Application_instance) *)*attr->ptr.c;
			break;
		}
	}
	return retValue;
}


int
STEPWrapper::getIntegerAttribute(int STEPid, const char *name)
{
	int retValue = 0;
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = *attr->ptr.i;
			break;
		}
	}
	return retValue;
}


double
STEPWrapper::getRealAttribute(int STEPid, const char *name)
{
	double retValue = 0.0;
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = *attr->ptr.r;
			break;
		}
	}
	return retValue;
}


LIST_OF_ENTITIES*
STEPWrapper::getListOfEntities(int STEPid, const char *name)
{
	LIST_OF_ENTITIES *l = new LIST_OF_ENTITIES;

	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();
	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
    std::string attrval;
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			STEPaggregate *sa = (STEPaggregate *)attr->ptr.a;

			EntityNode *sn = (EntityNode *)sa->GetHead();
			SCLP23(Application_instance) *se;
			while ( sn != NULL) {
				se = (SCLP23(Application_instance) *)sn->node;

				l->push_back(se);
				sn = (EntityNode *)sn->NextNode();
			}
			break;
		}
	}

	return l;
}


LIST_OF_LIST_OF_POINTS*
STEPWrapper::getListOfListOfPoints(int STEPid, const char *attrName)
{
	LIST_OF_LIST_OF_POINTS *l = new LIST_OF_LIST_OF_POINTS;

	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();
	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
    std::string attrval;
	std::string name = attr->Name();

		if (name.compare(attrName) == 0) {
			ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


			GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

			STEPnode *sn = (STEPnode *)gp->GetHead();
			//EntityAggregate *ag = new EntityAggregate();


			const char *eaStr;

			LIST_OF_POINTS *points;
			while ( sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
				eaStr = sn->asStr(attrval);
				points = parseListOfPointEntities(eaStr);
				l->push_back(points);
				sn = (STEPnode *)sn->NextNode();
			}
			break;
		}
	}

	return l;
}


MAP_OF_SUPERTYPES *
STEPWrapper::getMapOfSuperTypes(int STEPid)
{
	MAP_OF_SUPERTYPES *m = new MAP_OF_SUPERTYPES;
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	if (sse->IsComplex()) {
		STEPcomplex *sc = ((STEPcomplex *)sse)->head;
		while(sc) {
			(*m)[sc->EntityName()] = sc;
			sc = sc->sc;
		}
	}

	return m;
}


void
STEPWrapper::getSuperTypes(int STEPid, MAP_OF_SUPERTYPES &m)
{
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	if (sse->IsComplex()) {
		STEPcomplex *sc = ((STEPcomplex *)sse)->head;
		while(sc) {
			m[sc->EntityName()] = sc;
			sc = sc->sc;
		}
	}
}


SCLP23(Application_instance) *
STEPWrapper::getSuperType(int STEPid, const char *name)
{
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();
	std::string attrval;

	if (sse->IsComplex()) {
		STEPcomplex *sc = ((STEPcomplex *)sse)->head;
		while(sc) {
	    std::string ename = sc->EntityName();

			if (ename.compare(name) == 0) {
				return sc;
			}
			sc = sc->sc;
		}
	}
	return NULL;
}


std::string
STEPWrapper::getStringAttribute(int STEPid, const char *name)
{
    std::string retValue = "";
	SCLP23(Application_instance) *sse = instance_list.FindFileId(STEPid)->GetSTEPentity();

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
		std::string attrval;
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = attr->asStr(attrval);
			//if (retValue.empty())
	    //	std::cout << "String retValue:" << retValue << ":" << std::endl;
			break;
		}
	}
	return retValue;
}


STEPattribute *
STEPWrapper::getAttribute(SCLP23(Application_instance) *sse, const char *name)
{
	STEPattribute *retValue = NULL;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();
		if (attrname.compare(name) == 0) {
			retValue = attr;
			break;
		}
	}

	return retValue;
}


LIST_OF_STRINGS *
STEPWrapper::getAttributes( SCLP23(Application_instance) *sse) {
	LIST_OF_STRINGS *l = new LIST_OF_STRINGS;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string name = attr->Name();

		l->push_back(name);
	}

	return l;
}


Boolean
STEPWrapper::getBooleanAttribute(SCLP23(Application_instance) *sse, const char *name)
{
    Boolean retValue = BUnset;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
	    retValue = (Boolean)(*attr->ptr.e).asInt();
			if (retValue > BUnset)
				retValue = BUnset;
			break;
		}
	}
	return retValue;
}


int
STEPWrapper::getEnumAttribute(SCLP23(Application_instance) *sse, const char *name)
{
	int retValue = 0;
	std::string attrval;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = (*attr->ptr.e).asInt();
	    //std::cout << "debug enum: " << (*attr->ptr.e).asStr(attrval) << std::endl;
			break;
		}
	}
	return retValue;
}


SCLP23(Application_instance) *
STEPWrapper::getEntityAttribute(SCLP23(Application_instance) *sse, const char *name)
{
	SCLP23(Application_instance) *retValue = NULL;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attr->Nullable() && attr->is_null() && !attr->IsDerived()) {
			continue;
		}
		if (attrname.compare(name) == 0) {
			std::string attrval;
	    //std::cout << "attr:" << name << ":" << attr->TypeName() << ":" << attr->Name() << std::endl;
	    //std::cout << "attr:" << attr->asStr(attrval) << std::endl;
			retValue = (SCLP23(Application_instance) *)*attr->ptr.c;
			break;
		}
	}
	return retValue;
}


Logical
STEPWrapper::getLogicalAttribute(SCLP23(Application_instance) *sse, const char *name)
{
	Logical retValue = LUnknown;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = (Logical)(*attr->ptr.e).asInt();
			if (retValue > LUnknown)
				retValue = LUnknown;
			break;
		}
	}
	return retValue;
}


int
STEPWrapper::getIntegerAttribute(SCLP23(Application_instance) *sse, const char *name)
{
	int retValue = 0;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = *attr->ptr.i;
			break;
		}
	}
	return retValue;
}


double
STEPWrapper::getRealAttribute(SCLP23(Application_instance) *sse, const char *name)
{
	double retValue = 0.0;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = *attr->ptr.r;
			break;
		}
	}
	return retValue;
}


SCLP23(Select) *
STEPWrapper::getSelectAttribute(SCLP23(Application_instance) *sse, const char *name)
{
	SCLP23(Select) *retValue = NULL;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			retValue = (SCLP23(Select) *)attr->ptr.sh;
			break;
		}
	}
	return retValue;
}


LIST_OF_ENTITIES*
STEPWrapper::getListOfEntities(SCLP23(Application_instance) *sse, const char *name)
{
	LIST_OF_ENTITIES *l = new LIST_OF_ENTITIES;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
		std::string attrval;
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			STEPaggregate *sa = (STEPaggregate *)attr->ptr.a;

			EntityNode *sn = (EntityNode *)sa->GetHead();
			SCLP23(Application_instance) *se;
			while ( sn != NULL) {
				se = (SCLP23(Application_instance) *)sn->node;

				l->push_back(se);
				sn = (EntityNode *)sn->NextNode();
			}
			break;
		}
	}

	return l;
}


LIST_OF_SELECTS*
STEPWrapper::getListOfSelects(SCLP23(Application_instance) *sse, const char *name)
{
	LIST_OF_SELECTS *l = new LIST_OF_SELECTS;
	std::string attrval;

	sse->ResetAttributes();
	STEPattribute *attr;

	while ( (attr = sse->NextAttribute()) != NULL ) {
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {

			SelectAggregate *sa = (SelectAggregate *)attr->ptr.a;
			SelectNode *sn = (SelectNode*)sa->GetHead();
			while(sn) {
				l->push_back(sn->node);
				sn = (SelectNode*)sn->NextNode();
			}
			break;
		}
	}

	return l;
}


LIST_OF_LIST_OF_PATCHES*
STEPWrapper::getListOfListOfPatches(SCLP23(Application_instance) *sse, const char *attrName)
{
	LIST_OF_LIST_OF_PATCHES *l = new LIST_OF_LIST_OF_PATCHES;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
		std::string attrval;
	std::string name = attr->Name();

		if (name.compare(attrName) == 0) {
			ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


			GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

			STEPnode *sn = (STEPnode *)gp->GetHead();
			//EntityAggregate *ag = new EntityAggregate();


			const char *eaStr;

			LIST_OF_PATCHES *patches;
			while ( sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
				eaStr = sn->asStr(attrval);
				patches = parseListOfPatchEntities(eaStr);
				l->push_back(patches);
				sn = (STEPnode *)sn->NextNode();
			}
			break;
		}
	}

	return l;
}


LIST_OF_LIST_OF_POINTS*
STEPWrapper::getListOfListOfPoints(SCLP23(Application_instance) *sse, const char *attrName)
{
	LIST_OF_LIST_OF_POINTS *l = new LIST_OF_LIST_OF_POINTS;

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
		std::string attrval;
	std::string name = attr->Name();

		if (name.compare(attrName) == 0) {
			ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


			GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

			STEPnode *sn = (STEPnode *)gp->GetHead();
			//EntityAggregate *ag = new EntityAggregate();


			const char *eaStr;

			LIST_OF_POINTS *points;
			while ( sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
				eaStr = sn->asStr(attrval);
				points = parseListOfPointEntities(eaStr);
				l->push_back(points);
				sn = (STEPnode *)sn->NextNode();
			}
			break;
		}
	}

	return l;
}


MAP_OF_SUPERTYPES *
STEPWrapper::getMapOfSuperTypes(SCLP23(Application_instance) *sse)
{
	MAP_OF_SUPERTYPES *m = new MAP_OF_SUPERTYPES;

	if (sse->IsComplex()) {
		STEPcomplex *sc = ((STEPcomplex *)sse)->head;
		while(sc) {
			(*m)[sc->EntityName()] = sc;
			sc = sc->sc;
		}
	}

	return m;
}


void
STEPWrapper::getSuperTypes(SCLP23(Application_instance) *sse, MAP_OF_SUPERTYPES &m)
{
	if (sse->IsComplex()) {
		STEPcomplex *sc = ((STEPcomplex *)sse)->head;
		while(sc) {
			m[sc->EntityName()] = sc;
			sc = sc->sc;
		}
	}
}


SCLP23(Application_instance) *
STEPWrapper::getSuperType(SCLP23(Application_instance) *sse, const char *name)
{
	std::string attrval;

	if (sse->IsComplex()) {
		STEPcomplex *sc = ((STEPcomplex *)sse)->head;
		while(sc) {
	    std::string ename = sc->EntityName();

			if (ename.compare(name) == 0) {
				return sc;
			}
			sc = sc->sc;
		}
	}
	return NULL;
}


std::string
STEPWrapper::getStringAttribute(SCLP23(Application_instance) *sse, const char *name)
{
    std::string retValue = "";

	sse->ResetAttributes();

	STEPattribute *attr;
	while ( (attr = sse->NextAttribute()) != NULL ) {
		std::string attrval;
	std::string attrname = attr->Name();

		if (attrname.compare(name) == 0) {
			const char *str = attr->asStr(attrval);
	    if (str != NULL)
				retValue = str;
			break;
		}
	}
	return retValue;
}


bool
STEPWrapper::load(std::string &step_file)
{
	registry = new Registry(SchemaInit);
	sfile = new STEPfile(*registry, instance_list);

	stepfile = step_file;
	try {
		/* load STEP file */
		sfile->ReadExchangeFile(stepfile.c_str());

	} catch (std::exception& e) {
	std::cerr << e.what() << std::endl;
		return false;
	}
	return true;

}


LIST_OF_REALS*
STEPWrapper::parseListOfReals(const char *in)
{
	LIST_OF_REALS *l = new LIST_OF_REALS;
	ErrorDescriptor errdesc;
	RealAggregate *ra = new RealAggregate();

	//ra->StrToVal(in,&errdesc,SCLP23(Real),&instance_list,0);
	ra->StrToVal(in,&errdesc,config_control_designt_parameter_value,&instance_list,0);
	RealNode *rn = (RealNode *)ra->GetHead();
	while ( rn != NULL) {
		l->push_back(rn->value);
		rn = (RealNode *)rn->NextNode();
	}
	/*
	EntityNode *sn = (EntityNode *)ra->GetHead();

	SCLP23(Application_instance) *sse;
	while ( sn != NULL) {
		sse = (SCLP23(Application_instance) *)sn->node;
		CartesianPoint *aCP = new CartesianPoint(this, sse->STEPfile_id);
		if ( aCP->Load(this,sse)) {
			l->push_back(aCP);
		} else {
      std::cout << "Error loading Real list." << std::endl;
		}
		sn = (EntityNode *)sn->NextNode();
	}*/
	delete ra;

	return l;
}


LIST_OF_POINTS*
STEPWrapper::parseListOfPointEntities(const char *in)
{
	LIST_OF_POINTS *l = new LIST_OF_POINTS;
	ErrorDescriptor errdesc;
	EntityAggregate *ag = new EntityAggregate();

	ag->StrToVal(in,&errdesc,config_control_designe_cartesian_point,&instance_list,0);
	EntityNode *sn = (EntityNode *)ag->GetHead();

	SCLP23(Application_instance) *sse;
	while ( sn != NULL) {
		sse = (SCLP23(Application_instance) *)sn->node;
		CartesianPoint *aCP = dynamic_cast<CartesianPoint *>(CartesianPoint::Create(this, sse));
		if ( aCP != NULL) {
			l->push_back(aCP);
		} else {
	    std::cout << "Error loading CartesianPoint." << std::endl;
		}
		sn = (EntityNode *)sn->NextNode();
	}
	delete ag;

	return l;
}


LIST_OF_PATCHES*
STEPWrapper::parseListOfPatchEntities(const char *in)
{
	LIST_OF_PATCHES *l = new LIST_OF_PATCHES;
	ErrorDescriptor errdesc;
	EntityAggregate *ag = new EntityAggregate();

	ag->StrToVal(in,&errdesc,config_control_designe_cartesian_point,&instance_list,0);
	EntityNode *sn = (EntityNode *)ag->GetHead();

	SCLP23(Application_instance) *sse;
	while ( sn != NULL) {
		sse = (SCLP23(Application_instance) *)sn->node;
		SurfacePatch *aCP = dynamic_cast<SurfacePatch *>(SurfacePatch::Create(this, sse));
		if ( aCP != NULL) {
			l->push_back(aCP);
		} else {
	    std::cout << "Error loading SurfacePatch." << std::endl;
		}
		sn = (EntityNode *)sn->NextNode();
	}
	delete ag;

	return l;
}


void
STEPWrapper::printEntity(SCLP23(Application_instance) *se, int level)
{
	for ( int i=0; i< level; i++)
	std::cout << "    ";
    std::cout << "Entity:" << se->EntityName() << "(" << se->STEPfile_id << ")" << std::endl;
	for ( int i=0; i< level; i++)
	std::cout << "    ";
    std::cout << "Description:" << se->eDesc->Description() << std::endl;
	for ( int i=0; i< level; i++)
	std::cout << "    ";
    std::cout << "Entity Type:" << se->eDesc->Type() << std::endl;
	for ( int i=0; i< level; i++)
	std::cout << "    ";
    std::cout << "Attributes:" << std::endl;

	STEPattribute *attr;
	se->ResetAttributes();
	while ( (attr = se->NextAttribute()) != NULL ) {
		std::string attrval;

		for ( int i=0; i<= level; i++)
	    std::cout << "    ";
	std::cout << attr->Name() << ": " << attr->asStr(attrval) << " TypeName: " << attr->TypeName() << " Type: " << attr->Type() << std::endl;
		if ( attr->Type() == 256 ) {
			if (attr->IsDerived()) {
				for ( int i=0; i<= level; i++)
		    std::cout << "    ";
		std::cout << "        ********* DERIVED *********" << std::endl;
			} else {
			    printEntity(*(attr->ptr.c),level+2);
			}
		} else if ((attr->Type() == SET_TYPE)||(attr->Type() == LIST_TYPE)) {
			STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);

	    // std::cout << "aggr:" << sa->asStr(attrval) << "  BaseType:" << attr->BaseType() << std::endl;

			if ( attr->BaseType() == ENTITY_TYPE ) {
				printEntityAggregate(sa, level+2);
			}
		}

	}
    //std::cout << std::endl << std::endl;
}


void
STEPWrapper::printEntityAggregate(STEPaggregate *sa, int level)
{
	std::string strVal;

	for ( int i=0; i< level; i++)
	std::cout << "    ";
    std::cout << "Aggregate:" << sa->asStr(strVal) << std::endl;

	EntityNode *sn = (EntityNode *)sa->GetHead();
	SCLP23(Application_instance) *sse;
	while ( sn != NULL) {
		sse = (SCLP23(Application_instance) *)sn->node;

		if (((sse->eDesc->Type() == SET_TYPE)||(sse->eDesc->Type() == LIST_TYPE))&&(sse->eDesc->BaseType() == ENTITY_TYPE)) {
			printEntityAggregate((STEPaggregate *)sse,level+2);
		} else if ( sse->eDesc->Type() == ENTITY_TYPE ) {
			printEntity(sse,level+2);
		} else {
	    std::cout << "Instance Type not handled:" << std::endl;
		}
	//std::cout << "sn - " << sn->asStr(attrval) << std::endl;

		sn = (EntityNode *)sn->NextNode();
	}
    //std::cout << std::endl << std::endl;
}


void
STEPWrapper::printLoadStatistics()
{

	int num_ents = instance_list.InstanceCount();

    //std::cout << "Loaded " << num_ents << "instances from STEP file \"" << stepfile << "\"" << std::endl;
	//for( int i=0; i < num_ents; i++){
	//	SCLP23(Application_instance) *se = instance_list.GetSTEPentity(i);
//
    //	std::cout << i << ":" << std::endl;
	//	printEntity(se,1);
    //	std::cout << std::endl;
	//}

    int num_schma_ents = registry->GetEntityCnt();

    // "Reset" the Schema and Entity hash tables... this sets things up
    // so we can walk through the table using registry->NextEntity()

    registry->ResetSchemas();
    registry->ResetEntities();

    // Print out what schema we're running through.

    const SchemaDescriptor *schema = registry->NextSchema();

    // "Loop" through the schema, building one of each entity type.

    const EntityDescriptor *ent;   // needs to be declared const...
    std::string filler = "....................................................................................";
    std::cout << "Loaded " << num_ents << " instances from STEP file \"" << stepfile << "\"" << std::endl;

	int numEntitiesUsed=0;
    for (int i=0; i < num_schma_ents; i++) {
		ent = registry->NextEntity();

		int entCount = instance_list.EntityKeywordCount(ent->Name());
		// fix below with boost string formater when available
		if ( entCount > 0 ) {
	    std::cout << "\t" << ent->Name() << filler.substr(0, filler.length() - ((std::string)ent->Name()).length()) << entCount << std::endl;
			numEntitiesUsed++;
		}
    }
    std::cout << "Used " << numEntitiesUsed << " entities of the available " << num_schma_ents << " in schema \"" << schema->Name() << std::endl;
}


const char *
STEPWrapper::getBaseType(int type)
{
	const char *retValue = NULL;

    switch(type) {
        case sdaiINSTANCE:
            retValue = "sdaiINSTANCE";
            break;
        case sdaiSELECT: // The name of a select is never written DAS 1/31/97
            retValue = "sdaiSELECT";
            break;
        case sdaiNUMBER:
            retValue = "sdaiNUMBER";
            break;
        case sdaiREAL:
            retValue = "sdaiREAL";
            break;
        case sdaiINTEGER:
            retValue = "sdaiINTEGER";
            break;
        case sdaiSTRING:
            retValue = "sdaiSTRING";
            break;
        case sdaiBOOLEAN:
            retValue = "sdaiBOOLEAN";
            break;
        case sdaiLOGICAL:
            retValue = "sdaiLOGICAL";
            break;
        case sdaiBINARY:
            retValue = "sdaiBINARY";
            break;
        case sdaiENUMERATION:
            retValue = "sdaiENUMERATION";
            break;
        case sdaiAGGR:
            retValue = "sdaiAGGR";
            break;
        case ARRAY_TYPE:
            retValue = "ARRAY_TYPE";
            break;
        case BAG_TYPE:
            retValue = "BAG_TYPE";
            break;
        case SET_TYPE:
            retValue = "SET_TYPE";
            break;
        case LIST_TYPE:
            retValue = "LIST_TYPE";
            break;
        case REFERENCE_TYPE: // this should never happen? DAS
            retValue = "REFERENCE_TYPE";
            break;
        default:
            retValue = "Unknown";
            break;
      }
    return retValue;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
