/*
 *			V I E W P P
 *
 *  Ray Tracing program RTPP bottom half.
 *
 *  This module takes the first hit from rt_shootray(), and produces
 *  GIFT format .PP (pretty picture) files
 *
 *  To make a picture out of a .PP file, use:
 *	pp-ik or
 *	pp-fb
 *
 *  Author -
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

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"

int		use_air = 0;		/* Handling of air in librt */

char usage[] = "\
Usage:  rtpp [options] model.g objects... >file.pp\n\
Options:\n\
 -s #		Grid size in pixels, default 512, max 1024\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -x #		Set librt debug flags\n\
";

extern int	width;
extern int	height;

int		using_mlib = 0;		/* Material routines NOT used */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,	FUNC_NULL }
};

/* Stuff for pretty-picture output format */
static struct soltab *last_solidp;	/* pointer to last solid hit */
static int last_item;			/* item number of last region hit */
static int last_ihigh;			/* last inten_high */
static int ntomiss;			/* number of pixels to miss */
static int col;				/* column; for PP 75 char/line crap */

void	view_pixel() {}
void	pknum();

/* Support for pretty-picture files */
int
pphit( ap, PartHeadp, segHeadp )
register struct application *ap;
struct partition *PartHeadp;
struct seg	*segHeadp;
{
	register struct partition *pp;
	register struct hit *hitp;
	LOCAL double cosI0;
	register int i,j;
	vect_t		normal;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("pphit:  no hit out front?\n");
		return(0);
	}
	hitp = pp->pt_inhit;
	RT_HIT_NORMAL( normal, hitp, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip );

#define pchar(c) {putc(c,stdout);if(col++==74){putc('\n',stdout);col=0;}}

	cosI0 = -VDOT(normal, ap->a_ray.r_dir);
	if( pp->pt_inflip )
		cosI0 = -cosI0;
	if( cosI0 <= 0.0 )  {
		ntomiss++;
		return(0);
	}
	if( ntomiss > 0 )  {
		pchar(' ');	/* miss target cmd */
		pknum( ntomiss );
		ntomiss = 0;
		last_solidp = SOLTAB_NULL;
	}
	if( last_item != pp->pt_regionp->reg_regionid )  {
		last_item = pp->pt_regionp->reg_regionid;
		pchar( '#' );	/* new item cmd */
		pknum( last_item );
		last_solidp = SOLTAB_NULL;
	}
	if( last_solidp != pp->pt_inseg->seg_stp )  {
		last_solidp = pp->pt_inseg->seg_stp;
		pchar( '!' );		/* new solid cmd */
	}
	i = cosI0 * 255.0;		/* integer angle */
	j = (i>>5) & 07;
	if( j != last_ihigh )  {
		last_ihigh = j;
		pchar( '0'+j );		/* new inten high */
	}
	j = i & 037;
	pchar( '@'+j );			/* low bits of pixel */
	return(1);
}

int
ppmiss(ap)
struct application *ap;
{
	last_solidp = SOLTAB_NULL;
	ntomiss++;
	return(0);
}

void
view_eol()
{
	pchar( '.' );		/* End of scanline */
	last_solidp = SOLTAB_NULL;
	ntomiss = 0;
}

/*
 *  Called when the picture is finally done.
 */
void
view_end()
{
	fprintf( stdout, "/\n" );	/* end of view */
	fflush( stdout );
}

/*
 *  			P K N U M
 *  
 *  Oddball 5-bits in a char ('@', 'A', ... on up) number packing.
 *  Number is written 5 bits at a time, right to left (low to high)
 *  until there are no more non-zero bits remaining.
 */
void
pknum( arg )
int arg;
{
	register long i = arg;

	do {
		pchar( (int)('@'+(i & 037)) );
		i >>= 5;
	} while( i > 0 );
}

/*
 *  			V I E W _ I N I T
 */
int
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
{
	extern double azimuth, elevation;

	ap->a_hit = pphit;
	ap->a_miss = ppmiss;
	ap->a_onehit = 1;

	if( !minus_o )
		fprintf(stderr,"Warning:  -o ignored, .PP goes to stdout\n");

	fprintf(stdout, "%s: %s (RT)\n", file, obj );
	fprintf(stdout, "%10d%10d", (int)azimuth, (int)elevation );
	fprintf(stdout, "%10d%10d\n", width, height );
	return(0);		/* no framebuffer needed */
}

void	view_2init()  {;}

void	view_setup() {}
void	view_cleanup() {}

void application_init () {}
