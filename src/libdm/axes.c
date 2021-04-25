/*                          A X E S . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2021 United States Government as represented by
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
/** @file libdm/axes.c
 *
 * Functions -
 *	draw_axes	Common axes drawing routine that draws axes at the
 *			specified point and orientation.
 *
 */

#include "common.h"

#include <math.h>
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"
#include "bview/defines.h"
#include "./include/private.h"

void
dm_draw_data_axes(struct dm *dmp,
		  fastf_t sf,
		  struct bview_data_axes_state *bndasp)
{

    if (dmp->i->dm_draw_data_axes) {
	dmp->i->dm_draw_data_axes(dmp, sf, bndasp);
	return;
    }

    int i, j;
    fastf_t halfAxesSize;		/* half the length of an axis */
    point_t ptA, ptB;
    int npoints = bndasp->num_points * 6;
    point_t *points;
    /* Save the line attributes */
    int saveLineWidth = dmp->i->dm_lineWidth;
    int saveLineStyle = dmp->i->dm_lineStyle;

    if (npoints < 1)
	return;

    /* set color */
    dm_set_fg(dmp, bndasp->color[0], bndasp->color[1], bndasp->color[2], 1, 1.0);

    points = (point_t *)bu_calloc(npoints, sizeof(point_t), "data axes points");
    halfAxesSize = bndasp->size * 0.5 * sf;

    /* set linewidth */
    dm_set_line_attr(dmp, bndasp->line_width, 0);  /* solid lines */

    for (i = 0, j = -1; i < bndasp->num_points; ++i) {
	/* draw X axis with x/y offsets */
	VSET(ptA, bndasp->points[i][X] - halfAxesSize, bndasp->points[i][Y], bndasp->points[i][Z]);
	VSET(ptB, bndasp->points[i][X] + halfAxesSize, bndasp->points[i][Y], bndasp->points[i][Z]);
	++j;
	VMOVE(points[j], ptA);
	++j;
	VMOVE(points[j], ptB);

	/* draw Y axis with x/y offsets */
	VSET(ptA, bndasp->points[i][X], bndasp->points[i][Y] - halfAxesSize, bndasp->points[i][Z]);
	VSET(ptB, bndasp->points[i][X], bndasp->points[i][Y] + halfAxesSize, bndasp->points[i][Z]);
	++j;
	VMOVE(points[j], ptA);
	++j;
	VMOVE(points[j], ptB);

	/* draw Z axis with x/y offsets */
	VSET(ptA, bndasp->points[i][X], bndasp->points[i][Y], bndasp->points[i][Z] - halfAxesSize);
	VSET(ptB, bndasp->points[i][X], bndasp->points[i][Y], bndasp->points[i][Z] + halfAxesSize);
	++j;
	VMOVE(points[j], ptA);
	++j;
	VMOVE(points[j], ptB);
    }

    dm_draw_lines_3d(dmp, npoints, points, 0);
    bu_free((void *)points, "data axes points");

    /* Restore the line attributes */
    dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}

void
dm_draw_scene_axes(struct dm *dmp,  struct bview_scene_obj *s)
{
    if (!(s->s_type_flags & BVIEW_AXES))
	return;

    struct bview_axes *bndasp = (struct bview_axes *)s->s_i_data;
    fastf_t halfAxesSize;		/* half the length of an axis */
    point_t ptA, ptB;
    /* Save the line attributes */
    int saveLineWidth = dmp->i->dm_lineWidth;
    int saveLineStyle = dmp->i->dm_lineStyle;

    /* set color */
    dm_set_fg(dmp, bndasp->axes_color[0], bndasp->axes_color[1], bndasp->axes_color[2], 1, 1.0);

    halfAxesSize = bndasp->axes_size * 0.5;

    /* set linewidth */
    dm_set_line_attr(dmp, bndasp->line_width, 0);  /* solid lines */

    /* draw X axis with x/y offsets */
    VSET(ptA, bndasp->axes_pos[X] - halfAxesSize, bndasp->axes_pos[Y], bndasp->axes_pos[Z]);
    VSET(ptB, bndasp->axes_pos[X] + halfAxesSize, bndasp->axes_pos[Y], bndasp->axes_pos[Z]);
    dm_draw_line_3d(dmp, ptA, ptB);

    /* draw Y axis with x/y offsets */
    VSET(ptA, bndasp->axes_pos[X], bndasp->axes_pos[Y] - halfAxesSize, bndasp->axes_pos[Z]);
    VSET(ptB, bndasp->axes_pos[X], bndasp->axes_pos[Y] + halfAxesSize, bndasp->axes_pos[Z]);
    dm_draw_line_3d(dmp, ptA, ptB);

    /* draw Z axis with x/y offsets */
    VSET(ptA, bndasp->axes_pos[X], bndasp->axes_pos[Y], bndasp->axes_pos[Z] - halfAxesSize);
    VSET(ptB, bndasp->axes_pos[X], bndasp->axes_pos[Y], bndasp->axes_pos[Z] + halfAxesSize);
    dm_draw_line_3d(dmp, ptA, ptB);


    /* Restore the line attributes */
    dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}

void
dm_draw_hud_axes(struct dm		        *dmp,
	     fastf_t			viewSize, /* in mm */
	     const mat_t		rmat,       /* view rotation matrix */
	     struct bview_axes	 	*bnasp)
{
    fastf_t halfAxesSize;		/* half the length of an axis */
    fastf_t xlx, xly;			/* X axis label position */
    fastf_t ylx, yly;			/* Y axis label position */
    fastf_t zlx, zly;			/* Z axis label position */
    fastf_t l_offset = 0.0078125;	/* axis label offset from axis endpoints */
    point_t v2;
    point_t rxv1, rxv2;
    point_t ryv1, ryv2;
    point_t rzv1, rzv2;
    point_t o_rv2;
    /* Save the line attributes */
    int saveLineWidth = dmp->i->dm_lineWidth;
    int saveLineStyle = dmp->i->dm_lineStyle;

    halfAxesSize = bnasp->axes_size * 0.5;

    /* set axes line width */
    dm_set_line_attr(dmp, bnasp->line_width, 0);  /* solid lines */

    /* build X axis about view center */
    VSET(v2, halfAxesSize, 0.0, 0.0);

    /* rotate X axis into position */
    MAT4X3PNT(rxv2, rmat, v2);
    if (bnasp->pos_only) {
	VSET(rxv1, 0.0, 0.0, 0.0);
    } else {
	VSCALE(rxv1, rxv2, -1.0);
    }

    /* find the X axis label position about view center */
    VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
    MAT4X3PNT(o_rv2, rmat, v2);
    xlx = o_rv2[X];
    xly = o_rv2[Y];

    if (bnasp->triple_color) {
	/* set X axis color - red */
	dm_set_fg(dmp, 255, 0, 0, 1, 1.0);

	/* draw the X label */
	if (bnasp->label_flag)
	    dm_draw_string_2d(dmp, "X", xlx + bnasp->axes_pos[X], xly + bnasp->axes_pos[Y], 1, 1);
    } else
	/* set axes color */
	dm_set_fg(dmp, bnasp->axes_color[0], bnasp->axes_color[1], bnasp->axes_color[2], 1, 1.0);

    /* draw X axis with x/y offsets */
    dm_draw_line_2d(dmp, rxv1[X] + bnasp->axes_pos[X], (rxv1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
		    rxv2[X] + bnasp->axes_pos[X], (rxv2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

    /* build Y axis about view center */
    VSET(v2, 0.0, halfAxesSize, 0.0);

    /* rotate Y axis into position */
    MAT4X3PNT(ryv2, rmat, v2);
    if (bnasp->pos_only) {
	VSET(ryv1, 0.0, 0.0, 0.0);
    } else {
	VSCALE(ryv1, ryv2, -1.0);
    }

    /* find the Y axis label position about view center */
    VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
    MAT4X3PNT(o_rv2, rmat, v2);
    ylx = o_rv2[X];
    yly = o_rv2[Y];

    if (bnasp->triple_color) {
	/* set Y axis color - green */
	dm_set_fg(dmp, 0, 255, 0, 1, 1.0);

	/* draw the Y label */
	if (bnasp->label_flag)
	    dm_draw_string_2d(dmp, "Y", ylx + bnasp->axes_pos[X], yly + bnasp->axes_pos[Y], 1, 1);
    }

    /* draw Y axis with x/y offsets */
    dm_draw_line_2d(dmp, ryv1[X] + bnasp->axes_pos[X], (ryv1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
		    ryv2[X] + bnasp->axes_pos[X], (ryv2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

    /* build Z axis about view center */
    VSET(v2, 0.0, 0.0, halfAxesSize);

    /* rotate Z axis into position */
    MAT4X3PNT(rzv2, rmat, v2);
    if (bnasp->pos_only) {
	VSET(rzv1, 0.0, 0.0, 0.0);
    } else {
	VSCALE(rzv1, rzv2, -1.0);
    }

    /* find the Z axis label position about view center */
    VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
    MAT4X3PNT(o_rv2, rmat, v2);
    zlx = o_rv2[X];
    zly = o_rv2[Y];

    if (bnasp->triple_color) {
	/* set Z axis color - blue*/
	dm_set_fg(dmp, 0, 0, 255, 1, 1.0);

	/* draw the Z label */
	if (bnasp->label_flag)
	    dm_draw_string_2d(dmp, "Z", zlx + bnasp->axes_pos[X], zly + bnasp->axes_pos[Y], 1, 1);
    }

    /* draw Z axis with x/y offsets */
    dm_draw_line_2d(dmp, rzv1[X] + bnasp->axes_pos[X], (rzv1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
		    rzv2[X] + bnasp->axes_pos[X], (rzv2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

    if (!bnasp->triple_color) {
	/* set axes string color */
	dm_set_fg(dmp, bnasp->label_color[0], bnasp->label_color[1], bnasp->label_color[2], 1, 1.0);

	/* draw axes strings/labels with x/y offsets */
	if (bnasp->label_flag) {
	    dm_draw_string_2d(dmp, "X", xlx + bnasp->axes_pos[X], xly + bnasp->axes_pos[Y], 1, 1);
	    dm_draw_string_2d(dmp, "Y", ylx + bnasp->axes_pos[X], yly + bnasp->axes_pos[Y], 1, 1);
	    dm_draw_string_2d(dmp, "Z", zlx + bnasp->axes_pos[X], zly + bnasp->axes_pos[Y], 1, 1);
	}
    }

    if (bnasp->tick_enabled) {
	/* number of ticks in one direction of a coordinate axis */
	int numTicks = viewSize / bnasp->tick_interval * 0.5 * halfAxesSize;
	int doMajorOnly = 0;
	int i;
	vect_t xend1 = VINIT_ZERO, xend2 = VINIT_ZERO;
	vect_t yend1 = VINIT_ZERO, yend2 = VINIT_ZERO;
	vect_t zend1 = VINIT_ZERO, zend2 = VINIT_ZERO;
	vect_t dir;
	vect_t rxdir, neg_rxdir;
	vect_t rydir, neg_rydir;
	vect_t rzdir, neg_rzdir;
	fastf_t interval;
	fastf_t tlen;
	fastf_t maj_tlen;
	vect_t maj_xend1, maj_xend2;
	vect_t maj_yend1, maj_yend2;
	vect_t maj_zend1, maj_zend2;

	if (dmp->i->dm_width <= numTicks / halfAxesSize * bnasp->tick_threshold * 2) {
	    int numMajorTicks = numTicks / bnasp->ticks_per_major;

	    if (dmp->i->dm_width <= numMajorTicks / halfAxesSize * bnasp->tick_threshold * 2) {
		/* Restore the line attributes */
		dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
		return;
	    }

	    doMajorOnly = 1;
	}

	/* convert tick interval in model space to view space */
	interval = bnasp->tick_interval / viewSize * 2.0;

	/* convert tick length in pixels to view space */
	tlen = bnasp->tick_length / (fastf_t)dmp->i->dm_width * 2.0;

	/* convert major tick length in pixels to view space */
	maj_tlen = bnasp->tick_major_length / (fastf_t)dmp->i->dm_width * 2.0;

	if (!doMajorOnly) {
	    /* calculate end points for x ticks */
	    VSET(dir, tlen, 0.0, 0.0);
	    MAT4X3PNT(xend1, rmat, dir);
	    VSCALE(xend2, xend1, -1.0);

	    /* calculate end points for y ticks */
	    VSET(dir, 0.0, tlen, 0.0);
	    MAT4X3PNT(yend1, rmat, dir);
	    VSCALE(yend2, yend1, -1.0);

	    /* calculate end points for z ticks */
	    VSET(dir, 0.0, 0.0, tlen);
	    MAT4X3PNT(zend1, rmat, dir);
	    VSCALE(zend2, zend1, -1.0);
	}

	/* calculate end points for major x ticks */
	VSET(dir, maj_tlen, 0.0, 0.0);
	MAT4X3PNT(maj_xend1, rmat, dir);
	VSCALE(maj_xend2, maj_xend1, -1.0);

	/* calculate end points for major y ticks */
	VSET(dir, 0.0, maj_tlen, 0.0);
	MAT4X3PNT(maj_yend1, rmat, dir);
	VSCALE(maj_yend2, maj_yend1, -1.0);

	/* calculate end points for major z ticks */
	VSET(dir, 0.0, 0.0, maj_tlen);
	MAT4X3PNT(maj_zend1, rmat, dir);
	VSCALE(maj_zend2, maj_zend1, -1.0);

	/* calculate the rotated x direction vector */
	VSET(dir, interval, 0.0, 0.0);
	MAT4X3PNT(rxdir, rmat, dir);
	VSCALE(neg_rxdir, rxdir, -1.0);

	/* calculate the rotated y direction vector */
	VSET(dir, 0.0, interval, 0.0);
	MAT4X3PNT(rydir, rmat, dir);
	VSCALE(neg_rydir, rydir, -1.0);

	/* calculate the rotated z direction vector */
	VSET(dir, 0.0, 0.0, interval);
	MAT4X3PNT(rzdir, rmat, dir);
	VSCALE(neg_rzdir, rzdir, -1.0);

	/* draw ticks along X axis */
	for (i = 1; i <= numTicks; ++i) {
	    vect_t tvec;
	    point_t t1, t2;
	    int notMajor;

	    if (bnasp->ticks_per_major == 0)
		notMajor = 1;
	    else
		notMajor = i % bnasp->ticks_per_major;

	    /********* draw ticks along X *********/
	    /* positive X direction */
	    VSCALE(tvec, rxdir, i);

	    /* draw tick in XY plane */
	    if (notMajor) {
		if (doMajorOnly)
		    continue;

		/* set tick color */
		dm_set_fg(dmp, bnasp->tick_color[0], bnasp->tick_color[1], bnasp->tick_color[2], 1, 1.0);

		VADD2(t1, yend1, tvec);
		VADD2(t2, yend2, tvec);
	    } else {
		/* set major tick color */
		dm_set_fg(dmp, bnasp->tick_major_color[0], bnasp->tick_major_color[1], bnasp->tick_major_color[2], 1, 1.0);

		VADD2(t1, maj_yend1, tvec);
		VADD2(t2, maj_yend2, tvec);
	    }
	    dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
			    t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

	    /* draw tick in XZ plane */
	    if (notMajor) {
		VADD2(t1, zend1, tvec);
		VADD2(t2, zend2, tvec);
	    } else {
		VADD2(t1, maj_zend1, tvec);
		VADD2(t2, maj_zend2, tvec);
	    }
	    dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
			    t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

	    if (!bnasp->pos_only) {
		/* negative X direction */
		VSCALE(tvec, neg_rxdir, i);

		/* draw tick in XY plane */
		if (notMajor) {
		    VADD2(t1, yend1, tvec);
		    VADD2(t2, yend2, tvec);
		} else {
		    VADD2(t1, maj_yend1, tvec);
		    VADD2(t2, maj_yend2, tvec);
		}
		dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
				t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

		/* draw tick in XZ plane */
		if (notMajor) {
		    VADD2(t1, zend1, tvec);
		    VADD2(t2, zend2, tvec);
		} else {
		    VADD2(t1, maj_zend1, tvec);
		    VADD2(t2, maj_zend2, tvec);
		}
		dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
				t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);
	    }

	    /********* draw ticks along Y *********/
	    /* positive Y direction */
	    VSCALE(tvec, rydir, i);

	    /* draw tick in YX plane */
	    if (notMajor) {
		VADD2(t1, xend1, tvec);
		VADD2(t2, xend2, tvec);
	    } else {
		VADD2(t1, maj_xend1, tvec);
		VADD2(t2, maj_xend2, tvec);
	    }
	    dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
			    t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

	    /* draw tick in YZ plane */
	    if (notMajor) {
		VADD2(t1, zend1, tvec);
		VADD2(t2, zend2, tvec);
	    } else {
		VADD2(t1, maj_zend1, tvec);
		VADD2(t2, maj_zend2, tvec);
	    }
	    dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
			    t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

	    if (!bnasp->pos_only) {
		/* negative Y direction */
		VSCALE(tvec, neg_rydir, i);

		/* draw tick in YX plane */
		if (notMajor) {
		    VADD2(t1, xend1, tvec);
		    VADD2(t2, xend2, tvec);
		} else {
		    VADD2(t1, maj_xend1, tvec);
		    VADD2(t2, maj_xend2, tvec);
		}
		dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
				t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

		/* draw tick in XZ plane */
		if (notMajor) {
		    VADD2(t1, zend1, tvec);
		    VADD2(t2, zend2, tvec);
		} else {
		    VADD2(t1, maj_zend1, tvec);
		    VADD2(t2, maj_zend2, tvec);
		}
		dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
				t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);
	    }

	    /********* draw ticks along Z *********/
	    /* positive Z direction */
	    VSCALE(tvec, rzdir, i);

	    /* draw tick in ZX plane */
	    if (notMajor) {
		VADD2(t1, xend1, tvec);
		VADD2(t2, xend2, tvec);
	    } else {
		VADD2(t1, maj_xend1, tvec);
		VADD2(t2, maj_xend2, tvec);
	    }
	    dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
			    t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

	    /* draw tick in ZY plane */
	    if (notMajor) {
		VADD2(t1, yend1, tvec);
		VADD2(t2, yend2, tvec);
	    } else {
		VADD2(t1, maj_yend1, tvec);
		VADD2(t2, maj_yend2, tvec);
	    }
	    dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
			    t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

	    if (!bnasp->pos_only) {
		/* negative Z direction */
		VSCALE(tvec, neg_rzdir, i);

		/* draw tick in ZX plane */
		if (notMajor) {
		    VADD2(t1, xend1, tvec);
		    VADD2(t2, xend2, tvec);
		} else {
		    VADD2(t1, maj_xend1, tvec);
		    VADD2(t2, maj_xend2, tvec);
		}
		dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
				t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);

		/* draw tick in ZY plane */
		if (notMajor) {
		    VADD2(t1, yend1, tvec);
		    VADD2(t2, yend2, tvec);
		} else {
		    VADD2(t1, maj_yend1, tvec);
		    VADD2(t2, maj_yend2, tvec);
		}
		dm_draw_line_2d(dmp, t1[X] + bnasp->axes_pos[X], (t1[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect,
				t2[X] + bnasp->axes_pos[X], (t2[Y] + bnasp->axes_pos[Y]) * dmp->i->dm_aspect);
	    }
	}
    }

    /* Restore the line attributes */
    dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
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
