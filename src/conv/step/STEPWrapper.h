/*                   S T E P W R A P P E R . H
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @file step/STEPWrapper.h
 *
 * Class definition for C++ wrapper to NIST STEP parser/database
 * functions.
 *
 */
#ifndef STEPWRAPPER_H_
#define STEPWRAPPER_H_

#include "common.h"

#ifdef DEBUG
#define ERROR(arg) std::cerr << __FILE__ << ":" << __LINE__ << ":" << __func__ << ":" << arg << std::endl
#else
#define ERROR(arg)
#endif

/* system headers */
#include <list>
#include <map>
#include <vector>

/* interface headers */
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

typedef std::list<CartesianPoint *> LIST_OF_POINTS;
typedef std::list<LIST_OF_POINTS *> LIST_OF_LIST_OF_POINTS;
typedef std::list<SurfacePatch *> LIST_OF_PATCHES;
typedef std::list<LIST_OF_PATCHES *> LIST_OF_LIST_OF_PATCHES;
typedef std::list<std::string> LIST_OF_STRINGS;
typedef std::list<SCLP23(Application_instance) *> LIST_OF_ENTITIES;
typedef std::list<SDAI_Select *> LIST_OF_SELECTS;
typedef std::map<std::string,STEPcomplex *> MAP_OF_SUPERTYPES;
typedef std::vector<double> VECTOR_OF_REALS;
typedef std::list<int> LIST_OF_INTEGERS;
typedef std::list<double> LIST_OF_REALS;
typedef std::list<LIST_OF_REALS *> LIST_OF_LIST_OF_REALS;


class STEPWrapper {
private:
    std::string stepfile;
    std::string dotgfile;
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
    std::string getLogicalString( Logical v );
    std::string getBooleanString( Boolean v );

    // helper functions based on STEP id
    STEPattribute *getAttribute( int STEPid, const char *name );
    LIST_OF_STRINGS *getAttributes( int STEPid );
    Boolean getBooleanAttribute( int STEPid, const char *name );
    SCLP23(Application_instance) *getEntityAttribute( int STEPid, const char *name );
    int getEnumAttribute( int STEPid, const char *name );
    int getIntegerAttribute( int STEPid, const char *name );
    Logical getLogicalAttribute( int STEPid, const char *name );
    double getRealAttribute( int STEPid, const char *name );
    LIST_OF_ENTITIES *getListOfEntities( int STEPid, const char *name );
    LIST_OF_LIST_OF_POINTS *getListOfListOfPoints( int STEPid, const char *attrName);
    MAP_OF_SUPERTYPES *getMapOfSuperTypes(int STEPid);
    void getSuperTypes(int STEPid, MAP_OF_SUPERTYPES &m);
    SCLP23(Application_instance) *getSuperType(int STEPid, const char *name);
    std::string getStringAttribute( int STEPid, const char *name );

    // helper functions based on entity instance pointer
    STEPattribute *getAttribute( SCLP23(Application_instance) *sse, const char *name );
    LIST_OF_STRINGS *getAttributes( SCLP23(Application_instance) *sse );
    Boolean getBooleanAttribute( SCLP23(Application_instance) *sse, const char *name );
    SCLP23(Application_instance) *getEntityAttribute( SCLP23(Application_instance) *sse, const char *name );
    SCLP23(Select) *getSelectAttribute( SCLP23(Application_instance) *sse, const char *name );
    int getEnumAttribute( SCLP23(Application_instance) *sse, const char *name );
    int getIntegerAttribute( SCLP23(Application_instance) *sse, const char *name );
    Logical getLogicalAttribute( SCLP23(Application_instance) *sse, const char *name );
    double getRealAttribute( SCLP23(Application_instance) *sse, const char *name );
    LIST_OF_ENTITIES *getListOfEntities( SCLP23(Application_instance) *sse, const char *name );
    LIST_OF_SELECTS *getListOfSelects( SCLP23(Application_instance) *sse, const char *name );
    LIST_OF_LIST_OF_PATCHES *getListOfListOfPatches( SCLP23(Application_instance) *sse, const char *attrName);
    LIST_OF_LIST_OF_POINTS *getListOfListOfPoints( SCLP23(Application_instance) *sse, const char *attrName);
    MAP_OF_SUPERTYPES *getMapOfSuperTypes(SCLP23(Application_instance) *sse);
    void getSuperTypes(SCLP23(Application_instance) *sse, MAP_OF_SUPERTYPES &m);
    SCLP23(Application_instance) *getSuperType(SCLP23(Application_instance) *sse, const char *name);
    std::string getStringAttribute( SCLP23(Application_instance) *sse, const char *name );

    bool load(std::string &step_file);
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
