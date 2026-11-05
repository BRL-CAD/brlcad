/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#include "common.h"

#include <cmath>

#include "bu/app.h"
#include "bu/log.h"
#include "brep.h"

#include "shape_recognition/shape_recognition.h"


static int
add_arc_edge(
    ON_Brep &brep,
    int start_vertex,
    int end_vertex,
    const ON_3dPoint &middle)
{
    const ON_Arc arc(brep.m_V[start_vertex].Point(), middle,
	brep.m_V[end_vertex].Point());
    if (!arc.IsValid()) {
	return -1;
    }

    const int curve_index = brep.AddEdgeCurve(new ON_ArcCurve(arc));
    if (curve_index < 0) {
	return -1;
    }

    return brep.NewEdge(brep.m_V[start_vertex], brep.m_V[end_vertex],
	curve_index).m_edge_index;
}


static int
test_sphere_implicit_plane()
{
    ON_Brep brep;
    brep.NewVertex(ON_3dPoint(0.0, 0.0, 0.0), 0.0);
    brep.NewVertex(ON_3dPoint(2.0, 0.0, 0.0), 0.0);
    brep.NewVertex(ON_3dPoint(0.0, 2.0, 0.0), 0.0);

    int edges[3];
    edges[0] = add_arc_edge(brep, 0, 1, ON_3dPoint(1.0, -1.0, 0.0));
    edges[1] = add_arc_edge(brep, 1, 2, ON_3dPoint(2.0, 2.0, 0.0));
    edges[2] = add_arc_edge(brep, 2, 0, ON_3dPoint(-1.0, 1.0, 0.0));
    if (edges[0] < 0 || edges[1] < 0 || edges[2] < 0) {
	bu_log("Unable to construct sphere implicit-plane arc edges\n");
	return 1;
    }

    ON_SimpleArray<ON_Plane> planes;
    const int plane_index = sph_implicit_plane(&brep, 3, edges, &planes);
    if (plane_index != 0 || planes.Count() != 1 || !planes[0].IsValid()) {
	bu_log("Sphere implicit-plane recognition did not produce one plane\n");
	return 1;
    }

    for (int vertex_index = 0; vertex_index < brep.m_V.Count(); ++vertex_index) {
	if (std::fabs(planes[0].DistanceTo(brep.m_V[vertex_index].Point())) >
	    BREP_SPHERICAL_TOL) {
	    bu_log("Sphere implicit plane does not contain an arc vertex\n");
	    return 1;
	}
    }

    return 0;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc != 1) {
	return 1;
    }

    return test_sphere_implicit_plane();
}
