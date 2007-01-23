/*                          A X E S . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2007 United States Government as represented by
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
/** @file axes.c
 *
 * Functions -
 *	draw_axes	Common axes drawing routine that draws axes at the
 *			specifed point and orientation.
 * Author -
 *	Robert G. Parker
 *
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	berdeen Proving Ground, Maryland  21005
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"

void
dmo_drawAxes_cmd(struct dm *dmp,
		 fastf_t viewSize, /* in mm */
		 mat_t rmat,       /* view rotation matrix */
		 point_t axesPos,  /* in view coordinates */
		 fastf_t axesSize, /* in view coordinates */
		 int *axesColor,
		 int *labelColor,
		 int lineWidth,    /* in pixels */
		 int posOnly,
		 int threeColor,
		 int tickEnabled,
		 int tickLen,      /* in pixels */
		 int majorTickLen,      /* in pixels */
		 fastf_t tickInterval, /* in mm */
		 int ticksPerMajor,
		 int *tickColor,
		 int *majorTickColor,
		 int tickThreshold)
{
  register fastf_t halfAxesSize;		/* half the length of an axis */
  register fastf_t xlx, xly;			/* X axis label position */
  register fastf_t ylx, yly;			/* Y axis label position */
  register fastf_t zlx, zly;			/* Z axis label position */
  register fastf_t l_offset = 0.0078125;	/* axis label offset from axis endpoints */
  point_t v2;
  point_t rxv1, rxv2;
  point_t ryv1, ryv2;
  point_t rzv1, rzv2;
  point_t o_rv2;

  halfAxesSize = axesSize * 0.5;

  /* set axes line width */
  DM_SET_LINE_ATTR(dmp, lineWidth, 0);  /* solid lines */

  /* build X axis about view center */
  VSET(v2, halfAxesSize, 0.0, 0.0);

  /* rotate X axis into position */
  MAT4X3PNT(rxv2, rmat, v2);
  if (posOnly) {
    VSET(rxv1, 0.0, 0.0, 0.0);
  } else {
    VSCALE(rxv1, rxv2, -1.0);
  }

  /* find the X axis label position about view center */
  VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
  MAT4X3PNT(o_rv2, rmat, v2);
  xlx = o_rv2[X];
  xly = o_rv2[Y];

  if (threeColor) {
    /* set X axis color - red */
    DM_SET_FGCOLOR(dmp, 255, 0, 0, 1, 1.0);

    /* draw the X label */
    DM_DRAW_STRING_2D(dmp, "X", xlx + axesPos[X], xly + axesPos[Y], 1, 1);
  } else
    /* set axes color */
    DM_SET_FGCOLOR(dmp, axesColor[0], axesColor[1], axesColor[2], 1, 1.0);

  /* draw X axis with x/y offsets */
  DM_DRAW_LINE_2D(dmp, rxv1[X] + axesPos[X], (rxv1[Y] + axesPos[Y]) * dmp->dm_aspect,
		  rxv2[X] + axesPos[X], (rxv2[Y] + axesPos[Y]) * dmp->dm_aspect);

  /* build Y axis about view center */
  VSET(v2, 0.0, halfAxesSize, 0.0);

  /* rotate Y axis into position */
  MAT4X3PNT(ryv2, rmat, v2);
  if (posOnly) {
    VSET(ryv1, 0.0, 0.0, 0.0);
  } else {
    VSCALE(ryv1, ryv2, -1.0);
  }

  /* find the Y axis label position about view center */
  VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
  MAT4X3PNT(o_rv2, rmat, v2);
  ylx = o_rv2[X];
  yly = o_rv2[Y];

  if (threeColor) {
    /* set Y axis color - green */
    DM_SET_FGCOLOR(dmp, 0, 255, 0, 1, 1.0);

    /* draw the Y label */
    DM_DRAW_STRING_2D(dmp, "Y", ylx + axesPos[X], yly + axesPos[Y], 1, 1);
  }

  /* draw Y axis with x/y offsets */
  DM_DRAW_LINE_2D(dmp, ryv1[X] + axesPos[X], (ryv1[Y] + axesPos[Y]) * dmp->dm_aspect,
		  ryv2[X] + axesPos[X], (ryv2[Y] + axesPos[Y]) * dmp->dm_aspect);

  /* build Z axis about view center */
  VSET(v2, 0.0, 0.0, halfAxesSize);

  /* rotate Z axis into position */
  MAT4X3PNT(rzv2, rmat, v2);
  if (posOnly) {
    VSET(rzv1, 0.0, 0.0, 0.0);
  } else {
    VSCALE(rzv1, rzv2, -1.0);
  }

  /* find the Z axis label position about view center */
  VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
  MAT4X3PNT(o_rv2, rmat, v2);
  zlx = o_rv2[X];
  zly = o_rv2[Y];

  if (threeColor) {
    /* set Z axis color - blue*/
    DM_SET_FGCOLOR(dmp, 0, 0, 255, 1, 1.0);

    /* draw the Z label */
    DM_DRAW_STRING_2D(dmp, "Z", zlx + axesPos[X], zly + axesPos[Y], 1, 1);
  }

  /* draw Z axis with x/y offsets */
  DM_DRAW_LINE_2D(dmp, rzv1[X] + axesPos[X], (rzv1[Y] + axesPos[Y]) * dmp->dm_aspect,
		  rzv2[X] + axesPos[X], (rzv2[Y] + axesPos[Y]) * dmp->dm_aspect);

  if (!threeColor) {
    /* set axes string color */
    DM_SET_FGCOLOR(dmp, labelColor[0], labelColor[1], labelColor[2], 1, 1.0);

    /* draw axes strings/labels with x/y offsets */
    DM_DRAW_STRING_2D(dmp, "X", xlx + axesPos[X], xly + axesPos[Y], 1, 1);
    DM_DRAW_STRING_2D(dmp, "Y", ylx + axesPos[X], yly + axesPos[Y], 1, 1);
    DM_DRAW_STRING_2D(dmp, "Z", zlx + axesPos[X], zly + axesPos[Y], 1, 1);
  }

  if (tickEnabled) {
    /* number of ticks in one direction of a coordinate axis */
    int numTicks = viewSize / tickInterval * 0.5 * halfAxesSize;
    int doMajorOnly = 0;
    int i;
    vect_t xend1, xend2;
    vect_t yend1, yend2;
    vect_t zend1, zend2;
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

    if (dmp->dm_width <= numTicks / halfAxesSize * tickThreshold * 2) {
      int numMajorTicks = numTicks / ticksPerMajor;

      if (dmp->dm_width <= numMajorTicks / halfAxesSize * tickThreshold * 2) {
	return;
      }

      doMajorOnly = 1;
    }

    /* convert tick interval in model space to view space */
    interval = tickInterval / viewSize * 2.0;

    /* convert tick length in pixels to view space */
    tlen = tickLen / (fastf_t)dmp->dm_width * 2.0;

    /* convert major tick length in pixels to view space */
    maj_tlen = majorTickLen / (fastf_t)dmp->dm_width * 2.0;

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

      if (ticksPerMajor == 0)
	notMajor = 1;
      else
	notMajor = i % ticksPerMajor;

      /********* draw ticks along X *********/
      /* positive X direction */
      VSCALE(tvec, rxdir, i);

      /* draw tick in XY plane */
      if (notMajor) {
	if (doMajorOnly)
	  continue;

	/* set tick color */
	DM_SET_FGCOLOR(dmp, tickColor[0], tickColor[1], tickColor[2], 1, 1.0);

	VADD2(t1, yend1, tvec);
	VADD2(t2, yend2, tvec);
      } else {
	/* set major tick color */
	DM_SET_FGCOLOR(dmp, majorTickColor[0], majorTickColor[1], majorTickColor[2], 1, 1.0);

	VADD2(t1, maj_yend1, tvec);
	VADD2(t2, maj_yend2, tvec);
      }
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);

      /* draw tick in XZ plane */
      if (notMajor) {
	VADD2(t1, zend1, tvec);
	VADD2(t2, zend2, tvec);
      } else {
	VADD2(t1, maj_zend1, tvec);
	VADD2(t2, maj_zend2, tvec);
      }
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);

      if (!posOnly) {
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
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);

      /* draw tick in XZ plane */
      if (notMajor) {
	VADD2(t1, zend1, tvec);
	VADD2(t2, zend2, tvec);
      } else {
	VADD2(t1, maj_zend1, tvec);
	VADD2(t2, maj_zend2, tvec);
      }
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);
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
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);

      /* draw tick in YZ plane */
      if (notMajor) {
	VADD2(t1, zend1, tvec);
	VADD2(t2, zend2, tvec);
      } else {
	VADD2(t1, maj_zend1, tvec);
	VADD2(t2, maj_zend2, tvec);
      }
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);

      if (!posOnly) {
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
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);

      /* draw tick in XZ plane */
      if (notMajor) {
	VADD2(t1, zend1, tvec);
	VADD2(t2, zend2, tvec);
      } else {
	VADD2(t1, maj_zend1, tvec);
	VADD2(t2, maj_zend2, tvec);
      }
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);
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
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);

      /* draw tick in ZY plane */
      if (notMajor) {
	VADD2(t1, yend1, tvec);
	VADD2(t2, yend2, tvec);
      } else {
	VADD2(t1, maj_yend1, tvec);
	VADD2(t2, maj_yend2, tvec);
      }
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);

      if (!posOnly) {
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
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);

      /* draw tick in ZY plane */
      if (notMajor) {
	VADD2(t1, yend1, tvec);
	VADD2(t2, yend2, tvec);
      } else {
	VADD2(t1, maj_yend1, tvec);
	VADD2(t2, maj_yend2, tvec);
      }
      DM_DRAW_LINE_2D(dmp, t1[X] + axesPos[X], (t1[Y] + axesPos[Y]) * dmp->dm_aspect,
		      t2[X] + axesPos[X], (t2[Y] + axesPos[Y]) * dmp->dm_aspect);
      }
    }
  }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
