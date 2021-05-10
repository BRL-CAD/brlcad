/*               T R I M E S H _ I S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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
/** @file trimesh_isect.cpp
 *
 * Test if two meshes intersect and return the set of intersecting faces.
 *
 */

#include "common.h"

#include <vector>
#include <set>
#include <map>
#include <cmath>

#include <float.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "bview/plot3.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn/tol.h"
#include "bg/tri_tri.h"
#include "bg/trimesh.h"

HIDDEN void
plot_faces(const char *fname, std::set<int> *faces, int *f, point_t *v)
{
    std::set<int>::iterator f_it;
    FILE* plot_file = fopen(fname, "wb");
    int r = int(256*drand48() + 100.0);
    int g = int(256*drand48() + 100.0);
    int b = int(256*drand48() + 100.0);
    pl_color(plot_file, r, g, b);
    for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	point_t p1, p2, p3;
	VMOVE(p1, v[f[(*f_it)*3+0]]);
	VMOVE(p2, v[f[(*f_it)*3+1]]);
	VMOVE(p3, v[f[(*f_it)*3+2]]);
	pdv_3move(plot_file, p1);
	pdv_3cont(plot_file, p2);
	pdv_3move(plot_file, p1);
	pdv_3cont(plot_file, p3);
	pdv_3move(plot_file, p2);
	pdv_3cont(plot_file, p3);
    }
    fclose(plot_file);
}

/* For NURBS refinement, we do this once and keep the set of "inside" faces from
 * each mesh.  If any of the vertices from those faces are still inside after a
 * refinement and remeshing step, they are either genuinely inside the mesh (bad
 * NURBS geometry) or our closestpoint routines haven't found the right match. */


/* See Mesh Arrangements for Solid Geometry - this implements the initial stages
 * of that workflow, although it does not go on to construct the new faces and
 * assemble the new meshes. */
extern "C" int
bg_trimesh_isect(
    int **faces_inside_1, int *num_faces_inside_1, int **faces_inside_2, int *num_faces_inside_2,
    int **faces_isect_1, int *num_faces_isect_1, int **faces_isect_2, int *num_faces_isect_2,
    int *faces_1, int num_faces_1, point_t *vertices_1, int num_vertices_1,
    int *faces_2, int num_faces_2, point_t *vertices_2, int num_vertices_2)
{
    int ret = 0;
    if (!faces_1 || !num_faces_1 || !vertices_1 || !num_vertices_1) return 0;
    if (!faces_2 || !num_faces_2 || !vertices_2 || !num_vertices_2) return 0;

    /* TODO - check solidity.  If these aren't both valid/solid, this test
     * won't (currently) handle it */


    /* Step 1 - construct and check bboxes.  If they don't overlap, there's no
     * need for any further work. */

    point_t bb1min, bb2min;
    point_t bb1max, bb2max;

    VSETALL(bb1min, MAX_FASTF);
    VSETALL(bb2min, MAX_FASTF);
    VSETALL(bb1max, -1.0*MAX_FASTF);
    VSETALL(bb2max, -1.0*MAX_FASTF);

    for (int i = 0; i < num_vertices_1; i++) {
	VMINMAX(bb1min, bb1max, vertices_1[i]);
    }

    for (int i = 0; i < num_vertices_2; i++) {
	VMINMAX(bb2min, bb2max, vertices_2[i]);
    }

    int isect = (
	(bb1min[0] <= bb2max[0] && bb1max[0] >= bb2min[0]) &&
	(bb1min[1] <= bb2max[1] && bb1max[1] >= bb2min[1]) &&
	(bb1min[2] <= bb2max[2] && bb1max[2] >= bb2min[2])
	) ? 1 : 0;

    if (!isect) {
	if (num_faces_inside_1) (*num_faces_inside_1) = 0;
	if (num_faces_inside_2) (*num_faces_inside_2) = 0;
	if (num_faces_isect_1) (*num_faces_isect_1) = 0;
	if (num_faces_isect_2) (*num_faces_isect_2) = 0;
	if (faces_inside_1) (*faces_inside_1) = NULL;
	if (faces_inside_2) (*faces_inside_2) = NULL;
	if (faces_isect_1) (*faces_isect_1) = NULL;
	if (faces_isect_2) (*faces_isect_2) = NULL;
	return 0;
    }

    /*if (!isect) {
      bu_log("in bbox1.s rpp %f %f %f %f %f %f\n", bb1min[0], bb1max[0], bb1min[1], bb1max[1], bb1min[2], bb1max[2]);
      bu_log("in bbox2.s rpp %f %f %f %f %f %f\n", bb2min[0], bb2max[0], bb2min[1], bb2max[1], bb2min[2], bb2max[2]);
      }*/

    /* If the bboxes overlap, build the sets of faces with at least one vertex in the
     * other mesh's bounding box.  Those are the only ones we need to worry about */
    std::vector<int> m1_working_faces;
    std::vector<int> m2_working_faces;

    for (int i = 0; i < num_faces_1; i++) {

	if (V3PNT_IN_RPP(vertices_1[faces_1[3*i]], bb2min, bb2max)) {
	    m1_working_faces.push_back(i);
	    continue;
	}

	if (V3PNT_IN_RPP(vertices_1[faces_1[3*i+1]], bb2min, bb2max)) {
	    m1_working_faces.push_back(i);
	    continue;
	}

	if (V3PNT_IN_RPP(vertices_1[faces_1[3*i+2]], bb2min, bb2max)) {
	    m1_working_faces.push_back(i);
	    continue;
	}

    }

    for (int i = 0; i < num_faces_2; i++) {

	if (V3PNT_IN_RPP(vertices_2[faces_2[3*i]], bb1min, bb1max)) {
	    m2_working_faces.push_back(i);
	    continue;
	}

	if (V3PNT_IN_RPP(vertices_2[faces_2[3*i+1]], bb1min, bb1max)) {
	    m2_working_faces.push_back(i);
	    continue;
	}

	if (V3PNT_IN_RPP(vertices_2[faces_2[3*i+2]], bb1min, bb1max)) {
	    m2_working_faces.push_back(i);
	    continue;
	}

    }

    bu_log("m1_working_faces size: %zd\n", m1_working_faces.size());
    bu_log("m2_working_faces size: %zd\n", m2_working_faces.size());

    /* For each "working" face in faces_1, check it against "working" faces in
     * face_2 for intersections.  IFF it intersects, add it and the other face
     * to their intersects set.
     *
     * TODO - this is the naive NxM implementation - it can undoubtedly be improved
     * upon with one form or another of acceleration structure - but for now
     * we're after simple and working.  Once this is established to be a
     * performance bottleneck we can dig deeper into it. */
    std::set<int> m1_intersecting_faces;
    std::set<int> m2_intersecting_faces;
    for (size_t i = 0; i < m1_working_faces.size(); i++) {
	for (size_t j = 0; j < m2_working_faces.size(); j++) {
	    int v1_1 = faces_1[3*m1_working_faces[i]];
	    int v1_2 = faces_1[3*m1_working_faces[i]+1];
	    int v1_3 = faces_1[3*m1_working_faces[i]+2];
	    int v2_1 = faces_2[3*m2_working_faces[j]];
	    int v2_2 = faces_2[3*m2_working_faces[j]+1];
	    int v2_3 = faces_2[3*m2_working_faces[j]+2];
	    if (bg_tri_tri_isect(
		    vertices_1[v1_1], vertices_1[v1_2], vertices_1[v1_3],
		    vertices_2[v2_1], vertices_2[v2_2], vertices_2[v2_3]))
	    {
		m1_intersecting_faces.insert(m1_working_faces[i]);
		m2_intersecting_faces.insert(m2_working_faces[j]);
	    }
	}
    }

    bu_log("m1_intersecting_faces size: %zd\n", m1_intersecting_faces.size());

    plot_faces("m1.plot3", &m1_intersecting_faces, faces_1, vertices_1);

    bu_log("m2_intersecting_faces size: %zd\n", m2_intersecting_faces.size());

    plot_faces("m2.plot3", &m2_intersecting_faces, faces_2, vertices_2);



#if 0

    /* From the set of intersecting faces in each face, characterize the vertices
     * of the face as inside or outside the other faces via the face normals (we
     * are assuming CCW meshes with correct outward facing normals.)  Based on those
     * vertex characterizations, look up other faces associated with those vertices.
     * If those faces are not intersecting faces and are in the working_faces set,
     * characterize them according to the vertex status and pull the other faces
     * associated with that faces vertices that are not intersecting faces for
     * processing.  In essence, we "walk" the mesh until all triangles are
     * accounted for as either intersecting, inside the other mesh, or outside
     * the mesh.
     *
     * This will work ONLY on valid solid meshes.  Anything else will need
     * https://github.com/sideeffects/WindingNumber for an inside/outside
     * determination... */
    std::set<int> m1_inside_m2_faces;
    std::set<int> m2_inside_m1_faces;
#endif

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

