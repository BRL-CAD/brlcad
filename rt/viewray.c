/*
 *			V I E W R A Y
 *
 *  Ray Tracing program RTRAY bottom half.
 *
 *  This module turns RT library partition lists into VLD Standard Format
 *  ray files, as defined by /vld/include/ray.h.  A variety of VLD programs
 *  exist to manipulate these files, including rayvect.
 *
 *  To obtain a UNIX-plot of a ray file, the procedure is:
 *	/vld/bin/rayvect -mMM < file.ray > file.vect
 *	/vld/bin/vectplot -mMM < file.vect > file.plot
 *	tplot -Tmeg file.plot		# or equivalent
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
static char RCSrayview[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"

#include "rdebug.h"

int		use_air = 1;		/* Handling of air in librt */

int		using_mlib = 0;		/* Material routines NOT used */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,	FUNC_NULL }
};

extern FILE	*outfp;			/* optional output file */

char usage[] = "\
Usage:  rtray [options] model.g objects... >file.ray\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -o model.ray	Specify output file, ray(5V) format (default=stdout)\n\
 -U #		Set use_air boolean to #\n\
 -x #		Set librt debug flags\n\
";

void	view_pixel() {}

void	view_setup() {}
void	view_cleanup() {}

/* "paint" types are negative ==> interpret as "special" air codes */
#define PAINT_FIRST_ENTRY	(-999)
#define PAINT_INTERN_EXIT	(-998)
#define PAINT_INTERN_ENTRY	(-997)
#define PAINT_FINAL_EXIT	(-996)
#define PAINT_AIR		(-1)

/* Handle a miss */
int
raymiss(ap)
struct application *ap;
{
	return(0);
}

/*
 *			R A Y H I T
 *
 *  Write a hit to the ray file.
 *  Also generate various forms of "paint".
 */
int
rayhit( ap, PartHeadp, segHeadp )
struct application *ap;
register struct partition *PartHeadp;
struct seg		*segHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;
	struct partition	*np;	/* next partition */
	vect_t		inormal;
	vect_t		onormal;
	vect_t		inormal2;

	if( pp == PartHeadp )
		return(0);		/* nothing was actually hit?? */

	/* "1st entry" paint */
	RT_HIT_NORMAL( inormal, pp->pt_inhit, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip );
	wraypaint( pp->pt_inhit->hit_point, inormal,
		PAINT_FIRST_ENTRY, ap, outfp );

	for( ; pp != PartHeadp; pp = pp->pt_forw )  {
		/* Write the ray for this partition */
		RT_HIT_NORMAL( inormal, pp->pt_inhit, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip );

		if( pp->pt_outhit->hit_dist < INFINITY )  {
			/* next macro must be on one line for 3d compiler */
			RT_HIT_NORMAL( onormal, pp->pt_outhit, pp->pt_outseg->seg_stp, &(ap->a_ray), pp->pt_outflip );
		}
		wray( pp, ap, outfp, inormal );

		/*
		 * If there is a subsequent partition that does not
		 * directly join this one, output an invented
		 * "air" partition between them.
		 */
		if( (np = pp->pt_forw) == PartHeadp )
			break;		/* end of list */

		/* Obtain next inhit normals & hit point, for code below */
		RT_HIT_NORMAL( inormal2, np->pt_inhit, np->pt_inseg->seg_stp, &(ap->a_ray), np->pt_inflip );

		if( rt_fdiff( pp->pt_outhit->hit_dist,
			      np->pt_inhit->hit_dist) >= 0 )  {
			/*
			 *  The two partitions touch (or overlap!).
			 *  If both are air, or both are solid, then don't
			 *  output any paint.
			 */
			if( pp->pt_regionp->reg_regionid > 0 )  {
				/* Exiting a solid */
				if( np->pt_regionp->reg_regionid > 0 )
					continue;	/* both are solid */
				/* output "internal exit" paint */
				wraypaint( pp->pt_outhit->hit_point,
					onormal,
					PAINT_INTERN_EXIT, ap, outfp );
			} else {
				/* Exiting air */
				if( np->pt_regionp->reg_regionid <= 0 )
					continue;	/* both are air */
				/* output "internal entry" paint */
				wraypaint( np->pt_inhit->hit_point,
					inormal2,
					PAINT_INTERN_ENTRY, ap, outfp );
			}
			continue;
		}

		/*
		 *  The two partitions do not touch.
		 *  Put "internal exit" paint on out point,
		 *  Install "general air" in between,
		 *  and put "internal entry" paint on in point.
		 */
		wraypaint( pp->pt_outhit->hit_point,
			onormal,
			PAINT_INTERN_EXIT, ap, outfp );

		wraypts( pp->pt_outhit->hit_point,
			onormal,
			np->pt_inhit->hit_point,
			PAINT_AIR, ap, outfp );

		wraypaint( np->pt_inhit->hit_point,
			inormal2,
			PAINT_INTERN_ENTRY, ap, outfp );
	}

	/* "final exit" paint -- ray va(r)nishes off into the sunset */
	pp = PartHeadp->pt_back;
	if( pp->pt_outhit->hit_dist < INFINITY )  {
		RT_HIT_NORMAL( inormal, pp->pt_inhit, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip );
		wraypaint( pp->pt_outhit->hit_point,
			inormal,
			PAINT_FINAL_EXIT, ap, outfp );
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

	return(0);		/* No framebuffer needed */
}

void	view_eol() {;}

void
view_end()
{
	fflush(outfp);
}

void
view_2init( ap )
struct application	*ap;
{

	if( outfp == NULL )
		rt_bomb("outfp is NULL\n");
}

void application_init () {}
