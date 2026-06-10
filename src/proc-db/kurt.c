/*                          K U R T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file proc-db/kurt.c
 *
 * Program to generate polygons from a multi-valued function.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/malloc.h"
#include "bn.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"


struct val {
    double v_z[3];
    double v_x;
    double v_y;
    int v_n;
} val[20][20];

void do_cell(struct val *vp, double xc, double yc);
void make_surface(int quant);
void pnorms(fastf_t (*norms)[3], fastf_t (*verts)[3], fastf_t *centroid, int npts);
void do_light(char *name, fastf_t *pos, fastf_t *dir_at, int da_flag, double r, unsigned char *rgb);

struct rt_wdb *outfp;


int
main(int argc, char **argv)
{
    int ix, iy;
    double x, y;
    double size;
    double base;
    int quant;

    bu_setprogname(argv[0]);

    if (argc > 1) {
	bu_exit(1, "Usage: %s\n", argv[0]);
    }

    bu_log("Writing out geometry to file [kurt.g] ...");

    outfp = wdb_fopen("kurt.g");
    mk_id(outfp, "Kurt's multi-valued function");

    /* Create the detail cells */
    size = 10.0; /* mm */
    quant = 18;
    base = -size*(quant/2);
    for (ix=quant-1; ix>=0; ix--) {
	x = base + ix*size;
	for (iy=quant-1; iy>=0; iy--) {
	    y = base + iy*size;
	    do_cell(&val[ix][iy], x, y);
	}
    }

    /* Turn the sampled grid of evaluated points into a renderable
     * surface, then assemble a top-level region named "all".
     */
    make_surface(quant);

    db_close(outfp->dbip);
    bu_log(" done.\n");

    return 0;
}


void
do_cell(struct val *vp, double xc, double yc)

/* center coordinates, z=0+ */
{
    bn_poly_t polynom;
    bn_complex_t roots[4];	/* roots of final equation */
    int l;
    int nroots;
    int lim;

    polynom.dgr = 3;
    polynom.cf[0] = 1;
    polynom.cf[1] = 0;
    polynom.cf[2] = yc;
    polynom.cf[3] = xc;
    vp->v_n = 0;
    vp->v_x = xc;
    vp->v_y = yc;
    nroots = rt_poly_roots(&polynom, roots, "");
    if (nroots < 0 || (nroots & 1) == 0) {
	fprintf(stderr, "%d roots?\n", nroots);
	return;
    }
    for (l=0; l < nroots; l++) {
	if (NEAR_ZERO(roots[l].im, 0.0001))
	    vp->v_z[vp->v_n++] = roots[l].re;
    }
    /* Sort in increasing Z */
    for (lim = nroots-1; lim > 0; lim--) {
	for (l=0; l < lim; l++) {
	    double t;
	    if ((t=vp->v_z[l]) > vp->v_z[l+1]) {
		vp->v_z[l] = vp->v_z[l+1];
		vp->v_z[l+1] = t;
	    }
	}
    }
}


/*
 * Build a renderable surface from the grid of evaluated points.
 *
 * The multi-valued cubic always has at least one real root for every
 * sampled (x, y), so the lowest sheet (v_z[0]) provides a complete,
 * hole-free height field across the whole quant x quant grid.  We
 * triangulate that height field into a single BoT surface and wrap it
 * in a top-level region named "all" with a pleasant material.
 */
void
make_surface(int quant)
{
    size_t nverts;
    size_t nfaces;
    fastf_t *verts;
    int *faces;
    int ix, iy;
    size_t v;
    size_t f;
    struct wmember head;
    unsigned char rgb[3];

    if (quant < 2)
	return;

    nverts = (size_t)quant * (size_t)quant;
    nfaces = (size_t)(quant-1) * (size_t)(quant-1) * 2;

    verts = (fastf_t *)bu_malloc(nverts * 3 * sizeof(fastf_t), "kurt verts");
    faces = (int *)bu_malloc(nfaces * 3 * sizeof(int), "kurt faces");

    /* Lay out one vertex per grid cell using the lowest sheet. */
    v = 0;
    for (ix=0; ix < quant; ix++) {
	for (iy=0; iy < quant; iy++) {
	    struct val *vp = &val[ix][iy];
	    verts[v*3 + 0] = vp->v_x;
	    verts[v*3 + 1] = vp->v_y;
	    verts[v*3 + 2] = (vp->v_n > 0) ? vp->v_z[0] : 0.0;
	    v++;
	}
    }

    /* Two triangles per grid quad.  Vertex index is ix*quant + iy. */
    f = 0;
    for (ix=0; ix < quant-1; ix++) {
	for (iy=0; iy < quant-1; iy++) {
	    int v00 = ix*quant + iy;
	    int v01 = ix*quant + (iy+1);
	    int v10 = (ix+1)*quant + iy;
	    int v11 = (ix+1)*quant + (iy+1);

	    faces[f*3 + 0] = v00;
	    faces[f*3 + 1] = v10;
	    faces[f*3 + 2] = v11;
	    f++;

	    faces[f*3 + 0] = v00;
	    faces[f*3 + 1] = v11;
	    faces[f*3 + 2] = v01;
	    f++;
	}
    }

    mk_bot(outfp, "kurt.bot", RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0,
	   nverts, nfaces, verts, faces,
	   (fastf_t *)NULL, (struct bu_bitv *)NULL);

    bu_free((void *)verts, "kurt verts");
    bu_free((void *)faces, "kurt faces");

    /* Top-level region "all" with a pleasant blue-green plastic. */
    BU_LIST_INIT(&head.l);
    (void)mk_addmember("kurt.bot", &head.l, NULL, WMOP_UNION);
    rgb[0] = 80;
    rgb[1] = 160;
    rgb[2] = 200;
    mk_lcomb(outfp, "all", &head, 1, "plastic", "sh=10 sp=0.6 di=0.4", rgb, 0);
}


/*
 * Find the single outward pointing normal for a facet.
 * Assumes all points are coplanar (they better be!).
 */
void
pnorms(fastf_t (*norms)[3], fastf_t (*verts)[3], fastf_t *out, int npts)

/* hopefully points outwards */

{
    int i;
    vect_t ab, ac;
    vect_t n;

    VSUB2(ab, verts[1], verts[0]);
    VSUB2(ac, verts[2], verts[0]);
    VCROSS(n, ab, ac);
    VUNITIZE(n);

    /* If normal points inwards, flip it */
    if (VDOT(n, out) < 0)
	VREVERSE(n, n);

    /* Use same normal for all vertices (flat shading) */
    for (i=0; i<npts; i++)
	VMOVE(norms[i], n);
}


void
do_light(char *name, fastf_t *pos, fastf_t *dir_at, int da_flag, double r, unsigned char *rgb)

/* direction or aim point */
/* 0 = direction, !0 = aim point */
/* radius of light */

{
    char nbuf[64];
    vect_t center;
    mat_t rot;
    mat_t xlate;
    mat_t both;
    vect_t from;
    vect_t dir;

    if (da_flag) {
	VSUB2(dir, dir_at, pos);
	VUNITIZE(dir);
    } else
	VMOVE(dir, dir_at);

    snprintf(nbuf, 64, "%s.s", name);
    VSETALL(center, 0);
    mk_sph(outfp, nbuf, center, r);

    /*
     * Need to rotate from 0, 0, -1 to vect "dir",
     * then xlate to final position.
     */
    VSET(from, 0, 0, -1);
    bn_mat_fromto(rot, from, dir, &outfp->wdb_tol);
    MAT_IDN(xlate);
    MAT_DELTAS_VEC(xlate, pos);
    bn_mat_mul(both, xlate, rot);

    mk_region1(outfp, name, nbuf, "light", "shadows=1", rgb);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
