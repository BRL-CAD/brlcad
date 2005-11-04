/*                          P R N T . C
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
/** @file prnt.c
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
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "./vecmath.h"
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "libtermio.h"

#include "./Sc.h"
#include "./ascii.h"
#include "./extern.h"


#define MAX_COLS	128

#define PHANTOM_ARMOR	111

static fastf_t getNormThickness();

int
doMore( linesp )
int	*linesp;
	{	register int	ret = 1;
	if( ! tty )
		return	1;
	save_Tty( HmTtyFd );
	set_Raw( HmTtyFd );
	clr_Echo( HmTtyFd );
	ScSetStandout();
	prompt( "-- More -- " );
	ScClrStandout();
	(void) fflush( stdout );
	switch( HmGetchar() )
		{
	case 'q' :
	case 'n' :
		ret = 0;
		break;
	case LF :
	case CR :
		*linesp = 1;
		break;
	default :
		*linesp = (PROMPT_Y-SCROLL_TOP);
		break;
		}
	reset_Tty( HmTtyFd );
	return	ret;
	}

static int
f_Nerror( ap )
struct application *ap;
	{
	brst_log( "Couldn't compute thickness or exit point %s.\n",
		"along normal direction" );
	V_Print( "\tpnt", ap->a_ray.r_pt, brst_log );
	V_Print( "\tdir", ap->a_ray.r_dir, brst_log );
	ap->a_rbeam = 0.0;
	return	0;
	}

/*	f_Normal()

	Shooting from surface of object along reversed entry normal to
	compute exit point along normal direction and normal thickness.
	Thickness returned in "a_rbeam".
 */
static int
f_Normal( ap, pt_headp, segp )
struct application *ap;
struct partition *pt_headp;
struct seg *segp;
	{	register struct partition *pp = pt_headp->pt_forw;
		register struct partition *cp;
		register struct hit *ohitp;
	for(	cp = pp->pt_forw;
		cp != pt_headp && SameCmp( pp->pt_regionp, cp->pt_regionp );
		cp = cp->pt_forw
		)
		;
	ohitp = cp->pt_back->pt_outhit;
	ap->a_rbeam = ohitp->hit_dist - pp->pt_inhit->hit_dist;
#ifdef VDEBUG
	brst_log( "f_Normal: thickness=%g dout=%g din=%g\n",
		ap->a_rbeam*unitconv, ohitp->hit_dist, pp->pt_inhit->hit_dist );
#endif
	return	1;
	}


void
locPerror( msg )
char    *msg;
	{
	if( errno > 0 )
#ifdef HAVE_STRERROR
		brst_log( "%s: %s\n", msg, strerror(errno) );
#else
#  ifdef _WIN32
		brst_log( "%s: %s\n", msg, _sys_errlist[errno] );
#  else
		brst_log( "%s: %s\n", msg, sys_errlist[errno] );
#  endif
#endif
	else
		brst_log( "BUG: errno not set, shouldn't call perror.\n" );
	return;
	}

int
notify( str, mode )
char    *str;
int	mode;
	{       register int    i;
		static int      lastlen = -1;
		register int    len;
		static char	buf[LNBUFSZ] = { 0 };
		register char	*p='\0';
	if( ! tty )
		return	0;
	switch( mode )
		{
	case NOTIFY_APPEND :
		p = buf + lastlen;
		break;
	case NOTIFY_DELETE :
		for( p = buf+lastlen; p > buf && *p != NOTIFY_DELIM; p-- )
			;
		break;
	case NOTIFY_ERASE :
		p = buf;
		break;
		}
	if( str != NULL )
		{
		if( p > buf )
			*p++ = NOTIFY_DELIM;
		(void) strcpy( p, str );
		}
	else
		*p = NUL;
	(void) ScMvCursor( PROMPT_X, PROMPT_Y );
	len = strlen( buf );
	if( len > 0 )
		{
		(void) ScSetStandout();
		(void) fputs( buf, stdout );
		(void) ScClrStandout();
		}

	/* Blank out remainder of previous command. */
	for( i = len; i < lastlen; i++ )
		(void) putchar( ' ' );
	(void) ScMvCursor( PROMPT_X, PROMPT_Y );
	(void) fflush( stdout );
	lastlen = len;
	return	1;
	}

/*
	void prntAspectInit( void )

	Burst Point Library and Shotline file: header record for each view.
	Ref. Figure 20., Line Number 1 and Figure 19., Line Number 1 of ICD.
 */
void
prntAspectInit()
	{	fastf_t projarea;	/* projected area */
	/* Convert to user units before squaring cell size. */
	projarea = cellsz*unitconv;
	projarea *= projarea;
	if(	outfile[0] != NUL
	    &&	fprintf(outfp,
			"%c % 9.4f % 8.4f % 5.2f % 10.2f %-6s % 9.6f\n",
			PB_ASPECT_INIT,
			viewazim*DEGRAD, /* attack azimuth in degrees */
			viewelev*DEGRAD, /* attack elevation in degrees */
			bdist*unitconv,  /* BDIST */
			projarea, /* projected area associated with burst pt. */
			units == U_INCHES ?      "inches" :
			units == U_FEET ?        "feet" :
			units == U_MILLIMETERS ? "mm" :
			units == U_CENTIMETERS ? "cm" :
			units == U_METERS ?      "meters" : "units?",
			raysolidangle
			) < 0
		)
		{
		brst_log( "Write failed to file (%s)!\n", outfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	if(	shotlnfile[0] != NUL
	    &&	fprintf(shotlnfp,
		     "%c % 9.4f % 8.4f % 7.2f % 7.2f %7.2f %7.2f %7.2f %-6s\n",
			PS_ASPECT_INIT,
			viewazim*DEGRAD, /* attack azimuth in degrees */
			viewelev*DEGRAD, /* attack elevation in degrees */
			cellsz*unitconv, /* shotline separation */
			modlrt*unitconv, /* maximum Y'-coordinate of target */
			modllf*unitconv, /* minimum Y'-coordinate of target */
			modlup*unitconv, /* maximum Z'-coordinate of target */
			modldn*unitconv, /* minimum Z'-coordinate of target */
			units == U_INCHES ?      "inches" :
			units == U_FEET ?        "feet" :
			units == U_MILLIMETERS ? "mm" :
			units == U_CENTIMETERS ? "cm" :
			units == U_METERS ?      "meters" : "units?"
			) < 0
		)
		{
		brst_log( "Write failed to file (%s)!\n", shotlnfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	return;
	}

/*
	void prntBurstHdr( fastf_t *bpt, fastf_t *shotdir )

	This routine must be called before bursting when doing either a
	ground plane burst or bursting at user-specified coordinates.  The
	purpose is to output fake PB_CELL_IDENT and PB_RAY_INTERSECT records
	to the Burst Point Library so that the coordinates of the burst point
	can be made available.

 */
void
prntBurstHdr( bpt, shotdir )
fastf_t *bpt;		/* burst point in model coords */
fastf_t *shotdir;	/* shotline direction vector */
	{	fastf_t vec[3];
	/* Transform burst point (model coordinate system) into the shotline
	   coordinate system. */
	vec[Y] = Dot( gridhor, bpt );	/* Y' */
	vec[Z] = Dot( gridver, bpt );	/* Z' */
	vec[X] = -Dot( shotdir, bpt );	/* X' - shotdir is reverse of X' */

	if(	outfile[0] != NUL
	    &&	fprintf(outfp,
			"%c % 8.3f % 8.3f\n",
			PB_CELL_IDENT,
			vec[Y]*unitconv,
			 	/* horizontal coordinate of burst point (Y') */
			vec[Z]*unitconv
			 	/* vertical coordinate of burst point (Z') */
			) < 0
		)
		{
		brst_log( "Write failed to file (%s)!\n", outfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	if(	outfile[0] != NUL
	    &&	fprintf( outfp,
			"%c % 8.2f % 8.2f %4d %2d % 7.3f % 7.2f % 7.3f %c\n",
			PB_RAY_INTERSECT,
			vec[X]*unitconv, /* X' coordinate of burst point */
			0.0,		/* LOS thickness of component */
			9999,		/* dummy component code number */
			9,		/* dummy space code */
			0.0,		/* N/A */
			0.0,		/* N/A */
			0.0,		/* N/A */
			'1'		/* burst was generated */
			) < 0
		)
		{
		brst_log( "Write failed to file (%s)!\n", outfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	}

/*
	void prntCellIdent( struct application *ap )

	Burst Point Library and Shotline file: information about shotline.
	Ref. Figure 20., Line Number 2 and Figure 19., Line Number 2 of ICD.

	NOTE: field width of first 2 floats compatible with PB_RAY_HEADER
	record.
 */
void
prntCellIdent( ap )
register struct application *ap;
	{
	if(	outfile[0] != NUL
	    &&	fprintf(outfp,
			"%c % 8.3f % 8.3f\n",
			PB_CELL_IDENT,
			ap->a_uvec[X]*unitconv,
			 	/* horizontal coordinate of shotline (Y') */
			ap->a_uvec[Y]*unitconv
			 	/* vertical coordinate of shotline (Z') */
			) < 0
		)
		{
		brst_log( "Write failed to file (%s)!\n", outfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	if(	shotlnfile[0] != NUL
	    &&	fprintf(shotlnfp,
			"%c % 8.3f % 8.3f\n",
			PS_CELL_IDENT,
			ap->a_uvec[X]*unitconv,
			 	/* horizontal coordinate of shotline (Y') */
			ap->a_uvec[Y]*unitconv
			 	/* vertical coordinate of shotline (Z') */
			) < 0
		)
		{
		brst_log( "Write failed to file (%s)!\n", shotlnfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	return;
	}

/*
	void prntSeg( struct application *ap, struct partition *cpp, int space,
			fastf_t entrynorm[3], fastf_t exitnorm[3],
			boolean burstflag )

	Burst Point Library and Shotline file: information about each component
	hit along path of main penetrator (shotline).
	Ref. Figure 20., Line Number 3 and Figure 19., Line Number 2 of ICD.
 */
void
prntSeg( ap, cpp, space, entrynorm, exitnorm, burstflag )
register struct application *ap;
register struct partition *cpp;		/* component partition */
int space;
fastf_t entrynorm[3];
fastf_t exitnorm[3];
boolean burstflag; /* Was a burst generated by this partition? */
	{	fastf_t icosobliquity;	/* cosine of obliquity at entry */
		fastf_t ocosobliquity;	/* cosine of obliquity at exit */
		fastf_t	entryangle;	/* obliquity angle at entry */
		fastf_t exitangle;	/* obliquity angle at exit */
		fastf_t los;		/* line-of-sight thickness */
		fastf_t normthickness;	/* normal thickness */
		fastf_t	rotangle;	/* rotation angle */
		fastf_t sinfbangle;	/* sine of fall back angle */

	/* This *should* give negative of desired result. */
	icosobliquity = Dot( ap->a_ray.r_dir, entrynorm );
	icosobliquity = -icosobliquity;

	ocosobliquity = Dot( ap->a_ray.r_dir, exitnorm );

	if( exitnorm[Y] == 0.0 && exitnorm[X] == 0.0 )
		rotangle = 0.0;
	else
		{
		rotangle = atan2( exitnorm[Y], exitnorm[X] );
		rotangle *= DEGRAD; /* convert to degrees */
		if( rotangle < 0.0 )
			rotangle += 360.0;
		}
	/* Compute sine of fallback angle.  NB: the Air Force measures the
		fallback angle from the horizontal (X-Y) plane. */
	sinfbangle = Dot( exitnorm, zaxis );

	los = (cpp->pt_outhit->hit_dist-cpp->pt_inhit->hit_dist)*unitconv;
#ifdef VDEBUG
	brst_log( "prntSeg: los=%g dout=%g din=%g\n",
		los, cpp->pt_outhit->hit_dist, cpp->pt_inhit->hit_dist );
#endif

	if(	outfile[0] != NUL
	    &&	fprintf( outfp,
			"%c % 8.2f % 8.2f %4d %2d % 7.3f % 7.2f % 7.3f %c\n",
			PB_RAY_INTERSECT,
			(standoff - cpp->pt_inhit->hit_dist)*unitconv,
					/* X'-coordinate of intersection */
			los,		/* LOS thickness of component */
			cpp->pt_regionp->reg_regionid,
					/* component code number */
			space,		/* space code */
			sinfbangle,	/* sine of fallback angle at exit */
			rotangle,	/* rotation angle in degrees at exit */
			icosobliquity,	/* cosine of obliquity angle at entry */
			burstflag ? '1' : '0' /* flag generation of burst */
			) < 0
		)
		{
		brst_log( "Write failed to file (%s)!\n", outfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	if( shotlnfile[0] == NUL )
		return;
	entryangle = AproxEq( icosobliquity, 1.0, COS_TOL ) ?
			0.0 : acos( icosobliquity ) * DEGRAD;
	if(	(normthickness =
		 getNormThickness( ap, cpp, icosobliquity, entrynorm )) <= 0.0
	    &&	fatalerror )
		{
		brst_log( "Couldn't compute normal thickness.\n" );
		brst_log( "\tshotline coordinates <%g,%g>\n",
			ap->a_uvec[X]*unitconv,
			ap->a_uvec[Y]*unitconv
			);
		brst_log( "\tregion name '%s' solid name '%s'\n",
			cpp->pt_regionp->reg_name,
			cpp->pt_inseg->seg_stp->st_name );
		return;
		}
	exitangle = AproxEq( ocosobliquity, 1.0, COS_TOL ) ?
			0.0 : acos( ocosobliquity ) * DEGRAD;
	if( fprintf( shotlnfp,
	       "%c % 8.2f % 7.3f % 7.2f %4d % 8.2f % 8.2f %2d % 7.2f % 7.2f\n",
			PS_SHOT_INTERSECT,
			(standoff - cpp->pt_inhit->hit_dist)*unitconv,
					/* X'-coordinate of intersection */
			sinfbangle,	/* sine of fallback angle at exit */
			rotangle,	/* rotation angle in degrees at exit */
			cpp->pt_regionp->reg_regionid,
					/* component code number */
			normthickness*unitconv,
					/* normal thickness of component */
			los,		/* LOS thickness of component */
			space,		/* space code */
			entryangle,	/* entry obliquity angle in degrees */
			exitangle	/* exit obliquity angle in degrees */
			) < 0
		)
		{
		brst_log( "Write failed to file (%s)!\n", shotlnfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	}

/*
	void prntRayHeader( fastf_t *raydir, fastf_t *shotdir, unsigned rayno )

	Burst Point Library: information about burst ray.  All angles are
	WRT the shotline coordinate system, represented by X', Y' and Z'.
	Ref. Figure 20., Line Number 19 of ICD.

	NOTE: field width of first 2 floats compatible with PB_CELL_IDENT
	record.
 */
void
prntRayHeader( raydir, shotdir, rayno )
fastf_t	*raydir;	/* burst ray direction vector */
fastf_t *shotdir;	/* shotline direction vector */
unsigned rayno;		/* ray number for this burst point */
	{	double cosxr;	 /* cosine of angle between X' and raydir */
		double cosyr;	 /* cosine of angle between Y' and raydir */
		fastf_t azim;	 /* ray azim in radians */
		fastf_t sinelev; /* sine of ray elevation */
	if( outfile[0] == NUL )
		return;
	cosxr = -Dot( shotdir, raydir ); /* shotdir is reverse of X' */
	cosyr = Dot( gridhor, raydir );
	if( cosyr == 0.0 && cosxr == 0.0 )
		azim = 0.0;
	else
		azim = atan2( cosyr, cosxr );
	sinelev = Dot( gridver, raydir );
	if(	fprintf( outfp,
			"%c %8.3f %8.3f %6u\n",
			PB_RAY_HEADER,
			azim,   /* ray azimuth angle WRT shotline (radians). */
			sinelev, /* sine of ray elevation angle WRT shotline. */
			rayno
			) < 0
		)
		{
		brst_log( "Write failed to file (%s)!\n", outfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	}

/*
	void prntRegionHdr( struct application *ap, struct partition *pt_headp,
				struct partition *pp, fastf_t entrynorm[3],
				fastf_t exitnorm[3] )

	Burst Point Libary: intersection along burst ray.
	Ref. Figure 20., Line Number 20 of ICD.
 */
void
prntRegionHdr( ap, pt_headp, pp, entrynorm, exitnorm )
struct application *ap;
struct partition *pt_headp;
struct partition *pp;
fastf_t entrynorm[3], exitnorm[3];
	{	fastf_t	cosobliquity;
		fastf_t normthickness;
		register struct hit *ihitp = pp->pt_inhit;
		register struct hit *ohitp = pp->pt_outhit;
		register struct region *regp = pp->pt_regionp;
		register struct xray *rayp = &ap->a_ray;
	/* Get entry/exit normals and fill in hit points */
	getRtHitNorm( ihitp, pp->pt_inseg->seg_stp, rayp,
		(boolean) pp->pt_inflip, entrynorm );
	if( ! chkEntryNorm( pp, rayp, entrynorm,
		"spall ray component entry normal" ) )
		{
#ifdef DEBUG
		prntDbgPartitions( ap, pt_headp,
			"prntRegionHdr: entry normal flipped." );
#endif
		}
	getRtHitNorm( ohitp, pp->pt_outseg->seg_stp, rayp,
		(boolean) pp->pt_outflip, exitnorm );
	if( ! chkExitNorm( pp, rayp, exitnorm,
		"spall ray component exit normal" ) )
		{
#ifdef DEBUG
		prntDbgPartitions( ap, pt_headp,
			"prntRegionHdr: exit normal flipped." );
#endif
		}


	/* calculate cosine of obliquity angle */
	cosobliquity = Dot( ap->a_ray.r_dir, entrynorm );
	cosobliquity = -cosobliquity;
#if DEBUG
	if( cosobliquity - COS_TOL > 1.0 )
		{
		brst_log( "cosobliquity=%12.8f\n", cosobliquity );
		brst_log( "normal=<%g,%g,%g>\n",
			entrynorm[X],
			entrynorm[Y],
			entrynorm[Z]
			);
		brst_log( "ray direction=<%g,%g,%g>\n",
			ap->a_ray.r_dir[X],
			ap->a_ray.r_dir[Y],
			ap->a_ray.r_dir[Z]
			);
		brst_log( "region name '%s'\n", regp->reg_name );
		}
#endif
	if( outfile[0] == NUL )
		return;


	/* Now we must find normal thickness through component. */
	normthickness = getNormThickness( ap, pp, cosobliquity, entrynorm );
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	if(	fprintf( outfp,
			"%c % 10.3f % 9.3f % 9.3f %4d %4d % 6.3f\n",
			PB_REGION_HEADER,
			ihitp->hit_dist*unitconv, /* distance from burst pt. */
			(ohitp->hit_dist - ihitp->hit_dist)*unitconv, /* LOS */
			normthickness*unitconv,	  /* normal thickness */
			pp->pt_forw == pt_headp ?
				EXIT_AIR : pp->pt_forw->pt_regionp->reg_aircode,
			regp->reg_regionid,
			cosobliquity
			) < 0
		)
		{
		bu_semaphore_release( BU_SEM_SYSCALL );	/* unlock */
		brst_log( "Write failed to file (%s)!\n", outfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	bu_semaphore_release( BU_SEM_SYSCALL );	/* unlock */
	}

/*
	fastf_t getNormThickness( struct application *ap, struct partition *pp,
				fastf_t cosobliquity, fastf_t normvec[3] )

	Given a partition structure with entry hit point and a private copy
	of the associated normal vector, the current application structure
	and the cosine of the obliquity at entry to the component, return
	the normal thickness through the component at the given hit point.

 */
static fastf_t
getNormThickness( ap, pp, cosobliquity, normvec )
register struct application *ap;
register struct partition *pp;
fastf_t cosobliquity;
fastf_t normvec[3];
	{
#ifdef VDEBUG
	brst_log( "getNormThickness() pp 0x%x normal %g,%g,%g\n",
		pp, normvec[X], normvec[Y], normvec[Z] );
#endif
	if( AproxEq( cosobliquity, 1.0, COS_TOL ) )
		{ /* Trajectory was normal to surface, so no need
			to shoot another ray. */
			fastf_t	thickness = pp->pt_outhit->hit_dist -
					pp->pt_inhit->hit_dist;
#ifdef VDEBUG
		brst_log( "getNormThickness: using existing partitions.\n" );
		brst_log( "\tthickness=%g dout=%g din=%g normal=%g,%g,%g\n",
			thickness*unitconv,
			pp->pt_outhit->hit_dist, pp->pt_inhit->hit_dist,
			normvec[X], normvec[Y], normvec[Z] );
#endif
		return	thickness;
		}
	else
		{ /* need to shoot ray */
			struct application a_thick;
			register struct hit *ihitp = pp->pt_inhit;
			register struct region *regp = pp->pt_regionp;
		a_thick = *ap;
		a_thick.a_hit = f_Normal;
		a_thick.a_miss = f_Nerror;
		a_thick.a_level++;
		a_thick.a_user = regp->reg_regionid;
		a_thick.a_purpose = "normal thickness";
		CopyVec( a_thick.a_ray.r_pt, ihitp->hit_point );
		Scale2Vec( normvec, -1.0, a_thick.a_ray.r_dir );
		if( rt_shootray( &a_thick ) == -1 && fatalerror )
			{ /* Fatal error in application routine. */
			brst_log( "Fatal error: raytracing aborted.\n" );
			return	0.0;
			}
		return	a_thick.a_rbeam;
		}
	/*NOTREACHED*/
	}

void
prntDbgPartitions( ap, pt_headp, label )
struct application *ap;
struct partition *pt_headp;
char *label;
	{	struct partition *dpp;
	brst_log( "%s (0x%x)\n", label, pt_headp );
	if( ap != NULL )
		brst_log( "\tPnt %g,%g,%g Dir %g,%g,%g\n",
			ap->a_ray.r_pt[X],
			ap->a_ray.r_pt[Y],
			ap->a_ray.r_pt[Z],
			ap->a_ray.r_dir[X],
			ap->a_ray.r_dir[Y],
			ap->a_ray.r_dir[Z] );
	for( dpp = pt_headp->pt_forw; dpp != pt_headp; dpp = dpp->pt_forw )
		{
		brst_log( "\t0x%x: reg \"%s\" sols \"%s\",\"%s\" in %g out %g\n",
			dpp,
			dpp->pt_regionp->reg_name,
			dpp->pt_inseg->seg_stp->st_name,
			dpp->pt_outseg->seg_stp->st_name,
			dpp->pt_inhit->hit_dist,
			dpp->pt_outhit->hit_dist );
		brst_log( "\tinstp 0x%x outstp 0x%x inhit 0x%x outhit 0x%x\n",
			dpp->pt_inseg->seg_stp, dpp->pt_outseg->seg_stp,
			dpp->pt_inhit, dpp->pt_outhit );
		}
	brst_log( "--\n" );
	}

/*
	void prntShieldComp( struct application *ap, struct partition *pt_headp,
				Pt_Queue *qp )
 */
void
prntShieldComp( ap, pt_headp, qp )
struct application *ap;
struct partition *pt_headp;
register Pt_Queue *qp;
	{	fastf_t entrynorm[3], exitnorm[3];
	if( outfile[0] == NUL )
		return;
	if( qp == PT_Q_NULL )
		return;
	prntShieldComp( ap, pt_headp, qp->q_next );
	prntRegionHdr( ap, pt_headp, qp->q_part, entrynorm, exitnorm );
	}
void
prntColors( colorp, str )
register Colors	*colorp;
char	*str;
	{
	brst_log( "%s:\n", str );
	for(	colorp = colorp->c_next;
		colorp != COLORS_NULL;
		colorp = colorp->c_next )
		{
		brst_log( "\t%d..%d\t%d,%d,%d\n",
			(int)colorp->c_lower,
			(int)colorp->c_upper,
			(int)colorp->c_rgb[0],
			(int)colorp->c_rgb[1],
			(int)colorp->c_rgb[2]
			);
		}
	}

/*
	void prntFiringCoords( register fastf_t *vec )

	If the user has asked for grid coordinates to be saved, write
	them to the output stream 'gridfp'.
 */
void
prntFiringCoords( vec )
register fastf_t *vec;
	{
	if( gridfile[0] == '\0' )
		return;
	assert( gridfp != (FILE *) NULL );
	if( fprintf( gridfp, "%7.2f %7.2f\n", vec[X]*unitconv, vec[Y]*unitconv )
		< 0 )
		{
		brst_log( "Write failed to file (%s)!\n", gridfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	}

void
prntGridOffsets( x, y )
int	x, y;
	{
	if( ! tty )
		return;
	(void) ScMvCursor( GRID_X, GRID_Y );
	(void) printf( "[% 4d:% 4d,% 4d:% 4d]",
			x, gridxfin, y, gridyfin
			);
	(void) fflush( stdout );
	return;
	}

void
prntIdents( idp, str )
register Ids	*idp;
char	*str;
	{
	brst_log( "%s:\n", str );
	for( idp = idp->i_next ; idp != IDS_NULL; idp = idp->i_next )
		{
		if( idp->i_lower == idp->i_upper )
			brst_log( "\t%d\n", (int) idp->i_lower );
		else
			brst_log( "\t%d..%d\n",
				(int)idp->i_lower,
				(int)idp->i_upper
				);
		}
	return;
	}

/**/
void
prntPagedMenu( menu )
register char	**menu;
	{	register int	done = 0;
		int		lines =	(PROMPT_Y-SCROLL_TOP);
	if( ! tty )
		{
		for( ; *menu != NULL; menu++ )
			brst_log( "%s\n", *menu );
		return;
		}
	for( ; *menu != NULL && ! done;  )
		{
		for( ; lines > 0 && *menu != NULL; menu++, --lines )
			brst_log( "%-*s\n", co, *menu );
		if( *menu != NULL )
			done = ! doMore( &lines );
		prompt( "" );
		}
	(void) fflush( stdout );
	return;
	}

/*
	void prntPhantom( struct hit *hitp, int space, fastf_t los )

	Output "phantom armor" pseudo component.  This component has no
	surface normal or thickness, so many zero fields are used for
	conformity with the normal component output formats.
 */
/*ARGSUSED*/
void
prntPhantom( hitp, space, los )
struct hit *hitp;	/* ptr. to phantom's intersection information */
int space;		/* space code behind phantom */
fastf_t	los;		/* LOS of space */
	{
	if(	outfile[0] != NUL
	    &&	fprintf( outfp,
			"%c % 8.2f % 8.2f %4d %2d % 7.3f % 7.3f % 7.3f %c\n",
			PB_RAY_INTERSECT,
			(standoff-hitp->hit_dist)*unitconv,
				/* X'-coordinate of intersection */
			0.0,	/* LOS thickness of component */
			PHANTOM_ARMOR, /* component code number */
			space,	/* space code */
			0.0,	/* sine of fallback angle */
			0.0,	/* rotation angle (degrees) */
			0.0, /* cosine of obliquity angle at entry */
			'0'	/* no burst from phantom armor */
			) < 0
		)
		{
		brst_log( "Write failed to file!\n", outfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	if(	shotlnfile[0] != NUL
	    &&	fprintf( shotlnfp,
	       "%c % 8.2f % 7.3f % 7.3f %4d % 8.2f % 8.2f %2d % 7.2f % 7.2f\n",
			PS_SHOT_INTERSECT,
			(standoff-hitp->hit_dist)*unitconv,
					/* X'-coordinate of intersection */
			0.0,		/* sine of fallback angle */
			0.0,		/* rotation angle in degrees */
			PHANTOM_ARMOR,	/* component code number */
			0.0,		/* normal thickness of component */
			0.0,		/* LOS thickness of component */
			space,		/* space code */
			0.0,		/* entry obliquity angle in degrees */
			0.0		/* exit obliquity angle in degrees */
			) < 0
		)
		{
		brst_log( "Write failed to file (%s)!\n", shotlnfile );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	return;
	}
#if defined(HAVE_STDARG_H)
/* STDARG */
void
prntScr( char *format, ... )
	{
		va_list	ap;
	va_start( ap, format );
#else
/* VARARGS */
void
prntScr( va_alist )
va_dcl
	{	register char *format; /* picked up by va_arg() */
		va_list	ap;
	va_start( ap );
#endif
	format  = va_arg( ap, char * );
	if( tty )
		{
		clr_Tabs( HmTtyFd );
		if( ScDL != NULL )
			{
			(void) ScMvCursor( 1, SCROLL_TOP );
			(void) ScDeleteLn();
			(void) ScMvCursor( 1, SCROLL_BTM );
			(void) ScClrEOL();
			(void) vprintf( format, ap );
			}
		else
		if( ScSetScrlReg( SCROLL_TOP, SCROLL_BTM+1 ) )
			{	char buf[LNBUFSZ];
			(void) ScMvCursor( 1, SCROLL_BTM+1 );
			(void) ScClrEOL();
			/* Work around for problem with vprintf(): it doesn't
				cause the screen to scroll, don't know why. */
			(void) vsprintf( buf, format, ap );
			(void) puts( buf );
			/*(void) vprintf( format, ap );*/
			(void) ScMvCursor( 1, SCROLL_BTM+1 );
			(void) ScClrScrlReg();
			}
		else
			{
			(void) vprintf( format, ap );
			(void) fputs( "\n", stdout );
			}
		(void) fflush( stdout );
		}
	else
		{
		(void) vfprintf( stderr, format, ap );
		(void) fputs( "\n", stderr );
		}
	va_end( ap );
	return;
	}



/*
	void	prntTimer( char *str )
 */
void
prntTimer( str )
char    *str;
	{
	(void) rt_read_timer( timer, TIMER_LEN-1 );
	if( tty )
		{
		(void) ScMvCursor( TIMER_X, TIMER_Y );
		if( str == NULL )
			(void) printf( "%s", timer );
		else
			(void) printf( "%s:\t%s", str, timer );
		(void) ScClrEOL();
		(void) fflush( stdout );
		}
	else
		brst_log( "%s:\t%s\n", str == NULL ? "(null)" : str, timer );
	}

void
prntTitle( title )
char	*title;
	{
	if( ! tty || RT_G_DEBUG )
		brst_log( "%s\n", title == NULL ? "(null)" : title );
	}

static char	*usage[] =
	{
	"Usage: burst [-b]",
	"\tThe -b option suppresses the screen display (for batch jobs).",
	NULL
	};
void
prntUsage()
	{	register char   **p = usage;
	while( *p != NULL )
		(void) fprintf( stderr, "%s\n", *p++ );
	}

void
prompt( str )
char    *str;
	{
	(void) ScMvCursor( PROMPT_X, PROMPT_Y );
	if( str == (char *) NULL )
		(void) ScClrEOL();
	else
		{
		(void) ScSetStandout();
		(void) fputs( str, stdout );
		(void) ScClrStandout();
		}
	(void) fflush( stdout );
	}

int
qAdd( pp, qpp )
struct partition	*pp;
Pt_Queue		**qpp;
	{	Pt_Queue	*newq;
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	if( (newq = (Pt_Queue *) malloc( sizeof(Pt_Queue) )) == PT_Q_NULL )
		{
		Malloc_Bomb( sizeof(Pt_Queue) );
		bu_semaphore_release( BU_SEM_SYSCALL );
		return	0;
		}
	bu_semaphore_release( BU_SEM_SYSCALL );
	newq->q_next = *qpp;
	newq->q_part = pp;
	*qpp = newq;
	return	1;
	}

void
qFree( qp )
Pt_Queue	*qp;
	{
	if( qp == PT_Q_NULL )
		return;
	qFree( qp->q_next );
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	free( (char *) qp );
	bu_semaphore_release( BU_SEM_SYSCALL );
	}

void
warning( str )
char	*str;
	{
	if( tty )
		HmError( str );
	else
		prntScr( str );
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
