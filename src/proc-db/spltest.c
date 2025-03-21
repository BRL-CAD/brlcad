/*                       S P L T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
/** @file proc-db/spltest.c
 * spltest.c
 *
 * Simple spline test.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "nmg.h"
#include "bu/app.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"


#define SSET(_fp, _srf, _col, _row, _val) { \
	_fp = &_srf->ctl_points[((_col*_srf->s_size[1])+_row)*3]; \
	VMOVE(_fp, _val); \
    }


void
make_face(struct rt_nurb_internal *s, fastf_t *a, fastf_t *b, fastf_t *c, fastf_t *d, int order)
{
    int i;
    int ki;
    int cur_kv;
    int interior_pts = 2;
    fastf_t *fp = NULL;
    struct face_g_snurb *srf = NULL;

    srf = nmg_nurb_new_snurb(order, order,
			    2*order+interior_pts, 2*order+interior_pts,	/* # knots */
			    2+interior_pts, 2+interior_pts,
			    RT_NURB_MAKE_PT_TYPE(3, RT_NURB_PT_XYZ, RT_NURB_PT_NONRAT));

    /* Build both knot vectors */

    /* current knot value */
    cur_kv = 0;
    /* current knot index */
    ki = 0;

    for (i=0; i<order; i++, ki++) {
	srf->u.knots[ki] = srf->v.knots[ki] = cur_kv;
    }
    cur_kv++;
    for (i=0; i<interior_pts; i++, ki++) {
	srf->u.knots[ki] = srf->v.knots[ki] = cur_kv++;
    }
    for (i=0; i<order; i++, ki++) {
	srf->u.knots[ki] = srf->v.knots[ki] = cur_kv;
    }

    nmg_nurb_pr_kv(&srf->u);

    /*
     * The control mesh is stored in row-major order.
     */

    SSET(fp, srf, 0, 0, a);
    SSET(fp, srf, 0, 1, b);
    SSET(fp, srf, 1, 0, d);
    SSET(fp, srf, 1, 1, c);

    s->srfs[s->nsrf++] = srf;
}


void
printusage(char *argv[]) {
    bu_log("Usage: %s [filename, default to spltest.g]\n", argv[0]);
}


int
main(int argc, char *argv[])
{
    point_t a, b, c, d;
    struct rt_wdb *fp;
    struct rt_nurb_internal *si;
    char *filename = "spltest.g";
    int helpflag;

    bu_setprogname(argv[0]);

    if (argc < 1 || argc > 2) {
    	printusage(argv);
	bu_exit(1,NULL);
    }

    helpflag = (argc == 2 && ( BU_STR_EQUAL(argv[1],"-h") || BU_STR_EQUAL(argv[1],"-?")));
    if (helpflag) {
    	printusage(argv);
	if (helpflag)
		bu_exit(1,NULL);
    }

    if (argc == 2)
	filename = argv[1];

    bu_log("Writing out geometry to file [%s] ...", filename);

    if ((fp = wdb_fopen(filename)) == NULL) {
	perror("unable to open geometry database for writing");
	bu_exit(1, "unable to open new database [%s]\n", filename);
    }

    mk_id(fp, "Mike's Spline Test");

    VSET(a,  0,  0,  0);
    VSET(b, 10,  0,  -5);
    VSET(c, 10, 10,  10);
    VSET(d,  0, 10,  0);

    BU_ALLOC(si, struct rt_nurb_internal);
    si->magic = RT_NURB_INTERNAL_MAGIC;
    si->nsrf = 0;
    si->srfs = (struct face_g_snurb **)bu_malloc(sizeof(struct face_g_snurb *)*100, "allocate snurb ptrs");

    make_face(si, a, b, c, d, 2);

    /* wdb_export */
    mk_export_fwrite(fp, "spltest", (void *)si, ID_BSPLINE);

    db_close(fp->dbip);
    bu_log(" done.\n");

    return 0;
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
