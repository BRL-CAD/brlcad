/*                      S P H E R E . C P P
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
/** @file sphere.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include <set>
#include <map>

#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

int
sph_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand)
{
    ON_Sphere sorig;
    ON_Surface *ssorig = forig->SurfaceOf()->Duplicate();
    ssorig->IsSphere(&sorig, BREP_SPHERICAL_TOL);
    delete ssorig;

    ON_Sphere scand;
    ON_Surface *sscand = fcand->SurfaceOf()->Duplicate();
    sscand->IsSphere(&scand, BREP_SPHERICAL_TOL);
    delete sscand;

    // Make sure the sphere centers match
    if (sorig.Center().DistanceTo(scand.Center()) > VUNITIZE_TOL) return 0;

    // Make sure the radii match
    if (!NEAR_ZERO(sorig.Radius() - scand.Radius(), VUNITIZE_TOL)) return 0;

    // TODO - make sure negative/positive status for both faces is the same.

    return 1;
}


/* Return -1 if the sphere face is pointing in toward the center,
 * 1 if it is pointing out, and 0 if there is some other problem */
int
negative_sphere(const ON_Brep *brep, int face_index, double sph_tol)
{
    int ret = 0;
    const ON_Surface *surf = brep->m_F[face_index].SurfaceOf();
    ON_Sphere sph;
    ON_Surface *cs = surf->Duplicate();
    cs->IsSphere(&sph, sph_tol);
    delete cs;

    ON_3dPoint pnt;
    ON_3dVector normal;
    double u = surf->Domain(0).Mid();
    double v = surf->Domain(1).Mid();
    if (!surf->EvNormal(u, v, pnt, normal)) return 0;

    ON_3dVector vect = pnt - sph.Center();
    double dotp = ON_DotProduct(vect, normal);

    if (NEAR_ZERO(dotp, VUNITIZE_TOL)) return 0;
    ret = (dotp < 0) ? -1 : 1;
    if (brep->m_F[face_index].m_bRev) ret = -1 * ret;
    return ret;
}


int
sph_implicit_plane(const ON_Brep *brep, int ec, int *edges, ON_SimpleArray<ON_Plane> *sph_planes)
{
    // We need to find vertices that are connected to two edges that are not on
    // the same circle.

    if ((*sph_planes).Count() != 3) return -1;


    // Step 1 - build sets of vertices in the edges
    std::set<int> verts;
    std::set<int>::iterator v_it;
    std::set<int> edge_set;
    for (int i = 0; i < ec; i++) {
	const ON_BrepEdge *edge = &(brep->m_E[edges[i]]);
	edge_set.insert(edges[i]);
	verts.insert(edge->Vertex(0)->m_vertex_index);
	verts.insert(edge->Vertex(1)->m_vertex_index);
    }

    // Step 2 - for all vertices, check the edges in the edge set to see
    // if they are on different circles.  If so, vertex is a candidate for
    // plane building
    int pverts[3];
    int pind = -1;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	int ind = -1;
	int e_ind[2] = {-1, -1};
	const ON_BrepVertex *v = &(brep->m_V[*v_it]);
	for (int ei = 0; ei < v->m_ei.Count(); ei++) {
	    const ON_BrepEdge *e = &(brep->m_E[v->m_ei[ei]]);
	    if (edge_set.find(e->m_edge_index) != edge_set.end()) {
		ind++;
		// vert should have only two non-degen edges in the sphere if
		// we're actually looking for an implicit plane
		if (ind > 1) break;
		e_ind[ind] = e->m_edge_index;
	    }
	}
	// Check the arcs
	if (ind == 1) {
	    const ON_BrepEdge *e1 = &(brep->m_E[e_ind[0]]);
	    ON_Curve *c1 = e1->EdgeCurveOf()->Duplicate();
	    const ON_BrepEdge *e2 = &(brep->m_E[e_ind[1]]);
	    ON_Curve *c2 = e2->EdgeCurveOf()->Duplicate();
	    ON_Arc a1, a2;
	    if (c1->IsArc(NULL, &a1, BREP_SPHERICAL_TOL) && c2->IsArc(NULL, &a1, BREP_SPHERICAL_TOL)) {
		ON_Circle circ1(a1.StartPoint(), a1.MidPoint(), a1.EndPoint());
		ON_Circle circ2(a2.StartPoint(), a2.MidPoint(), a2.EndPoint());
		if ((circ1.Center().DistanceTo(circ2.Center()) > VUNITIZE_TOL) || (!NEAR_ZERO(circ1.Radius() - circ2.Radius(), BREP_SPHERICAL_TOL))) {
		    pind++;
		    if (pind > 2) return -1;
		    pverts[pind] = v->m_vertex_index;
		}
	    } else {
		// If we don't have two arcs, bail.
		return -1;
	    }
	}
    }

    if (pind != 2) return -1;

    // Step 3 - build a plane from three of the candidate points.
    ON_Plane imp(brep->m_V[pverts[0]].Point(), brep->m_V[pverts[1]].Point(), brep->m_V[pverts[2]].Point());
    (*sph_planes).Append(imp);

    return (*sph_planes).Count() - 1;
}


int
sph_implicit_params(struct subbrep_shoal_data *data, ON_SimpleArray<ON_Plane> *sph_planes, int shoal_nonplanar_face)
{
    ON_Sphere sph;
    ON_Surface *cs = data->i->brep->m_L[data->shoal_loops[0]].Face()->SurfaceOf()->Duplicate();
    cs->IsSphere(&sph, BREP_SPHERICAL_TOL);
    delete cs;

    int need_arbn = ((*sph_planes).Count() == 0) ? 0 : 1;

    // Populate the CSG implicit primitive data
    data->params->csg_type = SPHERE;
    // Flag the sphere according to the negative or positive status of the
    // sphere surface.
    data->params->negative = negative_sphere(data->i->brep, shoal_nonplanar_face, BREP_SPHERICAL_TOL);
    data->params->bool_op = (data->params->negative == -1) ? '-' : 'u';

    ON_3dPoint origin = sph.Center();
    BN_VMOVE(data->params->origin, origin);
    data->params->radius = sph.Radius();

    if (need_arbn) {
	ON_3dVector xplus = sph.PointAt(0, 0) - sph.Center();
	ON_3dVector yplus = sph.PointAt(M_PI/2, 0) - sph.Center();
	ON_3dVector zplus = sph.NorthPole() - sph.Center();
	ON_3dPoint xmax = sph.Center() + 1.01*xplus;
	ON_3dPoint xmin = sph.Center() - 1.01*xplus;
	ON_3dPoint ymax = sph.Center() + 1.01*yplus;
	ON_3dPoint ymin = sph.Center() - 1.01*yplus;
	ON_3dPoint zmax = sph.Center() + 1.01*zplus;
	ON_3dPoint zmin = sph.Center() - 1.01*zplus;
	xplus.Unitize();
	yplus.Unitize();
	zplus.Unitize();
	(*sph_planes).Append(ON_Plane(xmin, -1 * xplus));
	(*sph_planes).Append(ON_Plane(xmax, xplus));
	(*sph_planes).Append(ON_Plane(ymin, -1 * yplus));
	(*sph_planes).Append(ON_Plane(ymax, yplus));
	(*sph_planes).Append(ON_Plane(zmin, -1 * zplus));
	(*sph_planes).Append(ON_Plane(zmax, zplus));
    }

    return need_arbn;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
