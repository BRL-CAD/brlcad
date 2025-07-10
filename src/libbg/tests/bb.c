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
