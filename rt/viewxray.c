/*
 *			V I E W G R I D . C
 *
 *  Ray Tracing program RTGRID bottom half.
 */

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "../rt/rdebug.h"
#include <brlcad/fb.h>

extern	FBIO	*fbp;
extern	FILE	*outfp;
extern	mat_t	view2model;
extern	fastf_t	viewsize;
extern	int	lightmodel;
extern	double	AmbientIntensity;	/* XXX - temp hack for max ref! */

/* firing grid model */
extern	point_t	viewbase_model;		/* lower_left of viewing plane */
extern	vect_t	dx_model;
extern	vect_t	dy_model;
extern	point_t	eye_model;		/* ray origin for perspective */

extern	int	width, height;
extern	double	azimuth, elevation;

static	unsigned char scanbuf[1024*3];	/* CRAY hack */
static	int	gridline[1024];
static	double	volume;

/* Pacify RT */
mlib_setup() { return(1); }
mlib_free() {}

char usage[] = "\
Usage: rtgrid [options] model.g objects... >stuff\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -o file.rad	Output file name, else stdout\n\
 -x #		Set librt debug flags\n\
 -l 0		line buffered B&W X-rays (default)\n\
 -l 1		Uniform grid output (LOTS of data!)\n\
 -l 2		Floating point X-rays\n\
 -l ?		Center of Mass, Moments of Inertia?\n\
";

/* lighting models */
#define	LBWXRAY	0
#define	LCUBES	1
#define	LFXRAY	2

/*
 *  Called at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
{
	if( lightmodel == LBWXRAY )
		return(1);		/* we need a framebuffer */
	else
		return(0);		/* no framebuffer needed */
}

static int gridhit();
static int gridmiss();

/* beginning of a frame */
view_2init( ap )
struct application *ap;
{
#ifdef save_model_rpp
	struct rt_i *rtip = ap.a_rt_i;
	if( lightmodel == LGRID ) {
		rtip->mdl_min[X], rtip->mdl_max[X], etc.
	}
#endif

	rt_log( "Cube size: %f x %f x ? mm\n",
		viewsize/width, viewsize/height );

	ap->a_hit = gridhit;
	ap->a_miss = gridmiss;
	ap->a_onehit = 0;
}

/* end of each pixel */
view_pixel()
{
	if( lightmodel == LCUBES )
		fwrite( gridline, sizeof(*gridline), width, outfp );
}

/* end of each line */
view_eol( ap )
register struct application *ap;
{
	if( lightmodel == LBWXRAY ) {
		fb_write( fbp, 0, ap->a_y, scanbuf, width );
	}
}

/* end of each frame */
view_end()
{
	volume *= (viewsize/width)*(viewsize/height);
	volume *= 1.0e-9;
	rt_log( "Volume = %f m^3\n", volume );
	volume = 0;
}

static int
gridhit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp;
	register struct hit *hitp;
	static RGBpixel color = { 255, 255, 255 };
	fastf_t	totdist;
	fastf_t	fvalue;
	unsigned char value;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("radhit:  no hit out front?\n");
		return(0);
	}

	if(rdebug&RDEBUG_HITS)  {
		rt_pr_pt( ap->a_rt_i, pp );
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
			rt_pr_pt( ap->a_rt_i, pp );
		return(0);
	}

	/* Finally! We are ready to walk the partition chain */
	if( lightmodel == LCUBES ) {
		cubeout( pp, PartHeadp );
	}

	/* Compute the total thickness */
	totdist = 0;
	while( pp != PartHeadp ) {
		double	dist;

		dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
		totdist += dist;

		pp = pp->pt_forw;
	}

	switch( lightmodel ) {
	case LFXRAY:
		fwrite( &totdist, sizeof(totdist), 1, outfp );
		break;
	case LBWXRAY:
		fvalue = 255.0 * (1.0 - 1.4*totdist/viewsize);
		if( fvalue > 255 ) value == 255;
		else if( fvalue < 0 ) value = 0;
		else value = fvalue;
		scanbuf[ap->a_x*3+RED] = value;
		scanbuf[ap->a_x*3+GRN] = value;
		scanbuf[ap->a_x*3+BLU] = value;
		break;
	}

	/*
	 *  Mass = SUM density(x,y,z)
	 *
	 *  C.G. = SUM(x * density(x,y,z))/Mass,  etc.
	 *
	 *  M.I. = SUM(y^2+z^2 * density(x,y,z))/Mass,  etc.
	 */
	volume += totdist;

	return(1);	/* report hit to main routine */
}

static int
gridmiss( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	static	double	zero = 0;

	switch( lightmodel ) {
	case LCUBES:
		cubeout( ap, ap );	/* XXX - HACK! */
		break;
	case LBWXRAY:
		scanbuf[ap->a_x*3+RED] = 0;
		scanbuf[ap->a_x*3+GRN] = 0;
		scanbuf[ap->a_x*3+BLU] = 0;
		break;
	case LFXRAY:
		fwrite( &zero, sizeof(zero), 1, outfp );
		break;
	default:
		rt_log( "gridmiss: Bad lighting model %d\n", lightmodel );
		break;
	}

	return(0);	/* report miss to main routine */
}

cubeout( pp, PartHeadp )
register struct partition *pp;
struct partition *PartHeadp;
{
	int	i;
	double	z1, z2;
	double	DELTA;

	DELTA = viewsize/width;

	for( i = 0; i < width; i++ ) {
		gridline[i] = 0;
	}

	if( pp == PartHeadp )
		return;

	for( i = 0; i < width; i++ ) {
		z1 = i * DELTA;		/* start of cell */
		z2 = (i+1) * DELTA;	/* end of cell */
checkagain:
		if( z2 < pp->pt_inhit->hit_dist ) {
			/* haven't gotten there yet */
			continue;
		} else if( z1 < pp->pt_outhit->hit_dist ) {
			/* inside region */
			gridline[i] = pp->pt_regionp->reg_regionid;
		} else {
			/* we are beyond the current partition */
			while( pp->pt_inhit->hit_dist < z1 ) {
				pp = pp->pt_forw;
				if( pp == PartHeadp )
					return;
			}
			goto checkagain;
		}
	}
}
