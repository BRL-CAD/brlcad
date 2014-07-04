/*                      T R I _ F A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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
 * Implements triangulateFace routine for triangulating convex/concave planar
 * N-gons. Uses nmg for triangulation, but accepts and returns simple arrays.
 *
 */

#include "common.h"

#include "bu.h"
#include "nmg.h"
#include "raytrace.h"

static const int VERTICES_PER_FACE = 3;

/* nmg access routines */

HIDDEN struct shell*
get_faceuse_shell(struct faceuse *fu)
{
    return fu->s_p;
}

/* nmg construction routines */

HIDDEN struct vertex_g*
make_nmg_vertex_g(struct shell *s, double x, double y, double z, long index)
{
    struct vertex_g *vg;

    GET_VERTEX_G(vg, s);
    vg->magic = NMG_VERTEX_G_MAGIC;

    VSET(vg->coord, x, y, z);
    vg->index = index;

    return vg;
}

HIDDEN struct vertex*
make_nmg_vertex(struct shell *s, double x, double y, double z, long index)
{
    struct vertex *v;

    GET_VERTEX(v, s);
    v->magic = NMG_VERTEX_MAGIC;

    BU_LIST_INIT(&v->vu_hd);
    v->vg_p = make_nmg_vertex_g(s, x, y, z, index);
    v->index = index;

    return v;
}

HIDDEN void
attach_face_g_plane(struct shell *s, struct face *f)
{
    struct face_g_plane *plane;

    GET_FACE_G_PLANE(plane, s);
    plane->magic = NMG_FACE_G_PLANE_MAGIC;

    /* link up and down */
    BU_LIST_INIT(&plane->f_hd);
    BU_LIST_PUSH(&plane->f_hd, &f->l);

    f->g.plane_p = plane;
}

/* builds an nmg model containing a single faceuse which represents the face
 * specified in points[]
 */
HIDDEN struct shell*
make_shell_from_face(const double points[], int numPoints)
{
    int i;
    struct shell *s;
    struct faceuse *fu;
    struct vertex **verts;
    const double *p;

    /* make base nmg model */
    s = nmg_ms();

    /* copy each point into vertex to create verts array */
    verts = (struct vertex**)bu_malloc(sizeof(struct vertex*) * numPoints,
	"verts");

    for (i = 0; i < numPoints; i++) {
	p = &points[i * ELEMENTS_PER_POINT];
	verts[i] = make_nmg_vertex(s, p[X], p[Y], p[Z], (long)i);
    }

    /* add face from verts */
    nmg_cface(s, verts, numPoints);
    bu_free(verts, "verts");

    /* add geometry to face */
    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
    attach_face_g_plane(s, fu->f_p);
    if (nmg_calc_face_plane(fu, fu->f_p->g.plane_p->N)) {
	nmg_ks(s);
	s = NULL;
    } else {
	fu->orientation = OT_SAME;
    }

    return s;
}

struct faceuse*
make_faceuse_from_face(const double points[], int numPoints)
{
    struct shell *s;
    struct faceuse *fu = NULL;

    s = make_shell_from_face(points, numPoints);

    if (s != NULL) {
	fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
    }

    return fu;
}

/* triangulation routines */

/* Searches points[] for the specified point. Match is determined using the
 * specified distance tolerance.
 *
 * If match is found, returns the point number (starting at 0) of the match.
 * Otherwise returns -1.
 */
HIDDEN int
getPointReference(
    const double point[],
    const double points[],
    size_t numPoints,
    double tol)
{
    size_t i;
    const double *currPoint;

    for (i = 0; i < numPoints; ++i) {
	currPoint = &points[i * ELEMENTS_PER_POINT];

	if (VNEAR_EQUAL(currPoint, point, tol)) {
	    return i;
	}
    }

    return -1;
}

/* points is the specification of face points. It should contain consecutive
 * three-coordinate vertices that specify a planar N-gon in CCW/CW order.
 *
 * faces will specify numFaces triangle faces with three consecutive vertex
 * references per face. Vertex reference v refers to the three consecutive
 * coordinates starting at points[v * ELEMENTS_PER_VERTEX].
 */
void
triangulateFace(
    int **faces,
    size_t *numFaces,
    const double points[],
    size_t numPoints,
    struct bn_tol tol)
{
    struct shell *s;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    size_t numFaceVertices;
    int i, ref;
    double point[3];

    /* get nmg faceuse that represents the face specified by points */
    fu = make_faceuse_from_face(points, numPoints);

    if (fu == NULL) {
	*faces = NULL;
	*numFaces = 0;
	return;
    }

    /* triangulate face */
    if (nmg_triangulate_fu(fu, &tol)) {
	*faces = NULL;
	*numFaces = 0;
	return;
    }

    /* face now composed of triangular loops */
    *numFaces = 0;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	(*numFaces)++;
    }

    /* create faces array */
    numFaceVertices = *numFaces * VERTICES_PER_FACE;
    *faces = (int*)bu_malloc(numFaceVertices * sizeof(int), "faces");

    i = 0;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    VMOVE(point, eu->vu_p->v_p->vg_p->coord);
	    ref = getPointReference(point, points, numPoints, tol.dist);
	    (*faces)[i++] = ref;
	}
    }

    s = get_faceuse_shell(fu);
    nmg_ks(s);
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
