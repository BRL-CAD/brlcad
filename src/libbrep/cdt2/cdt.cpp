/*                        C D T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "./cdt.h"

static int
brep_cdt_init(struct brep_cdt *s, void *bv, const char *objname)
{
    if (!s || !bv || !objname) return -1;
    s->i = new brep_cdt_impl;
    if (!s->i) return -1;
    s->i->s.orig_brep = (ON_Brep *)bv;
    s->i->s.name = std::string(objname);
    s->msgs = (struct bu_vls *)bu_calloc(1, sizeof(struct bu_vls), "message buffer");
    bu_vls_init(s->msgs);
    return 0;
}

extern "C" int
brep_cdt_create(struct brep_cdt **cdt, void *bv, const char *objname)
{
    if (!cdt || !bv) return -1;
    (*cdt) = new brep_cdt;
    return brep_cdt_init(*cdt);
}

static void
brep_cdt_clear(struct brep_cdt *s)
{
    delete s->i;
    bu_vls_free(s->msgs);
    bu_free(s->msgs, "message buffer free");
}

extern "C" void
brep_cdt_destroy(struct brep_cdt *s)
{
    if (!s) return;
    brep_cdt_clear(s);
    delete s;
}


// TODO - need a flag variable to enable various modes (watertightness,
// validity, overlap resolving - no need for separate functions for all that...)
int
brep_cdt_triangulate(struct brep_cdt *s_cdt, int face_cnt, int *faces)
{
    if (!s_cdt) return -1;

    // Check for any conditions that are show-stoppers
    ON_wString wonstr;
    ON_TextLog vout(wonstr);
    if (!s_cdt->i->s.orig_brep->IsValid(&vout)) {
	bu_vls_printf(s->msgs, "NOTE: brep is NOT valid, cannot produce watertight mesh\n");
    }

    // For now, edges must have 2 and only 2 trims for this to work.
    // TODO - only do this check for solid objects - plate mode objects should be processed.
    // Need to think about how to structure that - in such a case all of this logic should
    // collapse back to the original logic...
    // for that matter, if we want the fastest possible mesh regardless of quality we should
    // be able to do that...
    for (int index = 0; index < s_cdt->orig_brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = s_cdt->orig_brep->m_E[index];
	if (edge.TrimCount() != 2) {
	    bu_vls_printf(s->msgs, "Edge %d trim count: %d - can't (yet) do watertight meshing\n", edge.m_edge_index, edge.TrimCount());
	    return -1;
	}
    }

    // We may be changing the ON_Brep data, so work on a copy
    // rather than the original object
    if (!s_cdt->brep) {

	s_cdt->i->s.brep = new ON_Brep(*s_cdt->orig_brep);

	// Attempt to minimize situations where 2D and 3D distances get out of sync
	// by shrinking the surfaces down to the active area of the face
	s_cdt->i->s.brep->ShrinkSurfaces();

    }

    ON_Brep* brep = s_cdt->i->s.brep;

    // Reparameterize the face's surface and transform the "u" and "v"
    // coordinates of all the face's parameter space trimming curves to
    // minimize distortion in the map from parameter space to 3d.
    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace *face = brep->Face(face_index);
	const ON_Surface *s = face->SurfaceOf();
	double surface_width, surface_height;
	if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	    face->SetDomain(0, 0.0, surface_width);
	    face->SetDomain(1, 0.0, surface_height);
	}
    }

    // TODO - populate b_pnts with all the vertex points.
    // NOTE: At this stage need the index of each b_pnt to match the
    // ON_BrepVertex index value the edges use, so we can populate the edge
    // structures without having to build a std::map.  If for any reason
    // that should not prove viable, need to build and use the map.

    // TODO - populate b_edges_vect with all m_E edges.
    // NOTE: At this stage need the index of each b_edge to match the
    // ON_BrepEdge index value, so we can populate the loop structures without
    // having to build a std::map.  If for any reason that should not prove
    // viable, need to build and use the map.

    // Build the initial set of unordered edges.

    // Vertices and edges are the only two elements that are not unique to
    // faces.  However, to build up unordered edges we also want the polygon
    // edges in place for the faces.  Build initial polygons now.
    // At this point, with no splitting done, the ON_Brep indicies and brep_cdt
    // indices should still agree.

    // Build up the initial m_uedges set and tree, associating ordered edges
    // and polygon edges with the uedges.  polygon edges and ordered edges have
    // an unambiguous association, so the only difficult mapping is between the
    // ordered edge and the unordered edge - we need to translate the two
    // vertex indices into an unordered edge array index (if the associated
    // unordered edge has already been created by the other edge in the
    // pairing.)  Use an RTree lookup on the uedge tree for this, since by
    // definition the two ordered edges will have the same bounding box in
    // space.

    /* Check for singular trims.  Vertices associated with such a trim are
     * categorized as singular vertices.  Get vertex normals that are the
     * average of the surface normals at the junction from faces that don't use
     * a singular trim to reference the vertex. */

}



/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

