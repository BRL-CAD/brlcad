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
 *	Susanne L. Muuss, J.D.
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

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./material.h"
#include "./ext.h"
#include "../librt/debug.h"

#include "rdebug.h"

#define	MM2IN	0.03937008		/* mm times MM2IN gives inches */
#define TOL 0.01/MM2IN			/* GIFT has a 0.01 inch tolerance */

void	part_compact();

extern fastf_t	gift_grid_rounding;
extern point_t	viewbase_model;

extern int	npsw;			/* number of worker PSWs to run */

int		use_air = 1;		/* Handling of air in librt */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,		FUNC_NULL }
};

static mat_t	model2hv;		/* model coords to GIFT h,v in inches */

static FILE	*plotfp;		/* optional plotting file */

CONST char usage[] = "\
Usage:  rtg3 [options] model.g objects... >file.ray\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -g #		Grid cell width in millimeters (conflicts with -s\n\
 -G #		Grid cell height in millimeters (conflicts with -s\n\
 -J #		Jitter.  Default is off.  Any non-zero number is on\n\
 -o model.g3	Specify output file, GIFT-3 format (default=stdout)\n\
 -U #		Set use_air boolean to # (default=1)\n\
 -x #		Set librt debug flags\n\
";

int	rayhit(), raymiss();

/*
 *  			V I E W _ I N I T
 *
 *  This routine is called by main().  It prints the overall shotline
 *  header. Furthermore, pointers to rayhit() and raymiss() are set up
 *  and are later called from do_run().
 */

static char * save_file;
static char * save_obj;

int
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
{

	if( !minus_o )
		outfp = stdout;
	
	save_file = file;
	save_obj = obj;

	/*
	 *  Cause grid_setup() to align the grid on one inch boundaries,
	 *  or cell_width boundaries, if it is given.
	 */
	if( cell_width > 0 )
		gift_grid_rounding = cell_width;
	else if( cell_height > 0 )
		gift_grid_rounding = cell_height;
	else
		gift_grid_rounding = 25.4;		/* one inch */

	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 0;

	output_is_binary = 0;		/* output is printable ascii */

	if(rdebug & RDEBUG_RAYPLOT) {
		plotfp = fopen("rtg3.pl", "w");
		if( npsw > 1 )  {
			rt_log("Note: writing rtg3.pl file can only be done using only 1 processor\n");
			npsw = 1;
		}
	}


	return(0);		/* No framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  View_2init is called by do_frame(), which in turn is called by
 *  main() in rt.c.  It writes the view-specific COVART header.
 * 
 */
void
view_2init( ap )
struct application	*ap;
{
	if( outfp == NULL )
		rt_bomb("outfp is NULL\n");

	/*
	 *  Overall header, to be read by COVART format:
	 *  9220 FORMAT( BZ, I5, 10A4 )
	 *	number of views, title
	 *  Initially, do only one view per run of RTG3.
	 */
	fprintf(outfp,"%5d %s %s\n", 1, save_file, save_obj);

	/*
	 *  Header for each view, to be read by COVART format:
	 *  9230 FORMAT( BZ, 2( 5X, E15.8), 30X, E10.3 )
	 *	azimuth, elevation, grid_spacing
	 * NOTE that GIFT provides several other numbers that are not used
	 * by COVART;  this should be investigated.
	 * NOTE that grid_spacing is assumed to be square (by COVART),
	 * and that the units have been converted from MM to IN.
	 * COVART, given the appropriate code, will take, IN, M, 
	 * FT, MM, and CM.  However, GIFT  output is expected to be IN.
	 * NOTE that variables "azimuth and elevation" are not valid
	 * when the -M flag is used.
	 * NOTE:  %10g was changed to %10f so that a decimal point is generated
	 * even when the number is an integer.  Otherwise the client codes
	 * get confused get confused and are unable to convert the number to
	 * scientific notation.
	 */
	fprintf(outfp,
		"     %-15.8f     %-15.8f                              %10f\n",
		azimuth, elevation, cell_width*MM2IN );

	/*
	 *  GIFT uses an H,V coordinate system that is anchored at the
	 *  model origin, but rotated according to the view.
	 *  For convenience later, build a matrix that will take
	 *  a point in model space (with units of mm), and convert it
	 *  to a point in HV space, with units of inches.
	 */
	mat_copy( model2hv, Viewrotscale );
	model2hv[15] = 1/MM2IN;
}

/*
 *			R A Y M I S S
 *
 *  Null function -- handle a miss
 *  This function is called by rt_shootray(), which is called by
 *  do_frame().
 */
int
raymiss()
{
	return(0);
}

/*
 *			V I E W _ P I X E L
 *
 *  This routine is called from do_run(), and in this case does nothing.
 */
void
view_pixel()
{
	return;
}

/*
 *			R A Y H I T
 *
 *  Rayhit() is called by rt_shootray() when the ray hits one or more objects.
 *  A per-shotline header record is written, followed by information about
 *  each object hit.
 *
 *  Note that the GIFT-3 format uses a different convention for the "zero"
 *  distance along the ray.  RT has zero at the ray origin (emanation plain),
 *  while GIFT has zero at the screen plain translated so that it contains
 *  the model origin.  This difference is compensated for by adding the
 *  'dcorrection' distance correction factor.
 *
 *  Also note that the GIFT-3 format requires information about the start
 *  point of the ray in two formats.  First, the h,v coordinates of the
 *  grid cell CENTERS (in screen space coordinates) are needed.
 *  Second, the ACTUAL h,v coordinates fired from are needed.
 *
 *  An optional rtg3.pl UnixPlot file is written, permitting a
 *  color vector display of ray-model intersections.
 */
int
rayhit( ap, PartHeadp )
struct application *ap;
register struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	int 			comp_count;	/* component count */
	fastf_t			dfirst, dlast;	/* ray distances */
	static fastf_t		dcorrection = 0; /* RT to GIFT dist corr */
	int			card_count;	/* # comp. on this card */
	char			*fmt;		/* printf() format string */
	struct rt_vls		str;
	char			buf[128];	/* temp. sprintf() buffer */
	point_t			hv;		/* GIFT h,v coords, in inches */
	point_t			hvcen;

	if( pp == PartHeadp )
		return(0);		/* nothing was actually hit?? */

	part_compact(ap, PartHeadp, TOL);

	/*  comp components in partitions */
	comp_count = 0;
	for( pp=PartHeadp->pt_forw; pp!=PartHeadp; pp=pp->pt_forw )  {
		comp_count++;
	}
	if( comp_count == 0 )
		return( 0 );

	/* Set up variable length string, to buffer this shotline in.
	 * Note that there is one component per card, and that each card
	 * (line) is 80 characters long.  Hence the parameters given to
	 * rt-vls-extend().
	 */

	rt_vls_init( &str );
	rt_vls_extend( &str, 80 * (comp_count+1) );

	/*
	 *  Find the H,V coordinates of the grid cell center.
	 *  RT uses the lower left corner of each cell.
	 */
	{
		point_t		center;
		fastf_t		dx;
		fastf_t		dy;

		dx = ap->a_x + 0.5;
		dy = ap->a_y + 0.5;
		VJOIN2( center, viewbase_model, dx, dx_model, dy, dy_model );
		MAT4X3PNT( hvcen, model2hv, center );
	}

	/*
	 *  Find exact h,v coordinates of actual ray start by
	 *  projecting start point into GIFT h,v coordinates.
	 */
	MAT4X3PNT( hv, model2hv, ap->a_ray.r_pt );

	/*
	 *  In RT, rays are launched from the plane of the screen,
	 *  and ray distances are relative to the start point.
	 *  In GIFT-3 output files, ray distances are relative to
	 *  the (H,V) plane translated so that it contains the origin.
	 *  A distance correction is required to convert between the two.
	 *  Since this really should be computed only once, not every time,
	 *  the trip_count flag was added.
	 */
	{

		static int  trip_count;
		vect_t	tmp;
		vect_t	viewZdir;

		if( trip_count == 0) {

			VSET( tmp, 0, 0, -1 );		/* viewing direction */
			MAT4X3VEC( viewZdir, view2model, tmp );
			VUNITIZE( viewZdir );
			/* dcorrection will typically be negative */
			dcorrection = VDOT( ap->a_ray.r_pt, viewZdir );
			trip_count = 1;
		}
	}

	/* This code is for diagnostics.
	 * rt_log("dcorrection=%g\n", dcorrection);
	 */

	/* dfirst and dlast have been made negative to account for GIFT looking
	 * in the opposite direction of RT.
	 */

	dfirst = -(PartHeadp->pt_forw->pt_inhit->hit_dist + dcorrection);
	dlast = -(PartHeadp->pt_back->pt_outhit->hit_dist + dcorrection);
#if 0
	/* This code is to note any occurances of negative distances. */
		if( PartHeadp->pt_forw->pt_inhit->hit_dist < 0)  {
			rt_log("ERROR: dfirst=%g at partition x%x\n", dfirst , PartHeadp->pt_forw );
			rt_log("\tdcorrection = %f\n" , dcorrection );
			rt_log("\tray start point is ( %f %f %f ) in direction ( %f %f %f )\n" , V3ARGS( ap->a_ray.r_pt ) , V3ARGS( ap->a_ray.r_dir ) );
			VJOIN1( PartHeadp->pt_forw->pt_inhit->hit_point , ap->a_ray.r_pt ,PartHeadp->pt_forw->pt_inhit->hit_dist , ap->a_ray.r_dir );
			VJOIN1( PartHeadp->pt_back->pt_outhit->hit_point , ap->a_ray.r_pt ,PartHeadp->pt_forw->pt_outhit->hit_dist , ap->a_ray.r_dir );
			rt_pr_partitions(ap->a_rt_i, PartHeadp, "Defective partion:");
		}
	/* End of bug trap. */
#endif
	/*
	 *  Output the ray header.  The GIFT statements that
	 *  would have generated this are:
	 *  410	write(1,411) hcen,vcen,h,v,ncomp,dfirst,dlast,a,e
	 *  411	format(2f7.1,2f9.3,i3,2f8.2,' A',f6.1,' E',f6.1)
	 */

#define	SHOT_FMT	"%7.1f%7.1f%9.3f%9.3f%3d%8.2f%8.2f A%6.1f E%6.1f\n"

	if( rt_perspective > 0 )  {
		ae_vec( &azimuth, &elevation, ap->a_ray.r_dir );
	}

#ifdef SPRINTF_NOT_PARALLEL
	/* On some systems, sprintf() is not parallel! ^%@#&^@^&#% */
	bu_semaphore_acquire( BU_SEM_SYSCALL );
#endif

	sprintf(buf, SHOT_FMT,
		hvcen[0], hvcen[1],
		hv[0], hv[1],
		comp_count,
		dfirst * MM2IN, dlast * MM2IN,
		azimuth, elevation );
#ifdef SPRINTF_NOT_PARALLEL
	bu_semaphore_release( BU_SEM_SYSCALL );
#endif
	rt_vls_strcat( &str, buf );

	/* loop here to deal with individual components */
	card_count = 0;
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
		fastf_t	dot_prod;	/* dot product of normal and ray dir */
		fastf_t	air_thickness;	/* air line of sight thickness */
		vect_t	normal;		/* surface normal */
		register struct partition	*nextpp = pp->pt_forw;

		if( (region_id = pp->pt_regionp->reg_regionid) <= 0 )  {
			rt_log("air region '%s' found when solid region expected, using id=111\n", pp->pt_regionp->reg_name);
			region_id = 111;
		}
		comp_thickness = pp->pt_outhit->hit_dist -
				 pp->pt_inhit->hit_dist;

		/* The below code is meant to catch components with zero or
		 * negative thicknesses.  This is not supposed to be possible,
		 * but the condition has been seen.
		 */
#if 0
		if( comp_thickness <= 0 )  {
			VJOIN1( pp->pt_inhit->hit_point , ap->a_ray.r_pt ,pp->pt_inhit->hit_dist , ap->a_ray.r_dir );
			VJOIN1( pp->pt_outhit->hit_point , ap->a_ray.r_pt ,pp->pt_outhit->hit_dist , ap->a_ray.r_dir );
			rt_log("ERROR: comp_thickness=%g for region id = %d at h=%g, v=%g (x=%d, y=%d), partition at x%x\n",
				comp_thickness, region_id, hv[0], hv[1], ap->a_x, ap->a_y , pp );
			rt_pr_partitions(ap->a_rt_i, PartHeadp, "Defective partion:");
			rt_log("Send this output to Sue Muuss (sue@brl.mil)\n");
			if ( ! (rt_g.debug & DEBUG_ARB8)) {
				rt_g.debug |= DEBUG_ARB8;
				rt_shootray(ap);
				rt_g.debug &= ~DEBUG_ARB8;
			}
		}
#endif

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
				if( rdebug & RDEBUG_HITS )
					rt_log("air gap added\n");
			} else {
				air_thickness = 0.0;
			}
		}

		/*
		 *  Compute the obliquity angles in degrees, ie,
		 *  the "declension" angle down off the normal vector.
		 *  RT normals always point outwards;
		 *  the "inhit" normal points opposite the ray direction,
		 *  the "outhit" normal points along the ray direction.
		 *  Hence the one sign change.
		 *  XXX this should probably be done with atan2()
		 */
		RT_HIT_NORMAL( normal, pp->pt_inhit, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip );
		dot_prod = VDOT( ap->a_ray.r_dir, normal );
		if( dot_prod > 1.0 )
			dot_prod = 1.0;
		if( dot_prod < -1.0 )
			dot_prod = (-1.0);

		in_obliq = acos( -dot_prod ) *
			mat_radtodeg;
		RT_HIT_NORMAL( normal, pp->pt_outhit, pp->pt_outseg->seg_stp, &(ap->a_ray), pp->pt_outflip );
		dot_prod = VDOT( ap->a_ray.r_dir, normal );
		if( dot_prod > 1.0 )
			dot_prod = 1.0;
		if( dot_prod < -1.0 )
			dot_prod = (-1.0);

		out_obliq = acos( dot_prod ) *
			mat_radtodeg;

		/* Check for exit obliquties greater than 90 degrees. */
#if 0
		if( in_obliq > 90 || in_obliq < 0 )  {
			rt_log("ERROR: in_obliquity=%g\n", in_obliq);
			rt_pr_partitions(ap->a_rt_i, PartHeadp, "Defective partion:");
		}
		if( out_obliq > 90 || out_obliq < 0 )  {
			rt_log("ERROR: out_obliquity=%g\n", out_obliq);
			VPRINT(" r_dir", ap->a_ray.r_dir);
			VPRINT("normal", normal);
			rt_log("dot=%g, acos(dot)=%g\n",
				VDOT( ap->a_ray.r_dir, normal ),
				acos( VDOT( ap->a_ray.r_dir, normal ) ) );
			/* Print the defective one */
			rt_pr_pt( ap->a_rt_i, pp );
			/* Print the whole ray's partition list */
			rt_pr_partitions(ap->a_rt_i, PartHeadp, "Defective partion:");
		}
#endif

		if( in_obliq > 90.0 )
			in_obliq = 90.0;
		if( in_obliq < 0.0 )
			in_obliq = 0.0;
		if( out_obliq > 90.0 )
			out_obliq = 90.0;
		if( out_obliq < 0.0 )
			out_obliq = 0.0;

		/*
		 *  Handle 3-components per card output format, with
		 *  a leading space in front of the first component.
		 */
		if( card_count == 0 )  {
			rt_vls_strcat( &str, " " );
		}
		comp_thickness *= MM2IN;
		/* Check thickness fields for format overflow */
		if( comp_thickness > 999.99 || air_thickness*MM2IN > 999.9 )
			fmt = "%4d%6.1f%5.1f%5.1f%1d%5.0f";
		else
			fmt = "%4d%6.2f%5.1f%5.1f%1d%5.1f";
#ifdef SPRINTF_NOT_PARALLEL
		bu_semaphore_acquire( BU_SEM_SYSCALL );
#endif
		sprintf(buf, fmt,
			region_id,
			comp_thickness,
			in_obliq, out_obliq,
			air_id, air_thickness*MM2IN );
#ifdef SPRINTF_NOT_PARALLEL
		bu_semaphore_release( BU_SEM_SYSCALL );
#endif
		rt_vls_strcat( &str, buf );
		card_count++;
		if( card_count >= 3 )  {
			rt_vls_strcat( &str, "\n" );
			card_count = 0;
		}

		/* A color rtg3.pl UnixPlot file of output commands
		 * is generated.  This is processed by plot(1)
		 * plotting filters such as pl-fb or pl-sgi.
		 * Portions of a ray passing through air within the
		 * model are represented in blue, while portions 
		 * passing through a solid are assigned green.
		 * This will always be done single CPU,
		 * to prevent output garbling.  (See view_init).
		 */
		if(rdebug & RDEBUG_RAYPLOT) {
			vect_t     inpt;
			vect_t     outpt;
			VJOIN1(inpt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
				ap->a_ray.r_dir);
			VJOIN1(outpt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist,
				ap->a_ray.r_dir);
				pl_color(plotfp, 0, 255, 0);	/* green */
			pdv_3line(plotfp, inpt,outpt);
			
			if(air_thickness > 0) {
				vect_t     air_end;
				VJOIN1(air_end, ap->a_ray.r_pt,
					pp->pt_outhit->hit_dist + air_thickness,
					ap->a_ray.r_dir);
				pl_color(plotfp, 0, 0, 255);	/* blue */
				pdv_3cont(plotfp, air_end);
			}
		}
	}

	/* If partway through building the line, add a newline */
	if( card_count > 0 )  {
		/*
		 *  Note that GIFT zero-fills the unused component slots,
		 *  but neither COVART II nor COVART III require it,
		 *  so just end the line here.
		 */
		rt_vls_strcat( &str, "\n" );
	}

	/* Single-thread through file output.
	 * COVART will accept non-sequential ray data provided the
	 * ray header and its associated data are not separated.  CAVEAT:
	 * COVART will not accept headers out of sequence.
	 */
	bu_semaphore_acquire( BU_SEM_SYSCALL );

	fputs( rt_vls_addr( &str ), outfp );

	/* End of single-thread region */
	bu_semaphore_release( BU_SEM_SYSCALL );

	/* Release vls storage */
	rt_vls_free( &str );

	return(0);
}

/*
 *			V I E W _ E O L
 *
 *  View_eol() is called by rt_shootray() in do_run().  In this case,
 *  it does nothing.
 */
void	view_eol()
{
}

/*
 *			V I E W _ E N D
 *
 *  View_end() is called by rt_shootray in do_run().  It
 *  outputs a special 999.9 "end of view" marker, composed of
 *  a "999.9" shotline header, with one all-zero component record.
 *  This is the way GIFT did it.  Note that the component count must also
 *  be zero on this shotline, or else the client codes get confused.
 */
void
view_end()
{
	fprintf(outfp, SHOT_FMT,
		999.9, 999.9,
		999.9, 999.9,
		0,			/* component count */
		0.0, 0.0,
		azimuth, elevation );
	/* An abbreviated component record:  just give item code 0.  This is
	 * not required since GIFT truncates the above line at the first
	 * 999.9: putting out the abovementioned 0 caused a lot of oscillation
	 * over the last year, so the line has been removed. 
	 */

	fflush(outfp);
}

void view_setup() {}
void view_cleanup() {}


/*
 *			P A R T _ C O M P A C T
 *
 * This routine takes at partition-head pointer, an application
 * structure pointer,  and a tolerance.  It goes through the partition
 * list shot-line by shot-line and checks for regions with identical
 * region-ids abutting.  If one is found, and the distance
 * between the two abbutting regions is less than the tolerance, the two
 * corresponding partions are collapsed into one, and the outhit from the
 * second partions becomes the governing outhit.  This will prevent the
 * occurance of multiple hits per same region.
 *
 */

void
part_compact(ap, PartHeadp, tolerance)
register struct application		*ap;
register struct partition		*PartHeadp;
fastf_t					tolerance;
{

	fastf_t				gap;
	struct partition		*pp;
	struct partition		*nextpp;

	/* first eliminate zero thickness partitions */
	pp = PartHeadp->pt_forw;
	while( pp != PartHeadp )
	{
		fastf_t comp_thickness;

		nextpp = pp->pt_forw;
		comp_thickness = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
		if( comp_thickness <= 0.0 )
		{
			DEQUEUE_PT( pp );
			FREE_PT( pp, ap->a_resource);
		}
		pp = nextpp;
	}

	for(pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw)  {
top:		nextpp = pp->pt_forw;
		if(nextpp == PartHeadp)  {
			break;
		}
		if(pp->pt_regionp->reg_regionid != nextpp->pt_regionp->reg_regionid)  {
			continue;
		}
		gap = nextpp->pt_inhit->hit_dist - pp->pt_outhit->hit_dist;

		/* The following line is a diagnostic that is worth reusing:
		 * rt_log("gap=%e\n", gap);
		 */

		if(gap > tolerance)  {
			continue;
		}

		/* Eliminate the gap by collapsing the two partitions
		 * into one.  The below lines have been commented out but
		 * should be retained for debugging purposes.
		 */

		 /* rt_log("part_comp: collapsing gap of %e mm between id=%d and id=%d\n",
		  *	gap, pp->pt_regionp->reg_regionid, 
		  *	nextpp->pt_regionp->reg_regionid);
		  */
		
		  pp->pt_outseg = nextpp->pt_outseg;
		  pp->pt_outhit = nextpp->pt_outhit;
		  pp->pt_outflip = nextpp->pt_outflip;


		/*
		 *  Dequeue and free the unwanted partition structure.
		 *  Referenced segments, etc, will be freed by rt_shootray().
		 */
		DEQUEUE_PT(nextpp);
		FREE_PT(nextpp, ap->a_resource);

		goto top;
	}

}

void application_init () {}
