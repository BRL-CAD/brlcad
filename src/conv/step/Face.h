/*                 Face.h
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
/** @file step/Face.h
 *
 * Class definition used to convert STEP "Face" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef FACE_H_
#define FACE_H_

#include <list>

#include "TopologicalRepresentationItem.h"

// forward declaration of class
class ON_BoundingBox;
class FaceBound;
typedef list<FaceBound *> LIST_OF_FACE_BOUNDS;

class Face: public TopologicalRepresentationItem {
private:
	static string entityname;

protected:
	LIST_OF_FACE_BOUNDS bounds;

public:
	Face();
	virtual ~Face();
	Face(STEPWrapper *sw,int step_id);
	ON_BoundingBox *GetEdgeBounds(ON_Brep *brep);
	bool Load(STEPWrapper *sw, SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* FACE_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
