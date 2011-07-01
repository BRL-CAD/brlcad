/*                 VertexPoint.h
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
/** @file step/VertexPoint.h
 *
 * Class definition used to convert STEP "VertexPoint" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef VERTEXPOINT_H_
#define VERTEXPOINT_H_

#include "Point.h"
#include "Vertex.h"
#include "GeometricRepresentationItem.h"

class Point;
class ON_Brep;

class VertexPoint : public Vertex , public GeometricRepresentationItem {
public:
	typedef map<int,VertexPoint *> OBJECTS;

private:
	static string entityname;
	static OBJECTS verticies;

protected:
	Point *vertex_geometry;

public:
	VertexPoint();
	virtual ~VertexPoint();
	VertexPoint(STEPWrapper *sw,int step_id);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual const double *Point3d() { return vertex_geometry->Point3d(); };
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual void AddVertex(ON_Brep *brep);
};

#endif /* VERTEXPOINT_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
