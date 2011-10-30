/*                 Axis2Placement3D.cpp
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
/** @file step/Axis2Placement3D.cpp
 *
 * Routines to convert STEP "Axis2Placement3D" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "vmath.h"
#include "CartesianPoint.h"
#include "Direction.h"

#include "Axis2Placement3D.h"

#define CLASSNAME "Axis2Placement3D"
#define ENTITYNAME "Axis2_Placement_3d"
string Axis2Placement3D::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Axis2Placement3D::Create);

Axis2Placement3D::Axis2Placement3D() {
	step = NULL;
	id = 0;
	axis = NULL;
	ref_direction = NULL;
}

Axis2Placement3D::Axis2Placement3D(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
	axis = NULL;
	ref_direction = NULL;
}

Axis2Placement3D::~Axis2Placement3D() {
}

/*
 * Replica of STEP function:
 * FUNCTION build_axes() :
					 LIST [3:3] OF direction;
  LOCAL
    d1, d2 : direction;
  END_LOCAL;
 d1 := NVL(normalise(axis), dummy_gri || direction([0.0,0.0,1.0]));
 d2 := first_proj_axis(d1, ref_direction);
 RETURN [d2, normalise(cross_product(d1,d2)).orientation, d1];

END_FUNCTION;
/////////
 */
void
Axis2Placement3D::BuildAxis() {
	double d1[3];
	double d2[3];
	double d1Xd2[3];

	if (axis == NULL) {
		VSET(d1,0.0,0.0,1.0);
	} else {
		VMOVE(d1,axis->DirectionRatios());
		VUNITIZE(d1);
	}
	if (ref_direction == NULL) {
		FirstProjAxis(d2,d1,NULL);
	} else {
		FirstProjAxis(d2,d1,ref_direction->DirectionRatios());
	}
	VCROSS(d1Xd2,d1,d2);
	VUNITIZE(d1Xd2);
	VMOVE(p[0],d2);
	VMOVE(p[1],d1Xd2);
	VMOVE(p[2],d1);

	return;
}
/*
 * Replica of STEP function:
 *   FUNCTION first_proj_axis()
 */
void
Axis2Placement3D::FirstProjAxis(double *proj,double *zaxis, double *refdir) {
    double z[3];
    double v[3];
    double TOL = 1e-9;

    if (zaxis == NULL)
	return;

    VMOVE(z,zaxis);
    VUNITIZE(z);
    if (refdir == NULL) {
	double xplus[3]=  {1.0,0.0,0.0};
	double xminus[3]=  {-1.0,0.0,0.0};
	if (!VNEAR_EQUAL(z, xplus, TOL) &&
			!VNEAR_EQUAL(z, xminus, TOL))  {
		VSET(v,1.0,0.0,0.0);
	} else {
		VSET(v,0.0,1.0,0.0);
	}
    } else {
	double cross[3];
	double mag;

	VCROSS(cross, refdir, z);
	mag = MAGNITUDE(cross);
	if (NEAR_ZERO(mag,TOL)) {
		return;
	} else {
		VMOVE(v,refdir);
		VUNITIZE(v);
	}

    }
    double x_vec[3];
    double aproj[3];
    double dot = VDOT(v,z);
    ScalarTimesVector(x_vec, dot, z);
    VectorDifference(aproj,v,x_vec);
    VSCALE(x_vec,z,dot);
    VSUB2(aproj,v, x_vec);
    VUNITIZE(aproj);
    VMOVE(proj,aproj);

    return;
}
/*
 * Replica of STEP function:
 *  FUNCTION scalar_times_vector ()
 */
void
Axis2Placement3D::ScalarTimesVector(double *v, double scalar, double *vec) {
	VMOVE(v,vec);
	VUNITIZE(v);
	VSCALE(v,v,scalar);
}

/*
 * Replica of STEP function:
 *   FUNCTION vector_difference()
 */
void
Axis2Placement3D::VectorDifference(double *result, double *v1, double *v2) {
	double vec1[3];
	double vec2[3];

	VMOVE(vec1,v1);
	VMOVE(vec2,v2);
    VUNITIZE(vec1);
    VUNITIZE(vec2);

    VADD2(result,vec1,vec2);
}

const double *
Axis2Placement3D::GetAxis(int i) {
	return p[i];
}

const double *
Axis2Placement3D::GetOrigin() {
	return location->Coordinates();
}

const double *
Axis2Placement3D::GetNormal() {
	return p[2];
}

const double *
Axis2Placement3D::GetXAxis() {
	return p[0];
}

const double *
Axis2Placement3D::GetYAxis() {
	return p[1];
}

bool
Axis2Placement3D::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Placement::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Placement." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (axis == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"axis");
		if (entity) {
			axis = dynamic_cast<Direction *>(Factory::CreateObject(sw,entity));
		} else { // optional so no problem if not here
			axis = NULL;
		}
	}

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
Axis2Placement3D::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	if (axis) {
		TAB(level+1); std::cout << "axis:" << std::endl;
		axis->Print(level+1);
	}
	if (ref_direction) {
		TAB(level+1); std::cout << "ref_direction:" << std::endl;
		ref_direction->Print(level+1);
	}

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	Placement::Print(level+1);
}

STEPEntity *
Axis2Placement3D::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		Axis2Placement3D *object = new Axis2Placement3D(sw,sse->STEPfile_id);

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
Axis2Placement3D::LoadONBrep(ON_Brep *UNUSED(brep))
{
	//TODO: check other axis2placement3d usage notice being loaded from advanced brep in some instances
	//std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
	return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
