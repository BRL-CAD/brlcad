/*                         D M - P S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
/** @file libdm/dm-ps.c
 *
 * A useful hack to allow GED to generate
 * PostScript files that not only contain the drawn objects, but
 * also contain the faceplate display as well.
 * Mostly, used for making viewgraphs and photographs
 * of an editing session.
 *
 */

#include "common.h"

#include <string.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* for struct timeval */
#endif

#include "tcl.h"

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

#include "dm.h"
#include "./dm-ps.h"
#include "../null/dm-Null.h"

#include "bv/defines.h"

#include "../include/private.h"

#define EPSILON 0.0001

struct ps_mvars {
    int zclip;
    double bound;
    int boundFlag;
};

/* Display Manager package interface */

#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

char ps_usage[] = "Usage: ps [-f font] [-t title] [-c creator] [-s size in inches]\
 [-l linewidth] file";

struct ps_vars head_ps_vars;
static mat_t psmat;
static mat_t disp_mat;
static mat_t mod_mat;


static int ps_close(struct dm *dmp);

/*
 * Open the output file, and output the PostScript prolog.
 *
 */
struct dm *
ps_open(void *UNUSED(ctx), void *vinterp, int UNUSED(argc), const char *argv[])
{
    static int count = 0;
    struct dm *dmp;
    Tcl_Obj *obj;
    Tcl_Interp *interp = (Tcl_Interp *)vinterp;

    if (!interp)
	return NULL;

    BU_ALLOC(dmp, struct dm);
    dmp->magic = DM_MAGIC;
    dmp->start_time = 0;

    BU_ALLOC(dmp->i, struct dm_impl);

    *dmp->i = *dm_ps.i;  /* struct copy */
    dmp->i->dm_interp = interp;

    BU_ALLOC(dmp->i->dm_vars.priv_vars, struct ps_vars);

    BU_ALLOC(dmp->i->m_vars, struct ps_mvars);
    struct ps_mvars *m_vars = (struct ps_mvars *)dmp->i->m_vars;
    m_vars->zclip = 0;
    m_vars->bound = PLOTBOUND;
    m_vars->boundFlag = 1;

    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_printf(&dmp->i->dm_pathName, ".dm_ps%d", count++);
    bu_vls_printf(&dmp->i->dm_tkName, "dm_ps%d", count++);

    bu_vls_init(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->fname);
    bu_vls_init(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->font);
    bu_vls_init(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->title);
    bu_vls_init(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->creator);

    /* set defaults */
    bu_vls_strcpy(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->font, "Courier");
    bu_vls_strcpy(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->title, "No Title");
    bu_vls_strcpy(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->creator, "LIBDM dm-ps");
    ((struct ps_vars *)dmp->i->dm_vars.priv_vars)->scale = 0.0791;
    ((struct ps_vars *)dmp->i->dm_vars.priv_vars)->linewidth = 4;

    /* skip first argument */
    ++argv;

    /* Process any options */
    while (argv[0] != (char *)0 && argv[0][0] == '-') {
	switch (argv[0][1]) {
	    case 'f':               /* font */
		if (argv[0][2] != '\0')
		    bu_vls_strcpy(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->font, &argv[0][2]);
		else {
		    argv++;
		    if (argv[0] == (char *)0 || argv[0][0] == '-') {
			Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
			(void)ps_close(dmp);

			Tcl_SetObjResult(interp, obj);
			return DM_NULL;
		    } else
			bu_vls_strcpy(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->font, &argv[0][0]);
		}
		break;
	    case 't':               /* title */
		if (argv[0][2] != '\0')
		    bu_vls_strcpy(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->title, &argv[0][2]);
		else {
		    argv++;
		    if (argv[0] == (char *)0 || argv[0][0] == '-') {
			Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
			(void)ps_close(dmp);

			Tcl_SetObjResult(interp, obj);
			return DM_NULL;
		    } else
			bu_vls_strcpy(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->title, &argv[0][0]);
		}
		break;
	    case 'c':               /* creator */
		if (argv[0][2] != '\0')
		    bu_vls_strcpy(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->creator, &argv[0][2]);
		else {
		    argv++;
		    if (argv[0] == (char *)0 || argv[0][0] == '-') {
			Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
			(void)ps_close(dmp);

			Tcl_SetObjResult(interp, obj);
			return DM_NULL;
		    } else
			bu_vls_strcpy(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->creator, &argv[0][0]);
		}
		break;
	    case 's':               /* size in inches */
		{
		    double size;

		    if (argv[0][2] != '\0')
			sscanf(&argv[0][2], "%lf", &size);
		    else {
			argv++;
			if (argv[0] == (char *)0 || argv[0][0] == '-') {
			    Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
			    (void)ps_close(dmp);

			    Tcl_SetObjResult(interp, obj);
			    return DM_NULL;
			} else
			    sscanf(&argv[0][0], "%lf", &size);
		    }

		    ((struct ps_vars *)dmp->i->dm_vars.priv_vars)->scale = size * 0.017578125;
		}
		break;
	    case 'l':               /* line width */
		if (argv[0][2] != '\0')
		    sscanf(&argv[0][2], "%d", &((struct ps_vars *)dmp->i->dm_vars.priv_vars)->linewidth);
		else {
		    argv++;
		    if (argv[0] == (char *)0 || argv[0][0] == '-') {
			Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
			(void)ps_close(dmp);

			Tcl_SetObjResult(interp, obj);
			return DM_NULL;
		    } else
			sscanf(&argv[0][0], "%d", &((struct ps_vars *)dmp->i->dm_vars.priv_vars)->linewidth);
		}
		break;
	    case 'z':
		m_vars->zclip = 1;
		break;
	    default:
		Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
		(void)ps_close(dmp);

		Tcl_SetObjResult(interp, obj);
		return DM_NULL;
	}
	argv++;
    }

    if (argv[0] == (char *)0) {
	Tcl_AppendStringsToObj(obj, "no filename specified\n", (char *)NULL);
	(void)ps_close(dmp);

	Tcl_SetObjResult(interp, obj);
	return DM_NULL;
    }

    bu_vls_strcpy(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->fname, argv[0]);

    if ((((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp =
	 fopen(bu_vls_addr(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->fname), "wb")) == NULL) {
	Tcl_AppendStringsToObj(obj, "f_ps: Error opening file - ",
			       ((struct ps_vars *)dmp->i->dm_vars.priv_vars)->fname,
			       "\n", (char *)NULL);
	(void)ps_close(dmp);

	Tcl_SetObjResult(interp, obj);
	return DM_NULL;
    }

    setbuf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp,
	   ((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ttybuf);
    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "%%!PS-Adobe-1.0\n\
%%begin(plot)\n\
%%%%DocumentFonts:  %s\n",
	    bu_vls_addr(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->font));

    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "%%%%Title: %s\n",
	    bu_vls_addr(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->title));

    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "\
%%%%Creator: %s\n\
%%%%BoundingBox: 0 0 324 324	%% 4.5in square, for TeX\n\
%%%%EndComments\n\
\n",
	    bu_vls_addr(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->creator));

    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "\
%d setlinewidth\n\
\n\
%% Sizes, made functions to avoid scaling if not needed\n\
/FntH /%s findfont 80 scalefont def\n\
/DFntL { /FntL /%s findfont 73.4 scalefont def } def\n\
/DFntM { /FntM /%s findfont 50.2 scalefont def } def\n\
/DFntS { /FntS /%s findfont 44 scalefont def } def\n\
",
	    ((struct ps_vars *)dmp->i->dm_vars.priv_vars)->linewidth,
	    bu_vls_addr(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->font),
	    bu_vls_addr(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->font),
	    bu_vls_addr(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->font),
	    bu_vls_addr(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->font));

    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "\
\n\
%% line styles\n\
/NV { [] 0 setdash } def	%% normal vectors\n\
/DV { [8] 0 setdash } def	%% dotted vectors\n\
/DDV { [8 8 32 8] 0 setdash } def	%% dot-dash vectors\n\
/SDV { [32 8] 0 setdash } def	%% short-dash vectors\n\
/LDV { [64 8] 0 setdash } def	%% long-dash vectors\n\
\n\
/NEWPG {\n\
	%f %f scale	%% 0-4096 to 324 units (4.5 inches)\n\
} def\n\
\n\
FntH setfont\n\
NEWPG\n\
",
	    ((struct ps_vars *)dmp->i->dm_vars.priv_vars)->scale,
	    ((struct ps_vars *)dmp->i->dm_vars.priv_vars)->scale);

    MAT_IDN(mod_mat);
    MAT_IDN(disp_mat);
    MAT_COPY(psmat, mod_mat);

    Tcl_SetObjResult(interp, obj);
    return dmp;
}



/*
 * Gracefully release the display.
 */
static int
ps_close(struct dm *dmp)
{
    if (!((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp)
	return BRLCAD_ERROR;

    fputs("%end(plot)\n", ((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp);
    (void)fclose(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp);

    bu_vls_free(&dmp->i->dm_pathName);
    bu_vls_free(&dmp->i->dm_tkName);
    bu_vls_free(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->fname);
    bu_vls_free(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->font);
    bu_vls_free(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->title);
    bu_vls_free(&((struct ps_vars *)dmp->i->dm_vars.priv_vars)->creator);
    bu_free((void *)dmp->i->dm_vars.priv_vars, "ps_close: ps_vars");
    bu_free((void *)dmp->i, "ps_close: dmp impl");
    bu_free((void *)dmp, "ps_close: dmp");

    return BRLCAD_OK;
}


int
ps_viable(const char *UNUSED(dpy_string))
{
    return 1;
}


/*
 * There are global variables which are parameters to this routine.
 */
static int
ps_drawBegin(struct dm *dmp)
{
    if (!dmp)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


static int
ps_drawEnd(struct dm *dmp)
{
    if (!dmp)
	return BRLCAD_ERROR;

    if (!((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp)
	return BRLCAD_ERROR;

    fputs("% showpage	% uncomment to use raw file\n",
	  ((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp);
    (void)fflush(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp);

    return BRLCAD_OK;
}


/*
 * Load a new transformation matrix.  This will be followed by
 * many calls to ps_draw().
 */
static int
ps_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    Tcl_Obj *obj;
    Tcl_Interp *interp = (Tcl_Interp *)dmp->i->dm_interp;

    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    if (((struct ps_vars *)dmp->i->dm_vars.priv_vars)->debug) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	Tcl_AppendStringsToObj(obj, "ps_loadMatrix()\n", (char *)NULL);

	bu_vls_printf(&tmp_vls, "which eye = %d\t", which_eye);
	bu_vls_printf(&tmp_vls, "transformation matrix = \n");
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);

	Tcl_AppendStringsToObj(obj, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    MAT_COPY(mod_mat, mat);
    MAT_COPY(psmat, mat);

    Tcl_SetObjResult(interp, obj);
    return BRLCAD_OK;
}


/* ARGSUSED */
static int
ps_drawVList(struct dm *dmp, struct bv_vlist *vp)
{
    static vect_t last;
    struct bv_vlist *tvp;
    point_t *pt_prev=NULL;
    fastf_t dist_prev=1.0;
    fastf_t dist;
    fastf_t delta;
    point_t tlate;
    int useful = 0;

    if (!((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp)
	return BRLCAD_ERROR;

    /* delta is used in clipping to insure clipped endpoint is slightly
     * in front of eye plane (perspective mode only).
     * This value is a SWAG that seems to work OK.
     */
    delta = psmat[15]*0.0001;
    if (delta < 0.0)
	delta = -delta;
    if (delta < SQRT_SMALL_FASTF)
	delta = SQRT_SMALL_FASTF;

    for (BU_LIST_FOR(tvp, bv_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    static vect_t start, fin;
	    switch (*cmd) {
		case BV_VLIST_POLY_START:
		case BV_VLIST_POLY_VERTNORM:
		case BV_VLIST_TRI_START:
		case BV_VLIST_TRI_VERTNORM:
		    continue;
		case BV_VLIST_MODEL_MAT:
		    MAT_COPY(psmat, mod_mat);
		    continue;
		case BV_VLIST_DISPLAY_MAT:
		    MAT4X3PNT(tlate, (mod_mat), *pt);
		    disp_mat[3] = tlate[0];
		    disp_mat[7] = tlate[1];
		    disp_mat[11] = tlate[2];
		    MAT_COPY(psmat, disp_mat);
		    continue;
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_LINE_MOVE:
		case BV_VLIST_TRI_MOVE:
		    /* Move, not draw */
		    if (dmp->i->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &psmat[12]) + psmat[15];
			if (dist <= 0.0) {
			    pt_prev = pt;
			    dist_prev = dist;
			    continue;
			} else {
			    MAT4X3PNT(last, psmat, *pt);
			    dist_prev = dist;
			    pt_prev = pt;
			}
		    } else
			MAT4X3PNT(last, psmat, *pt);
		    continue;
		case BV_VLIST_POLY_DRAW:
		case BV_VLIST_POLY_END:
		case BV_VLIST_LINE_DRAW:
		case BV_VLIST_TRI_DRAW:
		case BV_VLIST_TRI_END:
		    /* draw */
		    if (dmp->i->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &psmat[12]) + psmat[15];
			if (dist <= 0.0) {
			    if (dist_prev <= 0.0) {
				/* nothing to plot */
				dist_prev = dist;
				pt_prev = pt;
				continue;
			    } else {
				if (pt_prev) {
				fastf_t alpha;
				vect_t diff;
				point_t tmp_pt;

				/* clip this end */
				VSUB2(diff, *pt, *pt_prev);
				alpha = (dist_prev - delta) / (dist_prev - dist);
				VJOIN1(tmp_pt, *pt_prev, alpha, diff);
				MAT4X3PNT(fin, psmat, tmp_pt);
				}
			    }
			} else {
			    if (dist_prev <= 0.0) {
				if (pt_prev) {
				fastf_t alpha;
				vect_t diff;
				point_t tmp_pt;

				/* clip other end */
				VSUB2(diff, *pt, *pt_prev);
				alpha = (-dist_prev + delta) / (dist - dist_prev);
				VJOIN1(tmp_pt, *pt_prev, alpha, diff);
				MAT4X3PNT(last, psmat, tmp_pt);
				MAT4X3PNT(fin, psmat, *pt);
				}
			    } else {
				MAT4X3PNT(fin, psmat, *pt);
			    }
			}
		    } else
			MAT4X3PNT(fin, psmat, *pt);
		    VMOVE(start, last);
		    VMOVE(last, fin);
		    break;
	    }

	    if (vclip(start, fin, dmp->i->dm_clipmin,
		      dmp->i->dm_clipmax) == 0)
		continue;

	    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp,
		    "newpath %d %d moveto %d %d lineto stroke\n",
		    GED_TO_PS(start[0] * 2047),
		    GED_TO_PS(start[1] * 2047),
		    GED_TO_PS(fin[0] * 2047),
		    GED_TO_PS(fin[1] * 2047));
	    useful = 1;
	}
    }

    if (useful)
	return BRLCAD_OK;

    return BRLCAD_ERROR;
}


/* ARGSUSED */
static int
ps_draw(struct dm *dmp, struct bv_vlist *(*callback_function)(void *), void **data)
{
    struct bv_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bv_vlist *)data;
	    ps_drawVList(dmp, vp);
	}
    } else {
	if (!data) {
	    return BRLCAD_ERROR;
	} else {
	    (void)callback_function(data);
	}
    }
    return BRLCAD_OK;
}


static int
ps_hud_begin(struct dm *dmp)
{
    if (!dmp)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

static int
ps_hud_end(struct dm *dmp)
{
    if (!dmp)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}



/*
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
static int
ps_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int UNUSED(use_aspect))
{
    int sx, sy;

    if (!((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp)
	return BRLCAD_ERROR;

    switch (size) {
	default:
	    /* Smallest */
	    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "DFntS ");
	    break;
	case 1:
	    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "DFntM ");
	    break;
	case 2:
	    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "DFntL ");
	    break;
	case 3:
	    /* Largest */
	    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "FntH ");
	    break;
    }

    sx = x * 2047.0 + 2048;
    sy = y * 2047.0 + 2048;
    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp,
	    "(%s) %d %d moveto show\n", str, sx, sy);

    return BRLCAD_OK;
}


static int
ps_drawLine2D(struct dm *dmp, fastf_t xpos1, fastf_t ypos1, fastf_t xpos2, fastf_t ypos2)
{
    int sx1, sy1;
    int sx2, sy2;

    if (!((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp)
	return BRLCAD_ERROR;

    sx1 = xpos1 * 2047.0 + 2048;
    sx2 = xpos2 * 2047.0 + 2048;
    sy1 = ypos1 * 2047.0 + 2048;
    sy2 = ypos2 * 2047.0 + 2048;

    fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp,
	    "newpath %d %d moveto %d %d lineto stroke\n",
	    sx1, sy1, sx2, sy2);

    return BRLCAD_OK;
}


static int
ps_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    return draw_Line3D(dmp, pt1, pt2);
}


static int
ps_drawLines3D(struct dm *dmp, int npoints, point_t *points, int UNUSED(sflag))
{
    if (!dmp || npoints < 0 || !points)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


static int
ps_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    return ps_drawLine2D(dmp, x, y, x, y);
}


static int
ps_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (!dmp) {
	bu_log("WARNING: NULL display (r/g/b => %d/%d/%d; strict => %d; transparency => %f)\n", r, g, b, strict, transparency);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
ps_setBGColor(struct dm *dmp,
	unsigned char r1, unsigned char g1, unsigned char b1,
	unsigned char r2, unsigned char g2, unsigned char b2
	)
{
    if (!dmp) {
	bu_log("WARNING: Null display (r/g/b==%d/%d/%d, color2==%d/%d/%d)\n", r1, g1, b1, r2, g2, b2);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
ps_setLineAttr(struct dm *dmp, int width, int style)
{
    dmp->i->dm_lineWidth = width;
    dmp->i->dm_lineStyle = style;

    if (style == DM_DASHED_LINE)
	fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "DDV "); /* Dot-dashed vectors */
    else
	fprintf(((struct ps_vars *)dmp->i->dm_vars.priv_vars)->ps_fp, "NV "); /* Normal vectors */

    return BRLCAD_OK;
}


/* ARGSUSED */
static int
ps_debug(struct dm *dmp, int lvl)
{
    dmp->i->dm_debugLevel = lvl;
    return BRLCAD_OK;
}

static int
ps_logfile(struct dm *dmp, const char *filename)
{
    bu_vls_sprintf(&dmp->i->dm_log, "%s", filename);
    return BRLCAD_OK;
}

static int
ps_setWinBounds(struct dm *dmp, fastf_t *w)
{
    struct ps_mvars *m_vars = (struct ps_mvars *)dmp->i->m_vars;

    /* Compute the clipping bounds */
    dmp->i->dm_clipmin[0] = w[0] / 2048.0;
    dmp->i->dm_clipmax[0] = w[1] / 2047.0;
    dmp->i->dm_clipmin[1] = w[2] / 2048.0;
    dmp->i->dm_clipmax[1] = w[3] / 2047.0;

    if (m_vars->zclip) {
	dmp->i->dm_clipmin[2] = w[4] / 2048.0;
	dmp->i->dm_clipmax[2] = w[5] / 2047.0;
    } else {
	dmp->i->dm_clipmin[2] = -1.0e20;
	dmp->i->dm_clipmax[2] = 1.0e20;
    }

    return BRLCAD_OK;
}

static int
ps_setZClip(struct dm *dmp, int zclip)
{
    struct ps_mvars *mvars = (struct ps_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("ps_setZClip");

    mvars->zclip = zclip;

    return BRLCAD_OK;
}

static int
ps_getZClip(struct dm *dmp)
{
    struct ps_mvars *mvars = (struct ps_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("ps_getZClip");

    return mvars->zclip;
}

static int
ps_setBound(struct dm *dmp, double bound)
{
    struct ps_mvars *mvars = (struct ps_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
       bu_log("ps_setBound");

    mvars->bound = bound;

    return BRLCAD_OK;
}

static double
ps_getBound(struct dm *dmp)
{
    struct ps_mvars *mvars = (struct ps_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
       bu_log("ps_getBound");

    return mvars->bound;
}

static int
ps_setBoundFlag(struct dm *dmp, int bound)
{
    struct ps_mvars *mvars = (struct ps_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
       bu_log("ps_setBoundFlag");

    mvars->boundFlag = bound;

    return BRLCAD_OK;
}

static int
ps_getBoundFlag(struct dm *dmp)
{
    struct ps_mvars *mvars = (struct ps_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
       bu_log("ps_getBoundFlag");

    return mvars->boundFlag;
}


#define ps_MV_O(_m) offsetof(struct ps_mvars, _m)
struct bu_structparse ps_vparse[] = {
    {"%g",  1, "bound",         ps_MV_O(bound),         dm_generic_hook, NULL, NULL},
    {"%d",  1, "useBound",      ps_MV_O(boundFlag),     dm_generic_hook, NULL, NULL},
    {"%d",  1, "zclip",         ps_MV_O(zclip),         dm_generic_hook, NULL, NULL},
    {"",    0, (char *)0,       0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};

struct dm_impl dm_ps_impl = {
    ps_open,
    ps_close,
    ps_viable,
    ps_drawBegin,
    ps_drawEnd,
    ps_hud_begin,
    ps_hud_end,
    ps_loadMatrix,
    null_loadPMatrix,
    null_popPMatrix,
    ps_drawString2D,
    null_String2DBBox,
    ps_drawLine2D,
    ps_drawLine3D,
    ps_drawLines3D,
    ps_drawPoint2D,
    null_drawPoint3D,
    null_drawPoints3D,
    ps_drawVList,
    ps_drawVList,
    null_draw_obj,
    NULL,
    ps_draw,
    ps_setFGColor,
    ps_setBGColor,
    ps_setLineAttr,
    null_configureWin,
    ps_setWinBounds,
    null_setLight,
    null_getLight,
    null_setTransparency,
    null_getTransparency,
    null_setDepthMask,
    null_setZBuffer,
    null_getZBuffer,
    ps_setZClip,
    ps_getZClip,
    ps_setBound,
    ps_getBound,
    ps_setBoundFlag,
    ps_getBoundFlag,
    ps_debug,
    ps_logfile,
    null_beginDList,
    null_endDList,
    null_drawDList,
    null_freeDLists,
    null_genDLists,
    NULL,
    null_getDisplayImage,	/* display to image function */
    null_reshape,
    null_makeCurrent,
    null_SwapBuffers,
    null_doevent,
    null_openFb,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    0,				/* not graphical */
    NULL,                       /* not graphical */
    0,				/* no displaylist */
    0,                          /* no stereo */
    "ps",
    "Screen to PostScript",
    0, /* top */
    0, /* width */
    0, /* height */
    0, /* dirty */
    0, /* bytes per pixel */
    0, /* bits per channel */
    0,
    0,
    1.0, /* aspect ratio */
    0,
    {0, 0},
    NULL,
    NULL,
    BU_VLS_INIT_ZERO,		/* bu_vls path name*/
    BU_VLS_INIT_ZERO,		/* bu_vls full name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls short name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls logfile */
    {0, 0, 0},			/* bg1 color */
    {0, 0, 0},			/* bg2 color */
    {0, 0, 0},			/* fg color */
    {0.0, 0.0, 0.0},		/* clipmin */
    {0.0, 0.0, 0.0},		/* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    0,				/* depth buffer is not writable */
    1,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    ps_vparse,
    FB_NULL,
    0,				/* Tcl interpreter */
    NULL,                       /* Drawing context */
    NULL                        /* App data */
};


struct dm dm_ps = { DM_MAGIC, &dm_ps_impl, 0 };

#ifdef DM_PLUGIN
const struct dm_plugin pinfo = { DM_API, &dm_ps };

COMPILER_DLLEXPORT const struct dm_plugin *dm_plugin_info(void)
{
    return &pinfo;
}
#endif


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
