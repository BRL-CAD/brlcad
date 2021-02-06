/*                        A R B N . C P P
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
/** @file arbn.cpp
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

void
analyze_arbn(struct ged *gedp, const struct rt_db_internal *ip)
{
    size_t i;
    fastf_t tot_vol = 0.0, tot_area = 0.0;
    table_t table;
    struct poly_face *faces;
    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
    struct rt_arbn_internal *aip = (struct rt_arbn_internal *)ip->idb_ptr;
    size_t *npts = (size_t *)bu_calloc(aip->neqn, sizeof(size_t), "analyze_arbn: npts");
    point_t **tmp_pts = (point_t **)bu_calloc(aip->neqn, sizeof(point_t *), "analyze_arbn: tmp_pts");
    plane_t *eqs= (plane_t *)bu_calloc(aip->neqn, sizeof(plane_t), "analyze_arbn: eqs");

    /* allocate array of face structs */
    faces = (struct poly_face *)bu_calloc(aip->neqn, sizeof(struct poly_face), "analyze_arbn: faces");
    for (i = 0; i < aip->neqn; i++) {
	HMOVE(faces[i].plane_eqn, aip->eqn[i]);
	VUNITIZE(faces[i].plane_eqn);
	/* allocate array of pt structs, max number of verts per faces = (# of faces) - 1 */
	faces[i].pts = (point_t *)bu_calloc(aip->neqn - 1, sizeof(point_t), "analyze_arbn: pts");
	tmp_pts[i] = faces[i].pts;
	HMOVE(eqs[i], faces[i].plane_eqn);
    }
    /* allocate table rows, 1 row per plane eqn */
    table.rows = (row_t *)bu_calloc(aip->neqn, sizeof(row_t), "analyze_arbn: rows");
    table.nrows = aip->neqn;

    bg_3d_polygon_make_pnts_planes(npts, tmp_pts, aip->neqn, (const plane_t *)eqs);

    for (i = 0; i < aip->neqn; i++) {
	vect_t tmp;
	bu_vls_sprintf(&tmpstr, "%4zu", i);
	snprintf(faces[i].label, sizeof(faces[i].label), "%s", bu_vls_addr(&tmpstr));

	faces[i].npts = npts[i];

	/* calculate surface area */
	analyze_poly_face(gedp, &faces[i], &table.rows[i]);
	tot_area += faces[i].area;

	/* calculate volume */
	VSCALE(tmp, faces[i].plane_eqn, faces[i].area);
	tot_vol += VDOT(faces[i].pts[0], tmp);
    }
    tot_vol /= 3.0;

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

    for (i = 0; i < aip->neqn; i++) {
	bu_free((char *)faces[i].pts, "analyze_arbn: pts");
    }
    bu_free((char *)faces, "analyze_arbn: faces");
    bu_free((char *)table.rows, "analyze_arbn: rows");
    bu_free((char *)tmp_pts, "analyze_arbn: tmp_pts");
    bu_free((char *)npts, "analyze_arbn: npts");
    bu_free((char *)eqs, "analyze_arbn: eqs");
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
