/*                        P O L Y I F . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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

#include "./mged.h"
#include "./mged_dm.h"


/* When finalized, this stuff belongs in a header file of its own */
struct polygon_header {
    int magic;			/* magic number */
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
    {"%f", 3, "normal", bu_offsetofarray(struct polygon_header, normal), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%c", 3, "color", bu_offsetofarray(struct polygon_header, color), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "npts", bu_offsetof(struct polygon_header, npts), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


struct bu_structparse vertex_desc[] = {
    {"%f", 3, "vertex", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/*
 * F _ P O L Y B I N O U T
 *
 * Experimental interface for writing binary polygons that represent
 * the current (evaluated) view.
 *
 * Usage:  polybinout file
 */
int
f_polybinout(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
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
    int need_normal = 0;
    struct bu_external obuf;

    if (argc < 2 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help polybinout");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((fp = fopen(argv[1], "w")) == NULL) {
	perror(argv[1]);
	return TCL_ERROR;
    }

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
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
			    /* Ignore per-vertex normal */
			    break;
			case BN_VLIST_POLY_START:
			    /* Start poly marker & normal, followed by POLY_MOVE */
			    ph.magic = POLYGON_HEADER_MAGIC;
			    ph.ident = pno++;
			    ph.interior = 0;
			    memcpy(ph.color, sp->s_basecolor, 3);
			    ph.npts = 0;
			    /* Set surface normal (vl_pnt points outward) */
			    VMOVE(ph.normal, *pt);
			    need_normal = 0;
			    break;
			case BN_VLIST_POLY_MOVE:
			    /* Start of polygon, has first point */
			    /* fall through to... */
			case BN_VLIST_POLY_DRAW:
			    /* Polygon Draw */
			    if (ph.npts >= MAX_VERTS) {
				Tcl_AppendResult(interp, "excess vertex skipped\n",
						 (char *)NULL);
				break;
			    }
			    VMOVE(verts[ph.npts], *pt);
			    ph.npts++;
			    break;
			case BN_VLIST_POLY_END:
			    /*
			     * End Polygon.  Point given is repeat of
			     * first one, ignore it.
			     * XXX note:  if poly_markers was not set,
			     * XXX poly will end with next POLY_MOVE.
			     */
			    if (ph.npts < 3) {
				struct bu_vls tmp_vls;

				bu_vls_init(&tmp_vls);
				bu_vls_printf(&tmp_vls, "polygon with %d points discarded\n",
					      ph.npts);
				Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
				bu_vls_free(&tmp_vls);
				break;
			    }
			    if (need_normal) {
				vect_t e1, e2;
				VSUB2(e1, verts[0], verts[1]);
				VSUB2(e2, verts[0], verts[2]);
				VCROSS(ph.normal, e1, e2);
			    }
			    if (bu_struct_export(&obuf, (genptr_t)&ph, polygon_desc) < 0) {
				Tcl_AppendResult(interp, "header export error\n", (char *)NULL);
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
				Tcl_AppendResult(interp, "vertex export error\n", (char *)NULL);
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
    return TCL_OK;
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
