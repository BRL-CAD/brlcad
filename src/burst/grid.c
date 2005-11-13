/*                          G R I D . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file grid.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <assert.h>

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "fb.h"
#include "plot3.h"

#include "./vecmath.h"
#include "./ascii.h"
#include "./extern.h"

#define DEBUG_GRID	0
#define DEBUG_SHOT	1

/* local communication with multitasking process */
static int currshot;	/* current shot index */
static int lastshot;	/* final shot index */

static fastf_t viewdir[3];	/* direction of attack */
static fastf_t delta;		/* angular delta ray of spall cone */
static fastf_t comphi;		/* angle between ring and cone axis */
static fastf_t phiinc;		/* angle between concentric rings */

static fastf_t cantdelta[3];	/* delta ray specified by yaw and pitch */

static struct application ag;	/* global application structure (zeroed out) */

/* functions local to this module */
static boolean doBursts();
static boolean burstPoint();
static boolean burstRay();
static boolean gridShot();
static fastf_t	max();
static fastf_t	min();
static int f_BurstHit();
static int f_BurstMiss();
static int f_HushOverlap();
static int f_Overlap();
static int f_ShotHit();
static int f_ShotMiss();
static int getRayOrigin();
static int readBurst();
static int readShot();
static void consVector();
static void lgtModel();
static void view_end();
static void view_pix();
static void spallVec();

/*
	void colorPartition( register struct region *regp, int type )

	If user has asked for a UNIX plot write a color command to
	the output stream plotfp which represents the region specified
	by regp.
 */
void
colorPartition( regp, type )
register struct region *regp;
int type;
	{	Colors	*colorp;
	if( plotfile[0] == NUL )
		return;
	assert( plotfp != NULL );
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	switch( type )
		{
	case C_CRIT :
		if( (colorp = findColors( regp->reg_regionid, &colorids ))
			== NULL )
			pl_color( plotfp, R_CRIT, G_CRIT, B_CRIT );
		else
			pl_color( plotfp,
				  (int) colorp->c_rgb[0],
				  (int) colorp->c_rgb[1],
				  (int) colorp->c_rgb[2]
				  );
		break;
	case C_MAIN :
		if( (colorp = findColors( regp->reg_regionid, &colorids ))
			== NULL )
			{
			if( InsideAir(regp ) )
				pl_color( plotfp,
					  R_INAIR, G_INAIR, B_INAIR );
			else
			if( Air(regp ) )
				pl_color( plotfp,
					  R_OUTAIR, G_OUTAIR, B_OUTAIR );
			else
				pl_color( plotfp, R_COMP, G_COMP, B_COMP );
			}
		else
			pl_color( plotfp,
				 (int) colorp->c_rgb[0],
				 (int) colorp->c_rgb[1],
				 (int) colorp->c_rgb[2]
				 );
		break;
	default :
		brst_log( "colorPartition: bad type %d.\n", type );
		break;
		}
	bu_semaphore_release( BU_SEM_SYSCALL );
	return;
	}

/*
	boolean doBursts( void )

	This routine gets called when explicit burst points are being
	input.  Crank through all burst points.  Return code of 0
	would indicate a failure in the application routine given to
	rt_shootray() or an error or EOF in getting the next set of
	burst point coordinates.
 */
static boolean
doBursts()
	{	boolean			status = 1;
	noverlaps = 0;
	CopyVec( ag.a_ray.r_dir, viewdir ); /* XXX -- could be done up in
						gridModel() */
	for( ; ! userinterrupt; view_pix( &ag ) )
		{
		if(	TSTBIT(firemode,FM_FILE)
		    &&	(!(status = readBurst( burstpoint )) || status == EOF)
			)
			break;
		ag.a_level = 0;	 /* initialize recursion level */
		plotGrid( burstpoint );

		prntBurstHdr( burstpoint, viewdir );
		if( ! burstPoint( &ag, zaxis, burstpoint ) )
			{
			/* fatal error in application routine */
			brst_log( "Fatal error: raytracing aborted.\n" );
			return	0;
			}
		if( ! TSTBIT(firemode,FM_FILE) )
			{
			view_pix( &ag );
			break;
			}
		}
	return	status == EOF ? 1 : status;
	}

/*
	void enforceLOS( register struct application *ap,
				register struct partition *pt_headp )

	Enforce the line-of-sight tolerance by deleting partitions that are
	too thin.
 */
static void
enforceLOS( ap, pt_headp )
register struct application	*ap;
register struct partition	*pt_headp;
	{	register struct partition	*pp;
	for( pp = pt_headp->pt_forw; pp != pt_headp; )
		{	register struct partition *nextpp = pp->pt_forw;
		if( pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist
			<= LOS_TOL )
			{
			DEQUEUE_PT( pp );
			FREE_PT( pp, ap->a_resource );
			}
		pp = nextpp;
		}
	return;
	}

/*
	int f_BurstHit( struct application *ap, struct partition *pt_headp )

	This routine handles all output associated with burst ray intersections.

	RETURN CODES: -1 indicates a fatal error, and fatalerror will be
	set to 1.  A positive number is interpreted as the count of critical
	component intersections.  A value of 0 would indicate that zero
	critical components were encountered.
 */
static int
f_BurstHit( ap, pt_headp, segp )
struct application *ap;
struct partition *pt_headp;
struct seg *segp;
	{	Pt_Queue *qshield = PT_Q_NULL;
		register struct partition *cpp, *spp;
		register int nbar;
		register int ncrit = 0;
#ifdef VDEBUG
	prntDbgPartitions( ap, pt_headp, "f_BurstHit: initial partitions" );
#endif
	/* Find first barrier in front of the burst point. */
	for(	spp = pt_headp->pt_forw;
		spp != pt_headp
	    &&	spp->pt_outhit->hit_dist < 0.1;
		spp = spp->pt_forw
		)
		;
	for(	cpp = spp, nbar = 0;
		cpp != pt_headp && nbar <= nbarriers;
		cpp = cpp->pt_forw
		)
		{	register struct region *regp = cpp->pt_regionp;
			struct xray *rayp = &ap->a_ray;
		if( Air( regp ) )
			continue; /* Air doesn't matter here. */
		if( findIdents( regp->reg_regionid, &critids ) )
			{	fastf_t entrynorm[3], exitnorm[3];
			if( ncrit == 0 )
				prntRayHeader( ap->a_ray.r_dir, viewdir,
						ap->a_user );
			/* Output queued non-critical components. */
			prntShieldComp( ap, pt_headp, qshield );
			qFree( qshield );
			qshield = PT_Q_NULL; /* queue empty */

			/* Output critical component intersection;
			   prntRegionHdr fills in hit entry/exit normals. */
			prntRegionHdr( ap, pt_headp, cpp, entrynorm, exitnorm );
			colorPartition( regp, C_CRIT );
			plotPartition( cpp->pt_inhit, cpp->pt_outhit,
					rayp, regp );
			if( fbfile[0] != NUL && ncrit == 0 )
				/* first hit on critical component */
				lgtModel( ap, cpp, cpp->pt_inhit, rayp,
					entrynorm );
			ncrit++;
			}
		else
		/* Queue up shielding components until we hit a critical one. */
		if( cpp->pt_forw != pt_headp )
			{
			if( ! qAdd( cpp, &qshield ) )
				{
				fatalerror = 1;
				return	-1;
				}
			nbar++;
			}
		}
	qFree( qshield );
	if( ncrit == 0 )
		return	ap->a_miss( ap );
	else
		return	ncrit;
	}


/*
	int f_HushOverlap( struct application *ap, struct partition *pp,
		struct region *reg1, struct region *reg2,
		struct partition *pheadp )

	Do not report diagnostics about individual overlaps, but keep count
	of significant ones (at least as thick as OVERLAP_TOL).
	Some of this code is from librt/bool.c:rt_defoverlap() for
	consistency of which region is picked.

	Returns -
	 0	to eliminate partition with overlap entirely
	 1	to retain partition in output list, claimed by reg1
	 2	to retain partition in output list, claimed by reg2
 */
/*ARGSUSED*/
static int
f_HushOverlap( ap, pp, reg1, reg2, pheadp )
struct application *ap;
struct partition *pp;
struct region *reg1, *reg2;
struct partition *pheadp;
	{	fastf_t depth;
	depth = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	if( depth >= OVERLAP_TOL )
		noverlaps++;

	/* Apply heuristics as to which region should claim partition. */
	if( reg1->reg_aircode != 0 )
		/* reg1 was air, replace with reg2 */
		return 2;
	if( pp->pt_back != pheadp )
		{ /* Repeat a prev region, if that is a choice */
		if( pp->pt_back->pt_regionp == reg1 )
			return 1;
		if( pp->pt_back->pt_regionp == reg2 )
			return 2;
		}
	/* To provide some consistency from ray to ray, use lowest bit # */
	if( reg1->reg_bit < reg2->reg_bit )
		return 1;
	return 2;
	}

/*
	int f_Overlap( struct application *ap, struct partition *pp,
		struct region *reg1, struct region *reg2,
		struct partition *pheadp )

	Do report diagnostics and keep count of individual overlaps
	that are at least as thick as OVERLAP_TOL.
	Some of this code is from librt/bool.c:rt_defoverlap() for
	consistency of which region is picked.

	Returns -
	 0	to eliminate partition with overlap entirely
	 1	to retain partition in output list, claimed by reg1
	 2	to retain partition in output list, claimed by reg2
 */
/*ARGSUSED*/
static int
f_Overlap( ap, pp, reg1, reg2, pheadp )
struct application *ap;
struct partition *pp;
struct region *reg1, *reg2;
struct partition *pheadp;
	{	fastf_t depth;
		point_t pt;
	depth = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	if( depth >= OVERLAP_TOL )
		{
		noverlaps++;

		VJOIN1( pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
			ap->a_ray.r_dir );
		brst_log( "OVERLAP:\n" );
		brst_log( "reg=%s isol=%s,\n",
			reg1->reg_name, pp->pt_inseg->seg_stp->st_name
			);
		brst_log( "reg=%s osol=%s,\n",
			reg2->reg_name, pp->pt_outseg->seg_stp->st_name
			);
		brst_log( "depth %.2fmm at (%g,%g,%g) x%d y%d lvl%d purpose=%s\n",
			depth,
			pt[X], pt[Y], pt[Z],
			ap->a_x, ap->a_y, ap->a_level, ap->a_purpose
			);
		}

	/* Apply heuristics as to which region should claim partition. */
	if( reg1->reg_aircode != 0 )
		/* reg1 was air, replace with reg2 */
		return 2;
	if( pp->pt_back != pheadp )
		{ /* Repeat a prev region, if that is a choice */
		if( pp->pt_back->pt_regionp == reg1 )
			return 1;
		if( pp->pt_back->pt_regionp == reg2 )
			return 2;
		}
	/* To provide some consistency from ray to ray, use lowest bit # */
	if( reg1->reg_bit < reg2->reg_bit )
		return 1;
	return 2;
	}

/*
	int f_ShotHit( struct application *ap, struct partition *pt_headp )

	This routine is called when a shotline hits the model.  All output
	associated with the main penetrator path is printed here.  If line-
	of-sight bursting is requested, burst point gridding is spawned by
	a call to burstPoint() which dispatches the burst ray task burstRay(),
	a recursive call to the ray tracer.

	RETURN CODES: 0 would indicate a failure in an application routine
	handed to rt_shootray() by burstRay().  Otherwise, 1 is returned.
 */
static int
f_ShotHit( ap, pt_headp, segp )
struct application *ap;
struct partition *pt_headp;
struct seg *segp;
	{	register struct partition *pp;
		struct partition *bp = PT_NULL;
		fastf_t burstnorm[3]; /* normal at burst point */
#if DEBUG_GRID
	brst_log( "f_ShotHit\n" );
	for( pp = pt_headp->pt_forw; pp != pt_headp; pp = pp->pt_forw )
		brst_log( "\tregion is '%s',\tid=%d\taircode=%d\n",
			pp->pt_regionp->reg_name,
			(int) pp->pt_regionp->reg_regionid,
			(int) pp->pt_regionp->reg_aircode );
#endif
	/* Output cell identification. */
	prntCellIdent( ap );
	/* Color cell if making frame buffer image. */
	if( fbfile[0] != NUL )
		paintCellFb( ap, pixtarg, zoom == 1 ? pixblack : pixbkgr );

	/* First delete thin partitions. */
	enforceLOS( ap, pt_headp );

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
	for( pp = pt_headp->pt_forw; pp != pt_headp; pp = pp->pt_forw )
		{	fastf_t	los = 0.0;
			int	voidflag = 0;
			register struct partition *np = pp->pt_forw;
			register struct partition *cp;
			register struct region *regp = pp->pt_regionp;
			register struct region *nregp = pp->pt_forw->pt_regionp;
		/* Fill in entry and exit points into hit structures. */
		{	register struct hit *ihitp = pp->pt_inhit;
			register struct hit *ohitp = pp->pt_outhit;
			register struct xray *rayp = &ap->a_ray;
		VJOIN1( ihitp->hit_point, rayp->r_pt, ihitp->hit_dist,
			rayp->r_dir );
		VJOIN1( ohitp->hit_point, rayp->r_pt, ohitp->hit_dist,
			rayp->r_dir );
		colorPartition( regp, C_MAIN );
		plotPartition( ihitp, ohitp, rayp, regp );
		}

		/* Check for voids. */
		if( np != pt_headp )
			{
#if DEBUG_GRID
			brst_log( "\tprocessing region '%s',\tid=%d\taircode=%d\n",
				pp->pt_regionp->reg_name,
				(int) pp->pt_regionp->reg_regionid,
				(int) pp->pt_regionp->reg_aircode );
			brst_log( "\tcheck for voids\n" );
#endif
			los = np->pt_inhit->hit_dist -
					pp->pt_outhit->hit_dist;
#if DEBUG_GRID
			brst_log( "\tlos=%g tolerance=%g\n",
				los, LOS_TOL );
#endif
			voidflag = ( los > LOS_TOL );
			/* If the void occurs adjacent to explicit outside
				air, extend the outside air to fill it. */
			if( OutsideAir( np->pt_regionp ) )
				{
#if DEBUG_GRID
				brst_log( "\t\toutside air\n" );
#endif
				if( voidflag )
					{
					np->pt_inhit->hit_dist =
						pp->pt_outhit->hit_dist;
					voidflag = 0;
					}
				/* Keep going until we are past 01 air. */
				for(	cp = np->pt_forw;
					cp != pt_headp;
					cp = cp->pt_forw )
					{
					if( OutsideAir( cp->pt_regionp ) )
						/* Include outside air. */
						np->pt_outhit->hit_dist =
						cp->pt_outhit->hit_dist;
					else
					if( cp->pt_inhit->hit_dist -
					    np->pt_outhit->hit_dist
						> LOS_TOL )
						/* Include void following
							outside air. */
						np->pt_outhit->hit_dist =
						cp->pt_inhit->hit_dist;
					else
						break;
					}
				}
			}
		/* Merge adjacent inside airs of same type. */
		if( np != pt_headp && InsideAir( np->pt_regionp ) )
			{
#if DEBUG_GRID
			brst_log( "\tmerging inside airs\n" );
#endif
			for(	cp = np->pt_forw;
				cp != pt_headp;
				cp = cp->pt_forw )
				{
				if(	InsideAir( cp->pt_regionp )
				    &&	SameAir(np->pt_regionp, cp->pt_regionp)
				    &&	cp->pt_inhit->hit_dist -
						np->pt_outhit->hit_dist
					<= LOS_TOL )
					np->pt_outhit->hit_dist =
						cp->pt_outhit->hit_dist;
				else
					break;
				}
			}

		/* Check for possible phantom armor before internal air,
			that is if it is the first thing hit. */
		if( pp->pt_back == pt_headp && InsideAir( regp ) )
			{	fastf_t	slos;
			/* If adjacent partitions are the same air, extend
				the first on to include them. */
#if DEBUG_GRID
			brst_log( "\tphantom armor before internal air\n" );
#endif
			for( cp = np; cp != pt_headp; cp = cp->pt_forw )
				{
				if(	InsideAir( cp->pt_regionp )
				    &&	SameAir( regp, cp->pt_regionp )
				    &&	cp->pt_inhit->hit_dist -
						pp->pt_outhit->hit_dist
					<= LOS_TOL )
					pp->pt_outhit->hit_dist =
						cp->pt_outhit->hit_dist;
				else
					break;

				}

			slos = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
			prntPhantom( pp->pt_inhit, (int) regp->reg_aircode,
					slos );
			}
		else
		if( ! Air( regp ) ) /* If we have a component, output it. */
			{	fastf_t entrynorm[3];	/* normal at entry */
				fastf_t exitnorm[3];	/* normal at exit */
			/* Get entry normal. */
			getRtHitNorm( pp->pt_inhit, pp->pt_inseg->seg_stp,
				&ap->a_ray, (boolean) pp->pt_inflip, entrynorm );
			(void) chkEntryNorm( pp, &ap->a_ray, entrynorm,
				"shotline entry normal" );
			/* Get exit normal. */
			getRtHitNorm( pp->pt_outhit, pp->pt_outseg->seg_stp,
				&ap->a_ray, (boolean) pp->pt_outflip, exitnorm );
			(void) chkExitNorm( pp, &ap->a_ray, exitnorm,
				"shotline exit normal" );

#if DEBUG_GRID
			brst_log( "\twe have a component\n" );
#endif
			/* In the case of fragmenting munitions, a hit on any
				component will cause a burst point. */
			if( bp == PT_NULL && bdist > 0.0 )
				{
				bp = pp;	/* register exterior burst */
				CopyVec( burstnorm, exitnorm );
				}

			/* If there is a void, output 01 air as space. */
			if( voidflag )
				{
#if DEBUG_GRID
				brst_log( "\t\tthere is a void, %s\n",
					"so outputting 01 air" );
#endif
				if(	bp == PT_NULL && ! reqburstair
				    &&	findIdents( regp->reg_regionid,
							&armorids ) )
					{ /* Bursting on armor/void (ouchh). */
					bp = pp;
					CopyVec( burstnorm, exitnorm );
					}
				prntSeg( ap, pp, OUTSIDE_AIR,
					entrynorm, exitnorm, pp == bp );
				}
			else
			/* If air expicitly follows, output space code. */
			if( np != pt_headp && Air( nregp ) )
				{
				/* Check for interior burst point. */
#if DEBUG_GRID
				brst_log( "\t\texplicit air follows\n" );
#endif
				if(	bp == PT_NULL && bdist <= 0.0
				    &&	findIdents( regp->reg_regionid,
							&armorids )
				    && ( ! reqburstair
				     ||	findIdents( nregp->reg_aircode,
							&airids ))
					)
					{
					bp = pp; /* register interior burst */
					CopyVec( burstnorm, exitnorm );
					}
				prntSeg( ap, pp, nregp->reg_aircode,
					entrynorm, exitnorm, pp == bp );
				}
			else
			if( np == pt_headp )
				{
				/* Last component gets 09 air. */
#if DEBUG_GRID
				brst_log( "\t\tlast component\n" );
#endif
				prntSeg( ap, pp, EXIT_AIR,
					entrynorm, exitnorm, pp == bp );
				}
			else
			/* No air follows component. */
			if( SameCmp( regp, nregp ) )
				{
#if DEBUG_GRID
				brst_log( "\t\tmerging adjacent components\n" );
#endif
				/* Merge adjacent components with same
					idents. */
				*np->pt_inhit = *pp->pt_inhit;
				np->pt_inseg = pp->pt_inseg;
				np->pt_inflip = pp->pt_inflip;
				continue;
				}
			else
				{
#if DEBUG_GRID
				brst_log( "\t\tdifferent component follows\n" );
#endif
				prntSeg( ap, pp, 0,
					entrynorm, exitnorm, pp == bp );
					/* component follows */
				}
			}
		/* Check for adjacency of differing airs, implicit or
			explicit and output phantom armor as needed. */
		if( InsideAir( regp ) )
			{
#if DEBUG_GRID
			brst_log( "\tcheck for adjacency of differing airs; inside air\n" );
#endif
			/* Inside air followed by implicit outside air. */
			if( voidflag )
				prntPhantom( pp->pt_outhit, OUTSIDE_AIR, los );
			}
		/* Check next partition for adjacency problems. */
		if( np != pt_headp )
			{
#if DEBUG_GRID
			brst_log( "\tcheck next partition for adjacency\n" );
#endif
			/* See if inside air follows impl. outside air. */
			if( voidflag && InsideAir( nregp ) )
				{	fastf_t	slos =
						np->pt_outhit->hit_dist -
						np->pt_inhit->hit_dist;
#if DEBUG_GRID
				brst_log( "\t\tinside air follows impl. outside air\n" );
#endif
				prntPhantom( np->pt_inhit, nregp->reg_aircode,
						slos );
				}
			else
			/* See if differing airs are adjacent. */
			if(   !	voidflag
			    &&	Air( regp )
			    &&	Air( nregp )
			    &&	DiffAir( nregp, regp )
				)
				{	fastf_t slos = np->pt_outhit->hit_dist -
						 np->pt_inhit->hit_dist;
#if DEBUG_GRID
				brst_log( "\t\tdiffering airs are adjacent\n" );
#endif
				prntPhantom( np->pt_inhit,
						(int) nregp->reg_aircode,
						slos );
				}
			}
		/* Output phantom armor if internal air is last hit. */
		if( np == pt_headp && InsideAir( regp ) )
			{
#if DEBUG_GRID
			brst_log( "\tinternal air last hit\n" );
#endif
			prntPhantom( pp->pt_outhit, EXIT_AIR, 0.0 );
			}
		}
	if( nriplevels == 0 )
		return	1;

	if( bp != PT_NULL )
		{	fastf_t burstpt[3];
		/* This is a burst point, calculate coordinates. */
		if( bdist > 0.0 )
			{ /* Exterior burst point (i.e. case-fragmenting
				munition with contact-fuzed set-back device):
				location is bdist prior to entry point. */
			VJOIN1( burstpt, bp->pt_inhit->hit_point, -bdist,
				ap->a_ray.r_dir );
			}
		else
		if( bdist < 0.0 )
			{ /* Interior burst point (i.e. case-fragment
				munition with delayed fuzing): location is
				the magnitude of bdist beyond the exit
				point. */
			VJOIN1( burstpt, bp->pt_outhit->hit_point, -bdist,
				ap->a_ray.r_dir );
			}
		else	  /* Interior burst point: no fuzing offset. */
			CopyVec( burstpt, bp->pt_outhit->hit_point );

		/* Only generate burst rays if nspallrays is greater then
			zero. */
		if( nspallrays < 1 )
			return	1;

		return	burstPoint( ap, burstnorm, burstpt );
		}
	return	1;
	}

/*
	void getRtHitNorm( struct hit *hitp, struct soltab *stp,
			struct xray *rayp, boolean flipped, fastf_t normvec[3] )

	Fill normal and hit point into hit struct and if the flipped
	flag is set, reverse the normal.  Return a private copy of the
	flipped normal in normvec.  NOTE: the normal placed in the hit
	struct should not be modified (ie reversed) by the application
	because it can be instanced by other solids.
 */
void
getRtHitNorm( hitp, stp, rayp, flipped, normvec )
struct hit *hitp;
struct soltab *stp;
struct xray *rayp;
boolean flipped;
fastf_t normvec[3];
	{
	RT_HIT_NORMAL( normvec, hitp, stp, rayp, flipped );
	}

boolean
chkEntryNorm( pp, rayp, normvec, purpose )
struct partition *pp;
struct xray *rayp;
fastf_t normvec[3];
char *purpose;
	{	fastf_t f;
		static int flipct = 0;
		static int totalct = 0;
		struct soltab *stp = pp->pt_inseg->seg_stp;
		boolean ret = 1;
	totalct++;
	/* Dot product of ray direction with normal *should* be negative. */
	f = Dot( rayp->r_dir, normvec );
	if( NearZero( f ) )
		{
#ifdef DEBUG
		brst_log( "chkEntryNorm: near 90 degree obliquity.\n" );
		brst_log( "\tPnt %g,%g,%g\n\tDir %g,%g,%g\n\tNorm %g,%g,%g.\n",
			rayp->r_pt[X], rayp->r_pt[Y], rayp->r_pt[Z],
			rayp->r_dir[X], rayp->r_dir[Y], rayp->r_dir[Z],
			normvec[X], normvec[Y], normvec[Z] );
#endif
		ret = 0;
		}
	if( f > 0.0 )
		{
		flipct++;
		brst_log( "Fixed flipped entry normal:\n" );
		brst_log( "\tregion \"%s\" solid \"%s\" type %d \"%s\".\n",
			pp->pt_regionp->reg_name, stp->st_name,
			stp->st_id, purpose );
#ifdef DEBUG
		brst_log( "\tPnt %g,%g,%g\n\tDir %g,%g,%g\n\tNorm %g,%g,%g.\n",
			rayp->r_pt[X], rayp->r_pt[Y], rayp->r_pt[Z],
			rayp->r_dir[X], rayp->r_dir[Y], rayp->r_dir[Z],
			normvec[X], normvec[Y], normvec[Z] );
		brst_log( "\tDist %g Hit Pnt %g,%g,%g\n",
			pp->pt_inhit->hit_dist,
			pp->pt_inhit->hit_point[X],
			pp->pt_inhit->hit_point[Y],
			pp->pt_inhit->hit_point[Z] );
		brst_log( "\t%d of %d normals flipped.\n", flipct, totalct );
#endif
		ScaleVec( normvec, -1.0 );
		ret = 0;
		}
	return ret;
	}

boolean
chkExitNorm( pp, rayp, normvec, purpose )
struct partition *pp;
struct xray *rayp;
fastf_t normvec[3];
char *purpose;
	{	fastf_t f;
		static int flipct = 0;
		static int totalct = 0;
		struct soltab *stp = pp->pt_outseg->seg_stp;
		boolean ret = 1;
	totalct++;
	/* Dot product of ray direction with normal *should* be positive. */
	f = Dot( rayp->r_dir, normvec );
	if( NearZero( f ) )
		{
#ifdef DEBUG
		brst_log( "chkExitNorm: near 90 degree obliquity.\n" );
		brst_log( "\tPnt %g,%g,%g\n\tDir %g,%g,%g\n\tNorm %g,%g,%g.\n",
			rayp->r_pt[X], rayp->r_pt[Y], rayp->r_pt[Z],
			rayp->r_dir[X], rayp->r_dir[Y], rayp->r_dir[Z],
			normvec[X], normvec[Y], normvec[Z] );
#endif
		ret = 0;
		}
	if( f < 0.0 )
		{
		flipct++;
		brst_log( "Fixed flipped exit normal:\n" );
		brst_log( "\tregion \"%s\" solid \"%s\" type %d \"%s\".\n",
			pp->pt_regionp->reg_name, stp->st_name,
			stp->st_id, purpose );
#ifdef DEBUG
		brst_log( "\tPnt %g,%g,%g\n\tDir %g,%g,%g\n\tNorm %g,%g,%g.\n",
			rayp->r_pt[X], rayp->r_pt[Y], rayp->r_pt[Z],
			rayp->r_dir[X], rayp->r_dir[Y], rayp->r_dir[Z],
			normvec[X], normvec[Y], normvec[Z] );
		brst_log( "\tDist %g Hit Pnt %g,%g,%g\n",
			pp->pt_outhit->hit_dist,
			pp->pt_outhit->hit_point[X],
			pp->pt_outhit->hit_point[Y],
			pp->pt_outhit->hit_point[Z] );
		brst_log( "\t%d of %d normals flipped.\n", flipct, totalct );
#endif
		ScaleVec( normvec, -1.0 );
		ret = 0;
		}
	return ret;
	}

/*
	int f_ShotMiss( register struct application *ap )

	Shot missed the model; if ground bursting is enabled, intersect with
	ground plane, else just arrange for appropriate background color for
	debugging.
 */
static int
f_ShotMiss( ap )
register struct application *ap;
	{
	if( groundburst )
		{	fastf_t dist;
			fastf_t	hitpoint[3];
		/* first find intersection of shot with ground plane */
		if( ap->a_ray.r_dir[Z] >= 0.0 )
			/* Shot direction is upward, can't hit the ground
				from underneath. */
			goto	missed_ground;
		if( ap->a_ray.r_pt[Z] <= -grndht )
			/* Must be above ground to hit it from above. */
			goto	missed_ground;
		/* ground plane is grndht distance below the target origin */
		hitpoint[Z] = -grndht;
		/* distance along ray from ray origin to ground plane */
		dist = (hitpoint[Z] - ap->a_ray.r_pt[Z]) / ap->a_ray.r_dir[Z];
		/* solve for X and Y intersection coordinates */
		hitpoint[X] = ap->a_ray.r_pt[X] + ap->a_ray.r_dir[X]*dist;
		hitpoint[Y] = ap->a_ray.r_pt[Y] + ap->a_ray.r_dir[Y]*dist;
		/* check for limits of ground plane */
		if(	hitpoint[X] <= grndfr && hitpoint[X] >= -grndbk
		    &&	hitpoint[Y] <= grndlf && hitpoint[Y] >= -grndrt
			) /* We have a hit. */
			{
			if( fbfile[0] != NUL )
				paintCellFb( ap, pixghit,
					zoom == 1 ? pixblack : pixbkgr );
			if( bdist > 0.0 )
				{ /* simulate standoff fuzing */
				VJOIN1( hitpoint, hitpoint, -bdist,
					ap->a_ray.r_dir );
				}
			else
			if( bdist < 0.0 )
				{ /* interior burst not implemented in ground */
				brst_log( "User error: %s %s.\n",
					"negative burst distance can not be",
					"specified with ground plane bursting"
					);
				fatalerror = 1;
				return	-1;
				}
			/* else bdist == 0.0, no adjustment necessary */
			/* only burst if nspallrays greater than zero */
			if( nspallrays > 0 )
				{
				prntBurstHdr( hitpoint, viewdir );
				return	burstPoint( ap, zaxis, hitpoint );
				}
			else
				return	1;
			}
		}
missed_ground :
	if( fbfile[0] != NUL )
		paintCellFb( ap, pixmiss, zoom == 1 ? pixblack : pixbkgr );
	VSETALL( ap->a_color, 0.0 ); /* All misses black. */
	return	0;
	}

/*
	int f_BurstMiss( register struct application *ap )

	Burst ray missed the model, so do nothing.
 */
static int
f_BurstMiss( ap )
register struct application *ap;
	{
	VSETALL( ap->a_color, 0.0 ); /* All misses black. */
	return	0;
	}

/*
	int getRayOrigin( register struct application *ap )

	This routine fills in the ray origin ap->a_ray.r_pt by folding
	together firing mode and dithering options. By-products of this
	routine include the grid offsets which are stored in ap->a_uvec,
	2-digit random numbers (when opted) which are stored in ap->a_user,
	and grid indices are stored in ap->a_x and ap->a_y.  Return
	codes are: 0 for failure to read new firing coordinates, or
	1 for success.
 */
static int
getRayOrigin( ap )
register struct application	*ap;
	{	register fastf_t	*vec = ap->a_uvec;
		fastf_t			gridyinc[3], gridxinc[3];
		fastf_t			scalecx, scalecy;
	if( TSTBIT(firemode,FM_SHOT) )
		{
		if( TSTBIT(firemode,FM_FILE) )
			{
			switch( readShot( vec ) )
				{
			case EOF :	return	EOF;
			case 1 :	break;
			case 0 :	return	0;
				}
			}
		else	/* Single shot specified. */
			CopyVec( vec, fire );
		if( TSTBIT(firemode,FM_3DIM) )
			{	fastf_t	hitpoint[3];
			/* Project 3-d hit-point back into grid space. */
			CopyVec( hitpoint, vec );
			vec[X] = Dot( gridhor, hitpoint );
			vec[Y] = Dot( gridver, hitpoint );
			}
		ap->a_x = vec[X] / cellsz;
		ap->a_y = vec[Y] / cellsz;
		scalecx = vec[X];
		scalecy = vec[Y];
		}
	else
		{	fastf_t xoffset = 0.0;
			fastf_t yoffset = 0.0;
		ap->a_x = currshot % gridwidth + gridxorg;
		ap->a_y = currshot / gridwidth + gridyorg;
		if( dithercells )
			{
			/* 2-digit random number, 1's place gives X
				offset, 10's place gives Y offset.
			 */
#ifdef SYSV /* Use lrand48() only if random() not available.  */
			ap->a_user = lrand48() % 100;
#else
			ap->a_user = random() % 100;
#endif
			xoffset = (ap->a_user%10)*0.1 - 0.5;
			yoffset = (ap->a_user/10)*0.1 - 0.5;
			}
		/* Compute magnitude of grid offsets. */
		scalecx = (fastf_t) ap->a_x + xoffset;
		scalecy = (fastf_t) ap->a_y + yoffset;
		vec[X] = scalecx *= cellsz;
		vec[Y] = scalecy *= cellsz;
		}
	/* Compute cell horizontal and vertical	vectors relative to
		grid origin. */
	Scale2Vec( gridhor, scalecx, gridxinc );
	Scale2Vec( gridver, scalecy, gridyinc );
	Add2Vec( gridsoff, gridyinc, ap->a_ray.r_pt );
	AddVec( ap->a_ray.r_pt, gridxinc );
	return	1;
	}

/*
	void gridInit( void )

	Grid initialization routine; must be done once per view.
 */
void
gridInit()
	{
	notify( "Initializing grid", NOTIFY_APPEND );
	rt_prep_timer();

#if DEBUG_SHOT
	if( TSTBIT(firemode,FM_BURST) )
		brst_log( "gridInit: reading burst points.\n" );
	else	{
		if( TSTBIT(firemode,FM_SHOT) )
			brst_log( "gridInit: shooting discrete shots.\n" );
		else
			brst_log( "gridInit: shooting %s.\n",
				TSTBIT(firemode,FM_PART) ?
				"partial envelope" : "full envelope" );
		}
	if( TSTBIT(firemode,FM_BURST) || TSTBIT(firemode,FM_SHOT) )
		{
		brst_log( "gridInit: reading %s coordinates from %s.\n",
			TSTBIT(firemode,FM_3DIM) ? "3-d" : "2-d",
			TSTBIT(firemode,FM_FILE) ? "file" : "command stream" );

		}
	else
	if( TSTBIT(firemode,FM_FILE) || TSTBIT(firemode,FM_3DIM) )
		brst_log( "BUG: insane combination of fire mode bits:0x%x\n",
			firemode );
	if( TSTBIT(firemode,FM_BURST) || shotburst )
		nriplevels = 1;
	else
		nriplevels = 0;
	if( !shotburst && groundburst )
		{
		(void) sprintf( scrbuf,
				"Ground bursting directive ignored: %s.\n",
				"only relevant if bursting along shotline" );
		warning( scrbuf );
		brst_log( scrbuf );
		}
#endif
	/* compute grid unit vectors */
	gridRotate( viewazim, viewelev, 0.0, gridhor, gridver );

	if( yaw != 0.0 || pitch != 0.0 )
		{	fastf_t	negsinyaw = -sin( yaw );
			fastf_t	sinpitch = sin( pitch );
			fastf_t	xdeltavec[3], ydeltavec[3];
#if DEBUG_SHOT
		brst_log( "gridInit: canting warhead\n" );
#endif
		cantwarhead = 1;
		Scale2Vec( gridhor,  negsinyaw, xdeltavec );
		Scale2Vec( gridver,  sinpitch,  ydeltavec );
		Add2Vec( xdeltavec, ydeltavec, cantdelta );
		}

	/* unit vector from origin of model toward eye */
	consVector( viewdir, viewazim, viewelev );

	/* reposition file pointers if necessary */
	if( TSTBIT(firemode,FM_SHOT) && TSTBIT(firemode,FM_FILE) )
		rewind( shotfp );
	else
	if( TSTBIT(firemode,FM_BURST) && TSTBIT(firemode,FM_FILE) )
		rewind( burstfp );

	/* Compute distances from grid origin (model origin) to each
		border of grid, and grid indices at borders of grid.
	 */
	if( ! TSTBIT(firemode,FM_PART) )
		{	fastf_t modelmin[3];
			fastf_t modelmax[3];
		if( groundburst )
			{ /* extend grid to include ground platform */
			modelmax[X] = Max( rtip->mdl_max[X], grndfr );
			modelmin[X] = Min( rtip->mdl_min[X], -grndbk );
			modelmax[Y] = Max( rtip->mdl_max[Y], grndlf );
			modelmin[Y] = Min( rtip->mdl_min[Y], -grndrt );
			modelmax[Z] = rtip->mdl_max[Z];
			modelmin[Z] = Min( rtip->mdl_min[Z], -grndht );
			}
		else
			{ /* size grid by model RPP */
			CopyVec( modelmin, rtip->mdl_min );
			CopyVec( modelmax, rtip->mdl_max );
			}
		/* Calculate extent of grid. */
		gridrt = max(	gridhor[X] * modelmax[X],
				gridhor[X] * modelmin[X]
				) +
			  max(	gridhor[Y] * modelmax[Y],
				gridhor[Y] * modelmin[Y]
				) +
			  max(	gridhor[Z] * modelmax[Z],
				gridhor[Z] * modelmin[Z]
				);
		gridlf = min(	gridhor[X] * modelmax[X],
				gridhor[X] * modelmin[X]
				) +
			  min(	gridhor[Y] * modelmax[Y],
				gridhor[Y] * modelmin[Y]
				) +
			  min(	gridhor[Z] * modelmax[Z],
				gridhor[Z] * modelmin[Z]
				);
		gridup = max(	gridver[X] * modelmax[X],
				gridver[X] * modelmin[X]
				) +
			  max(	gridver[Y] * modelmax[Y],
				gridver[Y] * modelmin[Y]
				) +
			  max(	gridver[Z] * modelmax[Z],
				gridver[Z] * modelmin[Z]
				);
		griddn = min(	gridver[X] * modelmax[X],
				gridver[X] * modelmin[X]
				) +
			  min(	gridver[Y] * modelmax[Y],
				gridver[Y] * modelmin[Y]
				) +
			  min(	gridver[Z] * modelmax[Z],
				gridver[Z] * modelmin[Z]
				);
		/* Calculate extent of model in plane of grid. */
		if( groundburst )
			{
			modlrt = max(	gridhor[X] * rtip->mdl_max[X],
					gridhor[X] * rtip->mdl_min[X]
					) +
				  max(	gridhor[Y] * rtip->mdl_max[Y],
					gridhor[Y] * rtip->mdl_min[Y]
					) +
				  max(	gridhor[Z] * rtip->mdl_max[Z],
					gridhor[Z] * rtip->mdl_min[Z]
					);
			modllf = min(	gridhor[X] * rtip->mdl_max[X],
					gridhor[X] * rtip->mdl_min[X]
					) +
				  min(	gridhor[Y] * rtip->mdl_max[Y],
					gridhor[Y] * rtip->mdl_min[Y]
					) +
				  min(	gridhor[Z] * rtip->mdl_max[Z],
					gridhor[Z] * rtip->mdl_min[Z]
					);
			modlup = max(	gridver[X] * rtip->mdl_max[X],
					gridver[X] * rtip->mdl_min[X]
					) +
				  max(	gridver[Y] * rtip->mdl_max[Y],
					gridver[Y] * rtip->mdl_min[Y]
					) +
				  max(	gridver[Z] * rtip->mdl_max[Z],
					gridver[Z] * rtip->mdl_min[Z]
					);
			modldn = min(	gridver[X] * rtip->mdl_max[X],
					gridver[X] * rtip->mdl_min[X]
					) +
				  min(	gridver[Y] * rtip->mdl_max[Y],
					gridver[Y] * rtip->mdl_min[Y]
					) +
				  min(	gridver[Z] * rtip->mdl_max[Z],
					gridver[Z] * rtip->mdl_min[Z]
				);
			}
		else
			{
			modlrt = gridrt;
			modllf = gridlf;
			modlup = gridup;
			modldn = griddn;
			}
		}
	gridxorg = gridlf / cellsz;
	gridxfin = gridrt / cellsz;
	gridyorg = griddn / cellsz;
	gridyfin = gridup / cellsz;

	/* allow for randomization of cells */
	if( dithercells )
		{
		gridxorg--;
		gridxfin++;
		gridyorg--;
		gridyfin++;
		}
#if DEBUG_SHOT
	brst_log( "gridInit: xorg,xfin,yorg,yfin=%d,%d,%d,%d\n",
		gridxorg, gridxfin, gridyorg, gridyfin );
	brst_log( "gridInit: left,right,down,up=%g,%g,%g,%g\n",
		gridlf, gridrt, griddn, gridup );
#endif

	/* compute stand-off distance */
	standoff = max(	viewdir[X] * rtip->mdl_max[X],
			viewdir[X] * rtip->mdl_min[X]
			) +
		  max(	viewdir[Y] * rtip->mdl_max[Y],
			viewdir[Y] * rtip->mdl_min[Y]
			) +
		  max(	viewdir[Z] * rtip->mdl_max[Z],
			viewdir[Z] * rtip->mdl_min[Z]
			);

	/* determine largest grid dimension for frame buffer display */
	gridwidth  = gridxfin - gridxorg + 1;
	gridheight = gridyfin - gridyorg + 1;
	gridsz = Max( gridwidth, gridheight );

	/* vector to grid origin from model origin */
	Scale2Vec( viewdir, standoff, gridsoff );

	/* direction of grid rays */
	ScaleVec( viewdir, -1.0 );

	prntTimer( "grid" );
	notify( NULL, NOTIFY_DELETE );
	return;
	}

/*
	void gridModel( void )

	This routine dispatches the top-level ray tracing task.
 */
void
gridModel()
	{
	ag.a_onehit = 0;
	ag.a_overlap = reportoverlaps ? f_Overlap : f_HushOverlap;
	ag.a_logoverlap = rt_silent_logoverlap;
	ag.a_rt_i = rtip;
	if( ! TSTBIT(firemode,FM_BURST) )
		{ /* set up for shotlines */
		ag.a_hit = f_ShotHit;
		ag.a_miss = f_ShotMiss;
		}

	plotInit();	/* initialize plot file if appropriate */

	if( ! imageInit() ) /* initialize frame buffer if appropriate */
		{
		warning( "Error: problem opening frame buffer." );
		return;
		}
	/* output initial line for this aspect */
	prntAspectInit();

	fatalerror = 0;
	userinterrupt = 0;	/* set by interrupt handler */

	rt_prep_timer();
	notify( "Raytracing", NOTIFY_ERASE );

	if( TSTBIT(firemode,FM_BURST) )  {
		if( ! doBursts() )
			return;
		else
			goto	endvu;
	}

	/* get starting and ending shot number */
	currshot = 0;
	lastshot = gridwidth * gridheight - 1;

	/* SERIAL case -- one CPU does all the work */
	if( ! gridShot() )
		return;
endvu:	view_end();
	return;
	}

/*
	boolean gridShot( void )

	This routine is the grid-level raytracing task; suitable for a
	multi-tasking process.  Return code of 0 would indicate a
	failure in the application routine given to rt_shootray() or an
	error or EOF in getting the next set of firing coordinates.
 */
static boolean
gridShot()
	{	boolean status = 1;
		struct application a;
	a = ag;
	a.a_resource = RESOURCE_NULL;

	assert( a.a_hit == ag.a_hit );
	assert( a.a_miss == ag.a_miss );
	assert( a.a_overlap == ag.a_overlap );
	assert( a.a_rt_i == ag.a_rt_i );
	assert( a.a_onehit == ag.a_onehit );
	a.a_user = 0;
	a.a_purpose = "shotline";
	prntGridOffsets( gridxorg, gridyorg );
	noverlaps = 0;
	for( ; ! userinterrupt; view_pix( &a ) )
		{
		if( ! TSTBIT(firemode,FM_SHOT) && currshot > lastshot )
			break;
		if( ! (status = getRayOrigin( &a )) || status == EOF )
			break;
		currshot++;
		prntFiringCoords( a.a_uvec );
		CopyVec( a.a_ray.r_dir, viewdir );
		a.a_level = 0;	 /* initialize recursion level */
		plotGrid( a.a_ray.r_pt );
		if( rt_shootray( &a ) == -1 && fatalerror )
			{
			/* fatal error in application routine */
			brst_log( "Fatal error: raytracing aborted.\n" );
			return	0;
			}
		if( ! TSTBIT(firemode,FM_FILE) && TSTBIT(firemode,FM_SHOT) )
			{
			view_pix( &a );
			break;
			}
		}
	return	status == EOF ? 1 : status;
	}

/*
	void lgtModel( register struct application *ap, struct partition *pp,
			struct hit *hitp, struct xray *rayp,
			fastf_t surfnorm[3] )

	This routine is a simple lighting model which places RGB coefficients
	(0 to 1) in ap->a_color based on the cosine of the angle between
	the surface normal and viewing direction and the color associated with
	the component.  Also, the distance to the surface is placed in
	ap->a_cumlen so that the impact location can be projected into grid
	space.
 */
static void
lgtModel( ap, pp, hitp, rayp, surfnorm )
register struct application *ap;
struct partition *pp;
struct hit *hitp;
struct xray *rayp;
fastf_t surfnorm[3];
	{	Colors  *colorp;
		fastf_t intensity = -Dot( viewdir, surfnorm );
	if( intensity < 0.0 )
		intensity = -intensity;

	if(	(colorp =
		findColors( pp->pt_regionp->reg_regionid, &colorids ))
		!= COLORS_NULL
		)
		{
		ap->a_color[RED] = (fastf_t)colorp->c_rgb[RED]/255.0;
		ap->a_color[GRN] = (fastf_t)colorp->c_rgb[GRN]/255.0;
		ap->a_color[BLU] = (fastf_t)colorp->c_rgb[BLU]/255.0;
		}
	else
		ap->a_color[RED] =
		ap->a_color[GRN] =
		ap->a_color[BLU] = 1.0;
	ScaleVec( ap->a_color, intensity );
	ap->a_cumlen = hitp->hit_dist;
	}

/*
	fastf_t max( fastf_t a, fastf_t b )

	Returns the maximum of a and b.  Useful when a macro would
	cause side-effects or redundant computation.
 */
static fastf_t
max( a, b )
fastf_t	a, b;
	{
	return	Max( a, b );
	}

/*
	fastf_t min( fastf_t a, fastf_t b )

	Returns the minimum of a and b.  Useful when a macro would
	cause side-effects or redundant computation.
 */
static fastf_t
min( a, b )
fastf_t	a, b;
	{
	return	Min( a, b );
	}

/*
	int readBurst( register fastf_t *vec )

	This routine reads the next set of burst point coordinates from the
	input stream burstfp.  Returns 1 for success, 0 for a format
	error and EOF for normal end-of-file.  If 0 is returned,
	fatalerror will be set to 1.
 */
static int
readBurst( vec )
register fastf_t	*vec;
	{	int	items;
	assert( burstfp != (FILE *) NULL );
	/* read 3D firing coordinates from input stream */
	if( (items =
#if SINGLE_PRECISION
	     fscanf( burstfp, "%f %f %f", &vec[X], &vec[Y], &vec[Z] )) != 3 )
#else
	     fscanf( burstfp, "%lf %lf %lf", &vec[X], &vec[Y], &vec[Z] )) != 3 )
#endif
		{
		if( items != EOF )
			{
			brst_log( "Fatal error: %d burst coordinates read.\n",
				items );
			fatalerror = 1;
			return	0;
			}
		else
			return	EOF;
		}
	else
		{
		vec[X] /= unitconv;
		vec[Y] /= unitconv;
		vec[Z] /= unitconv;
		}
	return	1;
	}

/*
	int readShot( register fastf_t *vec )

	This routine reads the next set of firing coordinates from the
	input stream shotfp, using the format selected by the firemode
	bitflag.  Returns 1 for success, 0 for a format error and EOF
	for normal end-of-file.  If 0 is returned, fatalerror will be
	set to 1.
 */
static int
readShot( vec )
register fastf_t	*vec;
	{
	assert( shotfp != (FILE *) NULL );
	if( ! TSTBIT(firemode,FM_3DIM) ) /* absence of 3D flag means 2D */
		{	int	items;
		/* read 2D firing coordinates from input stream */
		if( (items =
#if SINGLE_PRECISION
		     fscanf( shotfp, "%f %f", &vec[X], &vec[Y] )) != 2 )
#else
		     fscanf( shotfp, "%lf %lf", &vec[X], &vec[Y] )) != 2 )
#endif
			{
			if( items != EOF )
				{
				brst_log( "Fatal error: only %d firing coordinates read.\n", items );
				fatalerror = 1;
				return	0;
				}
			else
				return	EOF;
			}
		else
			{
			vec[X] /= unitconv;
			vec[Y] /= unitconv;
			}
		}
	else
	if( TSTBIT(firemode,FM_3DIM) ) /* 3D coordinates */
		{	int	items;
		/* read 3D firing coordinates from input stream */
		if( (items =
#if SINGLE_PRECISION
		     fscanf( shotfp, "%f %f %f", &vec[X], &vec[Y], &vec[Z] )) != 3 )
#else
		     fscanf( shotfp, "%lf %lf %lf", &vec[X], &vec[Y], &vec[Z] )) != 3 )
#endif

			{
			if( items != EOF )
				{
				brst_log( "Fatal error: %d firing coordinates read.\n", items );
				fatalerror = 1;
				return	0;
				}
			else
				return	EOF;
			}
		else
			{
			vec[X] /= unitconv;
			vec[Y] /= unitconv;
			vec[Z] /= unitconv;
			}
		}
	else
		{
		brst_log( "BUG: readShot called with bad firemode.\n" );
		return	0;
		}
	return	1;
	}

/*
	int roundToInt( fastf_t f )

	RETURN CODES: the nearest integer to f.
 */
int roundToInt( fastf_t f )
	{	register int a;
	a = f;
	if( f - a >= 0.5 )
		return	a + 1;
	else
		return	a;
	}

/*
	void spallInit( void )

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
spallInit()
	{	fastf_t	theta;	 /* solid angle of spall sampling cone */
		fastf_t phi;	 /* angle between ray and cone axis */
		fastf_t philast; /* guard against floating point error */
		int spallct = 0; /* actual no. of sampling rays */
		int n;
	if( nspallrays < 1 )
		{
		delta = 0.0;
		phiinc = 0.0;
		raysolidangle = 0.0;
		brst_log( "%d sampling rays\n", spallct );
		return;
		}

	/* Compute sampling cone of rays which are equally spaced. */
	theta = TWO_PI * (1.0 - cos( conehfangle )); /* solid angle */
	delta = sqrt( theta/nspallrays ); /* angular ray delta */
	n = conehfangle / delta;
	phiinc = conehfangle / n;
	philast = conehfangle + EPSILON;
	/* Crank through spall cone generation once to count actual number
		generated.
	 */
	for( phi = 0.0; phi <= philast; phi += phiinc )
		{	fastf_t	sinphi = sin( phi );
			fastf_t	gamma, gammainc, gammalast;
			int m;
		sinphi = Abs( sinphi );
		m = (TWO_PI * sinphi)/delta + 1;
		gammainc = TWO_PI / m;
		gammalast = TWO_PI-gammainc+EPSILON;
		for( gamma = 0.0; gamma <= gammalast; gamma += gammainc )
			spallct++;
		}
	raysolidangle = theta / spallct;
	brst_log( "Solid angle of sampling cone = %g\n", theta );
	brst_log( "Solid angle per sampling ray = %g\n", raysolidangle );
	brst_log( "%d sampling rays\n", spallct );
	return;
	}

/* To facilitate one-time per burst point initialization of the spall
	ray application structure while leaving burstRay() with the
	capability of being used as a multitasking process, a_burst must
	be accessible by both the burstPoint() and burstRay() routines, but
	can be local to this module. */
static struct application	a_burst; /* prototype spall ray */

/*
	int burstPoint( register struct application *ap,
			register fastf_t *normal, register fastf_t *bpt )

	This routine dispatches the burst point ray tracing task burstRay().
	RETURN CODES:	0 for fatal ray tracing error, 1 otherwise.
 */
static boolean
burstPoint( ap, normal, bpt )
register struct application *ap;
register fastf_t *normal;
register fastf_t *bpt; /* burst point coordinates */
	{
	a_burst = *ap;
	a_burst.a_miss = f_BurstMiss;
	a_burst.a_hit = f_BurstHit;
	a_burst.a_level++;
	a_burst.a_user = 0; /* ray number */
	a_burst.a_purpose = "spall ray";
	assert( a_burst.a_overlap == ap->a_overlap );

	/* If pitch or yaw is specified, cant the main penetrator
		axis. */
	if( cantwarhead )
		{
		AddVec( a_burst.a_ray.r_dir, cantdelta );
		V_Length( a_burst.a_ray.r_dir, 1.0 );
		}
	/* If a deflected cone is specified (the default) the spall cone
		axis is half way between the main penetrator axis and exit
		normal of the spalling component.
	 */
	if( deflectcone )
		{
		AddVec( a_burst.a_ray.r_dir, normal );
		V_Length( a_burst.a_ray.r_dir, 1.0 );
		}
	CopyVec( a_burst.a_ray.r_pt, bpt );

	comphi = 0.0; /* Initialize global for concurrent access. */

	/* SERIAL case -- one CPU does all the work. */
	return	burstRay();
	}

static boolean
burstRay()
	{ /* Need local copy of all but readonly variables for concurrent
		threads of execution. */
		struct application	a_spall;
		fastf_t			phi;
		boolean			hitcrit = 0;
	a_spall = a_burst;
	a_spall.a_resource = RESOURCE_NULL;
	for( ; ! userinterrupt; )
		{	fastf_t	sinphi;
			fastf_t	gamma, gammainc, gammalast;
			register int done;
			int m;
		bu_semaphore_acquire( RT_SEM_WORKER );
		phi = comphi;
		comphi += phiinc;
		done = phi > conehfangle;
		bu_semaphore_release( RT_SEM_WORKER );
		if( done )
			break;
		sinphi = sin( phi );
		sinphi = Abs( sinphi );
		m = (TWO_PI * sinphi)/delta + 1;
		gammainc = TWO_PI / m;
		gammalast = TWO_PI - gammainc + EPSILON;
		for( gamma = 0.0; gamma <= gammalast; gamma += gammainc )
			{	register int	ncrit;
			spallVec( a_burst.a_ray.r_dir, a_spall.a_ray.r_dir,
				 phi, gamma );
			plotRay( &a_spall.a_ray );
			bu_semaphore_acquire( RT_SEM_WORKER );
			a_spall.a_user = a_burst.a_user++;
			bu_semaphore_release( RT_SEM_WORKER );
			if(	(ncrit = rt_shootray( &a_spall )) == -1
			     &&	fatalerror )
				{
				/* Fatal error in application routine. */
				brst_log( "Error: ray tracing aborted.\n" );
				return	0;
				}
			if( fbfile[0] != NUL && ncrit > 0 )
				{
				paintSpallFb( &a_spall );
				hitcrit = 1;
				}
			if( histfile[0] != NUL )
				{
				bu_semaphore_acquire( BU_SEM_SYSCALL );
				(void) fprintf( histfp, "%d\n", ncrit );
				bu_semaphore_release( BU_SEM_SYSCALL );
				}
			}
		}
	if( fbfile[0] != NUL )
		{
		if( hitcrit )
			paintCellFb( &a_spall, pixcrit, pixtarg );
		else
			paintCellFb( &a_spall, pixbhit, pixtarg );
		}
	return	1;
	}

static void
spallVec( dvec, s_rdir, phi, gamma )
register fastf_t	*dvec, *s_rdir;
fastf_t			phi, gamma;
	{	fastf_t			cosphi = cos( phi );
		fastf_t			singamma = sin( gamma );
		fastf_t			cosgamma = cos( gamma );
		fastf_t			csgaphi, ssgaphi;
		fastf_t			sinphi = sin( phi );
		fastf_t			cosdphi[3];
		fastf_t			fvec[3];
		fastf_t			evec[3];

	if(	AproxEqVec( dvec, zaxis, VEC_TOL )
	    ||	AproxEqVec( dvec, negzaxis, VEC_TOL )
		)
		{
		CopyVec( evec, xaxis );
		}
	else
		{
		CrossProd( dvec, zaxis, evec );
		}
	CrossProd( evec, dvec, fvec );
	Scale2Vec( dvec, cosphi, cosdphi );
	ssgaphi = singamma * sinphi;
	csgaphi = cosgamma * sinphi;
	VJOIN2( s_rdir, cosdphi, ssgaphi, evec, csgaphi, fvec );
	V_Length( s_rdir, 1.0 ); /* unitize */
	return;
	}

/*	c o n s _ V e c t o r ( )
	Construct a direction vector out of azimuth and elevation angles
	in radians, allocating storage for it and returning its address.
 */
static void
consVector( vec, azim, elev )
register fastf_t	*vec;
fastf_t	azim, elev;
	{ /* Store cosine of the elevation to save calculating twice. */
		fastf_t	cosE;
	cosE = cos( elev );
	vec[0] = cos( azim ) * cosE;
	vec[1] = sin( azim ) * cosE;
	vec[2] = sin( elev );
	return;
	}

void
abort_RT( int sig )
	{
	(void) signal( SIGINT, abort_RT );
	userinterrupt = 1;
	return;
	}

#if 0
/*	i p o w ( )
	Integer exponent pow() function.
	Returns d to the nth power.
 */
static fastf_t
ipow( d, n )
register fastf_t	d;
register int	n;
	{	register fastf_t	result = 1.0;
	if( d == 0.0 )
		return	0.0;
	while( n-- > 0 )
		result *= d;
	return	result;
	}
#endif

/*	v i e w _ p i x ( ) */
static void
view_pix( ap )
register struct application	*ap;
	{
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	if( ! TSTBIT(firemode,FM_BURST) )
		prntGridOffsets( ap->a_x, ap->a_y );
	if( tty )
		prntTimer( NULL );
	bu_semaphore_release( BU_SEM_SYSCALL );
	return;
	}

/*	v i e w _ e n d ( ) */
static void
view_end()
	{
	if( gridfile[0] != NUL )
		(void) fflush( gridfp );
	if( histfile[0] != NUL )
		(void) fflush( histfp );
	if( plotfile[0] != NUL )
		(void) fflush( plotfp );
	if( outfile[0] != NUL )
		(void) fflush( outfp );
	if( shotlnfile[0] != NUL )
		(void) fflush( shotlnfp );
	prntTimer( "view" );
	if( noverlaps > 0 )
		brst_log( "%d overlaps detected over %g mm thick.\n",
			noverlaps, OVERLAP_TOL );
	return;
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
