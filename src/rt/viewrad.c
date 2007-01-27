/*                       V I E W R A D . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
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
/** @file viewrad.c
 *
 *  Ray Tracing program RTRAD bottom half.
 *
 *  This module takes the first hit from rt_shootray(), and produces
 *  a GIFT/SRIM format Radar file.  It tracks specular direction
 *  reflections.
 *
 *  Author -
 *	Phillip Dykstra
 *	From viewpp.c and viewray.c by
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
#ifndef lint
static const char RCSppview[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"
#include "./rad.h"

#define	MAXREFLECT	16
#define	DEFAULTREFLECT	8

int		use_air = 0;		/* Handling of air in librt */
int		using_mlib = 0;		/* Material routines NOT used */

static struct rayinfo {
	int	sight;
	vect_t	ip;		/* intersection point */
	vect_t	norm;		/* normal vector */
	vect_t	spec;		/* specular direction */
	struct	curvature curvature;
	fastf_t	dist;
	int	reg, sol, surf;
} rayinfo[MAXREFLECT], *rayp;


extern	FILE	*outfp;
extern	point_t	viewbase_model;		/* lower_left of viewing plane */
extern	mat_t	view2model;
extern	fastf_t	viewsize;

extern	int	width;
extern	int	height;

int	numreflect = DEFAULTREFLECT;	/* max number of reflections */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
#if !defined(__alpha)   /* XXX Alpha does not support this initialization! */

	{"%d",	1, "maxreflect",	bu_byteoffset(numreflect),	BU_STRUCTPARSE_FUNC_NULL },
#endif
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

void		dumpray(struct rayinfo *rp);
void		dumpall(struct application *ap, int depth);
static int	isvisible(struct application *ap, struct hit *hitp, const fastf_t *norm);

char usage[] = "\
Usage:  rtrad [options] model.g objects... >file.rad\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees\n\
 -e Elev	Elevation in degrees\n\
 -M		Read matrix, cmds on stdin\n\
 -o file.rad	Output file name, else stdout\n\
 -x #		Set librt debug flags\n\
";

static struct xray firstray;

/*
 *  Yucky Fortran/SRIM style I/O
 */
static char	physrec[256*sizeof(union radrec)];
static int	precindex = 0;
static int	precnum = 0;	/* number of physical records written */
static int	recnum = 0;	/* number of (useful) records written */

static int radhit(register struct application *ap, struct partition *PartHeadp, struct seg *segHeadp);
static int radmiss(struct application *ap);

/*
 *  			V I E W _ I N I T
 *
 *  Called at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
int view_init( register struct application *ap,
	       char *file,
	       char *obj,
	       int minus_o)
{
	ap->a_hit = radhit;
	ap->a_miss = radmiss;
	ap->a_onehit = 1;

	return(0);		/* no framebuffer needed */
}
/*
 *  Output a physical record (256 logical records)
 *
 *  Turns on -1 flags in unused logical records
 */
int
writephysrec(FILE *fp)
{
	union radrec	skiprec;
	long	length;
static int totbuf = 0;
int buf = 0;

	/* Pad out the record if not full */
	memset( (char *)&skiprec, 0, sizeof(skiprec) );
	skiprec.p.pad[16] = -1;
	while( precindex < 256 ) {
		bcopy( &skiprec, &physrec[precindex*sizeof(union radrec)], sizeof(skiprec) );
		precindex++;
buf++;
	}

	length = sizeof(physrec);
	fwrite( &length, sizeof(length), 1, fp );
	if( fwrite( physrec, sizeof(physrec), 1, fp ) != 1 ) {
		bu_log( "writephysrec: error writing physical record\n" );
		return( 0 );
	}
	fwrite( &length, sizeof(length), 1, fp );

	memset( (char *)physrec, 0, sizeof(physrec) );	/* paranoia */
	precindex = 0;
	precnum++;

totbuf += buf;
/*fprintf( stderr, "PREC %d, buf = %d, totbuf = %d\n", precnum, buf, totbuf );*/

	return( 1 );
}

/*
 *  Write a logical record
 *
 *  Outputs the current physical record if full.
 */
int
writerec(union radrec *rp, FILE *fp)
{
	if( precindex >= 256 ) {
		if( writephysrec( fp ) == 0 )
			return( 0 );
	}
	bcopy( rp, &physrec[precindex*sizeof(*rp)], sizeof(*rp) );

	precindex++;
	recnum++;

	if( precindex >= 256 ) {
		if( writephysrec( fp ) == 0 )
			return( 0 );
	}
	return( 1 );
}

/* beginning of a frame */
void
view_2init(struct application *ap)
{
	extern double azimuth, elevation;
	vect_t temp, aimpt;
	union radrec r;

	if( numreflect > MAXREFLECT ) {
		bu_log("Warning: maxreflect too large (%d), using %d\n",
			numreflect, MAXREFLECT );
		numreflect = MAXREFLECT;
	}

	bu_log( "Ray Spacing: %f rays/cm\n", 10.0*(width/viewsize) );

	/* Header Record */
	memset( (char *)&r, 0, sizeof(r) );

	/*XXX*/
	r.h.head[0] = 'h'; r.h.head[1] = 'e';
	r.h.head[2] = 'a'; r.h.head[3] = 'd';

	r.h.id = 1;
	r.h.iview = 1;
	r.h.miview = - r.h.iview;
	VSET( temp, 0.0, 0.0, -1.414 );	/* Point we are looking at */
	MAT4X3PNT( aimpt, view2model, temp );
	r.h.cx = aimpt[0];		/* aimpoint */
	r.h.cy = aimpt[1];
	r.h.cz = aimpt[2];
	r.h.back = 1.414*viewsize/2.0;	/* backoff */
	r.h.e = elevation;
	r.h.a = azimuth;
	r.h.vert = viewsize;
	r.h.horz = viewsize;
	r.h.nvert = height;
	r.h.nhorz = width;
	r.h.maxrfl = numreflect;

	writerec( &r, outfp );

	/* XXX - write extra header records */
	memset( (char *)&r, 0, sizeof(r) );
	writerec( &r, outfp );
	writerec( &r, outfp );
}

/* end of each pixel */
void	view_pixel(void) {}

/* end of each line */
void	view_eol(void) {}

void	view_setup(void) {}
void	view_cleanup(void) {}

/* end of a frame */
void
view_end(void)
{
	/* flush any partial output record */
	if( precindex > 0 ) {
		writephysrec( outfp );
	}

	bu_log( "view_end: %d physical records, (%d/%d) logical\n",
		precnum, recnum, precnum*256 );
}

static int
radhit(register struct application *ap, struct partition *PartHeadp, struct seg *segHeadp)
{
	register struct partition *pp;
	register struct hit *hitp;
	struct application sub_ap;
	LOCAL fastf_t	f;
	LOCAL vect_t	to_eye, work;
	LOCAL int	depth;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		bu_log("radhit:  no hit out front?\n");
		return(0);
	}

	if(R_DEBUG&RDEBUG_HITS)  {
		rt_pr_pt( ap->a_rt_i, pp );
	}

	hitp = pp->pt_inhit;
	if( hitp->hit_dist >= INFINITY )  {
		bu_log("radhit:  entry beyond infinity\n");
		return(1);
	}
	/* Check to see if eye is "inside" the solid */
	if( hitp->hit_dist < 0 )  {
		/* XXX */
		bu_log("radhit:  GAK, eye inside solid (%g)\n", hitp->hit_dist );
		for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
			rt_pr_pt( ap->a_rt_i, pp );
		return(0);
	}

	rayp = &rayinfo[ ap->a_level ];

	RT_HIT_NORMAL( rayp->norm, hitp, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip );

	if(R_DEBUG&RDEBUG_HITS)  {
		rt_pr_hit( " In", hitp );
	}

	rayp->dist = hitp->hit_dist;
	rayp->reg = pp->pt_regionp->reg_regionid;
	rayp->sol = pp->pt_inseg->seg_stp->st_id;
	rayp->surf = hitp->hit_surfno;
	RT_CURVATURE( &(rayp->curvature), hitp, pp->pt_inflip, pp->pt_inseg->seg_stp );
	if( VDOT( rayp->norm, ap->a_ray.r_dir ) < 0 ) {
		bu_log(" debug: flipping curvature\n");
		rayp->curvature.crv_c1 = - rayp->curvature.crv_c1;
		rayp->curvature.crv_c2 = - rayp->curvature.crv_c2;
	}
	VMOVE( rayp->ip, hitp->hit_point );

	/* Compute the specular direction */
	VREVERSE( to_eye, ap->a_ray.r_dir );
	f = 2 * VDOT( to_eye, rayp->norm );
	VSCALE( work, rayp->norm, f );
	/* I have been told this has unit length */
	VSUB2( rayp->spec, work, to_eye );

	/* Save info for 1st ray */
	if( ap->a_level == 0 ) {
		firstray = ap->a_ray;	/* struct copy */
		rayp->sight = 1;	/* the 1st intersect is always visible */
	} else {
		/* Check for visibility */
		rayp->sight = isvisible( ap, hitp, rayp->norm );
	}

	/*
	 * Shoot another ray in the specular direction.
	 */
	if( ap->a_level < numreflect-1 ) {
		sub_ap = *ap;	/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
		VMOVE( sub_ap.a_ray.r_dir, rayp->spec );
		depth = rt_shootray( &sub_ap );
	} else {
		bu_log( "radhit:  max reflections exceeded [%d %d]\n",
			ap->a_x, ap->a_y );
		depth = 0;
	}

	if( ap->a_level == 0 ) {
		/* We're the 1st ray, output the raylist */
		dumpall( ap, depth+1 );
	}
	return(depth+1);	/* report hit to main routine */
}

static int
radmiss(struct application *ap)
{
	return(0);
}

/*********** Eye Visibility Routines ************/
/*
 *  True if the intersection distance is >= distance back to the
 *  origin of the first ray in a chain.
 *  Called via isvisible on a hit.
 */
static int
hiteye(struct application *ap, struct partition *PartHeadp, struct seg *segHeadp)
{
	register struct partition *pp;
	register struct hit *hitp;
	LOCAL vect_t work;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist > 0 )  break;
	if( pp == PartHeadp )  {
		bu_log("hiteye:  no hit out front?\n");
		return(1);
	}
	hitp = pp->pt_inhit;
	if( hitp->hit_dist >= INFINITY )  {
		bu_log("hiteye:  entry beyond infinity\n");
		return(1);
	}
	/* The current ray segment exits "in front" of me,
	 * find out where it went in.
	 * Check to see if eye is "inside" of the solid.
	 */
	if( hitp->hit_dist < -1.0e-10 )  {
		/*
		 * If we are under 1.0 units inside of a solid, we pushed
		 * into it ourselves in trying to get away from the surface.
		 * Otherwise, its hard to tell how we got in here!
		 */
		if( hitp->hit_dist < -1.001 ) {
			bu_log("hiteye: *** GAK2, eye inside solid (%g) ***\n", hitp->hit_dist );
			for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
				rt_pr_pt( ap->a_rt_i, pp );
		}
		return(0);
	}

	VSUB2( work, firstray.r_pt, ap->a_ray.r_pt );
	if( hitp->hit_dist * hitp->hit_dist > MAGSQ(work) )
		return(1);
	else
		return(0);
}

/*
 *  Always true
 *  Called via isvisible on a miss.
 */
static int
hittrue(struct application *ap)
{
	return(1);
}

/*
 *  Determine whether the current hitpoint along a series of
 *  reflections is visible from the origin of the ray.
 *  (which is the location of our "point" eye for now)
 *
 *  Strategy: we shoot back toward the origin of the ray
 *   If we don't hit anything (i.e. miss) we made it.
 *   If we hit something we made it if that distance is greater
 *   than the distance back to the eye.
 */
static int
isvisible(struct application *ap, struct hit *hitp, const fastf_t *norm)
{
	struct application sub_ap;
	vect_t	rdir;

	/* compute the ray direction */
	VSUB2( rdir, firstray.r_pt, hitp->hit_point );
	VUNITIZE( rdir );
	if( VDOT(rdir, norm) < 0 )
		return( 0 );	/* backfacing */

	sub_ap = *ap;	/* struct copy */
	sub_ap.a_level = ap->a_level+1;
	sub_ap.a_onehit = 1;
	sub_ap.a_hit = hiteye;
	sub_ap.a_miss = hittrue;
	/*
	 * New origin is one unit in the ray direction in
	 * order to get away from the surface we intersected.
	 */
	VADD2( sub_ap.a_ray.r_pt, hitp->hit_point, rdir );
	VMOVE( sub_ap.a_ray.r_dir, rdir );

	return( rt_shootray( &sub_ap ) );
}

/************** Output Routines ***************/
void
dumpall(struct application *ap, int depth)
{
	int	i;
	union radrec r;

	if( depth > numreflect ) {
		bu_log( "dumpall: %d reflections!\n", depth );
	}

	/* Firing record */
	/*printf( "Ray [%d %d], depth = %d\n", ap->a_x, ap->a_y, depth );*/

	memset( (char *)&r, 0, sizeof(r) );

	/*XXX*/
	r.f.head[0] = 'f'; r.f.head[1] = 'i';
	r.f.head[2] = 'r'; r.f.head[3] = 'e';

	/*
	 * Make sure there's enough space in
	 * the physical record.
	 */
	i = 1 + depth;
	if( depth < numreflect )
		i++;	/* escape */
	if( precindex + i > 256 )
		writephysrec( outfp );

	r.f.irf = i-1;			/* num recs in ray, not counting fire */
	r.f.ox = ap->a_ray.r_pt[0];	/* ray origin */
	r.f.oy = ap->a_ray.r_pt[1];
	r.f.oz = ap->a_ray.r_pt[2];
	r.f.h = (ap->a_x - width/2) * viewsize/width;
	r.f.v = (ap->a_y - height/2) * viewsize/height;
	r.f.ih = ap->a_x + 1;		/* Radsim counts from 1 */
	r.f.iv = ap->a_y + 1;
	writerec( &r, outfp );

	for( i = 0; i < depth; i++ ) {
		dumpray( &rayinfo[i] );
	}

	if( depth == numreflect )
		return;			/* no escape */

	/* Escape record */
	memset( (char *)&r, 0, sizeof(r) );

	/*XXX*/
	r.e.head[0] = 'e'; r.e.head[1] = 's';
	r.e.head[2] = 'c'; r.e.head[3] = 'p';

	r.e.sight = -3;			/* XXX line of sight for escape? */
	r.e.dx = rayinfo[depth-1].spec[0];	/* escape direction */
	r.e.dy = rayinfo[depth-1].spec[1];
	r.e.dz = rayinfo[depth-1].spec[2];
	writerec( &r, outfp );
}

void
dumpray(struct rayinfo *rp)
{
	union radrec r;

#ifdef DEBUG1
	printf( " visible = %d\n", rp->sight );
	printf( " i = (%f %f %f)\n", rp->ip[0], rp->ip[1], rp->ip[2] );
	printf( " n = (%f %f %f)\n", rp->norm[0], rp->norm[1], rp->norm[2] );
	printf( " p = (%f %f %f)\n", rp->spec[0], rp->spec[1], rp->spec[2] );
#endif

	/* Reflection record */
	memset( (char *)&r, 0, sizeof(r) );

	/*XXX*/
	r.r.head[0] = 'r'; r.r.head[1] = 'e';
	r.r.head[2] = 'l'; r.r.head[3] = 'f';

	r.r.packedid = 12345;
	r.r.sight = rp->sight;
	r.r.ix = rp->ip[0];		/* intersection */
	r.r.iy = rp->ip[1];
	r.r.iz = rp->ip[2];
	r.r.nx = rp->norm[0];		/* normal */
	r.r.ny = rp->norm[1];
	r.r.nz = rp->norm[2];
	r.r.px = rp->curvature.crv_pdir[0];	/* principle plane */
	r.r.py = rp->curvature.crv_pdir[1];
	r.r.pz = rp->curvature.crv_pdir[2];
	r.r.rc1 = rp->curvature.crv_c1;
	r.r.rc2 = rp->curvature.crv_c2;
	r.r.dfirst = rp->dist;
	r.r.ireg = rp->reg;
	r.r.isol = rp->sol;
	r.r.isurf = rp->surf;

	writerec( &r, outfp );
}


void application_init (void) {}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
