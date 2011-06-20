/*                 FaceBound.h
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
/** @file step/FaceBound.h
 *
 * Class definition used to convert STEP "FaceBound" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef FACEBOUND_H_
#define FACEBOUND_H_

#include "TopologicalRepresentationItem.h"

// forward declaration of class
class Loop;
class ON_BoundingBox;
class ON_Brep;

class FaceBound : public TopologicalRepresentationItem {
private:
	static string entityname;

protected:
	Loop *bound;
	int ON_face_index;
	bool inner;
#ifdef YAYA
	Boolean orientation;
#else
	int orientation;
#endif

public:
	FaceBound();
	virtual ~FaceBound();
	FaceBound(STEPWrapper *sw,int step_id);
	void SetInner() { inner = true; };
	void SetOuter() { inner = false; };
	bool IsInner() {return inner;}
	bool IsOuter() {return !inner;}
	ON_BoundingBox *GetEdgeBounds(ON_Brep *brep);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);
	bool Oriented();
	void SetFaceIndex(int index) { ON_face_index = index; };

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* FACEBOUND_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
