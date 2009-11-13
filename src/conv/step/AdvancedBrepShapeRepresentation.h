/*                 AdvancedBrepShapeRepresentation.h
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
/** @file AdvancedBrepShapeRepresentation.h
 *
 * Class definition used to convert STEP "AdvancedBrepShapeRepresentation" to BRL-CAD BREP
 * structures.
 *
 */
#ifndef ADVANCEDBREPSHAPEREPRESENTATION_H_
#define ADVANCEDBREPSHAPEREPRESENTATION_H_

#include <list>

#include "ShapeRepresentation.h"

class ON_Brep;

class AdvancedBrepShapeRepresentation : public ShapeRepresentation {
private:
	static string entityname;

protected:

public:
	AdvancedBrepShapeRepresentation();
	AdvancedBrepShapeRepresentation(STEPWrapper *sw, int STEPid);
	virtual ~AdvancedBrepShapeRepresentation();

	ON_Brep *GetONBrep();
	virtual bool LoadONBrep(ON_Brep *brep);

	bool Load(STEPWrapper *sw, SCLP23(Application_instance) *sse);
	string Name() {return name;};
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* ADVANCEDBREPSHAPEREPRESENTATION_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
