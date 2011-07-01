/*                 Factory.h
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
/** @file step/Factory.h
 *
 * Class definition for STEP object "Factory".
 *
 */

#ifndef FACTORY_H_
#define FACTORY_H_

#include "common.h"

/* system interface headers */
#include <map>
#include <list>
#include <vector>
#include <string>

/* inteface headers */
#include "STEPWrapper.h"
#include "sclprefixes.h"


class STEPEntity;
typedef STEPEntity* (*FactoryMethod)(STEPWrapper*,SCLP23(Application_instance)*);
typedef std::map<std::string,FactoryMethod> FACTORYMAP;
typedef std::vector<STEPEntity *> VECTOR_OF_OBJECTS;


class Factory {
public:
    typedef std::map<int, STEPEntity *> OBJECTS;
    typedef std::map<int, int> ID_TO_INDEX_MAP;
    typedef std::map<int, int> INDEX_TO_ID_MAP;
    typedef std::list<STEPEntity *> UNMAPPED_OBJECTS;
	static OBJECTS objects;
	static UNMAPPED_OBJECTS unmapped_objects;

	static int vertex_count;
	static VECTOR_OF_OBJECTS vertices;
	static ID_TO_INDEX_MAP vertex_to_index;
	static INDEX_TO_ID_MAP vertex_index_to_id;

protected:
	Factory();

private:
	static STEPEntity *CreateCurveObject(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	static STEPEntity *CreateSurfaceObject(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	static STEPEntity *CreateNamedUnitObject(STEPWrapper *sw,SCLP23(Application_instance) *sse);

public:
	static const char *factoryname;
	virtual ~Factory();
	static STEPEntity *CreateObject(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	static FACTORYMAP &GetMap();
	static void Print();
    static std::string RegisterClass(std::string name, FactoryMethod f);
	static void DeleteObjects();
	static OBJECTS::iterator FindObject(int id);
	static void AddObject(STEPEntity *se);
	static void AddVertex(STEPEntity *se);
	static VECTOR_OF_OBJECTS *GetVertices();
	static int GetVertexIndex(int id);
	static STEPEntity *GetVertexByIndex(int index);
};


#endif /* FACTORY_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
