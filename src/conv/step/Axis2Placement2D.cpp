/*                 Axis2Placement2D.cpp
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
/** @file step/Axis2Placement2D.cpp
 *
 * Routines to convert STEP "Axis2Placement2D" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "vmath.h"
#include "CartesianPoint.h"
#include "Direction.h"

#include "Axis2Placement2D.h"

#define CLASSNAME "Axis2Placement2D"
#define ENTITYNAME "Axis2_Placement_2d"
string Axis2Placement2D::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Axis2Placement2D::Create);

Axis2Placement2D::Axis2Placement2D() {
	step = NULL;
	id = 0;
	ref_direction = NULL;
}

Axis2Placement2D::Axis2Placement2D(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
	ref_direction = NULL;
}

Axis2Placement2D::~Axis2Placement2D() {
}

/*
 * Replica of STEP function:
 *   FUNCTION build_2axes()
 */
void
Axis2Placement2D::BuildAxis() {
	double d[3];
	double ortho_comp[3] = {0.0, 0.0, 0.0};

	if (ref_direction == NULL) {
		VSET(d,1.0,0.0,0.0);
	} else {
		VMOVE(d,ref_direction->DirectionRatios());
		VUNITIZE(d);
	}
	OrthogonalComplement(ortho_comp,d);

	VMOVE(p[0],d);
	VMOVE(p[1],ortho_comp);
	VSETALL(p[2],0.0);

	return;
}

/*
 * Replica of STEP function:
 *   FUNCTION orthogonal_complement()
 */
void
Axis2Placement2D::OrthogonalComplement(double *ortho, double *vec) {
	ortho[0] = -vec[1];
	ortho[1] = vec[0];
}

const double *
Axis2Placement2D::GetAxis(int i) {
	return p[i];
}

const double *
Axis2Placement2D::GetOrigin() {
	return location->Coordinates();
}

const double *
Axis2Placement2D::GetNormal() {
	return p[0];
}

const double *
Axis2Placement2D::GetXAxis() {
	return p[0];
}

const double *
Axis2Placement2D::GetYAxis() {
	return p[1];
}

bool
Axis2Placement2D::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Placement::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Placement." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (ref_direction == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"ref_direction");
		if (entity) {
			ref_direction = dynamic_cast<Direction *>(Factory::CreateObject(sw,entity));
		} else { // optional so no problem if not here
			ref_direction = NULL;
		}
	}

	BuildAxis();

	return true;
}

void
Axis2Placement2D::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "ref_direction:" << std::endl;
	if (ref_direction)
		ref_direction->Print(level+1);

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	Placement::Print(level+1);

}

STEPEntity *
Axis2Placement2D::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		Axis2Placement2D *object = new Axis2Placement2D(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, sse)) {
			std::cerr << CLASSNAME << ":Error loading class in ::Create() method." << std::endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
	}
}

bool
Axis2Placement2D::LoadONBrep(ON_Brep *brep)
{
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
	return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
