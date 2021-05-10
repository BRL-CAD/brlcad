/*                          G R I D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
 *
 */
/** @file burst/grid.c
 *
 */

#include "common.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <regex>

#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "bview/plot3.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/units.h"

#include "raytrace.h"
#include "dm.h"

#include "./burst.h"

#define ASNBIT(w, b)    (w = (b))
#define SETBIT(w, b)    (w |= (b))
#define CLRBIT(w, b)    (w &= ~(b))
#define TSTBIT(w, b)    ((w)&(b))

#define C_MAIN		0
#define C_CRIT 		1

#define COS_TOL		0.01

/* colors for UNIX plot files */
#define R_GRID          255     /* grid - yellow */
#define G_GRID          255
#define B_GRID          0

#define R_BURST         255     /* burst cone - red */
#define G_BURST         0
#define B_BURST         0

#define R_OUTAIR        100     /* outside air - light blue */
#define G_OUTAIR        100
#define B_OUTAIR        255

#define R_INAIR         100     /* inside air - light green */
#define G_INAIR         255
#define B_INAIR         100

#define R_COMP          0       /* component (default) - blue */
#define G_COMP          0
#define B_COMP          255

#define R_CRIT          255     /* critical component (default) - purple */
#define G_CRIT          0
#define B_CRIT          255

#define LOS_TOL         0.1
#define VEC_TOL         0.001
#define OVERLAP_TOL     0.25    /* thinner overlaps not reported */
#define EXIT_AIR        9       /* exit air is called 09 air */
#define OUTSIDE_AIR     1       /* outside air is called 01 air */

#define Air(rp)         ((rp)->reg_aircode > 0)
#define DiffAir(rp, sp) ((rp)->reg_aircode != (sp)->reg_aircode)
#define SameAir(rp, sp) ((rp)->reg_aircode == (sp)->reg_aircode)
#define SameCmp(rp, sp) ((rp)->reg_regionid == (sp)->reg_regionid)
#define OutsideAir(rp)  ((rp)->reg_aircode == OUTSIDE_AIR)
#define InsideAir(rp)   (Air(rp)&& !OutsideAir(rp))

#define PB_ASPECT_INIT          '1'
#define PB_CELL_IDENT           '2'
#define PB_RAY_INTERSECT        '3'
#define PB_RAY_HEADER           '4'
#define PB_REGION_HEADER        '5'

#define PS_ASPECT_INIT          '1'
#define PS_CELL_IDENT           '2'
#define PS_SHOT_INTERSECT       '3'

#define DEBUG_GRID	0
#define DEBUG_SHOT	1

typedef struct pt_queue Pt_Queue;
struct pt_queue {
    struct partition *q_part;
    Pt_Queue *q_next;
};
#define PT_Q_NULL (Pt_Queue *) 0

int
qAdd(struct partition *pp, Pt_Queue **qpp)
{
    Pt_Queue *newq;
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    if ((newq = (Pt_Queue *) malloc(sizeof(Pt_Queue))) == PT_Q_NULL) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	return 0;
    }
    bu_semaphore_release(BU_SEM_SYSCALL);
    newq->q_next = *qpp;
    newq->q_part = pp;
    *qpp = newq;
    return 1;
}


void
qFree(Pt_Queue *qp)
{
    if (qp == PT_Q_NULL)
	return;
    qFree(qp->q_next);
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    free((char *) qp);
    bu_semaphore_release(BU_SEM_SYSCALL);
}


/* local communication with multitasking process */
static int currshot;	/* current shot index */
static int lastshot;	/* final shot index */

static fastf_t viewdir[3];	/* direction of attack */
static fastf_t delta;		/* angular delta ray of spall cone */
static fastf_t comphi;		/* angle between ring and cone axis */
static fastf_t phiinc;		/* angle between concentric rings */

static fastf_t cantdelta[3];	/* delta ray specified by yaw and pitch */

static struct application ag;	/* global application structure (zeroed out) */

/*
  void colorPartition(struct region *regp, int type)

  If user has asked for a UNIX plot write a color command to
  the output stream plotfp which represents the region specified
  by regp.
*/
void
colorPartition(struct burst_state *s, struct region *regp, int type)
{
    struct colors *colorp;
    if (!bu_vls_strlen(&s->plotfile))
	return;
    assert(s->plotfp != NULL);
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    switch (type) {
	case C_CRIT :
	    if ((colorp = findColors(regp->reg_regionid, &s->colorids))	== NULL) {
		pl_color(s->plotfp, R_CRIT, G_CRIT, B_CRIT);
	    } else {
		pl_color(s->plotfp,
			 (int) colorp->c_rgb[0],
			 (int) colorp->c_rgb[1],
			 (int) colorp->c_rgb[2]
			);
	    }
	    break;
	case C_MAIN :
	    if ((colorp = findColors(regp->reg_regionid, &s->colorids)) == NULL) {
		if (InsideAir(regp)) {
		    pl_color(s->plotfp, R_INAIR, G_INAIR, B_INAIR);
		} else {
		    if (Air(regp)) {
			pl_color(s->plotfp, R_OUTAIR, G_OUTAIR, B_OUTAIR);
		    } else {
			pl_color(s->plotfp, R_COMP, G_COMP, B_COMP);
		    }
		}
	    } else {
		pl_color(s->plotfp,
			 (int) colorp->c_rgb[0],
			 (int) colorp->c_rgb[1],
			 (int) colorp->c_rgb[2]
			);
	    }
	    break;
	default :
	    brst_log(s, MSG_OUT, "colorPartition: bad type %d.\n", type);
	    break;
    }
    bu_semaphore_release(BU_SEM_SYSCALL);
    return;
}


/*
  void enforceLOS(struct application *ap,
  struct partition *pt_headp)

  Enforce the line-of-sight tolerance by deleting partitions that are
  too thin.
*/
static void
enforceLOS(struct application *ap, struct partition *pt_headp)
{
    struct partition	*pp;
    for (pp = pt_headp->pt_forw; pp != pt_headp;) {
	struct partition *nextpp = pp->pt_forw;
	if (pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist
	    <= LOS_TOL) {
	    DEQUEUE_PT(pp);
	    FREE_PT(pp, ap->a_resource);
	}
	pp = nextpp;
    }
    return;
}


/*
  Burst Point Library: information about burst ray.  All angles are
  WRT the shotline coordinate system, represented by X', Y' and Z'.
  Ref. Figure 20., Line Number 19 of ICD.

  NOTE: field width of first 2 floats compatible with PB_CELL_IDENT
  record.
*/
void
prntRayHeader(struct burst_state *s,
	      fastf_t *raydir /* burst ray direction vector */,
	      fastf_t *shotdir /* shotline direction vector */,
	      unsigned rayno /* ray number for this burst point */)
{
    double cosxr;        /* cosine of angle between X' and raydir */
    double cosyr;        /* cosine of angle between Y' and raydir */
    fastf_t azim;        /* ray azim in radians */
    fastf_t sinelev; /* sine of ray elevation */
    if (!bu_vls_strlen(&s->outfile))
	return;
    cosxr = -VDOT(shotdir, raydir); /* shotdir is reverse of X' */
    cosyr = VDOT(s->gridhor, raydir);
    if (NEAR_ZERO(cosyr, VDIVIDE_TOL) && NEAR_ZERO(cosxr, VDIVIDE_TOL))
	azim = 0.0;
    else
	azim = atan2(cosyr, cosxr);
    sinelev = VDOT(s->gridver, raydir);
    if (fprintf(s->outfp,
		"%c %8.3f %8.3f %6u\n",
		PB_RAY_HEADER,
		azim,   /* ray azimuth angle WRT shotline (radians). */
		sinelev, /* sine of ray elevation angle WRT shotline. */
		rayno
	       ) < 0
       ) {
	bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->outfile));
    }
}


/*
  void getRtHitNorm(struct hit *hitp, struct soltab *stp,
  struct xray *rayp, int flipped, fastf_t normvec[3])

  Fill normal and hit point into hit struct and if the flipped
  flag is set, reverse the normal.  Return a private copy of the
  flipped normal in normvec.  NOTE: the normal placed in the hit
  struct should not be modified (i.e. reversed) by the application
  because it can be instanced by other solids.
*/
void
getRtHitNorm(struct hit *hitp, struct soltab *stp, struct xray *UNUSED(rayp), int flipped, fastf_t normvec[3])
{
    RT_HIT_NORMAL(normvec, hitp, stp, rayp, flipped);
}

int
chkEntryNorm(struct burst_state *s, struct partition *pp, struct xray *rayp, fastf_t normvec[3], const char *purpose)
{
    fastf_t f;
    static int flipct = 0;
    static int totalct = 0;
    struct soltab *stp = pp->pt_inseg->seg_stp;
    int ret = 1;
    totalct++;
    /* Dot product of ray direction with normal *should* be negative. */
    f = VDOT(rayp->r_dir, normvec);
    if (ZERO(f)) {
	ret = 0;
    }
    if (f > 0.0) {
	flipct++;
	brst_log(s, MSG_OUT, "Fixed flipped entry normal:\n");
	brst_log(s, MSG_OUT, "\tregion \"%s\" solid \"%s\" type %d \"%s\".\n",
		 pp->pt_regionp->reg_name, stp->st_name,
		 stp->st_id, purpose);
	VSCALE(normvec, normvec, -1.0);
	ret = 0;
    }
    return ret;
}

int
chkExitNorm(struct burst_state *s, struct partition *pp, struct xray *rayp, fastf_t normvec[3], const char *purpose)
{
    fastf_t f;
    static int flipct = 0;
    static int totalct = 0;
    struct soltab *stp = pp->pt_outseg->seg_stp;
    int ret = 1;
    totalct++;
    /* Dot product of ray direction with normal *should* be positive. */
    f = VDOT(rayp->r_dir, normvec);
    if (ZERO(f)) {
	ret = 0;
    }
    if (f < 0.0) {
	flipct++;
	brst_log(s, MSG_OUT, "Fixed flipped exit normal:\n");
	brst_log(s, MSG_OUT, "\tregion \"%s\" solid \"%s\" type %d \"%s\".\n",
		 pp->pt_regionp->reg_name, stp->st_name,
		 stp->st_id, purpose);
	VSCALE(normvec, normvec, -1.0);
	ret = 0;
    }
    return ret;
}

static int
f_Nerror(struct application *ap)
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    brst_log(s, MSG_OUT, "Couldn't compute thickness or exit point along normal direction.\n");
    brst_log(s, MSG_OUT, "\tpnt\t<%12.6f, %12.6f, %12.6f>\n", V3ARGS(ap->a_ray.r_pt));
    brst_log(s, MSG_OUT, "\tdir\t<%12.6f, %12.6f, %12.6f>\n", V3ARGS(ap->a_ray.r_dir));
    ap->a_rbeam = 0.0;
    return 0;
}

/*
 * Shooting from surface of object along reversed entry normal to
 * compute exit point along normal direction and normal thickness.
 * Thickness returned in "a_rbeam".
 */
static int
f_Normal(struct application *ap, struct partition *pt_headp, struct seg *UNUSED(segp))
{
    struct partition *pp = pt_headp->pt_forw;
    struct partition *cp;
    struct hit *ohitp;

    for (cp = pp->pt_forw;
	 cp != pt_headp && SameCmp(pp->pt_regionp, cp->pt_regionp);
	 cp = cp->pt_forw
	)
	;
    ohitp = cp->pt_back->pt_outhit;
    ap->a_rbeam = ohitp->hit_dist - pp->pt_inhit->hit_dist;
    return 1;
}

/*
  Given a partition structure with entry hit point and a private copy
  of the associated normal vector, the current application structure
  and the cosine of the obliquity at entry to the component, return
  the normal thickness through the component at the given hit point.

*/
static fastf_t
getNormThickness(struct application *ap, struct partition *pp, fastf_t cosobliquity, fastf_t normvec[3])
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    if (NEAR_EQUAL(cosobliquity, 1.0, COS_TOL)) {
	/* Trajectory was normal to surface, so no need
	   to shoot another ray. */
	fastf_t thickness = pp->pt_outhit->hit_dist -
			    pp->pt_inhit->hit_dist;
	return thickness;
    } else {
	/* need to shoot ray */
	struct application a_thick;
	struct hit *ihitp = pp->pt_inhit;
	struct region *regp = pp->pt_regionp;
	a_thick = *ap;
	a_thick.a_hit = f_Normal;
	a_thick.a_miss = f_Nerror;
	a_thick.a_level++;
	a_thick.a_user = regp->reg_regionid;
	a_thick.a_purpose = "normal thickness";
	VMOVE(a_thick.a_ray.r_pt, ihitp->hit_point);
	VSCALE(a_thick.a_ray.r_dir, normvec, -1.0);
	if (rt_shootray(&a_thick) == -1 && s->fatalerror) {
	    /* Fatal error in application routine. */
	    brst_log(s, MSG_OUT, "Fatal error: raytracing aborted.\n");
	    return 0.0;
	}
	return a_thick.a_rbeam;
    }
    /*NOTREACHED*/
}

/*
 * Burst Point Library: intersection along burst ray.
  Ref. Figure 20., Line Number 20 of ICD.
*/
void
prntRegionHdr(struct application *ap, struct partition *pt_headp, struct partition *pp, fastf_t entrynorm[3], fastf_t exitnorm[3])
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    fastf_t cosobliquity;
    fastf_t normthickness;
    struct hit *ihitp = pp->pt_inhit;
    struct hit *ohitp = pp->pt_outhit;
    struct region *regp = pp->pt_regionp;
    struct xray *rayp = &ap->a_ray;
    /* Get entry/exit normals and fill in hit points */
    getRtHitNorm(ihitp, pp->pt_inseg->seg_stp, rayp,
		 (int) pp->pt_inflip, entrynorm);
    if (! chkEntryNorm(s, pp, rayp, entrynorm,
		       "spall ray component entry normal")) {
    }
    getRtHitNorm(ohitp, pp->pt_outseg->seg_stp, rayp,
		 (int) pp->pt_outflip, exitnorm);
    if (! chkExitNorm(s, pp, rayp, exitnorm,
		      "spall ray component exit normal")) {
    }


    /* calculate cosine of obliquity angle */
    cosobliquity = VDOT(ap->a_ray.r_dir, entrynorm);
    cosobliquity = -cosobliquity;

    if (!bu_vls_strlen(&s->outfile))
	return;


    /* Now we must find normal thickness through component. */
    normthickness = getNormThickness(ap, pp, cosobliquity, entrynorm);
    bu_semaphore_acquire(BU_SEM_SYSCALL);               /* lock */
    if (fprintf(s->outfp,
		"%c % 10.3f % 9.3f % 9.3f %4d %5d % 6.3f\n",
		PB_REGION_HEADER,
		ihitp->hit_dist*s->unitconv, /* distance from burst pt. */
		(ohitp->hit_dist - ihitp->hit_dist)*s->unitconv, /* LOS */
		normthickness*s->unitconv,   /* normal thickness */
		pp->pt_forw == pt_headp ?
		EXIT_AIR : pp->pt_forw->pt_regionp->reg_aircode,
		regp->reg_regionid,
		cosobliquity
	       ) < 0
       ) {
	bu_semaphore_release(BU_SEM_SYSCALL);   /* unlock */
	bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->outfile));
    }
    bu_semaphore_release(BU_SEM_SYSCALL);       /* unlock */
}


void
prntShieldComp(struct application *ap, struct partition *pt_headp, Pt_Queue *qp)
{
    fastf_t entrynorm[3], exitnorm[3];
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    if (!bu_vls_strlen(&s->outfile))
	return;
    if (qp == PT_Q_NULL)
	return;
    prntShieldComp(ap, pt_headp, qp->q_next);
    prntRegionHdr(ap, pt_headp, qp->q_part, entrynorm, exitnorm);
}

void
plotPartition(struct burst_state *s, struct hit *ihitp, struct hit *ohitp)
{
    if (s->plotfp == NULL)
	return;
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    pl_3line(s->plotfp,
	     (int) ihitp->hit_point[X],
	     (int) ihitp->hit_point[Y],
	     (int) ihitp->hit_point[Z],
	     (int) ohitp->hit_point[X],
	     (int) ohitp->hit_point[Y],
	     (int) ohitp->hit_point[Z]
	    );
    bu_semaphore_release(BU_SEM_SYSCALL);
    return;
}

/*
  void lgtModel(struct application *ap, struct partition *pp,
  struct hit *hitp, struct xray *rayp,
  fastf_t surfnorm[3])

  This routine is a simple lighting model which places RGB coefficients
  (0 to 1) in ap->a_color based on the cosine of the angle between
  the surface normal and viewing direction and the color associated with
  the component.  Also, the distance to the surface is placed in
  ap->a_cumlen so that the impact location can be projected into grid
  space.
*/
static void
lgtModel(struct application *ap, struct partition *pp, struct hit *hitp, struct xray *UNUSED(rayp), fastf_t surfnorm[3])
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    struct colors *colorp;
    fastf_t intensity = -VDOT(viewdir, surfnorm);
    if (intensity < 0.0)
	intensity = -intensity;

    colorp = findColors(pp->pt_regionp->reg_regionid, &s->colorids);
    if (colorp != NULL) {
	ap->a_color[RED] = (fastf_t)colorp->c_rgb[RED]/255.0;
	ap->a_color[GRN] = (fastf_t)colorp->c_rgb[GRN]/255.0;
	ap->a_color[BLU] = (fastf_t)colorp->c_rgb[BLU]/255.0;
    } else {
	ap->a_color[RED] = ap->a_color[GRN] = ap->a_color[BLU] = 1.0;
    }
    VSCALE(ap->a_color, ap->a_color, intensity);
    ap->a_cumlen = hitp->hit_dist;
}

/*
  int f_BurstHit(struct application *ap, struct partition *pt_headp)

  This routine handles all output associated with burst ray intersections.

  RETURN CODES: -1 indicates a fatal error, and fatalerror will be
  set to 1.  A positive number is interpreted as the count of critical
  component intersections.  A value of 0 would indicate that zero
  critical components were encountered.
*/
static int
f_BurstHit(struct application *ap, struct partition *pt_headp, struct seg *UNUSED(segp))
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    Pt_Queue *qshield = PT_Q_NULL;
    struct partition *cpp, *spp;
    int nbar;
    int ncrit = 0;
#ifdef VDEBUG
    prntDbgPartitions(ap, pt_headp, "f_BurstHit: initial partitions");
#endif
    /* Find first barrier in front of the burst point. */
    for (spp = pt_headp->pt_forw;
	 spp != pt_headp
	 &&	spp->pt_outhit->hit_dist < 0.1;
	 spp = spp->pt_forw
	)
	;
    for (cpp = spp, nbar = 0;
	 cpp != pt_headp && nbar <= s->nbarriers;
	 cpp = cpp->pt_forw
	) {
	struct region *regp = cpp->pt_regionp;
	struct xray *rayp = &ap->a_ray;
	if (Air(regp))
	    continue; /* Air doesn't matter here. */
	if (findIdents(regp->reg_regionid, &s->critids)) {
	    fastf_t entrynorm[3], exitnorm[3];
	    if (ncrit == 0)
		prntRayHeader(s, ap->a_ray.r_dir, viewdir, ap->a_user);
	    /* Output queued non-critical components. */
	    prntShieldComp(ap, pt_headp, qshield);
	    qFree(qshield);
	    qshield = PT_Q_NULL; /* queue empty */

	    /* Output critical component intersection;
	       prntRegionHdr fills in hit entry/exit normals. */
	    prntRegionHdr(ap, pt_headp, cpp, entrynorm, exitnorm);
	    colorPartition(s, regp, C_CRIT);
	    plotPartition(s, cpp->pt_inhit, cpp->pt_outhit);

	    if (bu_vls_strlen(&s->fbfile) && ncrit == 0) {
		/* first hit on critical component */
		lgtModel(ap, cpp, cpp->pt_inhit, rayp, entrynorm);
	    }
	    ncrit++;
	} else
	    /* Queue up shielding components until we hit a critical one. */
	    if (cpp->pt_forw != pt_headp) {
		if (! qAdd(cpp, &qshield)) {
		    s->fatalerror = 1;
		    return -1;
		}
		nbar++;
	    }
    }
    qFree(qshield);
    if (ncrit == 0)
	return	ap->a_miss(ap);
    else
	return	ncrit;
}


/*
  int f_HushOverlap(struct application *ap, struct partition *pp,
  struct region *reg1, struct region *reg2,
  struct partition *pheadp)

  Do not report diagnostics about individual overlaps, but keep count
  of significant ones (at least as thick as OVERLAP_TOL).

  Returns -
  0	to eliminate partition with overlap entirely
  1	to retain partition in output list, claimed by reg1
  2	to retain partition in output list, claimed by reg2
*/
/*ARGSUSED*/
static int
f_HushOverlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2, struct partition *pheadp)
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    fastf_t depth;
    depth = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
    if (depth >= OVERLAP_TOL)
	s->noverlaps++;

    return rt_defoverlap(ap, pp, reg1, reg2, pheadp);
}

/*
  int f_Overlap(struct application *ap, struct partition *pp,
  struct region *reg1, struct region *reg2,
  struct partition *pheadp)

  Do report diagnostics and keep count of individual overlaps
  that are at least as thick as OVERLAP_TOL.

  Returns -
  0	to eliminate partition with overlap entirely
  1	to retain partition in output list, claimed by reg1
  2	to retain partition in output list, claimed by reg2
*/
/*ARGSUSED*/
static int
f_Overlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2, struct partition *pheadp)
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    fastf_t depth;
    point_t pt;
    depth = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
    if (depth >= OVERLAP_TOL) {
	s->noverlaps++;

	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
	       ap->a_ray.r_dir);
	brst_log(s, MSG_LOG, "OVERLAP:\n");
	brst_log(s, MSG_LOG, "reg=%s isol=%s, \n",
		 reg1->reg_name, pp->pt_inseg->seg_stp->st_name
		);
	brst_log(s, MSG_LOG, "reg=%s osol=%s, \n",
		 reg2->reg_name, pp->pt_outseg->seg_stp->st_name
		);
	brst_log(s, MSG_LOG, "depth %.2fmm at (%g, %g, %g) x%d y%d lvl%d purpose=%s\n",
		 depth,
		 pt[X], pt[Y], pt[Z],
		 ap->a_x, ap->a_y, ap->a_level, ap->a_purpose
		);
    }

    return rt_defoverlap(ap, pp, reg1, reg2, pheadp);
}

/*
  Burst Point Library and Shotline file: information about shotline.
  Ref. Figure 20., Line Number 2 and Figure 19., Line Number 2 of ICD.

  NOTE: field width of first 2 floats compatible with PB_RAY_HEADER
  record.
*/
void
prntCellIdent(struct application *ap)
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    if (bu_vls_strlen(&s->outfile)) {
	int ret = fprintf(s->outfp,
			  "%c % 8.3f % 8.3f\n",
			  PB_CELL_IDENT,
			  ap->a_uvec[X]*s->unitconv,
			  /* horizontal coordinate of shotline (Y') */
			  ap->a_uvec[Y]*s->unitconv
			  /* vertical coordinate of shotline (Z') */
			 );

	if (ret < 0) {
	    bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->outfile));
	}
    }
    if (bu_vls_strlen(&s->shotlnfile)) {
	int ret = fprintf(s->shotlnfp,
			  "%c % 8.3f % 8.3f\n",
			  PS_CELL_IDENT,
			  ap->a_uvec[X]*s->unitconv,
			  /* horizontal coordinate of shotline (Y') */
			  ap->a_uvec[Y]*s->unitconv
			  /* vertical coordinate of shotline (Z') */
			 );
	if (ret < 0) {
	    bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->shotlnfile));
	}
    }
    return;
}

#define PHANTOM_ARMOR 111
/*
  Output "phantom armor" pseudo component.  This component has no
  surface normal or thickness, so many zero fields are used for
  conformity with the normal component output formats.
*/
void
prntPhantom(
    struct burst_state *s,
    struct hit *hitp /* ptr. to phantom's intersection information */,
    int space        /* space code behind phantom */)
{
    if (bu_vls_strlen(&s->outfile)) {
	int ret = fprintf(s->outfp,
			  "%c % 8.2f % 8.2f %5d %2d % 7.3f % 7.3f % 7.3f %c\n",
			  PB_RAY_INTERSECT,
			  (s->standoff - hitp->hit_dist)*s->unitconv,
			  /* X'-coordinate of intersection */
			  0.0,    /* LOS thickness of component */
			  PHANTOM_ARMOR, /* component code number */
			  space,  /* space code */
			  0.0,    /* sine of fallback angle */
			  0.0,    /* rotation angle (degrees) */
			  0.0, /* cosine of obliquity angle at entry */
			  '0'     /* no burst from phantom armor */
			 );
	if (ret < 0) {
	    bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->outfile));
	}
    }
    if (bu_vls_strlen(&s->shotlnfile)) {
	int ret = fprintf(s->shotlnfp,
			  "%c % 8.2f % 7.3f % 7.3f %5d % 8.2f % 8.2f %2d % 7.2f % 7.2f\n",
			  PS_SHOT_INTERSECT,
			  (s->standoff - hitp->hit_dist)*s->unitconv,
			  /* X'-coordinate of intersection */
			  0.0,            /* sine of fallback angle */
			  0.0,            /* rotation angle in degrees */
			  PHANTOM_ARMOR,  /* component code number */
			  0.0,            /* normal thickness of component */
			  0.0,            /* LOS thickness of component */
			  space,          /* space code */
			  0.0,            /* entry obliquity angle in degrees */
			  0.0             /* exit obliquity angle in degrees */
			 );
	if (ret < 0) {
	    bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->shotlnfile));
	}
    }
    return;
}

/*
  Burst Point Library and Shotline file: information about each component
  hit along path of main penetrator (shotline).
  Ref. Figure 20., Line Number 3 and Figure 19., Line Number 2 of ICD.
*/
void
prntSeg(struct application *ap,
	struct partition *cpp /* component partition */,
	int space, fastf_t entrynorm[3], fastf_t exitnorm[3],
	int burstflag /* Was a burst generated by this partition? */
       )
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    fastf_t icosobliquity;      /* cosine of obliquity at entry */
    fastf_t ocosobliquity;      /* cosine of obliquity at exit */
    fastf_t entryangle; /* obliquity angle at entry */
    fastf_t exitangle;  /* obliquity angle at exit */
    fastf_t los;                /* line-of-sight thickness */
    fastf_t normthickness;      /* normal thickness */
    fastf_t rotangle;   /* rotation angle */
    fastf_t sinfbangle; /* sine of fall back angle */

    /* This *should* give negative of desired result. */
    icosobliquity = VDOT(ap->a_ray.r_dir, entrynorm);
    icosobliquity = -icosobliquity;

    ocosobliquity = VDOT(ap->a_ray.r_dir, exitnorm);

    if (NEAR_ZERO(exitnorm[Y], VDIVIDE_TOL) && NEAR_ZERO(exitnorm[X], VDIVIDE_TOL))
	rotangle = 0.0;
    else {
	rotangle = atan2(exitnorm[Y], exitnorm[X]);
	rotangle *= RAD2DEG; /* convert to degrees */
	if (rotangle < 0.0)
	    rotangle += 360.0;
    }
    /* Compute sine of fallback angle.  NB: the Air Force measures the
       fallback angle from the horizontal (X-Y) plane. */
    sinfbangle = VDOT(exitnorm, s->zaxis);

    los = (cpp->pt_outhit->hit_dist-cpp->pt_inhit->hit_dist)*s->unitconv;

    if (bu_vls_strlen(&s->outfile)
	&&      fprintf(s->outfp,
			"%c % 8.2f % 8.2f %5d %2d % 7.3f % 7.2f % 7.3f %c\n",
			PB_RAY_INTERSECT,
			(s->standoff - cpp->pt_inhit->hit_dist)*s->unitconv,
			/* X'-coordinate of intersection */
			los,            /* LOS thickness of component */
			cpp->pt_regionp->reg_regionid,
			/* component code number */
			space,          /* space code */
			sinfbangle,     /* sine of fallback angle at exit */
			rotangle,       /* rotation angle in degrees at exit */
			icosobliquity,  /* cosine of obliquity angle at entry */
			burstflag ? '1' : '0' /* flag generation of burst */
		       ) < 0
       ) {
	bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->outfile));
    }

    if (!bu_vls_strlen(&s->shotlnfile))
	return;
    entryangle = NEAR_EQUAL(icosobliquity, 1.0, COS_TOL) ? 0.0 : acos(icosobliquity) * RAD2DEG;
    if ((normthickness = getNormThickness(ap, cpp, icosobliquity, entrynorm)) <= 0.0 && s->fatalerror) {
	brst_log(s, MSG_LOG, "Couldn't compute normal thickness.\n");
	brst_log(s, MSG_LOG, "\tshotline coordinates <%g, %g>\n",
		 ap->a_uvec[X]*s->unitconv,
		 ap->a_uvec[Y]*s->unitconv
		);
	brst_log(s, MSG_LOG, "\tregion name '%s' solid name '%s'\n",
		 cpp->pt_regionp->reg_name,
		 cpp->pt_inseg->seg_stp->st_name);
	return;
    }
    exitangle = NEAR_EQUAL(ocosobliquity, 1.0, COS_TOL) ? 0.0 : acos(ocosobliquity) * RAD2DEG;
    if (fprintf(s->shotlnfp,
		"%c % 8.2f % 7.3f % 7.2f %5d % 8.2f % 8.2f %2d % 7.2f % 7.2f\n",
		PS_SHOT_INTERSECT,
		(s->standoff - cpp->pt_inhit->hit_dist)*s->unitconv,
		/* X'-coordinate of intersection */
		sinfbangle,     /* sine of fallback angle at exit */
		rotangle,       /* rotation angle in degrees at exit */
		cpp->pt_regionp->reg_regionid,
		/* component code number */
		normthickness*s->unitconv,
		/* normal thickness of component */
		los,            /* LOS thickness of component */
		space,          /* space code */
		entryangle,     /* entry obliquity angle in degrees */
		exitangle       /* exit obliquity angle in degrees */
	       ) < 0
       ) {
	bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->shotlnfile));
    }
}

/*
  int f_BurstMiss(struct application *ap)

  Burst ray missed the model, so do nothing.
*/
static int
f_BurstMiss(struct application *ap)
{
    VSETALL(ap->a_color, 0.0); /* All misses black. */
    return	0;
}

/* To facilitate one-time per burst point initialization of the spall
   ray application structure while leaving burstRay() with the
   capability of being used as a multitasking process, a_burst must
   be accessible by both the burstPoint() and burstRay() routines, but
   can be local to this module. */
static struct application	a_burst; /* prototype spall ray */

static void
spallVec(struct burst_state *s, fastf_t *dvec, fastf_t *s_rdir, fastf_t phi, fastf_t gammaval)
{
    fastf_t cosphi = cos(phi);
    fastf_t singamma = sin(gammaval);
    fastf_t cosgamma = cos(gammaval);
    fastf_t csgaphi, ssgaphi;
    fastf_t sinphi = sin(phi);
    fastf_t cosdphi[3];
    fastf_t fvec[3];
    fastf_t evec[3];

    if (VNEAR_EQUAL(dvec, s->zaxis, VEC_TOL) || VNEAR_EQUAL(dvec, s->negzaxis, VEC_TOL)) {
	VMOVE(evec, s->xaxis);
    } else {
	VCROSS(evec, dvec, s->zaxis);
    }
    VCROSS(fvec, evec, dvec);
    VSCALE(cosdphi, dvec, cosphi);
    ssgaphi = singamma * sinphi;
    csgaphi = cosgamma * sinphi;
    VJOIN2(s_rdir, cosdphi, ssgaphi, evec, csgaphi, fvec);
    VUNITIZE(s_rdir);
    return;
}

void
plotRayPoint(struct burst_state *s, struct xray *rayp)
{
    int endpoint[3];
    if (s->plotfp == NULL)
	return;
    VJOIN1(endpoint, rayp->r_pt, s->cellsz, rayp->r_dir);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    pl_color(s->plotfp, R_BURST, G_BURST, B_BURST);

    /* draw point */
    pl_3point(s->plotfp, (int) endpoint[X], (int) endpoint[Y], (int) endpoint[Z]);

    bu_semaphore_release(BU_SEM_SYSCALL);
    return;
}

void
plotRayLine(struct burst_state *s, struct xray *rayp)
{
    int endpoint[3];
    if (s->plotfp == NULL)
	return;
    VJOIN1(endpoint, rayp->r_pt, s->cellsz, rayp->r_dir);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    pl_color(s->plotfp, R_BURST, G_BURST, B_BURST);

    /* draw line */
    pl_3line(s->plotfp,
	     (int) rayp->r_pt[X],
	     (int) rayp->r_pt[Y],
	     (int) rayp->r_pt[Z],
	     endpoint[X],
	     endpoint[Y],
	     endpoint[Z]
	    );

    bu_semaphore_release(BU_SEM_SYSCALL);
    return;
}


static int
burstRay(struct burst_state *s)
{
    /* Need local copy of all but readonly variables for concurrent
       threads of execution. */
    struct application	a_spall;
    fastf_t			phi;
    int			hitcrit = 0;
    a_spall = a_burst;
    a_spall.a_resource = RESOURCE_NULL;
    for (; ! s->userinterrupt;) {
	fastf_t	sinphi;
	fastf_t	gammaval, gammainc, gammalast;
	int done;
	int m;
	bu_semaphore_acquire(RT_SEM_WORKER);
	phi = comphi;
	comphi += phiinc;
	done = phi > s->conehfangle;
	bu_semaphore_release(RT_SEM_WORKER);
	if (done)
	    break;
	sinphi = sin(phi);
	sinphi = fabs(sinphi);
	m = (M_2PI * sinphi)/delta + 1;
	gammainc = M_2PI / m;
	gammalast = M_2PI - gammainc + VUNITIZE_TOL;
	for (gammaval = 0.0; gammaval <= gammalast; gammaval += gammainc) {
	    int	ncrit;
	    spallVec(s, a_burst.a_ray.r_dir, a_spall.a_ray.r_dir,
		     phi, gammaval);

	    if (s->plotline)
		plotRayPoint(s, &a_spall.a_ray);
	    else
		plotRayLine(s, &a_spall.a_ray);

	    bu_semaphore_acquire(RT_SEM_WORKER);
	    a_spall.a_user = a_burst.a_user++;
	    bu_semaphore_release(RT_SEM_WORKER);
	    if ((ncrit = rt_shootray(&a_spall)) == -1 && s->fatalerror) {
		/* Fatal error in application routine. */
		brst_log(s, MSG_OUT, "Error: ray tracing aborted.\n");
		return	0;
	    }
	    if (bu_vls_strlen(&s->fbfile) && ncrit > 0) {
		paintSpallFb(&a_spall);
		hitcrit = 1;
	    }
	    if (bu_vls_strlen(&s->histfile)) {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		(void) fprintf(s->histfp, "%d\n", ncrit);
		bu_semaphore_release(BU_SEM_SYSCALL);
	    }
	}
    }
    if (bu_vls_strlen(&s->fbfile)) {
	if (hitcrit)
	    paintCellFb(&a_spall, s->pixcrit, s->pixtarg);
	else
	    paintCellFb(&a_spall, s->pixbhit, s->pixtarg);
    }
    return	1;
}

/*
 * This routine dispatches the burst point ray tracing task burstRay().
 * RETURN CODES:	0 for fatal ray tracing error, 1 otherwise.
 *
 * bpt is burst point coordinates.
 */
static int
burstPoint(struct application *ap, fastf_t *normal, fastf_t *bpt)
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    a_burst = *ap;
    a_burst.a_miss = f_BurstMiss;
    a_burst.a_hit = f_BurstHit;
    a_burst.a_level++;
    a_burst.a_user = 0; /* ray number */
    a_burst.a_purpose = "spall ray";
    a_burst.a_uptr = ap->a_uptr;
    assert(a_burst.a_overlap == ap->a_overlap);

    /* If pitch or yaw is specified, cant the main penetrator
       axis. */
    if (s->cantwarhead) {
	VADD2(a_burst.a_ray.r_dir, a_burst.a_ray.r_dir, cantdelta);
	VUNITIZE(a_burst.a_ray.r_dir);
    }
    /* If a deflected cone is specified (the default) the spall cone
       axis is half way between the main penetrator axis and exit
       normal of the spalling component.
    */
    if (s->deflectcone) {
	VADD2(a_burst.a_ray.r_dir, a_burst.a_ray.r_dir, normal);
	VUNITIZE(a_burst.a_ray.r_dir);
    }
    VMOVE(a_burst.a_ray.r_pt, bpt);

    comphi = 0.0; /* Initialize global for concurrent access. */

    /* SERIAL case -- one CPU does all the work. */
    return burstRay(s);
}


/*
  int f_ShotHit(struct application *ap, struct partition *pt_headp)

  This routine is called when a shotline hits the model.  All output
  associated with the main penetrator path is printed here.  If line-
  of-sight bursting is requested, burst point gridding is spawned by
  a call to burstPoint() which dispatches the burst ray task burstRay(),
  a recursive call to the ray tracer.

  RETURN CODES: 0 would indicate a failure in an application routine
  handed to rt_shootray() by burstRay().  Otherwise, 1 is returned.
*/
static int
f_ShotHit(struct application *ap, struct partition *pt_headp, struct seg *UNUSED(segp))
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    struct partition *pp;
    struct partition *bp = PT_NULL;
    vect_t burstnorm = VINIT_ZERO; /* normal at burst point */
    /* Output cell identification. */
    prntCellIdent(ap);
    /* Color cell if making frame buffer image. */
    if (bu_vls_strlen(&s->fbfile))
	paintCellFb(ap, s->pixtarg, s->zoom == 1 ? s->pixblack : s->pixbkgr);

    /* First delete thin partitions. */
    enforceLOS(ap, pt_headp);

    /* Output ray intersections.  This code is extremely cryptic because
       it is dealing with errors in the geometry, where there is
       either adjacent airs of differing types, or voids (gaps)
       in the description.  In the case of adjacent airs, phantom
       armor must be output.  For voids, outside air is the default
       (everyone knows that air rushes to fill a vacuum), so we
       must pretend that it is there.  Outside air is also called
       01 air because its aircode equals 1.  Please tread carefully
       on the code within this loop, it is filled with special
       cases involving adjacency of partitions both real (explicit)
       and imagined (implicit).
    */
    for (pp = pt_headp->pt_forw; pp != pt_headp; pp = pp->pt_forw) {
	int	voidflag = 0;
	struct partition *np = pp->pt_forw;
	struct partition *cp;
	struct region *regp = pp->pt_regionp;
	struct region *nregp = pp->pt_forw->pt_regionp;

	/* Fill in entry and exit points into hit structures. */
	{
	    struct hit *ihitp = pp->pt_inhit;
	    struct hit *ohitp = pp->pt_outhit;
	    struct xray *rayp = &ap->a_ray;
	    VJOIN1(ihitp->hit_point, rayp->r_pt, ihitp->hit_dist, rayp->r_dir);
	    VJOIN1(ohitp->hit_point, rayp->r_pt, ohitp->hit_dist, rayp->r_dir);
	    colorPartition(s, regp, C_MAIN);
	    plotPartition(s, ihitp, ohitp);
	}

	/* Check for voids. */
	if (np != pt_headp) {
	    fastf_t los = 0.0;

	    los = np->pt_inhit->hit_dist - pp->pt_outhit->hit_dist;
	    voidflag = (los > LOS_TOL);
	    /* If the void occurs adjacent to explicit outside
	       air, extend the outside air to fill it. */
	    if (OutsideAir(np->pt_regionp)) {
		if (voidflag) {
		    np->pt_inhit->hit_dist = pp->pt_outhit->hit_dist;
		    voidflag = 0;
		}
		/* Keep going until we are past 01 air. */
		for (cp = np->pt_forw;
		     cp != pt_headp;
		     cp = cp->pt_forw) {
		    if (OutsideAir(cp->pt_regionp)) {
			/* Include outside air. */
			np->pt_outhit->hit_dist = cp->pt_outhit->hit_dist;
		    } else {
			if (cp->pt_inhit->hit_dist - np->pt_outhit->hit_dist > LOS_TOL) {
			    /* Include void following outside air. */
			    np->pt_outhit->hit_dist = cp->pt_inhit->hit_dist;
			} else {
			    break;
			}
		    }
		}
	    }
	}
	/* Merge adjacent inside airs of same type. */
	if (np != pt_headp && InsideAir(np->pt_regionp)) {
	    for (cp = np->pt_forw; cp != pt_headp; cp = cp->pt_forw) {
		if (InsideAir(cp->pt_regionp) && SameAir(np->pt_regionp, cp->pt_regionp)
		    &&	cp->pt_inhit->hit_dist - np->pt_outhit->hit_dist <= LOS_TOL) {
		    np->pt_outhit->hit_dist = cp->pt_outhit->hit_dist;
		} else {
		    break;
		}
	    }
	}

	/* Check for possible phantom armor before internal air,
	   that is if it is the first thing hit. */
	if (pp->pt_back == pt_headp && InsideAir(regp)) {
	    /* If adjacent partitions are the same air, extend
	       the first on to include them. */
	    for (cp = np; cp != pt_headp; cp = cp->pt_forw) {
		if (InsideAir(cp->pt_regionp) && SameAir(regp, cp->pt_regionp)
		    &&	cp->pt_inhit->hit_dist - pp->pt_outhit->hit_dist <= LOS_TOL) {
		    pp->pt_outhit->hit_dist = cp->pt_outhit->hit_dist;
		} else {
		    break;
		}
	    }

	    prntPhantom(s, pp->pt_inhit, (int) regp->reg_aircode);
	} else {
	    if (!Air(regp)) {
		/* If we have a component, output it. */
		fastf_t entrynorm[3];	/* normal at entry */
		fastf_t exitnorm[3];	/* normal at exit */
		/* Get entry normal. */
		getRtHitNorm(pp->pt_inhit, pp->pt_inseg->seg_stp,
			     &ap->a_ray, (int) pp->pt_inflip, entrynorm);
		(void) chkEntryNorm(s, pp, &ap->a_ray, entrynorm, "shotline entry normal");
		/* Get exit normal. */
		getRtHitNorm(pp->pt_outhit, pp->pt_outseg->seg_stp,
			     &ap->a_ray, (int) pp->pt_outflip, exitnorm);
		(void) chkExitNorm(s, pp, &ap->a_ray, exitnorm, "shotline exit normal");

		/* In the case of fragmenting munitions, a hit on any
		   component will cause a burst point. */
		if (bp == PT_NULL && s->bdist > 0.0) {
		    bp = pp;	/* exterior burst */
		    VMOVE(burstnorm, exitnorm);
		}

		/* If there is a void, output 01 air as space. */
		if (voidflag) {
		    if (bp == PT_NULL && ! s->reqburstair && findIdents(regp->reg_regionid, &s->armorids)) {
			/* Bursting on armor/void (ouch). */
			bp = pp;
			VMOVE(burstnorm, exitnorm);
		    }
		    prntSeg(ap, pp, OUTSIDE_AIR, entrynorm, exitnorm, pp == bp);
		} else {
		    /* If air explicitly follows, output space code. */
		    if (np != pt_headp && Air(nregp)) {
			/* Check for interior burst point. */
			if (bp == PT_NULL && s->bdist <= 0.0 && findIdents(regp->reg_regionid, &s->armorids)
			    && (!s->reqburstair || findIdents(nregp->reg_aircode, &s->airids))) {
			    bp = pp; /* interior burst */
			    VMOVE(burstnorm, exitnorm);
			}
			prntSeg(ap, pp, nregp->reg_aircode, entrynorm, exitnorm, pp == bp);
		    } else {
			if (np == pt_headp) {
			    /* Last component gets 09 air. */
			    prntSeg(ap, pp, EXIT_AIR, entrynorm, exitnorm, pp == bp);
			} else {
			    /* No air follows component. */
			    if (SameCmp(regp, nregp)) {
				/* Merge adjacent components with same idents. */
				*np->pt_inhit = *pp->pt_inhit;
				np->pt_inseg = pp->pt_inseg;
				np->pt_inflip = pp->pt_inflip;
				continue;
			    } else {
				prntSeg(ap, pp, 0, entrynorm, exitnorm, pp == bp);
				/* component follows */
			    }
			}
		    }
		}
	    }
	}

	/* Check for adjacency of differing airs, implicit or
	   explicit and output phantom armor as needed. */
	if (InsideAir(regp)) {
	    /* Inside air followed by implicit outside air. */
	    if (voidflag)
		prntPhantom(s, pp->pt_outhit, OUTSIDE_AIR);
	}
	/* Check next partition for adjacency problems. */
	if (np != pt_headp) {
	    /* See if inside air follows impl. outside air. */
	    if (voidflag && InsideAir(nregp)) {
		prntPhantom(s, np->pt_inhit, nregp->reg_aircode);
	    } else
		/* See if differing airs are adjacent. */
		if (!voidflag && Air(regp) && Air(nregp) && DiffAir(nregp, regp)) {
		    prntPhantom(s, np->pt_inhit, (int) nregp->reg_aircode);
		}
	}
	/* Output phantom armor if internal air is last hit. */
	if (np == pt_headp && InsideAir(regp)) {
	    prntPhantom(s, pp->pt_outhit, EXIT_AIR);
	}
    }
    if (s->nriplevels == 0)
	return	1;

    if (bp != PT_NULL) {
	fastf_t burstpt[3];
	/* This is a burst point, calculate coordinates. */
	if (s->bdist > 0.0) {
	    /* Exterior burst point (i.e. case-fragmenting
	       munition with contact-fuzed set-back device):
	       location is bdist prior to entry point. */
	    VJOIN1(burstpt, bp->pt_inhit->hit_point, -s->bdist, ap->a_ray.r_dir);
	} else if (s->bdist < 0.0) {
	    /* Interior burst point (i.e. case-fragment
	       munition with delayed fuzing): location is
	       the magnitude of bdist beyond the exit
	       point. */
	    VJOIN1(burstpt, bp->pt_outhit->hit_point, -s->bdist, ap->a_ray.r_dir);
	} else	  /* Interior burst point: no fuzing offset. */
	    VMOVE(burstpt, bp->pt_outhit->hit_point);

	/* Only generate burst rays if nspallrays is greater than zero. */
	return	(s->nspallrays < 1) ? 1 : burstPoint(ap, burstnorm, burstpt);
    }
    return 1;
}


/*
  This routine must be called before bursting when doing either a
  ground plane burst or bursting at user-specified coordinates.  The
  purpose is to output fake PB_CELL_IDENT and PB_RAY_INTERSECT records
  to the Burst Point Library so that the coordinates of the burst point
  can be made available.

*/
void
prntBurstHdr(
    struct burst_state *s,
    fastf_t *bpt     /* burst point in model coords */,
    fastf_t *shotdir /* shotline direction vector */)
{
    fastf_t vec[3];
    /* Transform burst point (model coordinate system) into the shotline
       coordinate system. */
    vec[Y] = VDOT(s->gridhor, bpt);        /* Y' */
    vec[Z] = VDOT(s->gridver, bpt);        /* Z' */
    vec[X] = -VDOT(shotdir, bpt);       /* X' - shotdir is reverse of X' */

    if (bu_vls_strlen(&s->outfile)
	&&      fprintf(s->outfp,
			"%c % 8.3f % 8.3f\n",
			PB_CELL_IDENT,
			vec[Y]*s->unitconv,
			/* horizontal coordinate of burst point (Y') */
			vec[Z]*s->unitconv
			/* vertical coordinate of burst point (Z') */
		       ) < 0
       ) {
	bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->outfile));
    }
    if (bu_vls_strlen(&s->outfile)
	&&      fprintf(s->outfp,
			"%c % 8.2f % 8.2f %5d %2d % 7.3f % 7.2f % 7.3f %c\n",
			PB_RAY_INTERSECT,
			vec[X]*s->unitconv, /* X' coordinate of burst point */
			0.0,            /* LOS thickness of component */
			9999,           /* dummy component code number */
			9,              /* dummy space code */
			0.0,            /* N/A */
			0.0,            /* N/A */
			0.0,            /* N/A */
			'1'             /* burst was generated */
		       ) < 0
       ) {
	bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->outfile));
    }
}

/*
  int f_ShotMiss(struct application *ap)

  Shot missed the model; if ground bursting is enabled, intersect with
  ground plane, else just arrange for appropriate background color for
  debugging.
*/
static int
f_ShotMiss(struct application *ap)
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    if (s->groundburst) {
	fastf_t dist;
	fastf_t	hitpoint[3];
	/* first find intersection of shot with ground plane */
	if (ap->a_ray.r_dir[Z] >= 0.0)
	    /* Shot direction is upward, can't hit the ground
	       from underneath. */
	    goto	missed_ground;
	if (ap->a_ray.r_pt[Z] <= -s->grndht)
	    /* Must be above ground to hit it from above. */
	    goto	missed_ground;
	/* ground plane is grndht distance below the target origin */
	hitpoint[Z] = -s->grndht;
	/* distance along ray from ray origin to ground plane */
	dist = (hitpoint[Z] - ap->a_ray.r_pt[Z]) / ap->a_ray.r_dir[Z];
	/* solve for X and Y intersection coordinates */
	hitpoint[X] = ap->a_ray.r_pt[X] + ap->a_ray.r_dir[X]*dist;
	hitpoint[Y] = ap->a_ray.r_pt[Y] + ap->a_ray.r_dir[Y]*dist;
	/* check for limits of ground plane */
	if (hitpoint[X] <= s->grndfr && hitpoint[X] >= -s->grndbk
	    &&	hitpoint[Y] <= s->grndlf && hitpoint[Y] >= -s->grndrt
	   ) {
	    /* We have a hit. */
	    if (bu_vls_strlen(&s->fbfile))
		paintCellFb(ap, s->pixghit, s->zoom == 1 ? s->pixblack : s->pixbkgr);
	    if (s->bdist > 0.0) {
		/* simulate standoff fuzing */
		VJOIN1(hitpoint, hitpoint, -s->bdist, ap->a_ray.r_dir);
	    } else if (s->bdist < 0.0) {
		/* interior burst not implemented in ground */
		brst_log(s, MSG_OUT, "User error: negative burst distance can not be specified with ground plane bursting.\n");
		s->fatalerror = 1;
		return	-1;
	    }
	    /* else bdist == 0.0, no adjustment necessary */
	    /* only burst if nspallrays greater than zero */
	    if (s->nspallrays > 0) {
		prntBurstHdr(s, hitpoint, viewdir);
		return burstPoint(ap, s->zaxis, hitpoint);
	    } else
		return	1;
	}
    }
missed_ground :
    if (bu_vls_strlen(&s->fbfile)) {
	paintCellFb(ap, s->pixmiss, s->zoom == 1 ? s->pixblack : s->pixbkgr);
    }
    VSETALL(ap->a_color, 0.0); /* All misses black. */
    return	0;
}

/*
  int readShot(fastf_t *vec)

  This routine reads the next set of firing coordinates from the
  input stream shotfp, using the format selected by the firemode
  bitflag.  Returns 1 for success, 0 for a format error and EOF
  for normal end-of-file.  If 0 is returned, fatalerror will be
  set to 1.
*/
static int
readShot(struct burst_state *s, fastf_t *vec)
{
    double scan[3] = VINIT_ZERO;

    assert(s->shotfp != (FILE *) NULL);

    if (! TSTBIT(s->firemode, FM_3DIM)) {
	/* absence of 3D flag means 2D */
	int items;

	/* read 2D firing coordinates from input stream */
	items = fscanf(s->shotfp, "%lf %lf", &scan[0], &scan[1]);
	VMOVE(vec, scan);
	if (items != 2) {
	    if (items != EOF) {
		brst_log(s, MSG_OUT, "Fatal error: only %d firing coordinates read.\n", items);
		s->fatalerror = 1;
		return 0;
	    } else {
		return EOF;
	    }
	} else {
	    vec[X] /= s->unitconv;
	    vec[Y] /= s->unitconv;
	}
    } else if (TSTBIT(s->firemode, FM_3DIM)) {
	/* 3D coordinates */
	int	items;

	/* read 3D firing coordinates from input stream */
	items = fscanf(s->shotfp, "%lf %lf %lf", &scan[0], &scan[1], &scan[2]);
	VMOVE(vec, scan); /* double to fastf_t */
	if (items != 3) {
	    if (items != EOF) {
		brst_log(s, MSG_OUT, "Fatal error: %d firing coordinates read.\n", items);
		s->fatalerror = 1;
		return 0;
	    } else {
		return EOF;
	    }
	} else {
	    vec[X] /= s->unitconv;
	    vec[Y] /= s->unitconv;
	    vec[Z] /= s->unitconv;
	}
    } else {
	brst_log(s, MSG_OUT, "BUG: readShot called with bad firemode.\n");
	return 0;
    }
    return 1;
}

/*
  int getRayOrigin(struct application *ap)

  This routine fills in the ray origin ap->a_ray.r_pt by folding
  together firing mode and dithering options. By-products of this
  routine include the grid offsets which are stored in ap->a_uvec,
  2-digit random numbers (when opted) which are stored in ap->a_user,
  and grid indices are stored in ap->a_x and ap->a_y.  Return
  codes are: 0 for failure to read new firing coordinates, or
  1 for success.
*/
static int
getRayOrigin(struct burst_state *s, struct application *ap)
{
    fastf_t *vec = ap->a_uvec;
    fastf_t gridyinc[3], gridxinc[3];
    fastf_t scalecx, scalecy;
    if (TSTBIT(s->firemode, FM_SHOT)) {
	if (TSTBIT(s->firemode, FM_FILE)) {
	    switch (readShot(s, vec)) {
		case EOF :
		    return	EOF;
		case 1 :
		    break;
		case 0 :
		    return	0;
	    }
	} else	/* Single shot specified. */
	    VMOVE(vec, s->fire);
	if (TSTBIT(s->firemode, FM_3DIM)) {
	    fastf_t hitpoint[3];
	    /* Project 3-d hit-point back into grid space. */
	    VMOVE(hitpoint, vec);
	    vec[X] = VDOT(s->gridhor, hitpoint);
	    vec[Y] = VDOT(s->gridver, hitpoint);
	}
	ap->a_x = vec[X] / s->cellsz;
	ap->a_y = vec[Y] / s->cellsz;
	scalecx = vec[X];
	scalecy = vec[Y];
    } else {
	fastf_t xoffset = 0.0;
	fastf_t yoffset = 0.0;
	ap->a_x = currshot % s->gridwidth + s->gridxorg;
	ap->a_y = currshot / s->gridwidth + s->gridyorg;
	if (s->dithercells) {
	    /* 2-digit random number, 1's place gives X
	       offset, 10's place gives Y offset.
	    */
	    ap->a_user = lrint(bn_randmt() * 100.0) % 100;
	    xoffset = (ap->a_user%10)*0.1 - 0.5;
	    yoffset = (ap->a_user/10)*0.1 - 0.5;
	}
	/* Compute magnitude of grid offsets. */
	scalecx = (fastf_t) ap->a_x + xoffset;
	scalecy = (fastf_t) ap->a_y + yoffset;
	vec[X] = scalecx *= s->cellsz;
	vec[Y] = scalecy *= s->cellsz;
    }
    /* Compute cell horizontal and vertical	vectors relative to
       grid origin. */
    VSCALE(gridxinc, s->gridhor, scalecx);
    VSCALE(gridyinc, s->gridver, scalecy);
    VADD2(ap->a_ray.r_pt, s->gridsoff, gridyinc);
    VADD2(ap->a_ray.r_pt, ap->a_ray.r_pt, gridxinc);
    return	1;
}

/*
  int readBurst(fastf_t *vec)

  This routine reads the next set of burst point coordinates from the
  input stream burstfp.  Returns 1 for success, 0 for a format
  error and EOF for normal end-of-file.  If 0 is returned,
  fatalerror will be set to 1.
*/
static int
readBurst(struct burst_state *s, fastf_t *vec)
{
    int	items;
    double scan[3];

    assert(s->burstfp != (FILE *) NULL);
    /* read 3D firing coordinates from input stream */
    items = fscanf(s->burstfp, "%lf %lf %lf", &scan[0], &scan[1], &scan[2]);
    VMOVE(vec, scan); /* double to fastf_t */
    if (items != 3) {
	if (items != EOF) {
	    brst_log(s, MSG_OUT, "Fatal error: %d burst coordinates read.\n", items);
	    s->fatalerror = 1;
	    return 0;
	} else {
	    return EOF;
	}
    } else {
	vec[X] /= s->unitconv;
	vec[Y] /= s->unitconv;
	vec[Z] /= s->unitconv;
    }
    return 1;
}

/* TODO - in the origin code this ends up being a no-op - is that correct,
 * or did the initial disabling of the menu code introduce a bug? */
static void
view_pix(struct application *UNUSED(ap))
{
    return;
}

void
plotGrid(struct burst_state *s, fastf_t *r_pt)
{
    if (s->plotfp == NULL)
	return;
    pl_color(s->plotfp, R_GRID, G_GRID, B_GRID);
    pl_3point(s->plotfp, (int) r_pt[X], (int) r_pt[Y], (int) r_pt[Z]);
    return;
}

/*
  This routine gets called when explicit burst points are being
  input.  Crank through all burst points.  Return code of 0
  would indicate a failure in the application routine given to
  rt_shootray() or an error or EOF in getting the next set of
  burst point coordinates.
*/
static int
doBursts(struct burst_state *s)
{
    int			status = 1;
    s->noverlaps = 0;
    VMOVE(ag.a_ray.r_dir, viewdir);

    for (; ! s->userinterrupt; view_pix(&ag)) {
	if (TSTBIT(s->firemode, FM_FILE) && (!(status = readBurst(s, s->burstpoint)) || status == EOF))
	    break;
	ag.a_level = 0;	 /* initialize recursion level */
	plotGrid(s, s->burstpoint);

	prntBurstHdr(s, s->burstpoint, viewdir);
	if (! burstPoint(&ag, s->zaxis, s->burstpoint)) {
	    /* fatal error in application routine */
	    brst_log(s, MSG_OUT, "Fatal error: raytracing aborted.\n");
	    return 0;
	}
	if (! TSTBIT(s->firemode, FM_FILE)) {
	    view_pix(&ag);
	    break;
	}
    }

    return status == EOF ? 1 : status;
}

/*
  If the user has asked for grid coordinates to be saved, write
  them to the output stream 'gridfp'.
*/
void
prntFiringCoords(struct burst_state *s, fastf_t *vec)
{
    if (!bu_vls_strlen(&s->gridfile))
	return;
    assert(s->gridfp != (FILE *) NULL);
    if (fprintf(s->gridfp, "%7.2f %7.2f\n", vec[X]*s->unitconv, vec[Y]*s->unitconv) < 0) {
	bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->gridfile));
    }
}

/*
  int gridShot(void)

  This routine is the grid-level raytracing task; suitable for a
  multi-tasking process.  Return code of 0 would indicate a
  failure in the application routine given to rt_shootray() or an
  error or EOF in getting the next set of firing coordinates.
*/
static int
gridShot(struct burst_state *s)
{
    int status = 1;
    struct application a;
    a = ag;
    a.a_resource = RESOURCE_NULL;

    assert(a.a_hit == ag.a_hit);
    assert(a.a_miss == ag.a_miss);
    assert(a.a_overlap == ag.a_overlap);
    assert(a.a_rt_i == ag.a_rt_i);
    assert(a.a_onehit == ag.a_onehit);
    a.a_user = 0;
    a.a_purpose = "shotline";
    s->noverlaps = 0;
    for (; ! s->userinterrupt; view_pix(&a)) {
	if (! TSTBIT(s->firemode, FM_SHOT) && currshot > lastshot)
	    break;
	if (!(status = getRayOrigin(s, &a)) || status == EOF)
	    break;
	currshot++;
	prntFiringCoords(s, a.a_uvec);
	VMOVE(a.a_ray.r_dir, viewdir);
	a.a_level = 0;	 /* initialize recursion level */
	plotGrid(s, a.a_ray.r_pt);
	if (rt_shootray(&a) == -1 && s->fatalerror) {
	    /* fatal error in application routine */
	    brst_log(s, MSG_OUT, "Fatal error: raytracing aborted.\n");
	    return 0;
	}
	if (!TSTBIT(s->firemode, FM_FILE) && TSTBIT(s->firemode, FM_SHOT)) {
	    view_pix(&a);
	    break;
	}
    }
    return status == EOF ? 1 : status;
}

static void
prntTimer(struct burst_state *s, const char *str)
{
    (void) rt_read_timer(s->timer, TIMER_LEN-1);
    brst_log(s, MSG_OUT, "%s:\t%s\n", str == NULL ? "(null)" : str, s->timer);
}

static void
view_end(struct burst_state *s)
{
    if (bu_vls_strlen(&s->gridfile))
	(void) fflush(s->gridfp);
    if (bu_vls_strlen(&s->histfile))
	(void) fflush(s->histfp);
    if (bu_vls_strlen(&s->plotfile))
	(void) fflush(s->plotfp);
    if (bu_vls_strlen(&s->outfile))
	(void) fflush(s->outfp);
    if (bu_vls_strlen(&s->shotlnfile))
	(void) fflush(s->shotlnfp);
    prntTimer(s, "view");
    if (s->noverlaps > 0) {
	brst_log(s, MSG_OUT, "%d overlaps detected over %g mm thick.\n", s->noverlaps, OVERLAP_TOL);
    }
    return;
}

void
plotInit(struct burst_state *s)
{
    int x_1, y_1, z_1, x_2, y_2, z_2;
    if (s->plotfp == NULL)
	return;
    x_1 = (int) s->rtip->mdl_min[X] - 1;
    y_1 = (int) s->rtip->mdl_min[Y] - 1;
    z_1 = (int) s->rtip->mdl_min[Z] - 1;
    x_2 = (int) s->rtip->mdl_max[X] + 1;
    y_2 = (int) s->rtip->mdl_max[Y] + 1;
    z_2 = (int) s->rtip->mdl_max[Z] + 1;
    pl_3space(s->plotfp, x_1, y_1, z_1, x_2, y_2, z_2);
    return;
}

/*
  Burst Point Library and Shotline file: header record for each view.
  Ref. Figure 20., Line Number 1 and Figure 19., Line Number 1 of ICD.
*/
void
prntAspectInit(struct burst_state *s)
{
    fastf_t projarea;   /* projected area */
    /* Convert to user units before squaring cell size. */
    projarea = s->cellsz*s->unitconv;
    projarea *= projarea;
    if (bu_vls_strlen(&s->outfile)
	&&      fprintf(s->outfp,
			"%c % 9.4f % 8.4f % 5.2f % 10.2f %-6s % 9.6f\n",
			PB_ASPECT_INIT,
			s->viewazim*RAD2DEG, /* attack azimuth in degrees */
			s->viewelev*RAD2DEG, /* attack elevation in degrees */
			s->bdist*s->unitconv,  /* BDIST */
			projarea, /* projected area associated with burst pt. */
			bu_units_string(s->unitconv),
			s->raysolidangle
		       ) < 0
       ) {
	bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->outfile));
    }

    // For the original burst units, report back using the same string
    // burst would originally have used (for compatibility).  Eventually
    // would prefer to deprecated this...
    const char *ustr = bu_units_string(1/s->unitconv);
    if (BU_STR_EQUAL(ustr, "in")) {
	ustr = "inches";
    }
    if (BU_STR_EQUAL(ustr, "ft")) {
	ustr = "feet";
    }
    if (BU_STR_EQUAL(ustr, "mm")) {
	ustr = "millimeters";
    }
    if (BU_STR_EQUAL(ustr, "cm")) {
	ustr = "centimeters";
    }
    if (BU_STR_EQUAL(ustr, "m")) {
	ustr = "meters";
    }

    if (bu_vls_strlen(&s->shotlnfile)
	&&      fprintf(s->shotlnfp,
			"%c % 9.4f % 8.4f % 7.2f % 7.2f %7.2f %7.2f %7.2f %-6s\n",
			PS_ASPECT_INIT,
			s->viewazim*RAD2DEG, /* attack azimuth in degrees */
			s->viewelev*RAD2DEG, /* attack elevation in degrees */
			s->cellsz*s->unitconv, /* shotline separation */
			s->modlrt*s->unitconv, /* maximum Y'-coordinate of target */
			s->modllf*s->unitconv, /* minimum Y'-coordinate of target */
			s->modlup*s->unitconv, /* maximum Z'-coordinate of target */
			s->modldn*s->unitconv, /* minimum Z'-coordinate of target */
			ustr
		       ) < 0
       ) {
	bu_exit(EXIT_FAILURE, "Write failed to file (%s)!\n", bu_vls_cstr(&s->shotlnfile));
    }
    return;
}


/*
  This routine dispatches the top-level ray tracing task.
*/
void
gridModel(struct burst_state *s)
{
    ag.a_onehit = 0;
    ag.a_overlap = s->reportoverlaps ? f_Overlap : f_HushOverlap;
    ag.a_logoverlap = rt_silent_logoverlap;
    ag.a_rt_i = s->rtip;
    if (! TSTBIT(s->firemode, FM_BURST)) {
	/* set up for shotlines */
	ag.a_hit = f_ShotHit;
	ag.a_miss = f_ShotMiss;
    }

    /* NOTE:  this is an important assignment.  Because we are now minimizing
     * globals, the various functions using the application pointer need
     * another way to access information.  We use the user pointer to carry
     * along the burst_state container pointer, and unpack it as needed. */
    ag.a_uptr = (void *)s;

    /* initialize plot file if appropriate */
    plotInit(s);

    /* initialize frame buffer if appropriate */
    if (!imageInit(s)) {
	brst_log(s, MSG_OUT, "Error: problem opening frame buffer.\n");
	return;
    }
    /* output initial line for this aspect */
    prntAspectInit(s);

    s->fatalerror = 0;
    s->userinterrupt = 0;	/* set by interrupt handler */

    rt_prep_timer();
    brst_log(s, MSG_LOG, "Raytracing\n");

    if (TSTBIT(s->firemode, FM_BURST)) {
	if (! doBursts(s))
	    return;
	else
	    goto endvu;
    }

    /* get starting and ending shot number */
    currshot = 0;
    lastshot = s->gridwidth * s->gridheight - 1;

    /* SERIAL case -- one CPU does all the work */
    if (! gridShot(s))
	return;
endvu:
    view_end(s);
    return;
}

/*
   Construct a direction vector out of azimuth and elevation angles
   in radians, allocating storage for it and returning its address.
*/
static void
consVector(fastf_t *vec, fastf_t azim, fastf_t elev)
{
    /* Store cosine of the elevation to save calculating twice. */
    fastf_t	cosE;
    cosE = cos(elev);
    vec[0] = cos(azim) * cosE;
    vec[1] = sin(azim) * cosE;
    vec[2] = sin(elev);
    return;
}

/*
  Creates the unit vectors H and V which are the horizontal
  and vertical components of the grid in target coordinates.
  The vectors are found from the azimuth and elevation of the
  viewing angle according to a simplification of the rotation
  matrix from grid coordinates to target coordinates.
  To see that the vectors are, indeed, unit vectors, recall
  the trigonometric relation:

  sin(A)^2 + cos(A)^2  =  1 .
*/
void
gridRotate(fastf_t azim, fastf_t elev, fastf_t roll, fastf_t *des_H, fastf_t *des_V)
{
    fastf_t sn_azm = sin(azim);
    fastf_t cs_azm = cos(azim);
    fastf_t sn_elv = sin(elev);

    des_H[0] = -sn_azm;
    des_H[1] =  cs_azm;
    des_H[2] =  0.0;
    des_V[0] = -sn_elv*cs_azm;
    des_V[1] = -sn_elv*sn_azm;
    des_V[2] =  cos(elev);

    if (!ZERO(roll)) {
	fastf_t tmp_V[3], tmp_H[3], prime_V[3];
	fastf_t sn_roll = sin(roll);
	fastf_t cs_roll = cos(roll);

	VSCALE(tmp_V, des_V, cs_roll);
	VSCALE(tmp_H, des_H, sn_roll);
	VADD2(prime_V, tmp_V, tmp_H);
	VSCALE(tmp_V, des_V, -sn_roll);
	VSCALE(tmp_H, des_H, cs_roll);
	VADD2(des_H, tmp_V, tmp_H);
	VMOVE(des_V, prime_V);
    }
    return;
}


/*
  Grid initialization routine; must be done once per view.
*/
void
gridInit(struct burst_state *s)
{
    brst_log(s, MSG_LOG, "Initializing grid\n");
    rt_prep_timer();

    /* compute grid unit vectors */
    gridRotate(s->viewazim, s->viewelev, 0.0, s->gridhor, s->gridver);

    if (!NEAR_ZERO(s->yaw, VDIVIDE_TOL) || !NEAR_ZERO(s->pitch, VDIVIDE_TOL)) {
	fastf_t	negsinyaw = -sin(s->yaw);
	fastf_t	sinpitch = sin(s->pitch);
	fastf_t	xdeltavec[3], ydeltavec[3];
	s->cantwarhead = 1;
	VSCALE(xdeltavec, s->gridhor, negsinyaw);
	VSCALE(ydeltavec, s->gridver, sinpitch);
	VADD2(cantdelta, xdeltavec, ydeltavec);
    }

    /* unit vector from origin of model toward eye */
    consVector(viewdir, s->viewazim, s->viewelev);

    /* reposition file pointers if necessary */
    if (TSTBIT(s->firemode, FM_SHOT) && TSTBIT(s->firemode, FM_FILE))
	rewind(s->shotfp);
    else if (TSTBIT(s->firemode, FM_BURST) && TSTBIT(s->firemode, FM_FILE))
	rewind(s->burstfp);

    /* Compute distances from grid origin (model origin) to each
       border of grid, and grid indices at borders of grid.
    */
    if (! TSTBIT(s->firemode, FM_PART)) {
	fastf_t modelmin[3];
	fastf_t modelmax[3];
	if (s->groundburst) {
	    /* extend grid to include ground platform */
	    modelmax[X] = FMAX(s->rtip->mdl_max[X], s->grndfr);
	    modelmin[X] = FMIN(s->rtip->mdl_min[X], -s->grndbk);
	    modelmax[Y] = FMAX(s->rtip->mdl_max[Y], s->grndlf);
	    modelmin[Y] = FMIN(s->rtip->mdl_min[Y], -s->grndrt);
	    modelmax[Z] = s->rtip->mdl_max[Z];
	    modelmin[Z] = FMIN(s->rtip->mdl_min[Z], -s->grndht);
	} else {
	    /* size grid by model RPP */
	    VMOVE(modelmin, s->rtip->mdl_min);
	    VMOVE(modelmax, s->rtip->mdl_max);
	}
	/* Calculate extent of grid. */
	s->gridrt = FMAX(s->gridhor[X] * modelmax[X],
			 s->gridhor[X] * modelmin[X]
			) +
		    FMAX(s->gridhor[Y] * modelmax[Y],
			 s->gridhor[Y] * modelmin[Y]
			) +
		    FMAX(s->gridhor[Z] * modelmax[Z],
			 s->gridhor[Z] * modelmin[Z]
			);
	s->gridlf = FMIN(s->gridhor[X] * modelmax[X],
			 s->gridhor[X] * modelmin[X]
			) +
		    FMIN(s->gridhor[Y] * modelmax[Y],
			 s->gridhor[Y] * modelmin[Y]
			) +
		    FMIN(s->gridhor[Z] * modelmax[Z],
			 s->gridhor[Z] * modelmin[Z]
			);
	s->gridup = FMAX(s->gridver[X] * modelmax[X],
			 s->gridver[X] * modelmin[X]
			) +
		    FMAX(s->gridver[Y] * modelmax[Y],
			 s->gridver[Y] * modelmin[Y]
			) +
		    FMAX(s->gridver[Z] * modelmax[Z],
			 s->gridver[Z] * modelmin[Z]
			);
	s->griddn = FMIN(s->gridver[X] * modelmax[X],
			 s->gridver[X] * modelmin[X]
			) +
		    FMIN(s->gridver[Y] * modelmax[Y],
			 s->gridver[Y] * modelmin[Y]
			) +
		    FMIN(s->gridver[Z] * modelmax[Z],
			 s->gridver[Z] * modelmin[Z]
			);
	/* Calculate extent of model in plane of grid. */
	if (s->groundburst) {
	    s->modlrt = FMAX(s->gridhor[X] * s->rtip->mdl_max[X],
			     s->gridhor[X] * s->rtip->mdl_min[X]
			    ) +
			FMAX(s->gridhor[Y] * s->rtip->mdl_max[Y],
			     s->gridhor[Y] * s->rtip->mdl_min[Y]
			    ) +
			FMAX(s->gridhor[Z] * s->rtip->mdl_max[Z],
			     s->gridhor[Z] * s->rtip->mdl_min[Z]
			    );
	    s->modllf = FMIN(s->gridhor[X] * s->rtip->mdl_max[X],
			     s->gridhor[X] * s->rtip->mdl_min[X]
			    ) +
			FMIN(s->gridhor[Y] * s->rtip->mdl_max[Y],
			     s->gridhor[Y] * s->rtip->mdl_min[Y]
			    ) +
			FMIN(s->gridhor[Z] * s->rtip->mdl_max[Z],
			     s->gridhor[Z] * s->rtip->mdl_min[Z]
			    );
	    s->modlup = FMAX(s->gridver[X] * s->rtip->mdl_max[X],
			     s->gridver[X] * s->rtip->mdl_min[X]
			    ) +
			FMAX(s->gridver[Y] * s->rtip->mdl_max[Y],
			     s->gridver[Y] * s->rtip->mdl_min[Y]
			    ) +
			FMAX(s->gridver[Z] * s->rtip->mdl_max[Z],
			     s->gridver[Z] * s->rtip->mdl_min[Z]
			    );
	    s->modldn = FMIN(s->gridver[X] * s->rtip->mdl_max[X],
			     s->gridver[X] * s->rtip->mdl_min[X]
			    ) +
			FMIN(s->gridver[Y] * s->rtip->mdl_max[Y],
			     s->gridver[Y] * s->rtip->mdl_min[Y]
			    ) +
			FMIN(s->gridver[Z] * s->rtip->mdl_max[Z],
			     s->gridver[Z] * s->rtip->mdl_min[Z]
			    );
	} else {
	    s->modlrt = s->gridrt;
	    s->modllf = s->gridlf;
	    s->modlup = s->gridup;
	    s->modldn = s->griddn;
	}
    }
    s->gridxorg = s->gridlf / s->cellsz;
    s->gridxfin = s->gridrt / s->cellsz;
    s->gridyorg = s->griddn / s->cellsz;
    s->gridyfin = s->gridup / s->cellsz;

    /* allow for randomization of cells */
    if (s->dithercells) {
	s->gridxorg--;
	s->gridxfin++;
	s->gridyorg--;
	s->gridyfin++;
    }

    /* compute stand-off distance */
    s->standoff = FMAX(viewdir[X] * s->rtip->mdl_max[X],
		       viewdir[X] * s->rtip->mdl_min[X]
		      ) +
		  FMAX(viewdir[Y] * s->rtip->mdl_max[Y],
		       viewdir[Y] * s->rtip->mdl_min[Y]
		      ) +
		  FMAX(viewdir[Z] * s->rtip->mdl_max[Z],
		       viewdir[Z] * s->rtip->mdl_min[Z]
		      );

    /* determine largest grid dimension for frame buffer display */
    s->gridwidth  = s->gridxfin - s->gridxorg + 1;
    s->gridheight = s->gridyfin - s->gridyorg + 1;
    s->gridsz = FMAX(s->gridwidth, s->gridheight);

    /* vector to grid origin from model origin */
    VSCALE(s->gridsoff, viewdir, s->standoff);

    /* direction of grid rays */
    VSCALE(viewdir, viewdir, -1.0);

    prntTimer(s, "grid");
    return;
}

/*
  Burst grid initialization routine; should be called once per view.
  Does some one-time computation for current bursting parameters; the
  following globals are filled in:

  delta		-- the target ray delta angle
  phiinc		-- the actual angle between concentric rings
  raysolidangle	-- the average solid angle per spall ray

  Determines actual number of sampling rays yielded by the even-
  distribution algorithm from the number requested.
*/
void
spallInit(struct burst_state *s)
{
    fastf_t	theta;	 /* solid angle of spall sampling cone */
    fastf_t phi;	 /* angle between ray and cone axis */
    fastf_t philast; /* guard against floating point error */
    int spallct = 0; /* actual no. of sampling rays */
    int n;
    if (s->nspallrays < 1) {
	delta = 0.0;
	phiinc = 0.0;
	s->raysolidangle = 0.0;
	brst_log(s, MSG_OUT, "%d sampling rays\n", spallct);
	return;
    }

    /* Compute sampling cone of rays which are equally spaced. */
    theta = M_2PI * (1.0 - cos(s->conehfangle)); /* solid angle */
    delta = sqrt(theta/s->nspallrays); /* angular ray delta */
    n = s->conehfangle / delta;
    phiinc = s->conehfangle / n;
    philast = s->conehfangle + VUNITIZE_TOL;
    /* Crank through spall cone generation once to count actual number
       generated.
    */
    for (phi = 0.0; phi <= philast; phi += phiinc) {
	fastf_t	sinphi = sin(phi);
	fastf_t	gammaval, gammainc, gammalast;
	int m;
	sinphi = fabs(sinphi);
	m = (M_2PI * sinphi)/delta + 1;
	gammainc = M_2PI / m;
	gammalast = M_2PI-gammainc + VUNITIZE_TOL;
	for (gammaval = 0.0; gammaval <= gammalast; gammaval += gammainc)
	    spallct++;
    }
    s->raysolidangle = theta / spallct;
    brst_log(s, MSG_OUT, "Solid angle of sampling cone = %g\n", theta);
    brst_log(s, MSG_OUT, "Solid angle per sampling ray = %g\n", s->raysolidangle);
    brst_log(s, MSG_OUT, "%d sampling rays\n", spallct);
    return;
}

#define WHITESPACE	" \t"

int
execute_run(struct burst_state *s)
{
    static int gottree = 0;
    int loaderror = 0;
    if (!bu_vls_strlen(&s->gedfile)) {
	brst_log(s, MSG_OUT, "No target file has been specified.\n");
	return BRLCAD_ERROR;
    }
    brst_log(s, MSG_LOG, "Reading target data base\n");
    rt_prep_timer();
    if (s->rtip == RTI_NULL) {
	char db_title[TITLE_LEN+1];
	s->rtip = rt_dirbuild(bu_vls_cstr(&s->gedfile), db_title, TITLE_LEN);
    }
    if (s->rtip == RTI_NULL) {
	brst_log(s, MSG_OUT, "Ray tracer failed to read the target file.");
	return BRLCAD_ERROR;
    }
    prntTimer(s, "dir");
    /* Add air into int trees, must be set after rt_dirbuild() and before
     * rt_gettree(). */
    s->rtip->useair = 1;
    if (!gottree) {
	char *objline = bu_strdup(bu_vls_cstr(&s->objects));
	char **av = (char **)bu_calloc(strlen(objline) + 1, sizeof(char *), "argv array");
	int ac = bu_argv_from_string(av, strlen(objline), objline);

	rt_prep_timer();
	for (int i = 0; i < ac; i++) {
	    const char *obj = av[i];
	    brst_log(s, MSG_LOG, "Loading \"%s\"\n", obj);
	    if (rt_gettree(s->rtip, obj) != 0) {
		brst_log(s, MSG_OUT, "Bad object \"%s\"\n", obj);
		loaderror = 1;
	    }
	}
	bu_free(objline, "objline copy");
	bu_free(av, "av");

	gottree = 1;
	prntTimer(s, "load");
    }
    if (loaderror)
	return BRLCAD_ERROR;
    if (s->rtip->needprep) {
	brst_log(s, MSG_LOG, "Prepping solids\n");
	rt_prep_timer();
	rt_prep(s->rtip);
	prntTimer(s, "prep");
    }
    gridInit(s);
    if (s->nriplevels > 0) {
	spallInit(s);
    }
    gridModel(s);
    return BRLCAD_OK;
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
