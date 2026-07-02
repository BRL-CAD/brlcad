/*                 FacetedBrep.cpp
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
/** @file step/FacetedBrep.cpp
 *
 * Routines to convert STEP "FacetedBrep" to a BRL-CAD BoT (mesh).
 *
 */

#include <map>

#include "STEPWrapper.h"
#include "Factory.h"

#include "FacetedBrep.h"
#include "ClosedShell.h"
#include "Face.h"
#include "FaceBound.h"
#include "PolyLoop.h"
#include "CartesianPoint.h"
#include "LocalUnits.h"

#define CLASSNAME "FacetedBrep"
#define ENTITYNAME "Faceted_Brep"
string FacetedBrep::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)FacetedBrep::Create);

FacetedBrep::FacetedBrep()
{
    step = NULL;
    id = 0;
    outer = NULL;
}

FacetedBrep::FacetedBrep(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    outer = NULL;
}

FacetedBrep::~FacetedBrep()
{
    outer = NULL;
}

bool
FacetedBrep::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // faceted_brep adds no attributes of its own; the outer closed_shell is
    // parsed by the manifold_solid_brep base class.
    if (!ManifoldSolidBrep::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::ManifoldSolidBrep." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void
FacetedBrep::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ManifoldSolidBrep::Print(level + 1);
}

bool
FacetedBrep::GetBoT(std::vector<fastf_t> &vertices, std::vector<int> &faces)
{
    if (!outer) {
	return false;
    }

    // Map each unique cartesian_point (by STEP id) to a BoT vertex index so
    // that vertices shared between adjacent faces are welded together.
    std::map<int, int> id2idx;

    const LIST_OF_FACES &fl = outer->Faces();
    for (LIST_OF_FACES::const_iterator fi = fl.begin(); fi != fl.end(); ++fi) {
	Face *f = *fi;
	if (!f) {
	    continue;
	}
	const LIST_OF_FACE_BOUNDS &bl = f->Bounds();
	for (LIST_OF_FACE_BOUNDS::const_iterator bi = bl.begin(); bi != bl.end(); ++bi) {
	    FaceBound *fb = *bi;
	    if (!fb) {
		continue;
	    }
	    PolyLoop *pl = dynamic_cast<PolyLoop *>(fb->GetBound());
	    if (!pl) {
		// Non-polygonal bound - not representable in the faceted mesh.
		continue;
	    }
	    const LIST_OF_POINTS &poly = pl->GetPolygon();

	    std::vector<int> idx;
	    for (LIST_OF_POINTS::const_iterator pi = poly.begin(); pi != poly.end(); ++pi) {
		CartesianPoint *cp = *pi;
		if (!cp) {
		    continue;
		}
		int cid = cp->STEPid();
		int vi;
		std::map<int, int>::iterator it = id2idx.find(cid);
		if (it == id2idx.end()) {
		    vi = (int)(vertices.size() / 3);
		    vertices.push_back(cp->X() * LocalUnits::length);
		    vertices.push_back(cp->Y() * LocalUnits::length);
		    vertices.push_back(cp->Z() * LocalUnits::length);
		    id2idx[cid] = vi;
		} else {
		    vi = it->second;
		}
		idx.push_back(vi);
	    }

	    // Fan-triangulate the polygon.  faceted_brep faces are planar; this
	    // is exact for convex faces (the overwhelmingly common case, since
	    // most exporters already emit triangles/convex quads).  Degenerate
	    // (repeated-index) triangles are skipped.
	    for (size_t k = 1; k + 1 < idx.size(); ++k) {
		if (idx[0] == idx[k] || idx[k] == idx[k + 1] || idx[0] == idx[k + 1]) {
		    continue;
		}
		faces.push_back(idx[0]);
		faces.push_back(idx[k]);
		faces.push_back(idx[k + 1]);
	    }
	}
    }

    return !faces.empty();
}

STEPEntity *
FacetedBrep::GetInstance(STEPWrapper *sw, int id)
{
    return new FacetedBrep(sw, id);
}

STEPEntity *
FacetedBrep::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
