/*
 *			V I E W G 3
 *
 *  Ray Tracing program RTG3 bottom half.
 *
 *  This module turns RT library partition lists into
 *  the old GIFT type shotlines with three components per card,
 *  and with both the entrance and exit obliquity angles.
 *  The output format is:
 *	overall header card
 *		view header card
 *			ray (shotline) header card
 *				component card(s)
 *			ray (shotline) header card
 *			 :
 *			 :
 *
 *  At present, the main use for this format ray file is
 *  to drive the JTCG-approved COVART2 and COVART3 applications.
 *
 *  Authors -
 *	Dr. Susanne L. Muuss
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSrayg3[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"

#include "rdebug.h"

extern double	mat_radtodeg;

/***** view.c variables imported from rt.c *****/
extern mat_t	view2model;
extern mat_t	model2view;

/***** worker.c variables imported from rt.c *****/
extern int	jitter;			/* jitter ray starting positions */
extern fastf_t	aspect;			/* view aspect ratio X/Y */
extern vect_t	dx_model;		/* view delta-X as model-space vect */
extern vect_t	dy_model;		/* view delta-Y as model-space vect */
extern point_t	eye_model;		/* model-space location of eye */
extern int	width;			/* # of pixels in X */
extern int	height;			/* # of lines in Y */
/*****/

int		use_air = 1;		/* Handling of air in librt */

int		using_mlib = 0;		/* Material routines NOT used */

/* Viewing module specific "set" variables */
 struct structparse view_parse[] = {
	(char *)0,(char *)0,	(stroff_t)0,				FUNC_NULL
 };


extern FILE	*outfp;			/* optional output file */

extern double	azimuth, elevation;
extern vect_t	dx_model;		/* view delta-X as model-space vect */

char usage[] = "\
Usage:  rtg3 [options] model.g objects... >file.ray\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -o model.ray	Specify output file, ray(5V) format (default=stdout)\n\
 -U #		Set use_air boolean to #\n\
 -x #		Set librt debug flags\n\
";

/* Null function -- handle a miss */
int	raymiss() { return(0); }

void	view_pixel() {}

/*
 *			R A Y H I T
 *
 *  Write a hit to the ray file.
 *  Also generate various forms of "paint".
 */
int
rayhit( ap, PartHeadp )
struct application *ap;
register struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	struct partition	*np;	/* next partition */
	struct partition	air;
	int 			count;
	fastf_t			h, v;		/* h,v actual ray pos */
	fastf_t			hcen, vcen;	/* h,v cell center */
	fastf_t			dfirst, dlast;	/* ray distances */
	fastf_t			dcorrection;	/* RT to GIFT dist corr */

	if( pp == PartHeadp )
		return(0);		/* nothing was actually hit?? */

	/*  count components in partitions */
	count = 0;
	for( pp=PartHeadp->pt_forw; pp!=PartHeadp; pp=pp->pt_forw )
		count++;

	/*
	 *  GIFT format wants grid coordinates, which are the
	 *  h,v coordinates of the screen plane projected into model space.
	 */
	hcen = (ap->a_x + 0.5) * MAGNITUDE(dx_model);
	vcen = (ap->a_y + 0.5) * MAGNITUDE(dy_model);
	if( jitter )  {
		vect_t	hv;
		/*
		 *  Find exact h,v coordinates of ray by
		 *  projecting start point back into view coordinates,
		 *  and converting from view coordinates (-1..+1) to
		 *  screen (pixel) coordinates for h,v.
		 */
		MAT4X3PNT( hv, model2view, ap->a_ray.r_pt );

		h = (hv[X]+1)*0.5 * width * MAGNITUDE(dx_model);
		v = (hv[Y]+1)*0.5 * height * MAGNITUDE(dy_model);
	} else {
		/* h,v coordinates of ray are of the lower left corner */
		h = ap->a_x * MAGNITUDE(dx_model);
		v = ap->a_y * MAGNITUDE(dy_model);
	}

	/*
	 *  In RT, rays are launched from the plane of the screen,
	 *  and ray distances are relative to the start point.
	 *  In GIFT-3 output files, ray distances are relative to
	 *  the screen plane translated so that it contains the origin.
	 *  A distance correction is required to convert between the two.
	 *  XXX This really should be computed only once, not every time.
	 */
	{
		vect_t	tmp;
		vect_t	viewZdir;

		VSET( tmp, 0, 0, -1 );		/* viewing direction */
		MAT4X3VEC( viewZdir, view2model, tmp );
		VUNITIZE( viewZdir );
		/* dcorrection will typically be negative */
		dcorrection = VDOT( ap->a_ray.r_pt, viewZdir );
	}
	dfirst = PartHeadp->pt_forw->pt_inhit->hit_dist + dcorrection;
	dlast = PartHeadp->pt_back->pt_outhit->hit_dist + dcorrection;

	/*
	 *  Output the ray header.  The GIFT statements that
	 *  would have generated this are:
	 *  410	write(1,411) hcen,vcen,h,v,ncomp,dfirst,dlast,a,e
	 *  411	format(2f7.1,2f9.3,i3,2f8.2,' A',f6.1,' E',f6.1)
	 *
	 *  NOTE:  azimuth and elevation should really be computed
	 *  from ap->a_ray.r_dir;  for now, assume all rays are parallel.
	 */
	fprintf(stdout,"%7.1f%7.1f%9.3f%9.3f%3d%8.2f%8.2f A%6.1f E%6.1f\n",
		hcen, vcen, h, v,
		count,
		dfirst, dlast,
		azimuth, elevation );

	/* loop here to deal with individual components */
	for( pp=PartHeadp->pt_forw; pp!=PartHeadp; pp=pp->pt_forw )  {
		/*
		 *  The GIFT statements that would have produced
		 *  this output are:
		 *	do 632 i=icomp,iend
		 *	if(clos(icomp).gt.999.99.or.slos(i).gt.999.9) goto 635
		 * 632	continue
		 * 	write(1,633)(item(i),clos(i),cangi(i),cango(i),
		 * &			kspac(i),slos(i),i=icomp,iend)
		 * 633	format(1x,3(i4,f6.2,2f5.1,i1,f5.1))
		 *	goto 670
		 * 635	write(1,636)(item(i),clos(i),cangi(i),cango(i),
		 * &			kspac(i),slos(i),i=icomp,iend)
		 * 636	format(1x,3(i4,f6.1,2f5.1,i1,f5.0))
		 */
		fastf_t	comp_thickness;	/* component line of sight thickness */
		fastf_t	in_obliq;	/* in obliquity angle */
		fastf_t	out_obliq;	/* out obliquity angle */
		int	region_id;	/* solid region's id */
		int	air_id;		/* air id */
		fastf_t	air_thickness;	/* air line of sight thickness */
		register struct partition	*nextpp = pp->pt_forw;

		if( (region_id = pp->pt_regionp->reg_regionid) <= 0 )  {
			rt_log("air region found when solid region expected, using id=111\n");
			region_id = 111;
		}
		comp_thickness = pp->pt_outhit->hit_dist -
				 pp->pt_inhit->hit_dist;
		if( nextpp == PartHeadp )  {
			/* Last partition, no air follows, use code 9 */
			air_id = 9;
			air_thickness = 0.0;
		} else if( nextpp->pt_regionp->reg_regionid <= 0 &&
			nextpp->pt_regionp->reg_aircode != 0 )  {
			/* Next partition is air region */
			air_id = nextpp->pt_regionp->reg_aircode;
			air_thickness = nextpp->pt_outhit->hit_dist -
				nextpp->pt_inhit->hit_dist;
		} else {
			/* 2 solid regions, maybe with gap */
			air_id = 0;
			air_thickness = nextpp->pt_inhit->hit_dist -
				pp->pt_outhit->hit_dist;
			if( !NEAR_ZERO( air_thickness, 0.1 ) )  {
				air_id = 1;	/* air gap */
				rt_log("air gap added\n");
			} else {
				air_thickness = 0.0;
			}
		}

		/*
		 *  Compute the obliquity angles in degrees, ie,
		 *  the "elevation/declension" angle off the normal vector.
		 *  RT normals always point outwards, but the obliquity
		 *  angle seems be interpreted differently between
		 *  in_obliq and out_obliq, hence the sign change.  Check this.
		 *  XXX this should probably be done with atan2()
		 */
		RT_HIT_NORM( pp->pt_inhit,
			pp->pt_inseg->seg_stp, &(ap->a_ray) );
		if( pp->pt_inflip )  {
			VREVERSE( pp->pt_inhit->hit_normal,
				  pp->pt_inhit->hit_normal );
			pp->pt_inflip = 0;
		}
		in_obliq = acos( -VDOT( ap->a_ray.r_dir,
			pp->pt_inhit->hit_normal ) ) * mat_radtodeg;
		RT_HIT_NORM( pp->pt_outhit,
			pp->pt_outseg->seg_stp, &(ap->a_ray) );
		if( pp->pt_outflip )  {
			VREVERSE( pp->pt_outhit->hit_normal,
				  pp->pt_outhit->hit_normal );
			pp->pt_outflip = 0;
		}
		out_obliq = acos( VDOT( ap->a_ray.r_dir,
			pp->pt_outhit->hit_normal ) ) * mat_radtodeg;

		fprintf(stdout,"%4d%6.2f%5.1f%5.1f%1d%5.1f\n",
			region_id,
			comp_thickness,
			in_obliq, out_obliq,
			air_id, air_thickness );
	}

	return(0);
}

/*
 *  			V I E W _ I N I T
 */
int
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
{
	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 0;

	if( minus_o )
		rt_bomb("error is only to stdout\n");

	/*
	 *  Overall header, to be read by COVART format:
	 *  9220 FORMAT( BZ, I5, 10A4 )
	 *	number of views, title
	 *  Initially, do only one view per run of RTG3.
	 */
        fprintf(stdout,"%5d %s %s\n", 1, file, obj);

	return(0);		/* No framebuffer needed */
}

void	view_eol() {;}

void
view_end()
{
	/* Need to output special 999.9 record here */
	fflush(stdout);
}

void
view_2init( ap )
struct application	*ap;
{

	if( stdout == NULL )
		rt_bomb("stdout is NULL\n");

	/*
	 *  Header for each view, to be read by COVART format:
	 *  9230 FORMAT( BZ, 2( 5X, E15.8), 30X, E10.3 )
	 *	azimuth, elevation, grid_spacing
	 * NOTE that GIFT provides several other numbers that are not used
	 * by COVART;  this should be investigated.
	 * NOTE that grid_spacing is assumed to be square (by COVART),
	 * and that (for now), the units are MM.
	 * NOTE that variables "azimuth and elevation" are not valid
	 * when the -M flag is used.
	 */
	fprintf(stdout,
		"     %-15.8f     %-15.8f                              %10g\n",
		azimuth, elevation, MAGNITUDE(dx_model) );

	regionfix( ap, "rtray.regexp" );		/* XXX */
}
