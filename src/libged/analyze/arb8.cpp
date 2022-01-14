/*                        A R B 8 . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file arb8.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include "vmath.h"

#include "bg/polygon.h"
#include "rt/arb_edit.h"
#include "rt/geom.h"
#include "ged/defines.h"
#include "../ged_private.h"
#include "./ged_analyze.h"

/* edge definition array */
static const int nedge[5][24] = {
    {0, 1, 1, 2, 2, 0, 0, 3, 3, 2, 1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},   /* ARB4 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 1, 4, 2, 4, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1},       /* ARB5 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 1, 4, 2, 5, 3, 5, 4, 5, -1, -1, -1, -1, -1, -1},         /* ARB6 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 3, 4, 1, 5, 2, 6, 4, 5, 5, 6, 4, 6, -1, -1},             /* ARB7 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 4, 5, 1, 5, 5, 6, 6, 7, 4, 7, 3, 7, 2, 6},               /* ARB8 */
};

/* ARB face printout array */
static const int prface[5][6] = {
    {123, 124, 234, 134, -111, -111},		/* ARB4 */
    {1234, 125, 235, 345, 145, -111},		/* ARB5 */
    {1234, 2365, 1564, 512, 634, -111},		/* ARB6 */
    {1234, 567, 145, 2376, 1265, 4375},		/* ARB7 */
    {1234, 5678, 1584, 2376, 1265, 4378},	/* ARB8 */
};

void
analyze_edge(struct ged *gedp, const int edge, const struct rt_arb_internal *arb,
	     const int type, row_t *row)
{
    int a = nedge[type][edge*2];
    int b = nedge[type][edge*2+1];

    if (b == -1) {
	row->nfields = 0;
	return;
    }

    row->nfields = 2;
    row->fields[0].nchars = sprintf(row->fields[0].buf, "%d%d", a + 1, b + 1);
    row->fields[1].nchars = sprintf(row->fields[1].buf, "%10.8f",
				    DIST_PNT_PNT(arb->pt[a], arb->pt[b])*gedp->dbip->dbi_base2local);
}



void
analyze_arb8(struct ged *gedp, const struct rt_db_internal *ip)
{
    int i, type;
    int cgtype;     /* COMGEOM arb type: # of vertices */
    table_t table;  /* holds table data from child functions */
    fastf_t tot_vol = 0.0, tot_area = 0.0;
    point_t center_pt = VINIT_ZERO;
    struct poly_face face = POLY_FACE_INIT_ZERO;
    struct rt_arb_internal earb;
    struct rt_arb_internal *arb = (struct rt_arb_internal *)ip->idb_ptr;
    const int arb_faces[5][24] = rt_arb_faces;
    RT_ARB_CK_MAGIC(arb);

    /* find the specific arb type, in GIFT order. */
    if ((cgtype = rt_arb_std_type(ip, &gedp->ged_wdbp->wdb_tol)) == 0) {
	bu_vls_printf(gedp->ged_result_str, "analyze_arb: bad ARB\n");
	return;
    }

    type = cgtype - 4;

    /* to get formatting correct, we need to collect the actual string
     * lengths for each field BEFORE we start printing a table (fields
     * are allowed to overflow the stated printf field width) */

    /* TABLE 1 =========================================== */
    /* analyze each face, use center point of arb for reference */
    rt_arb_centroid(&center_pt, ip);

    /* allocate pts array, maximum 4 verts per arb8 face */
    face.pts = (point_t *)bu_calloc(4, sizeof(point_t), "analyze_arb8: pts");
    /* allocate table rows, 12 rows needed for arb8 edges */
    table.rows = (row_t *)bu_calloc(12, sizeof(row_t), "analyze_arb8: rows");

    table.nrows = 0;
    for (face.npts = 0, i = 0; i < 6; face.npts = 0, i++) {
	int a, b, c, d; /* 4 indices to face vertices */

	a = arb_faces[type][i*4+0];
	b = arb_faces[type][i*4+1];
	c = arb_faces[type][i*4+2];
	d = arb_faces[type][i*4+3];

	if (a == -1) {
	    table.rows[i].nfields = 0;
	    continue;
	}

	/* find plane eqn for this face */
	if (bg_make_plane_3pnts(face.plane_eqn, arb->pt[a], arb->pt[b], arb->pt[c], &gedp->ged_wdbp->wdb_tol) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "| %d%d%d%d |         ***NOT A PLANE***                                          |\n",
			  a+1, b+1, c+1, d+1);
	    /* this row has 1 special fields */
	    table.rows[i].nfields = NOT_A_PLANE;
	    continue;
	}

	ADD_PT(face, arb->pt[a]);
	ADD_PT(face, arb->pt[b]);
	ADD_PT(face, arb->pt[c]);
	ADD_PT(face, arb->pt[d]);

	/* The plane equations returned by bg_make_plane_3pnts above do
	 * not necessarily point outward. Use the reference center
	 * point for the arb and reverse direction for any errant planes.
	 * This corrects the output rotation, fallback angles so that
	 * they always give the outward pointing normal vector. */
	if (DIST_PNT_PLANE(center_pt, face.plane_eqn) > 0.0) {
	    HREVERSE(face.plane_eqn, face.plane_eqn);
	}

	snprintf(face.label, sizeof(face.label), "%d", prface[type][i]);

	analyze_poly_face(gedp, &face, &(table.rows[i]));
	tot_area += face.area;
	table.nrows++;
    }

    /* and print it */
    print_faces_table(gedp, &table);

    /* TABLE 2 =========================================== */
    /* analyze each edge */

    /* blank line following previous table */
    bu_vls_printf(gedp->ged_result_str, "\n");

    /* set up the records for arb4's and arb6's */
    earb = *arb; /* struct copy */
    if (cgtype == 4) {
	VMOVE(earb.pt[3], earb.pt[4]);
    } else if (cgtype == 6) {
	VMOVE(earb.pt[5], earb.pt[6]);
    }

    table.nrows = 0;
    for (i = 0; i < 12; i++) {
	analyze_edge(gedp, i, &earb, type, &(table.rows[i]));
	if (nedge[type][i*2] == -1) {
	    break;
	}
	table.nrows += 1;
    }

    print_edges_table(gedp, &table);

    /* TABLE 3 =========================================== */
    /* find the volume - break arb8 into 6 arb4s */

    if (OBJ[ID_ARB8].ft_volume)
	OBJ[ID_ARB8].ft_volume(&tot_vol, ip);

    print_volume_table(gedp,
		       tot_vol
		       * gedp->dbip->dbi_base2local
		       * gedp->dbip->dbi_base2local
		       * gedp->dbip->dbi_base2local,
		       tot_area
		       * gedp->dbip->dbi_base2local
		       * gedp->dbip->dbi_base2local,
		       tot_vol/GALLONS_TO_MM3
	);

    bu_free((char *)face.pts, "analyze_arb8: pts");
    bu_free((char *)table.rows, "analyze_arb8: rows");
}
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
