/*                       D M - P L O T . C
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
/** @file libdm/dm-plot.c
 *
 * An unsatisfying (but useful) hack to allow GED to generate
 * UNIX-plot files that not only contain the drawn objects, but also
 * contain the faceplate display as well.  Mostly, a useful hack for
 * making viewgraphs and photographs of an editing session.  We assume
 * that the UNIX-plot filter used can at least discard the
 * non-standard extension to specify color (a Doug Gwyn addition).
 *
 */

#include "common.h"

#define __USE_POSIX2 1
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* for struct timeval */
#endif

#include "tcl.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"

#include "./dm-plot.h"
#include "../null/dm-Null.h"

#include "bv/defines.h"
#include "bv/plot3.h"

#include "../include/private.h"

#if defined(HAVE_POPEN) && !defined(HAVE_POPEN_DECL) && !defined(popen)
extern FILE *popen(const char *command, const char *type);
#endif
#if defined(HAVE_POPEN) && !defined(HAVE_POPEN_DECL) && !defined(pclose)
extern int pclose(FILE *stream);
#endif

struct plot_mvars {
    int zclip;
    double bound;
    int boundFlag;
};

/* Display Manager package interface */

#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

struct plot_vars head_plot_vars;

static int plot_close(struct dm *dmp);

/*
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
plot_open(void *UNUSED(ctx), void *vinterp, int UNUSED(argc), const char *argv[])
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

    *dmp->i = *dm_plot.i; /* struct copy */
    dmp->i->dm_interp = interp;

    BU_ALLOC(dmp->i->dm_vars.priv_vars, struct plot_vars);
    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    BU_ALLOC(dmp->i->m_vars, struct plot_mvars);
    struct plot_mvars *m_vars = (struct plot_mvars *)dmp->i->m_vars;
    m_vars->zclip = 0;
    m_vars->bound = PLOTBOUND;
    m_vars->boundFlag = 1;

    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    bu_vls_init(&privars->vls);
    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_printf(&dmp->i->dm_pathName, ".dm_plot%d", count++);
    bu_vls_printf(&dmp->i->dm_tkName, "dm_plot%d", count++);

    /* skip first argument */
    ++argv;

    /* Process any options */
    privars->is_3D = 1;          /* 3-D w/color, by default */
    while (argv[0] != (char *)0 && argv[0][0] == '-') {
	switch (argv[0][1]) {
	    case '3':
		break;
	    case '2':
		privars->is_3D = 0;		/* 2-D, for portability */
		break;
	    case 'g':
		privars->grid = 1;
		break;
	    case 'f':
		privars->floating = 1;
		break;
	    case 'z':
	    case 'Z':
		/* Enable Z clipping */
		Tcl_AppendStringsToObj(obj, "Clipped in Z to viewing cube\n", (char *)NULL);

		m_vars->zclip = 1;
		break;
	    default:
		Tcl_AppendStringsToObj(obj, "bad PLOT option ", argv[0], "\n", (char *)NULL);
		(void)plot_close(dmp);

		Tcl_SetObjResult(interp, obj);
		return DM_NULL;
	}
	argv++;
    }
    if (argv[0] == (char *)0) {
	Tcl_AppendStringsToObj(obj, "no filename or filter specified\n", (char *)NULL);
	(void)plot_close(dmp);

	Tcl_SetObjResult(interp, obj);
	return DM_NULL;
    }

    if (argv[0][0] == '|') {
	bu_vls_strcpy(&privars->vls, &argv[0][1]);
	while ((++argv)[0] != (char *)0) {
	    bu_vls_strcat(&privars->vls, " ");
	    bu_vls_strcat(&privars->vls, argv[0]);
	}

	privars->is_pipe = 1;
    } else {
	bu_vls_strcpy(&privars->vls, argv[0]);
    }

    if (privars->is_pipe) {
	if ((privars->up_fp = popen(bu_vls_addr(&privars->vls), "wb")) == NULL) {
	    perror(bu_vls_addr(&privars->vls));
	    (void)plot_close(dmp);
	    Tcl_SetObjResult(interp, obj);
	    return DM_NULL;
	}

	Tcl_AppendStringsToObj(obj, "piped to ",
			       bu_vls_addr(&privars->vls),
			       "\n", (char *)NULL);
    } else {
	if ((privars->up_fp = fopen(bu_vls_addr(&privars->vls), "wb")) == NULL) {
	    perror(bu_vls_addr(&privars->vls));
	    (void)plot_close(dmp);
	    Tcl_SetObjResult(interp, obj);
	    return DM_NULL;
	}

	Tcl_AppendStringsToObj(obj, "plot stored in ",
			       bu_vls_addr(&privars->vls),
			       "\n", (char *)NULL);
    }

    setbuf(privars->up_fp, privars->ttybuf);

    if (privars->is_3D)
	pl_3space(privars->up_fp, -2048, -2048, -2048, 2048, 2048, 2048);
    else
	pl_space(privars->up_fp, -2048, -2048, 2048, 2048);

    MAT_IDN(privars->mod_mat);
    MAT_IDN(privars->disp_mat);
    MAT_COPY(privars->plotmat, privars->mod_mat);

    Tcl_SetObjResult(interp, obj);
    return dmp;
}

/**
 * Gracefully release the display.
 */
static int
plot_close(struct dm *dmp)
{
    if (!dmp)
	return BRLCAD_ERROR;

    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    (void)fflush(privars->up_fp);

    if (privars->is_pipe)
	pclose(privars->up_fp); /* close pipe, eat dead children */
    else
	fclose(privars->up_fp);

    bu_vls_free(&dmp->i->dm_pathName);
    bu_free((void *)dmp->i->m_vars, "plot_close: m_vars");
    bu_free((void *)dmp->i->dm_vars.priv_vars, "plot_close: plot_vars");
    bu_free((void *)dmp->i, "plot_close: dmp impl");
    bu_free((void *)dmp, "plot_close: dmp");

    return BRLCAD_OK;
}

int
plot_viable(const char *UNUSED(dpy_string))
{
    return 1;
}


/**
 * There are global variables which are parameters to this routine.
 */
static int
plot_drawBegin(struct dm *dmp)
{
    if (!dmp)
	return BRLCAD_ERROR;

    /* We expect the screen to be blank so far, from last frame flush */

    return BRLCAD_OK;
}


static int
plot_drawEnd(struct dm *dmp)
{
    if (!dmp)
	return BRLCAD_ERROR;

    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    pl_flush(privars->up_fp); /* BRL-specific command */
    pl_erase(privars->up_fp); /* forces drawing */
    (void)fflush(privars->up_fp);

    return BRLCAD_OK;
}


/**
 * Load a new transformation matrix.  This will be followed by
 * many calls to plot_draw().
 */
static int
plot_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    Tcl_Obj *obj;

    if (!dmp)
	return BRLCAD_ERROR;

    Tcl_Interp *interp = (Tcl_Interp *)dmp->i->dm_interp;
    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    if (privars->debug) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	Tcl_AppendStringsToObj(obj, "plot_loadMatrix()\n", (char *)NULL);

	bu_vls_printf(&tmp_vls, "which eye = %d\t", which_eye);
	bu_vls_printf(&tmp_vls, "transformation matrix = \n");
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);

	Tcl_AppendStringsToObj(obj, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    MAT_COPY(privars->mod_mat, mat);
    MAT_COPY(privars->plotmat, mat);
    Tcl_SetObjResult(interp, obj);
    return BRLCAD_OK;
}


/**
 * Set up for an object, transformed as indicated, and with an
 * object center as specified.  The ratio of object to screen size
 * is passed in as a convenience.
 *
 * Returns 0 if object could be drawn, !0 if object was omitted.
 */
static int
plot_drawVList(struct dm *dmp, struct bv_vlist *vp)
{
    static vect_t last;
    struct bv_vlist *tvp;
    point_t *pt_prev=NULL;
    fastf_t dist_prev=1.0;
    fastf_t dist;
    fastf_t delta;
    point_t tlate;
    int useful = 0;

    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    if (privars->floating) {
	bv_vlist_to_uplot(privars->up_fp, &vp->l);

	return BRLCAD_OK;
    }

    /* delta is used in clipping to insure clipped endpoint is
     * slightly in front of eye plane (perspective mode only).  This
     * value is a SWAG that seems to work OK.
     */
    delta = privars->plotmat[15]*0.0001;
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
		    MAT_COPY(privars->plotmat, privars->mod_mat);
		    continue;
		case BV_VLIST_DISPLAY_MAT:
		    MAT4X3PNT(tlate, (privars->mod_mat), *pt);
		    privars->disp_mat[3] = tlate[0];
		    privars->disp_mat[7] = tlate[1];
		    privars->disp_mat[11] = tlate[2];
		    MAT_COPY(privars->plotmat, privars->disp_mat);
		    continue;
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_LINE_MOVE:
		case BV_VLIST_TRI_MOVE:
		    /* Move, not draw */
		    if (dmp->i->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &privars->plotmat[12]) + privars->plotmat[15];
			if (dist <= 0.0) {
			    pt_prev = pt;
			    dist_prev = dist;
			    continue;
			} else {
			    MAT4X3PNT(last, privars->plotmat, *pt);
			    dist_prev = dist;
			    pt_prev = pt;
			}
		    } else
			MAT4X3PNT(last, privars->plotmat, *pt);
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
			dist = VDOT(*pt, &privars->plotmat[12]) + privars->plotmat[15];
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
				    MAT4X3PNT(fin, privars->plotmat, tmp_pt);
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
				    MAT4X3PNT(last, privars->plotmat, tmp_pt);
				    MAT4X3PNT(fin, privars->plotmat, *pt);
				}
			    } else {
				MAT4X3PNT(fin, privars->plotmat, *pt);
			    }
			}
		    } else
			MAT4X3PNT(fin, privars->plotmat, *pt);
		    VMOVE(start, last);
		    VMOVE(last, fin);
		    break;
	    }
	    if (vclip(start, fin, dmp->i->dm_clipmin, dmp->i->dm_clipmax) == 0)
		continue;

	    if (privars->is_3D)
		pl_3line(privars->up_fp,
			 (int)(start[X] * 2047),
			 (int)(start[Y] * 2047),
			 (int)(start[Z] * 2047),
			 (int)(fin[X] * 2047),
			 (int)(fin[Y] * 2047),
			 (int)(fin[Z] * 2047));
	    else
		pl_line(privars->up_fp,
			(int)(start[X] * 2047),
			(int)(start[Y] * 2047),
			(int)(fin[X] * 2047),
			(int)(fin[Y] * 2047));

	    useful = 1;
	}
    }

    if (useful)
	return BRLCAD_OK;

    return BRLCAD_ERROR;
}


static int
plot_draw(struct dm *dmp, struct bv_vlist *(*callback_function)(void *), void **data)
{
    struct bv_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bv_vlist *)data;
	    plot_drawVList(dmp, vp);
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
plot_hud_begin(struct dm *dmp)
{
    if (!dmp)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

static int
plot_hud_end(struct dm *dmp)
{
    if (!dmp)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


/**
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
static int
plot_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int UNUSED(use_aspect))
{
    int sx, sy;

    if (!dmp || !str || size < 0) {
	return BRLCAD_ERROR;
    }

    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    sx = x * 2047;
    sy = y + 2047;
    pl_move(privars->up_fp, sx, sy);
    pl_label(privars->up_fp, str);

    return BRLCAD_OK;
}


static int
plot_drawLine2D(struct dm *dmp, fastf_t xpos1, fastf_t ypos1, fastf_t xpos2, fastf_t ypos2)
{
    int sx1, sy1;
    int sx2, sy2;

    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    sx1 = xpos1 * 2047;
    sx2 = xpos2 * 2047;
    sy1 = ypos1 + 2047;
    sy2 = ypos2 + 2047;
    pl_move(privars->up_fp, sx1, sy1);
    pl_cont(privars->up_fp, sx2, sy2);

    return BRLCAD_OK;
}


static int
plot_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    return draw_Line3D(dmp, pt1, pt2);
}


static int
plot_drawLines3D(struct dm *dmp, int npoints, point_t *points, int UNUSED(sflag))
{
    if (!dmp || npoints < 0 || !points)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


static int
plot_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    return plot_drawLine2D(dmp, x, y, x, y);
}


static int
plot_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (!dmp) {
	bu_log("WARNING: NULL display (r/g/b => %d/%d/%d; strict => %d; transparency => %f)\n", r, g, b, strict, transparency);
	return BRLCAD_ERROR;
    }
    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    pl_color(privars->up_fp, (int)r, (int)g, (int)b);
    return BRLCAD_OK;
}
static int
plot_setBGColor(struct dm *dmp,
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
plot_setLineAttr(struct dm *dmp, int width, int style)
{
    dmp->i->dm_lineWidth = width;
    dmp->i->dm_lineStyle = style;

    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    if (style == DM_DASHED_LINE)
	pl_linmod(privars->up_fp, "dotdashed");
    else
	pl_linmod(privars->up_fp, "solid");

    return BRLCAD_OK;
}


static int
plot_debug(struct dm *dmp, int lvl)
{
    Tcl_Obj *obj;
    Tcl_Interp *interp = (Tcl_Interp *)dmp->i->dm_interp;
    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    dmp->i->dm_debugLevel = lvl;
    (void)fflush(privars->up_fp);
    Tcl_AppendStringsToObj(obj, "flushed\n", (char *)NULL);

    Tcl_SetObjResult(interp, obj);
    return BRLCAD_OK;
}

static int
plot_logfile(struct dm *dmp, const char *filename)
{
    Tcl_Obj *obj;
    Tcl_Interp *interp = (Tcl_Interp *)dmp->i->dm_interp;
    struct plot_vars *privars = (struct plot_vars *)dmp->i->dm_vars.priv_vars;

    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    bu_vls_sprintf(&dmp->i->dm_log, "%s", filename);
    (void)fflush(privars->up_fp);
    Tcl_AppendStringsToObj(obj, "flushed\n", (char *)NULL);

    Tcl_SetObjResult(interp, obj);
    return BRLCAD_OK;
}



static int
plot_setWinBounds(struct dm *dmp, fastf_t *w)
{
    struct plot_mvars *m_vars = (struct plot_mvars *)dmp->i->m_vars;
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
plot_setZClip(struct dm *dmp, int zclip)
{
    struct plot_mvars *mvars = (struct plot_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("plot_setZClip");

    mvars->zclip = zclip;

    return BRLCAD_OK;
}

static int
plot_getZClip(struct dm *dmp)
{
    struct plot_mvars *mvars = (struct plot_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("plot_getZClip");

    return mvars->zclip;
}

static int
plot_setBound(struct dm *dmp, double bound)
{
    struct plot_mvars *mvars = (struct plot_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
       bu_log("plot_setBound");

    mvars->bound = bound;

    return BRLCAD_OK;
}

static double
plot_getBound(struct dm *dmp)
{
    struct plot_mvars *mvars = (struct plot_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
       bu_log("plot_getBound");

    return mvars->bound;
}

static int
plot_setBoundFlag(struct dm *dmp, int bound)
{
    struct plot_mvars *mvars = (struct plot_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
       bu_log("plot_setBoundFlag");

    mvars->boundFlag = bound;

    return BRLCAD_OK;
}

static int
plot_getBoundFlag(struct dm *dmp)
{
    struct plot_mvars *mvars = (struct plot_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
       bu_log("plot_getBoundFlag");

    return mvars->boundFlag;
}

#define plot_MV_O(_m) offsetof(struct plot_mvars, _m)
struct bu_structparse plot_vparse[] = {
    {"%g",  1, "bound",         plot_MV_O(bound),       dm_generic_hook, NULL, NULL},
    {"%d",  1, "useBound",      plot_MV_O(boundFlag),   dm_generic_hook, NULL, NULL},
    {"%d",  1, "zclip",         plot_MV_O(zclip),       dm_generic_hook, NULL, NULL},
    {"",    0, (char *)0,       0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};

struct dm_impl dm_plot_impl = {
    plot_open,
    plot_close,
    plot_viable,
    plot_drawBegin,
    plot_drawEnd,
    plot_hud_begin,
    plot_hud_end,
    plot_loadMatrix,
    null_loadPMatrix,
    null_popPMatrix,
    plot_drawString2D,
    null_String2DBBox,
    plot_drawLine2D,
    plot_drawLine3D,
    plot_drawLines3D,
    plot_drawPoint2D,
    null_drawPoint3D,
    null_drawPoints3D,
    plot_drawVList,
    plot_drawVList,
    null_draw_obj,
    NULL,
    plot_draw,
    plot_setFGColor,
    plot_setBGColor,
    plot_setLineAttr,
    null_configureWin,
    plot_setWinBounds,
    null_setLight,
    null_getLight,
    null_setTransparency,
    null_getTransparency,
    null_setDepthMask,
    null_setZBuffer,
    null_getZBuffer,
    plot_setZClip,
    plot_getZClip,
    plot_setBound,
    plot_getBound,
    plot_setBoundFlag,
    plot_getBoundFlag,
    plot_debug,
    plot_logfile,
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
    0,				/* no stereo */
    "plot",
    "Screen to UNIX-Plot",
    0, /* top */
    0, /* width */
    0, /* height */
    0, /* dirty */
    0, /* bytes per pixel */
    0, /* bits per channel */
    0,
    0,
    1.0, /* aspect ratio */
    NULL,
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
    VINIT_ZERO,			/* clipmin */
    VINIT_ZERO,			/* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    0,				/* depth buffer is not writable */
    1,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    plot_vparse,
    FB_NULL,
    NULL,			/* Tcl interpreter */
    NULL,                       /* Drawing context */
    NULL                        /* App data */
};

struct dm dm_plot = { DM_MAGIC, &dm_plot_impl, 0 };

#ifdef DM_PLUGIN
const struct dm_plugin pinfo = { DM_API, &dm_plot };

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
