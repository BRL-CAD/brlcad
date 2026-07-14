/*                 OffsetSurface.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file step/OffsetSurface.cpp
 *
 * Routines to interface to STEP "OffsetSurface".
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Surface.h"
#include "OffsetSurface.h"

#define CLASSNAME "OffsetSurface"
#define ENTITYNAME "Offset_Surface"
string OffsetSurface::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)OffsetSurface::Create);

OffsetSurface::OffsetSurface()
{
    step = NULL;
    id = 0;
    basis_surface = NULL;
    distance = 0.0;
    self_intersect = LUnset;
}

OffsetSurface::OffsetSurface(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    basis_surface = NULL;
    distance = 0.0;
    self_intersect = LUnset;
}

OffsetSurface::~OffsetSurface()
{
}

bool
OffsetSurface::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{

    step = sw;
    id = sse->STEPfile_id;

    if (!Surface::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedSurface." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (basis_surface == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "basis_surface");
	if (entity) {
	    basis_surface = dynamic_cast<Surface *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cerr << CLASSNAME << ": error loading 'basis_surface' attribute." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    distance = step->getRealAttribute(sse, "distance");
    self_intersect = step->getLogicalAttribute(sse, "self_intersect");

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
OffsetSurface::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    basis_surface->Print(level + 1);

    TAB(level + 1);
    std::cout << "distance:" << distance << std::endl;
    TAB(level + 1);
    std::cout << "self_intersect:" << step->getLogicalString((Logical)self_intersect) << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Surface::Print(level + 1);
}

STEPEntity *
OffsetSurface::GetInstance(STEPWrapper *sw, int id)
{
    return new OffsetSurface(sw, id);
}

STEPEntity *
OffsetSurface::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
OffsetSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    // already loaded
    }

    if (basis_surface == NULL) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - no basis_surface." << std::endl;
	return false;
    }

    // propagate trimming-curve bounds so infinite basis surfaces
    // (plane/cylinder/cone) get finite extents, as FaceSurface does
    if (trim_curve_3d_bbox) {
	basis_surface->SetCurveBounds(trim_curve_3d_bbox);
    }

    // load the underlying surface into the brep
    if (!basis_surface->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading basis_surface." << std::endl;
	return false;
    }

    ON_Surface *base = brep->m_S[basis_surface->GetONId()];
    if (!base) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - basis_surface not in brep." << std::endl;
	return false;
    }

    // convert to NURBS so we can offset control points along the
    // surface normal (coords already in local units from the basis load)
    ON_NurbsSurface *surf = ON_NurbsSurface::New();
    if (!base->GetNurbForm(*surf)) {
	delete surf;
	std::cerr << "Error: " << entityname << "::LoadONBrep() - could not get NURBS form of basis_surface." << std::endl;
	return false;
    }

    double offset = distance * LocalUnits::length;

    int u_count = surf->CVCount(0);
    int v_count = surf->CVCount(1);
    for (int i = 0; i < u_count; i++) {
	for (int j = 0; j < v_count; j++) {
	    // parameter at greville abcissa for this control point
	    double u = surf->GrevilleAbcissa(0, i);
	    double v = surf->GrevilleAbcissa(1, j);

	    ON_3dPoint pnt;
	    ON_3dVector du, dv;
	    if (!surf->Ev1Der(u, v, pnt, du, dv)) {
		continue;
	    }
	    ON_3dVector normal = ON_CrossProduct(du, dv);
	    if (!normal.Unitize()) {
		continue;
	    }

	    ON_4dPoint cv;
	    surf->GetCV(i, j, cv);
	    double w = (cv.w != 0.0) ? cv.w : 1.0;
	    ON_3dPoint p(cv.x / w, cv.y / w, cv.z / w);
	    p = p + offset * normal;
	    surf->SetCV(i, j, ON_4dPoint(p.x * w, p.y * w, p.z * w, w));
	}
    }

    surf->BoundingBox(); // update cached bbox

    ON_id = brep->AddSurface(surf);

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
