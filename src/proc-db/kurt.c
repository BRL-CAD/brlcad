/*                          K U R T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"


mat_t identity;
double degtorad = 0.0174532925199433;

struct val {
    double v_z[3];
    double v_x;
    double v_y;
    int v_n;
} val[20][20];

void do_cell(struct val *vp, double xc, double yc);
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

    if (argc > 0)
	bu_log("Usage: %s\n", argv[0]);

    outfp = wdb_fopen("kurt.g");
    mk_id(outfp, "Kurt's multi-valued function");

    /* Create the detail cells */
    size = 10;	/* mm */
    quant = 18;
    base = -size*(quant/2);
    for (ix=quant-1; ix>=0; ix--) {
	x = base + ix*size;
	for (iy=quant-1; iy>=0; iy--) {
	    y = base + iy*size;
	    do_cell(&val[ix][iy], x, y);
	}
    }

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
    if (VDOT(n, out) < 0) {
	VREVERSE(n, n);
    }

    /* Use same normal for all vertices (flat shading) */
    for (i=0; i<npts; i++) {
	VMOVE(norms[i], n);
    }
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
    } else {
	VMOVE(dir, dir_at);
    }

    snprintf(nbuf, 64, "%s.s", name);
    VSETALL(center, 0);
    mk_sph(outfp, nbuf, center, r);

    /*
     * Need to rotate from 0, 0, -1 to vect "dir",
     * then xlate to final position.
     */
    VSET(from, 0, 0, -1);
    bn_mat_fromto(rot, from, dir);
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
