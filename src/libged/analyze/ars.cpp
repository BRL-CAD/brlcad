/*                        A R S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file ars.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include "vmath.h"

#include "rt/geom.h"
#include "ged/defines.h"
#include "../ged_private.h"
#include "./ged_analyze.h"

#define ARS_PT(ii, jj) (&arip->curves[i+(ii)][(j+(jj))*ELEMENTS_PER_VECT])

void
analyze_ars(struct ged *gedp, const struct rt_db_internal *ip)
{
    size_t i, j, k;
    size_t nfaces = 0;
    fastf_t tot_area = 0.0, tot_vol = 0.0;
    table_t table;
    plane_t old_plane = HINIT_ZERO;
    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
    struct poly_face face = POLY_FACE_INIT_ZERO;
    struct rt_ars_internal *arip = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(arip);

    /* allocate pts array, max 3 pts per triangular face */
    face.pts = (point_t *)bu_calloc(3, sizeof(point_t), "analyze_ars: pts");
    /* allocate table rows, probably overestimating the number of rows needed */
    table.rows = (row_t *)bu_calloc((arip->ncurves - 1) * 2 * arip->pts_per_curve, sizeof(row_t), "analyze_ars: rows");

    k = arip->pts_per_curve - 2;
    for (i = 0; i < arip->ncurves - 1; i++) {
	int double_ended = k != 1 && VEQUAL(&arip->curves[i][ELEMENTS_PER_VECT], &arip->curves[i][k * ELEMENTS_PER_VECT]);

	for (j = 0; j < arip->pts_per_curve; j++) {
	    vect_t tmp;

	    if (double_ended && i != 0 && (j == 0 || j == k || j == arip->pts_per_curve - 1)) continue;

	    /* first triangular face, make sure it's not a duplicate */
	    if (bn_make_plane_3pnts(face.plane_eqn, ARS_PT(0, 0), ARS_PT(1, 1), ARS_PT(0, 1), &gedp->ged_wdbp->wdb_tol) == 0
		&& !HEQUAL(old_plane, face.plane_eqn)) {
		HMOVE(old_plane, face.plane_eqn);
		ADD_PT(face, ARS_PT(0, 1));
		ADD_PT(face, ARS_PT(0, 0));
		ADD_PT(face, ARS_PT(1, 1));

		bu_vls_sprintf(&tmpstr, "%zu%zu", i, j);
		snprintf(face.label, sizeof(face.label), "%s", bu_vls_addr(&tmpstr));

		/* surface area */
		analyze_poly_face(gedp, &face, &(table.rows[nfaces]));
		tot_area += face.area;

		/* volume */
		VSCALE(tmp, face.plane_eqn, face.area);
		tot_vol += fabs(VDOT(face.pts[0], tmp));

		face.npts = 0;
		nfaces++;
	    }

	    /* second triangular face, make sure it's not a duplicate */
	    if (bn_make_plane_3pnts(face.plane_eqn, ARS_PT(1, 0), ARS_PT(1, 1), ARS_PT(0, 0), &gedp->ged_wdbp->wdb_tol) == 0
		&& !HEQUAL(old_plane, face.plane_eqn)) {
		HMOVE(old_plane, face.plane_eqn);
		ADD_PT(face, ARS_PT(1, 0));
		ADD_PT(face, ARS_PT(0, 0));
		ADD_PT(face, ARS_PT(1, 1));

		bu_vls_sprintf(&tmpstr, "%zu%zu", i, j);
		snprintf(face.label, sizeof(face.label), "%s", bu_vls_addr(&tmpstr));

		analyze_poly_face(gedp, &face, &table.rows[nfaces]);
		tot_area += face.area;

		VSCALE(tmp, face.plane_eqn, face.area);
		tot_vol += fabs(VDOT(face.pts[0], tmp));

		face.npts = 0;
		nfaces++;
	    }
	}
    }
    tot_vol /= 3.0;
    table.nrows = nfaces;

    print_faces_table(gedp, &table);
    print_volume_table(gedp,
		       tot_vol
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       tot_area
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       tot_vol/GALLONS_TO_MM3
	);

    bu_free((char *)face.pts, "analyze_ars: pts");
    bu_free((char *)table.rows, "analyze_ars: rows");
    bu_vls_free(&tmpstr);
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
