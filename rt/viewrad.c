/*
 *			V I E W R A D
 *
 *  Ray Tracing program RTRAD bottom half.
 *
 *  This module takes the first hit from rt_shootray(), and produces
 *  a GIFT/SRIM format Radar file.  It tracks principle direction
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSppview[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./rdebug.h"
#include "./rad.h"

#define	MAXREFLECT	8	/* XXX - should become command line arg */

extern	FILE	*outfp;
extern	point_t	viewbase_model;		/* lower_left of viewing plane */
extern	mat_t	view2model;
extern	fastf_t	viewsize;
#ifdef never
extern	point_t	eye_model;		/* ray origin for perspective */
extern	point_t	dx_model;
extern	point_t	dy_model;
#endif never
extern	int	npts;

char usage[] = "\
Usage:  rtrad [options] model.g objects... >file.rad\n\
Options:\n\
 -f #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -o file.rad	Output file name, else stdout\n\
 -x #		Set librt debug flags\n\
";

static struct rayinfo {
	int	sight;
	vect_t	ip;		/* intersection point */
	vect_t	norm;		/* normal vector */
	vect_t	prin;		/* principle direction */
	struct	curvature curvature;
	fastf_t	dist;
	int	reg, sol, surf;
} rayinfo[MAXREFLECT], *rayp;
static struct xray firstray;
static int numpts;

/*
 *  Yucky Fortran/SRIM style I/O
 */
static char	physrec[256*sizeof(union radrec)];
static int	precindex = 0;
static int	precnum = 0;	/* number of physical records written */
static int	recnum = 0;	/* number of (useful) records written */

/* Null function -- handle a miss */
raymiss() { ; }

view_pixel() {}

int
radhit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
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
		rt_log("radhit:  no hit out front?\n");
		return(0);
	}

	if(rdebug&RDEBUG_HITS)  {
		rt_pr_pt(pp);
	}

	hitp = pp->pt_inhit;
	if( hitp->hit_dist >= INFINITY )  {
		rt_log("radhit:  entry beyond infinity\n");
		return(1);
	}
	/* Check to see if eye is "inside" the solid */
	if( hitp->hit_dist < 0.0 )  {
		/* XXX */
		rt_log("radhit:  GAK, eye inside solid (%g)\n", hitp->hit_dist );
		for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
			rt_pr_pt(pp);
		return(0);
	}

	RT_HIT_NORM( hitp, pp->pt_inseg->seg_stp, &(ap->a_ray) );
	if( pp->pt_inflip ) {
		VREVERSE( hitp->hit_normal, hitp->hit_normal );
		pp->pt_inflip = 0;
	}

	if(rdebug&RDEBUG_HITS)  {
		rt_pr_hit( " In", hitp );
	}

	rayp = &rayinfo[ ap->a_level ];
	rayp->dist = hitp->hit_dist;
	rayp->reg = pp->pt_regionp->reg_regionid;
	rayp->sol = pp->pt_inseg->seg_stp->st_id;
	rayp->surf = 1;	/* XXX no surface numbers in RT */
	RT_CURVE( &(rayp->curvature), hitp, pp->pt_inseg->seg_stp, &(ap->a_ray) );
	VMOVE( rayp->ip, hitp->hit_point );
	VMOVE( rayp->norm, hitp->hit_normal );

	/* Compute the principal (specular) direction */
	VREVERSE( to_eye, ap->a_ray.r_dir );
	f = 2 * VDOT( to_eye, hitp->hit_normal );
	VSCALE( work, hitp->hit_normal, f );
	/* I have been told this has unit length */
	VSUB2( rayp->prin, work, to_eye );

	/* Save info for 1st ray */
	if( ap->a_level == 0 ) {
		firstray = ap->a_ray;	/* struct copy */
		rayp->sight = 1;	/* the 1st intersect is always visible */
	} else {
		/* Check for visibility */
		rayp->sight = isvisible( ap, hitp );
	}

	/*
	 * Shoot another ray in the principle direction.
	 */
	if( ap->a_level < MAXREFLECT-1 ) {
		sub_ap = *ap;	/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
		VMOVE( sub_ap.a_ray.r_dir, rayp->prin );
		depth = rt_shootray( &sub_ap );
	} else {
		rt_log( "radhit:  max reflections exceeded [%d %d]\n",
			ap->a_x, ap->a_y );
		depth = 0;
	}

	if( ap->a_level == 0 ) {
		/* We're the 1st ray, output the raylist */
		dumpall( ap, depth+1 );
	}
	return(depth+1);	/* report hit to main routine */
}

int
radmiss()  {
	return(0);
}

view_eol()
{
}

/*
 *  Called when the picture is finally done.
 */
view_end()
{
	/* flush any partial output record */
	if( precindex > 0 ) {
		writephysrec( outfp );
	}

	rt_log( "view_end: %d physical records, (%d/%d) logical\n",
		precnum, recnum, precnum*256 );
}

/*
 *  			V I E W _ I N I T
 */
view_init( ap, file, obj, npts, minus_o )
register struct application *ap;
char *file, *obj;
{
	ap->a_hit = radhit;
	ap->a_miss = radmiss;
	ap->a_onehit = 1;

	numpts = npts;		/* hang on to this for header */

	/* RT should do this for us... */
	if( minus_o == 0 ) {
		outfp = stdout;
	}

	return(0);		/* no framebuffer needed */
}

view_2init( ap )
struct application *ap;
{
	extern double azimuth, elevation;
	vect_t temp, aimpt;
	union radrec r;

	rt_log( "Ray Spacing: %f rays/cm\n", 10.0*(npts/viewsize) );

	/* Header Record */
	bzero( &r, sizeof(r) );

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
	r.h.e = - elevation;		/* RT and GIFT/SRIM are backward XXX */
	r.h.a = - azimuth;
	r.h.vert = viewsize;
	r.h.horz = viewsize;
	r.h.nvert = numpts;
	r.h.nhorz = numpts;
	r.h.maxrfl = MAXREFLECT;

	writerec( &r, outfp );

	/* XXX - write extra header records */
	bzero( &r, sizeof(r) );
	writerec( &r, outfp );
	writerec( &r, outfp );
}

mlib_setup() { return(1); }

/*********** Eye Visibility Routines ************/
/*
 *  True if the intersection distance is >= distance back to the
 *  origin of the first ray in a chain.
 *  Called via isvisible on a hit.
 */
static int
hiteye( ap, PartHeadp )
struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp;
	register struct hit *hitp;
	LOCAL vect_t work;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 1.0e-10 )  break;
	if( pp == PartHeadp )  {
		rt_log("hiteye:  no hit out front?\n");
		return(1);
	}
	hitp = pp->pt_inhit;
	if( hitp->hit_dist >= INFINITY )  {
		rt_log("hiteye:  entry beyond infinity\n");
		return(1);
	}
	/* Check to see if eye is "inside" the solid */
	if( hitp->hit_dist < -1.0e-10 )  {
		/* XXX */
		rt_log("hiteye:  GAK2, eye inside solid (%g)\n", hitp->hit_dist );
		for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
			rt_pr_pt(pp);
		return(0);
	}
	/*RT_HIT_NORM( hitp, pp->pt_inseg->seg_stp, &(ap->a_ray) );*/

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
hittrue( ap, PartHeadp )
struct application *ap;
struct partition *PartHeadp;
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
isvisible( ap, hitp )
struct application *ap;
struct hit *hitp;
{
	struct application sub_ap;

	sub_ap = *ap;	/* struct copy */
	sub_ap.a_level = ap->a_level+1;
	sub_ap.a_onehit = 1;
	sub_ap.a_hit = hiteye;
	sub_ap.a_miss = hittrue;
	VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
	VSUB2( sub_ap.a_ray.r_dir, firstray.r_pt, hitp->hit_point );
	VUNITIZE( sub_ap.a_ray.r_dir );
	return( rt_shootray( &sub_ap ) );
}

/************** Output Routines ***************/
dumpall( ap, depth )
struct application *ap;
int depth;
{
	int	i;
	union radrec r;

	if( depth > MAXREFLECT ) {
		rt_log( "dumpall: %d reflections!\n", depth );
	}

	/* Firing record */
	/*printf( "Ray [%d %d], depth = %d\n", ap->a_x, ap->a_y, depth );*/

	bzero( &r, sizeof(r) );

	/*XXX*/
	r.f.head[0] = 'f'; r.f.head[1] = 'i';
	r.f.head[2] = 'r'; r.f.head[3] = 'e';

	/*
	 * Make sure there's enough space in
	 * the physical record.
	 */
	i = 1 + depth;
	if( depth < MAXREFLECT )
		i++;	/* escape */
	if( precindex + i > 256 )
		writephysrec( outfp );

	r.f.irf = i-1;			/* num recs in ray, not counting fire */
	r.f.ox = ap->a_ray.r_pt[0];	/* ray origin */
	r.f.oy = ap->a_ray.r_pt[1];
	r.f.oz = ap->a_ray.r_pt[2];
	r.f.h = (ap->a_x - npts/2) * viewsize/npts;
	r.f.v = (ap->a_y - npts/2) * viewsize/npts;
	r.f.ih = ap->a_x + 1;		/* Radsim counts from 1 */
	r.f.iv = ap->a_y + 1;
	writerec( &r, outfp );

	for( i = 0; i < depth; i++ ) {
		dumpray( &rayinfo[i] );
	}

	if( depth == MAXREFLECT )
		return;			/* no escape */

	/* Escape record */
	bzero( &r, sizeof(r) );

	/*XXX*/
	r.e.head[0] = 'e'; r.e.head[1] = 's';
	r.e.head[2] = 'c'; r.e.head[3] = 'p';

	r.e.sight = -3;			/* XXX line of sight for escape? */
	r.e.dx = rayinfo[depth-1].prin[0];	/* escape direction */
	r.e.dy = rayinfo[depth-1].prin[1];
	r.e.dz = rayinfo[depth-1].prin[2];
	writerec( &r, outfp );
}

dumpray( rp )
struct rayinfo *rp;
{
	union radrec r;

#ifdef DEBUG1
	printf( " visible = %d\n", rp->sight );
	printf( " i = (%f %f %f)\n", rp->ip[0], rp->ip[1], rp->ip[2] );
	printf( " n = (%f %f %f)\n", rp->norm[0], rp->norm[1], rp->norm[2] );
	printf( " p = (%f %f %f)\n", rp->prin[0], rp->prin[1], rp->prin[2] );
#endif

	/* Reflection record */
	bzero( &r, sizeof(r) );

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

/*
 *  Write a logical record
 *
 *  Outputs the current physical record if full.
 */
writerec( rp, fp )
union radrec *rp;
FILE *fp;
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

/*
 *  Output a physical record (256 logical records)
 *
 *  Turns on -1 flags in unused logical records
 */
writephysrec( fp )
FILE *fp;
{
	union radrec	skiprec;
	long	length;
static int totbuf = 0;
int buf = 0;

	/* Pad out the record if not full */
	bzero( &skiprec, sizeof(skiprec) );
	skiprec.p.pad[16] = -1;
	while( precindex < 256 ) {
		bcopy( &skiprec, &physrec[precindex*sizeof(union radrec)], sizeof(skiprec) );
		precindex++;
buf++;
	}

	length = sizeof(physrec);
	fwrite( &length, sizeof(length), 1, fp );
	if( fwrite( physrec, sizeof(physrec), 1, fp ) != 1 ) {
		rt_log( "writephysrec: error writing physical record\n" );
		return( 0 );
	}
	fwrite( &length, sizeof(length), 1, fp );

	bzero( physrec, sizeof(physrec) );	/* paranoia */
	precindex = 0;
	precnum++;

totbuf += buf;
fprintf( stderr, "PREC %d, buf = %d, totbuf = %d\n", precnum, buf, totbuf );

	return( 1 );
}

mlib_free() { ; }
