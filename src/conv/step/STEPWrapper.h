/*                 STEPWrapper.h
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
/** @file STEPWrapper.h
 *
 * Class definition for C++ wrapper to NIST STEP parser/database functions.
 *
 */
#ifndef STEPWRAPPER_H_
#define STEPWRAPPER_H_

#define TAB(j) \
	{ \
		for ( int i=0; i< j; i++) \
			cout << "    "; \
	}

//TODO
//#define TRACE(s) std::cerr << __FILE__ << ":" << __LINE__ << ":" << __func__ << ":" << s << std::endl;
#define ERROR(s) std::cerr << __FILE__ << ":" << __LINE__ << ":" << __func__ << ":" << s << std::endl;

#include <list>
#include <map>
#include <vector>

using namespace std;

#include <sdai.h>

#include <STEPattribute.h>
#include <STEPcomplex.h>
#include <STEPfile.h>
#include <SdaiCONFIG_CONTROL_DESIGN.h>

#include <BRLCADWrapper.h>

/*
class SCLP23(Application_instance);
class SDAI_Select;
class STEPcomplex;
*/

class CartesianPoint;
class SurfacePatch;
typedef list<CartesianPoint *> LIST_OF_POINTS;
typedef list<LIST_OF_POINTS *> LIST_OF_LIST_OF_POINTS;
typedef list<SurfacePatch *> LIST_OF_PATCHES;
typedef list<LIST_OF_PATCHES *> LIST_OF_LIST_OF_PATCHES;
typedef list<string> LIST_OF_STRINGS;
typedef list<SCLP23(Application_instance) *> LIST_OF_ENTITIES;
typedef list<SDAI_Select *> LIST_OF_SELECTS;
typedef map<string,STEPcomplex *> MAP_OF_SUPERTYPES;
typedef vector<double> VECTOR_OF_REALS;
typedef list<int> LIST_OF_INTEGERS;
typedef list<double> LIST_OF_REALS;
typedef list<LIST_OF_REALS *> LIST_OF_LIST_OF_REALS;

class STEPWrapper {
private:
	string stepfile;
	string dotgfile;
	InstMgr instance_list;
	Registry  *registry;
	STEPfile  *sfile;
	BRLCADWrapper *dotg;
	void printEntity(SCLP23(Application_instance) *se, int level);
	void printEntityAggregate(STEPaggregate *sa, int level);
	const char *getBaseType(int type);

public:
	STEPWrapper();
	virtual ~STEPWrapper();

	bool convert(BRLCADWrapper *dotg);

	SCLP23(Application_instance) *getEntity( int STEPid );
	SCLP23(Application_instance) *getEntity( int STEPid, const char *name );
	SCLP23(Application_instance) *getEntity( SCLP23(Application_instance) *, const char *name );
	string getLogicalString( SCLLOG_H(Logical) v );
	string getBooleanString( SCLBOOL_H(Bool) v );

	// helper functions based on STEP id
	STEPattribute *getAttribute( int STEPid, const char *name );
	LIST_OF_STRINGS *getAttributes( int STEPid );
	SCLBOOL_H(Bool) getBooleanAttribute( int STEPid, const char *name );
	SCLP23(Application_instance) *getEntityAttribute( int STEPid, const char *name );
	int getEnumAttribute( int STEPid, const char *name );
	int getIntegerAttribute( int STEPid, const char *name );
	SCLLOG_H(Logical) getLogicalAttribute( int STEPid, const char *name );
	double getRealAttribute( int STEPid, const char *name );
	LIST_OF_ENTITIES *getListOfEntities( int STEPid, const char *name );
	LIST_OF_LIST_OF_POINTS *getListOfListOfPoints( int STEPid, const char *attrName);
	MAP_OF_SUPERTYPES *getMapOfSuperTypes(int STEPid);
	void getSuperTypes(int STEPid, MAP_OF_SUPERTYPES &m);
	SCLP23(Application_instance) *getSuperType(int STEPid, const char *name);
	string getStringAttribute( int STEPid, const char *name );

	//helper functions based on entity instance pointer
	STEPattribute *getAttribute( SCLP23(Application_instance) *sse, const char *name );
	LIST_OF_STRINGS *getAttributes( SCLP23(Application_instance) *sse );
	SCLBOOL_H(Bool) getBooleanAttribute( SCLP23(Application_instance) *sse, const char *name );
	SCLP23(Application_instance) *getEntityAttribute( SCLP23(Application_instance) *sse, const char *name );
	SCLP23(Select) *getSelectAttribute( SCLP23(Application_instance) *sse, const char *name );
	int getEnumAttribute( SCLP23(Application_instance) *sse, const char *name );
	int getIntegerAttribute( SCLP23(Application_instance) *sse, const char *name );
	SCLLOG_H(Logical) getLogicalAttribute( SCLP23(Application_instance) *sse, const char *name );
	double getRealAttribute( SCLP23(Application_instance) *sse, const char *name );
	LIST_OF_ENTITIES *getListOfEntities( SCLP23(Application_instance) *sse, const char *name );
	LIST_OF_SELECTS *getListOfSelects( SCLP23(Application_instance) *sse, const char *name );
	LIST_OF_LIST_OF_PATCHES *getListOfListOfPatches( SCLP23(Application_instance) *sse, const char *attrName);
	LIST_OF_LIST_OF_POINTS *getListOfListOfPoints( SCLP23(Application_instance) *sse, const char *attrName);
	MAP_OF_SUPERTYPES *getMapOfSuperTypes(SCLP23(Application_instance) *sse);
	void getSuperTypes(SCLP23(Application_instance) *sse, MAP_OF_SUPERTYPES &m);
	SCLP23(Application_instance) *getSuperType(SCLP23(Application_instance) *sse, const char *name);
	string getStringAttribute( SCLP23(Application_instance) *sse, const char *name );

	bool load(string &stepfile);
	LIST_OF_PATCHES *parseListOfPatchEntities( const char *in);
	LIST_OF_REALS *parseListOfReals( const char *in);
	LIST_OF_POINTS *parseListOfPointEntities( const char *in);
	void printLoadStatistics();
};

#endif /* STEPWRAPPER_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
