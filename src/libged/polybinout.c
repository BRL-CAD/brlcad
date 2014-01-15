/*                        P O L Y B I N O U T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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

#include "common.h"

#include <string.h>
#include <math.h>
#include <signal.h>
#include "bio.h"

#include "vmath.h"
#include "mater.h"
#include "dg.h"
#include "solid.h"

#include "ged.h"

/* When finalized, this stuff belongs in a header file of its own */
struct polygon_header {
    uint32_t magic;		/* magic number */
    int ident;			/* identification number */
    int interior;		/* >0 => interior loop, gives ident # of exterior loop */
    vect_t normal;			/* surface normal */
    unsigned char color[3];	/* Color from containing region */
    int npts;			/* number of points */
};
#define POLYGON_HEADER_MAGIC 0x8623bad2


struct bu_structparse polygon_desc[] = {
    {"%d", 1, "magic", bu_offsetof(struct polygon_header, magic), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "ident", bu_offsetof(struct polygon_header, ident), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "interior", bu_offsetof(struct polygon_header, interior), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "normal", bu_offsetof(struct polygon_header, normal), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%c", 3, "color", bu_offsetof(struct polygon_header, color), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "npts", bu_offsetof(struct polygon_header, npts), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


struct bu_structparse vertex_desc[] = {
    {"%f", 3, "vertex", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/*
 * Interface for writing binary polygons that represent
 * the current (evaluated) view.
 *
 * Usage:  polybinout file
 */
int
ged_polybinout(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid *sp;
    struct bn_vlist *vp;
    FILE *fp;
    int pno = 1;
    struct polygon_header ph;
#define MAX_VERTS 10000
    vect_t verts[MAX_VERTS];
    struct bu_external obuf;
    static const char *usage = "Usage:  polybinout file";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    ph.npts = 0;

    if ((fp = fopen(argv[1], "w")) == NULL) {
	perror(argv[1]);
	bu_vls_printf(gedp->ged_result_str, "%s: Unable to open %s for writing\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    gdlp = BU_LIST_NEXT(ged_display_list, gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		int i;
		int nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;
		for (i = 0; i < nused; i++, cmd++, pt++) {
		    /* For each polygon, spit it out.  Ignore vectors */
		    switch (*cmd) {
			case BN_VLIST_LINE_MOVE:
			    /* Move, start line */
			    break;
			case BN_VLIST_LINE_DRAW:
			    /* Draw line */
			    break;
			case BN_VLIST_POLY_VERTNORM:
			case BN_VLIST_TRI_VERTNORM:
			    /* Ignore per-vertex normal */
			    break;
			case BN_VLIST_POLY_START:
			case BN_VLIST_TRI_START:
			    /* Start poly marker & normal, followed by POLY_MOVE */
			    ph.magic = POLYGON_HEADER_MAGIC;
			    ph.ident = pno++;
			    ph.interior = 0;
			    memcpy(ph.color, sp->s_basecolor, 3);
			    ph.npts = 0;
			    /* Set surface normal (vl_pnt points outward) */
			    VMOVE(ph.normal, *pt);
			    break;
			case BN_VLIST_POLY_MOVE:
			case BN_VLIST_TRI_MOVE:
			    /* Start of polygon, has first point */
			    /* fall through to... */
			case BN_VLIST_POLY_DRAW:
			case BN_VLIST_TRI_DRAW:
			    /* Polygon Draw */
			    if (ph.npts >= MAX_VERTS) {
				bu_vls_printf(gedp->ged_result_str, "excess vertex skipped\n");
				break;
			    }
			    VMOVE(verts[ph.npts], *pt);
			    ph.npts++;
			    break;
			case BN_VLIST_POLY_END:
			case BN_VLIST_TRI_END:
			    /*
			     * End Polygon.  Point given is repeat of
			     * first one, ignore it.
			     * XXX note:  if poly_markers was not set,
			     * XXX poly will end with next POLY_MOVE.
			     */
			    if (ph.npts < 3) {
				bu_vls_printf(gedp->ged_result_str, "polygon with %d points discarded\n", ph.npts);
				break;
			    }
			    if (bu_struct_export(&obuf, (genptr_t)&ph, polygon_desc) < 0) {
				bu_vls_printf(gedp->ged_result_str, "header export error\n");
				break;
			    }
			    if (bu_struct_put(fp, &obuf) != obuf.ext_nbytes) {
				perror("bu_struct_put");
				break;
			    }
			    bu_free_external(&obuf);
			    /* Now export the vertices */
			    vertex_desc[0].sp_count = ph.npts * 3;
			    if (bu_struct_export(&obuf, (genptr_t)verts, vertex_desc) < 0) {
				bu_vls_printf(gedp->ged_result_str, "vertex export error\n");
				break;
			    }
			    if (bu_struct_put(fp, &obuf) != obuf.ext_nbytes) {
				perror("bu_struct_wrap_buf");
				break;
			    }
			    bu_free_external(&obuf);
			    ph.npts = 0;		/* sanity */
			    break;
		    }
		}
	    }
	}

	gdlp = next_gdlp;
    }

    fclose(fp);
    return GED_OK;
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
