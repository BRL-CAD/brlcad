/*                 Path.h
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
/** @file step/Path.h
 *
 * Class definition used to convert STEP "Path" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef PATH_H_
#define PATH_H_

#include <list>

#include "TopologicalRepresentationItem.h"

// forward declaration of class
class ON_BoundingBox;
class ON_Brep;
class OrientedEdge;
typedef list<OrientedEdge *> LIST_OF_ORIENTED_EDGES;

typedef struct trim_curve2d {
    double start[2];
    double end[2];
    int curve_index;
} TrimCurve2d;
typedef list<TrimCurve2d *> LIST_OF_TRIMS;
typedef list<LIST_OF_TRIMS *> LIST_OF_TRIM_PATHS;

class Path : public TopologicalRepresentationItem {
private:
	LIST_OF_TRIM_PATHS paths;
	static string entityname;
	bool isSeam(LIST_OF_ORIENTED_EDGES::iterator i);
	LIST_OF_ORIENTED_EDGES::iterator getNext(LIST_OF_ORIENTED_EDGES::iterator i);
	LIST_OF_ORIENTED_EDGES::iterator getPrev(LIST_OF_ORIENTED_EDGES::iterator i);

protected:
	LIST_OF_ORIENTED_EDGES edge_list;
	int ON_path_index;

public:
	Path();
	virtual ~Path();
	Path(STEPWrapper *sw,int step_id);
	virtual ON_BoundingBox *GetEdgeBounds(ON_Brep *brep);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	bool LoadONTrimmingCurves(ON_Brep *brep);
	virtual void Print(int level);
	void SetPathIndex(int index) { ON_path_index = index; };
	bool ShiftSurfaceSeam(ON_Brep *brep, double *t);
	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* PATH_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
