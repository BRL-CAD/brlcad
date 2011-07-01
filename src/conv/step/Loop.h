/*                 Loop.h
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
/** @file step/Loop.h
 *
 * Class definition used to convert STEP "Loop" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef LOOP_H_
#define LOOP_H_

#include "TopologicalRepresentationItem.h"

class ON_BoundingBox;
class ON_Brep;

class Loop : public TopologicalRepresentationItem {
private:
	static string entityname;

protected:
	int ON_loop_index;

public:
	Loop();
	virtual ~Loop();
	Loop(STEPWrapper *sw,int step_id);
	virtual ON_BoundingBox *GetEdgeBounds(ON_Brep *brep);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);
	void SetLoopIndex(int index) { ON_loop_index = index; };

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* LOOP_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
