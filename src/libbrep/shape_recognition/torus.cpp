/*                       T O R U S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file torus.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

int
subbrep_is_torus(struct subbrep_object_data *data, fastf_t torus_tol)
{
    std::set<int>::iterator f_it;
    std::set<int> toridal_surfaces;
    // First, check surfaces.  If a surface is anything other than a sphere,
    // the verdict is no.
    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
        int surface_type = (int)GetSurfaceType(data->brep->m_F[f_ind].SurfaceOf(), NULL);
        switch (surface_type) {
            case SURFACE_TORUS:
                toridal_surfaces.insert(f_ind);
                break;
            default:
                return 0;
                break;
        }
    }

    // Second, check if all toridal surfaces share the same parameters.
    ON_Torus torus;
    ON_Surface *cs = data->brep->m_F[*toridal_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsTorus(&torus, torus_tol);
    ON_3dPoint n_pt = torus.Center() + torus.plane.Normal();
    delete cs;
    for (f_it = toridal_surfaces.begin(); f_it != toridal_surfaces.end(); f_it++) {
	ON_Torus f_torus;
	ON_Surface *fcs = data->brep->m_F[(*f_it)].SurfaceOf()->Duplicate();
	fcs->IsTorus(&f_torus, torus_tol);
	delete fcs;
	if (f_torus.Center().DistanceTo(torus.Center()) > torus_tol) return 0;
	ON_3dPoint fn_pt = f_torus.Center() + f_torus.plane.Normal();
	if (fn_pt.DistanceTo(n_pt) > torus_tol) return 0;
	if (!NEAR_ZERO(f_torus.major_radius - torus.major_radius, torus_tol)) return 0;
	if (!NEAR_ZERO(f_torus.minor_radius - torus.minor_radius, torus_tol)) return 0;
    }

    data->type = TORUS;

    return 1;
}


/* Return -1 if the torus normals are defining a negative volume,
 * 1 if it is positive, and 0 if there is some other problem */
int
negative_torus(struct subbrep_object_data *data, int face_index, double torus_tol) {
    const ON_Surface *surf = data->brep->m_F[face_index].SurfaceOf();
    ON_Torus torus;
    ON_Surface *cs = surf->Duplicate();
    cs->IsTorus(&torus, torus_tol);
    delete cs;
    return -1;
}


int
torus_csg(struct subbrep_object_data *data, fastf_t torus_tol)
{
    std::set<int> planar_surfaces;
    std::set<int> toridal_surfaces;
    std::set<int>::iterator f_it;
    //std::cout << "processing toridal surface: \n";
    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
        int surface_type = (int)GetSurfaceType(data->brep->m_F[f_ind].SurfaceOf(), NULL);
        switch (surface_type) {
            case SURFACE_PLANE:
		//std::cout << "planar face " << f_ind << "\n";
                planar_surfaces.insert(f_ind);
                break;
            case SURFACE_TORUS:
		//std::cout << "torus face " << f_ind << "\n";
                toridal_surfaces.insert(f_ind);
		break;
            default:
		bu_log("what???\n");
                return 0;
                break;
        }
    }
    data->params->bool_op = 'u'; // Initialize to union

    // Check for multiple tori.
    ON_Torus torus;
    ON_Surface *cs = data->brep->m_F[*toridal_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsTorus(&torus, torus_tol);
    ON_3dPoint n_pt = torus.Center() + torus.plane.Normal();
    delete cs;
    for (f_it = toridal_surfaces.begin(); f_it != toridal_surfaces.end(); f_it++) {
	ON_Torus f_torus;
	ON_Surface *fcs = data->brep->m_F[(*f_it)].SurfaceOf()->Duplicate();
	fcs->IsTorus(&f_torus, torus_tol);
	delete fcs;
	if (f_torus.Center().DistanceTo(torus.Center()) > torus_tol) return 0;
	ON_3dPoint fn_pt = f_torus.Center() + f_torus.plane.Normal();
	if (fn_pt.DistanceTo(n_pt) > torus_tol) return 0;
	if (!NEAR_ZERO(f_torus.major_radius - torus.major_radius, torus_tol)) return 0;
	if (!NEAR_ZERO(f_torus.minor_radius - torus.minor_radius, torus_tol)) return 0;
    }

    // Categorize edges by whether their planes are parallel to the major
    // circle plane, perpendicular to it, or neither.  If we have the latter
    // situation, right now it's too complicated to handle.
    std::set<int> major_circle_edges;
    std::set<int> minor_circle_edges;
    ON_3dVector torus_normal = torus.plane.Normal();
    for (int i = 0; i < data->edges_cnt; i++) {
	int ei = data->edges[i];
	const ON_BrepEdge *edge = &(data->brep->m_E[ei]);
	ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
	ON_Arc arc;
	if (ecv->IsArc(NULL, &arc, torus_tol)) {
	    int categorized = 0;
	    ON_Plane edge_plane(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
	    ON_3dVector ep_normal = torus.plane.Normal();
	    // TODO - define a sensible constant for this...
	    if (ep_normal.IsParallelTo(torus_normal, 0.01)) {
		//std::cout << "major edge: " << ei << "\n";
		major_circle_edges.insert(ei);
		categorized = 1;
	    }
	    // TODO - define a sensible constant for this...
	    if (ep_normal.IsPerpendicularTo(torus_normal, 0.01)) {
		//std::cout << "minor edge: " << ei << "\n";
		minor_circle_edges.insert(ei);
		categorized = 1;
	    }
	    if (!categorized) {
		//std::cout << "Edge " << ei << " is neither parallel nor perpendicular to torus - can't handle yet\n";
		return 0;
	    }

	} else {
	    // We've got non-arc edges in a toridal situation - not ready
	    // to handle this case yet.
	    //std::cout << "Edge " << ei << " is not an arc - can't handle yet\n";
	    delete ecv;
	    return 0;
	}
    }

    // TODO - we need to determine if the portion of the torus we are working with is using the
    // inner or outer portion of the torus.  If the surface is facing in towards the center point,
    // it will be necessary to subtract a tgc sliver from the parent arbn (if there is a parent)
    // in order to clear the way for the insertion of the toridal volume.  Conversely, if the
    // surface is facing away from the center, we may need to add a tgc sliver to make up the
    // difference for a planar insertion.  If there is no planar parent this is not a concern,
    // but in situations where there is one the preparatory subtraction in particular needs
    // some thought since it cannot be added to the subcomb dedicated to the torus shape - it
    // must be subtracted from the arb parent before the subcomb is added.
    //
    // NOTE! it will also be important to process all cylindrical and spherical surfaces before
    // we do the toroidal cases, since the insertion answers are different for toridal volumes
    // depending on whether or not there is a parent brep

    // If all edge planes are either parallel t  the torus plane or on the same
    // vertical plane, we have either a full torus or a portion of a torus that
    // involves the full 360 degree revolution.  If not, we have an arc of the
    // torus, which may be further trimmed down by torus-plane parallel edges.

    return -1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
