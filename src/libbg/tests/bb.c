/*                           B B . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2025 United States Government as represented by
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

#include "common.h"

#include <stdio.h>

#include "bu.h"
#include "bg.h"

int
obb_diff(
	point_t ac, vect_t av1, vect_t av2, vect_t av3,
	point_t bc, vect_t bv1, vect_t bv2, vect_t bv3
	)
{
    point_t diff;
    VSUB2(diff, ac, bc);

    vect_t bv[3];
    VMOVE(bv[0], bv1);
    VMOVE(bv[1], bv2);
    VMOVE(bv[2], bv3);
    VSCALE(bv[0], bv[0], -1);
    VSCALE(bv[1], bv[1], -1);
    VSCALE(bv[2], bv[2], -1);

    if (!VNEAR_EQUAL(ac, bc, VUNITIZE_TOL)) {
	bu_log("Note: obb (%g %g %g) and obb verts obb (%g %g %g) center point differs: %0.15f %0.15f %0.15f\n", V3ARGS(ac), V3ARGS(bc), V3ARGS(diff));
	return 1;
    }

    VSUB2(diff, av1, bv1);
    if (!VNEAR_EQUAL(av1, bv1, VUNITIZE_TOL) && !VNEAR_EQUAL(av1, bv[0], VUNITIZE_TOL)) {
	bu_log("Note: obb (%g %g %g) and obb verts obb (%g %g %g) v1 differs: %0.15f %0.15f %0.15f\n", V3ARGS(av1), V3ARGS(bv1), V3ARGS(diff));
	return 1;
    }

    VSUB2(diff, av2, bv2);
    if (!VNEAR_EQUAL(av2, bv2, VUNITIZE_TOL) && !VNEAR_EQUAL(av2, bv[1], VUNITIZE_TOL)) {
	bu_log("Note: obb (%g %g %g) and obb verts obb (%g %g %g) v2 differs: %0.15f %0.15f %0.15f\n", V3ARGS(av2), V3ARGS(bv2), V3ARGS(diff));
	return 1;
    }

    VSUB2(diff, av3, bv3);
    if (!VNEAR_EQUAL(av3, bv3, VUNITIZE_TOL) && !VNEAR_EQUAL(av3, bv[2], VUNITIZE_TOL)) {
	bu_log("Note: obb (%g %g %g) and obb verts obb (%g %g %g) v3 differs: %0.15f %0.15f %0.15f\n", V3ARGS(av3), V3ARGS(bv3), V3ARGS(diff));
	return 1;
    }

    return 0;
}

int
arb_diff(point_t *p1, point_t *p2)
{
    for (int i = 0; i < 8; i++) {
	int have_match = 0;
	for (int j = 0; j < 8; j++) {
	    if (VNEAR_EQUAL(p1[i], p2[j], VUNITIZE_TOL))
		have_match = 1;
	}
	if (!have_match) {
	    bu_log("Note: p1[%d] (%g %g %g) has no matching point in p2, arbs differ\n", i, V3ARGS(p1[i]));
	    return 1;
	}
    }

    return 0;
}

int
main(int UNUSED(argc), const char *argv[])
{
    bu_setprogname(argv[0]);

    int faces[12] = {
	1, 3, 2,
	1, 2, 4,
	1, 4, 3,
	3, 4, 2};
    point_t verts[5];
    VSET(verts[0], -1, -1, -1);
    VSET(verts[1], 5000, 4000, 3000);
    VSET(verts[2], 5000, 6000, 3000);
    VSET(verts[3], 5000, 6000, 5000);
    VSET(verts[4], 3000, 6000, 3000);

    for (int i = 0; i < 10; i++) {
	verts[1][X] += i;
	verts[1][Y] -= i;
	verts[2][Y] += i;
	verts[3][Z] -= i;

	bu_log("\nTest %d\n", i+1);

	{
	    point_t paabmin, paabmax;
	    if (bg_pnts_aabb(&paabmin, &paabmax, (const point_t *)verts, 5) != BRLCAD_OK)
		bu_exit(-1, "Error calculating pnts aabb\n");
	    bu_log("pnts_aabb: %g %g %g -> %g %g %g\n", V3ARGS(paabmin), V3ARGS(paabmax));
	}

	{
	    point_t pobbc;
	    vect_t pv1, pv2, pv3;
	    if (bg_pnts_obb(&pobbc, &pv1, &pv2, &pv3, (const point_t *)verts, 5) != BRLCAD_OK)
		bu_exit(-1, "Error calculating pnts obb\n");
	    bu_log("pnts_obb: %g %g %g: %g %g %g, %g %g %g, %g %g %g\n", V3ARGS(pobbc), V3ARGS(pv1), V3ARGS(pv2), V3ARGS(pv3));

	    point_t *pbverts = (point_t *)bu_calloc(8, sizeof(point_t), "p out");
	    if (bg_obb_pnts(pbverts, (const point_t *)&pobbc, (const vect_t *)&pv1, (const vect_t *)&pv2, (const vect_t *)&pv3) != BRLCAD_OK)
		bu_exit(-1, "Error calculating obb pnts\n");
	    bu_log("pnts_obb_verts: %g %g %g: %g %g %g, %g %g %g, %g %g %g\n", V3ARGS(pbverts[0]), V3ARGS(pbverts[1]), V3ARGS(pbverts[2]), V3ARGS(pbverts[3]));
	    bu_log("                %g %g %g: %g %g %g, %g %g %g, %g %g %g\n", V3ARGS(pbverts[4]), V3ARGS(pbverts[5]), V3ARGS(pbverts[6]), V3ARGS(pbverts[7]));
	}

	{
	    point_t taabmin, taabmax;
	    if (bg_trimesh_aabb(&taabmin, &taabmax, faces, 4, (const point_t *)verts, 5) != BRLCAD_OK)
		bu_exit(-1, "Error calculating trimesh aabb\n");
	    bu_log("trimesh_aabb: %g %g %g -> %g %g %g\n", V3ARGS(taabmin), V3ARGS(taabmax));
	}

	{
	    point_t tobbc;
	    vect_t tv1, tv2, tv3;
	    if (bg_trimesh_obb(&tobbc, &tv1, &tv2, &tv3, faces, 4, (const point_t *)verts, 5) != BRLCAD_OK)
		bu_exit(-1, "Error calculating trimesh obb\n");
	    bu_log("trimesh_obb: %g %g %g: %g %g %g, %g %g %g, %g %g %g\n", V3ARGS(tobbc), V3ARGS(tv1), V3ARGS(tv2), V3ARGS(tv3));

	    point_t *tbverts = (point_t *)bu_calloc(8, sizeof(point_t), "p out");
	    if (bg_obb_pnts(tbverts, (const point_t *)&tobbc, (const vect_t *)&tv1, (const vect_t *)&tv2, (const vect_t *)&tv3) != BRLCAD_OK)
		bu_exit(-1, "Error calculating obb pnts\n");
	    bu_log("trimesh_obb_verts: %g %g %g: %g %g %g, %g %g %g, %g %g %g\n", V3ARGS(tbverts[0]), V3ARGS(tbverts[1]), V3ARGS(tbverts[2]), V3ARGS(tbverts[3]));
	    bu_log("                   %g %g %g: %g %g %g, %g %g %g, %g %g %g\n", V3ARGS(tbverts[4]), V3ARGS(tbverts[5]), V3ARGS(tbverts[6]), V3ARGS(tbverts[7]));

	    point_t btobbc;
	    vect_t btv1, btv2, btv3;
	    if (bg_pnts_obb(&btobbc, &btv1, &btv2, &btv3, (const point_t *)tbverts, 8) != BRLCAD_OK)
		bu_exit(-1, "Error calculating trimesh obb verts obb\n");
	    bu_log("trimesh_obb_pnts_obb: %g %g %g: %g %g %g, %g %g %g, %g %g %g\n", V3ARGS(tobbc), V3ARGS(tv1), V3ARGS(tv2), V3ARGS(tv3));

	    if (obb_diff(tobbc, tv1, tv2, tv3, btobbc, btv1, btv2, btv3)) {
		bu_log("NOTE:  obb info differs between obb calculated from the original mesh and that calculated from the bounding box verts.\nNeed to generate verts from the obb calculated from the bounding box verts and verify that all points in the first obb vert set are present in the second (ordering may differ).\n");
		point_t *tbpntverts = (point_t *)bu_calloc(8, sizeof(point_t), "p out");
		if (bg_obb_pnts(tbpntverts, (const point_t *)&btobbc, (const vect_t *)&btv1, (const vect_t *)&btv2, (const vect_t *)&btv3) != BRLCAD_OK)
		    bu_exit(-1, "Error calculating obb pnts\n");
		bu_log("trimesh_obb_verts: %g %g %g: %g %g %g, %g %g %g, %g %g %g\n", V3ARGS(tbpntverts[0]), V3ARGS(tbpntverts[1]), V3ARGS(tbpntverts[2]), V3ARGS(tbpntverts[3]));
		bu_log("                   %g %g %g: %g %g %g, %g %g %g, %g %g %g\n", V3ARGS(tbpntverts[4]), V3ARGS(tbpntverts[5]), V3ARGS(tbpntverts[6]), V3ARGS(tbpntverts[7]));

		if (arb_diff(tbverts, tbpntverts)) {
		    bu_log("Arb points differ.  The implication of this is the original mesh obb can't be faithfully recreated from the center point and extents, which is a problem!\n");
		} else {
		    bu_log("All original arb points found.  Should verify whether either arb is twisted, but at least all points are present.\n");
		}
	    }

	}
    }

    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
