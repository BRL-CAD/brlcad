/*                      T R I _ F A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
 * Implements triangulateFace routine for triangulating convex/concave planar N-gons.
 * Uses nmg for triangulation, but accepts and returns simple arrays.
 *
 */

#include "bu.h"
#include "nmg.h"
#include "raytrace.h"

static const int VERTICES_PER_FACE = 3;

/* nmg access routines */

struct shell*
get_first_shell(struct model *model)
{
    struct nmgregion *region;
    struct shell *shell;

    region = BU_LIST_FIRST(nmgregion, &model->r_hd);
    shell = BU_LIST_FIRST(shell, &region->s_hd);

    return shell;
}

struct model*
get_faceuse_model(struct faceuse *fu)
{
    return fu->s_p->r_p->m_p;
}

/* nmg construction routines */

struct vertex_g*
make_nmg_vertex_g(double x, double y, double z, long index)
{
    struct vertex_g *vg;

    BU_GETSTRUCT(vg, vertex_g);
    vg->magic = NMG_VERTEX_G_MAGIC;

    VSET(vg->coord, x, y, z);
    vg->index = index;

    return vg;
}

struct vertex*
make_nmg_vertex(double x, double y, double z, long index)
{
    struct vertex *v;

    BU_GETSTRUCT(v, vertex);
    v->magic = NMG_VERTEX_MAGIC;
    
    BU_LIST_INIT(&v->vu_hd);
    v->vg_p = make_nmg_vertex_g(x, y, z, index);
    v->index = index;

    return v;
}

void
attach_face_g_plane(struct face *f)
{
    struct face_g_plane *plane;

    BU_GETSTRUCT(plane, face_g_plane);
    plane->magic = NMG_FACE_G_PLANE_MAGIC;

    /* link up and down */
    BU_LIST_INIT(&plane->f_hd);
    BU_LIST_PUSH(&plane->f_hd, &f->l);

    f->g.plane_p = plane;
}

/* builds an nmg model containing a single faceuse which represents the face
 * specified in points[]
 */
struct model*
make_model_from_face(const double points[], int numPoints)
{
    int i;
    struct model *model;
    struct shell *shell;
    struct faceuse *fu;
    struct vertex **verts;
    const double *p;

    /* copy each point into vertex to create verts array */
    verts = (struct vertex**)bu_malloc(sizeof(struct vertex*) * numPoints,
	"verts");

    for (i = 0; i < numPoints; i++) {
	p = &points[i * ELEMENTS_PER_POINT];
	verts[i] = make_nmg_vertex(p[X], p[Y], p[Z], (long)i);
    }

    /* make base nmg model */
    model = nmg_mm();
    nmg_mrsv(model);

    /* add face from verts */
    shell = get_first_shell(model);
    nmg_cface(shell, verts, numPoints);
    bu_free(verts, "verts");

    /* add geometry to face */
    fu = BU_LIST_FIRST(faceuse, &shell->fu_hd);
    attach_face_g_plane(fu->f_p);
    nmg_calc_face_plane(fu, fu->f_p->g.plane_p->N);
    fu->orientation = OT_SAME;

    return model;
}

struct faceuse*
make_faceuse_from_face(const double points[], int numPoints)
{
    struct model *model;
    struct shell *shell;
    struct faceuse *fu;

    model = make_model_from_face(points, numPoints);
    shell = get_first_shell(model);
    fu = BU_LIST_FIRST(faceuse, &shell->fu_hd);

    return fu;
}

/* triangulation routines */

/* Searches points[] for the specified point. Match is determined using the
 * specified distance tolerance.
 *
 * If match is found, returns the point number (starting at 0) of the match.
 * Otherwise returns -1.
 */
int
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
 * three-cordinate vertices that specify a planar N-gon in CCW/CW order.
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
    struct model *model;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    size_t numFaceVertices;
    int i, ref;
    double *point;

    /* get nmg faceuse that represents the face specified by points */
    fu = make_faceuse_from_face(points, numPoints);

    /* triangulate face */
    nmg_triangulate_fu(fu, &tol);

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
	    point = eu->vu_p->v_p->vg_p->coord;
	    ref = getPointReference(point, points, numPoints, tol.dist);
	    (*faces)[i++] = ref;
	}
    }

    model = get_faceuse_model(fu);
    nmg_km(model);
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
