/*                      T R I _ F A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2026 United States Government as represented by
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
/** @file tri_face.c
 *
 * Triangulates OBJ polygon faces and returns their original corner indices.
 *
 */

#include "common.h"

#include <limits.h>

#include "bu/malloc.h"
#include "nmg.h"

#include "tri_face.h"

enum { VERTICES_PER_TRIANGLE = 3 };


static int
point_reference(const point_t point, const double points[], size_t num_points)
{
    for (size_t i = 0; i < num_points; i++) {
	if (VEQUAL(point, &points[i * ELEMENTS_PER_POINT]))
	    return (int)i;
    }

    return -1;
}


static int
read_triangle(int triangle[VERTICES_PER_TRIANGLE], const struct loopuse *lu, const double points[], size_t num_points)
{
    struct edgeuse *eu;
    size_t corner = 0;

    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	return 0;

    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	int ref = point_reference(eu->vu_p->v_p->vg_p->coord, points, num_points);
	if (ref < 0 || corner >= VERTICES_PER_TRIANGLE)
	    return 0;
	triangle[corner++] = ref;
    }

    return corner == VERTICES_PER_TRIANGLE;
}


void
obj_triangulate_face(int **faces, size_t *num_faces, const double points[], size_t num_points,
		     const struct bn_tol *tol, struct bu_list *vlfree)
{
    *faces = NULL;
    *num_faces = 0;
    if (num_points < VERTICES_PER_TRIANGLE || num_points > INT_MAX)
	return;

    struct model *model = nmg_mm();
    struct nmgregion *region = nmg_mrsv(model);
    struct shell *shell = BU_LIST_FIRST(shell, &region->s_hd);
    struct vertex **verts = (struct vertex **)bu_calloc(num_points, sizeof(*verts), "verts");
    struct faceuse *fu = nmg_cface(shell, verts, (int)num_points);
    for (size_t i = 0; i < num_points; i++)
	nmg_vertex_gv(verts[i], &points[i * ELEMENTS_PER_POINT]);
    bu_free(verts, "verts");

    if (nmg_calc_face_g(fu, vlfree)) {
	nmg_km(model);
	return;
    }

    size_t max_faces = num_points - 2;
    int *triangles = (int *)bu_calloc(max_faces, VERTICES_PER_TRIANGLE * sizeof(*triangles), "faces");
    size_t count = 0;

    if (nmg_lu_is_convex(BU_LIST_FIRST(loopuse, &fu->lu_hd), vlfree, tol)) {
	for (; count < max_faces; count++) {
	    int *triangle = &triangles[count * VERTICES_PER_TRIANGLE];
	    triangle[0] = 0;
	    triangle[1] = (int)count + 1;
	    triangle[2] = (int)count + 2;
	}
    } else if (nmg_triangulate_fu(fu, vlfree, tol) == 0) {
	/* CDT replaces the faceuse. The fallback splits its loops in place. */
	for (BU_LIST_FOR(fu, faceuse, &shell->fu_hd)) {
	    struct loopuse *lu;
	    if (fu->orientation != OT_SAME)
		continue;
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (count == max_faces || !read_triangle(&triangles[count * VERTICES_PER_TRIANGLE], lu, points, num_points)) {
		    count = 0;
		    goto done;
		}
		count++;
	    }
	}
    }

done:
    nmg_km(model);
    if (count) {
	*faces = triangles;
	*num_faces = count;
    } else {
	bu_free(triangles, "faces");
    }
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
