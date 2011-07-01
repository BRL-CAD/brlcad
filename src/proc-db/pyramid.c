/*                       P Y R A M I D . C
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
/** @file proc-db/pyramid.c
 *
 * Program to generate recursive 3-d pyramids (arb4).
 * Inspired by the SigGraph paper of Glasser.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"


void do_leaf(char *name), do_pleaf(), pnorms(fastf_t (*norms)[3], fastf_t (*verts)[3], fastf_t *centroid, int npts), do_tree(char *name, char *lname, int level);

double sin60;

struct rt_wdb *outfp;

int
main(int argc, char **argv)
{
    int depth;

    if (argc != 2) {
	fprintf(stderr, "Usage:  pyramid recursion\n");
	return 1;
    }
    depth = atoi(argv[1]);
    sin60 = sin(60.0 * 3.14159265358979323846264 / 180.0);

    outfp = wdb_fopen("pyramid.g");

    mk_id(outfp, "3-D Pyramids");

    do_leaf("leaf");
    do_tree("tree", "leaf", depth);

    return 0;
}

/* Make a leaf node out of an ARB4 */
void
do_leaf(char *name)
{
    point_t pt[4];

    VSET(pt[0], 0, 0, 0);
    VSET(pt[1], 100, 0, 0);
    VSET(pt[2], 50, 100*sin60, 0);
    VSET(pt[3], 50, 100*sin60/3, 100*sin60);

    mk_arb4(outfp, name, &pt[0][X]);
}


/*
 * Find the single outward pointing normal for a facet.
 * Assumes all points are coplanar (they better be!).
 */
void
pnorms(fastf_t (*norms)[3], fastf_t (*verts)[3], fastf_t *centroid, int npts)
{
    int i;
    vect_t ab, ac;
    vect_t n;
    vect_t out;		/* hopefully points outwards */

    VSUB2(ab, verts[1], verts[0]);
    VSUB2(ac, verts[2], verts[0]);
    VCROSS(n, ab, ac);
    VUNITIZE(n);

    /* If normal points inwards (towards centroid), flip it */
    VSUB2(out, verts[0], centroid);
    if (VDOT(n, out) < 0) {
	VREVERSE(n, n);
    }

    /* Use same normal for all vertices (flat shading) */
    for (i=0; i<npts; i++) {
	VMOVE(norms[i], n);
    }
}

void
do_tree(char *name, char *lname, int level)
{
    int i;
    char nm[64];
    char *leafp;
    int scale;
    struct wmember head;
    struct wmember *wp;

    BU_LIST_INIT(&head.l);

    if (level <= 1)
	leafp = lname;
    else
	leafp = nm;

    scale = 100;
    for (i=1; i<level; i++)
	scale *= 2;

    snprintf(nm, 64, "%sL", name);
    wp = mk_addmember(leafp, &head.l, NULL, WMOP_UNION);
    MAT_IDN(wp->wm_mat);

    snprintf(nm, 64, "%sR", name);
    wp = mk_addmember(leafp, &head.l, NULL, WMOP_UNION);
    MAT_DELTAS(wp->wm_mat, 1*scale, 0, 0);

    snprintf(nm, 64, "%sB", name);
    wp = mk_addmember(leafp, &head.l, NULL, WMOP_UNION);
    MAT_DELTAS(wp->wm_mat, 0.5*scale, sin60*scale, 0);

    snprintf(nm, 64, "%sT", name);
    wp = mk_addmember(leafp, &head.l, NULL, WMOP_UNION);
    MAT_DELTAS(wp->wm_mat, 0.5*scale, sin60/3*scale, sin60*scale);

    /* Set region flag on lowest level */
    mk_lcomb(outfp, name, &head, level<=1, NULL, NULL, NULL, 0);

    /* Loop for children if level > 1 */
    if (level <= 1)
	return;
    for (i=0; i<4; i++) {
	snprintf(nm, 64, "%s%c", name, "LRBTx"[i]);
	do_tree(nm, lname, level-1);
    }
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
