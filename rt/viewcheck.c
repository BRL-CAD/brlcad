/*
 *			V I E W C H E C K
 *
 *  Ray Tracing program RTCHECK bottom half.
 *
 *  This module outputs overlapping partitions, no other information.
 *  The partitions are written to the output file (typically stdout)
 *  as BRL-UNIX-plot 3-D floating point lines, so that they can be
 *  processed by any tool that reads UNIX-plot.  Because the BRL UNIX
 *  plot format is defined in a machine independent way, this program
 *  can be run anywhere, and the results piped back for local viewing,
 *  for example, on a workstation.
 *
 *  ToDo: It would be nice if we could pass in (1) an overlap depth
 *  tolerance, (2) choose either region pair or solid pair grouping
 *  and (3) set the verbosity, e.g. whether to print *every* overlap
 *  or not.
 *
 *  Authors -
 *	Michael John Muuss
 *	Gary S. Moss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCScheckview[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"

#define OVLP_TOL	0.1

extern int	rpt_overlap;		/* report overlapping region names */
int		use_air = 0;		/* Handling of air in librt */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,		FUNC_NULL }
};

extern FILE	*outfp;

char usage[] = "Usage:  rtcheck [options] model.g objects...\n\
Options:\n\
 -s #       Square grid size in pixels (default 512)\n\
 -w # -n #  Grid size width and height in pixels\n\
 -V #       View (pixel) aspect ratio (width/height)\n\
 -a #       Azimuth in degrees\n\
 -e #       Elevation in degrees\n\
 -M         Read matrix, cmds on stdin\n\
 -N #	    Set NMG debug flags\n\
 -o file.pl Specify UNIX-plot output file\n\
 -x #       Set librt debug flags\n\
 -X #       Set rt debug flags\n\
 -r         Report only unique overlaps\n\
 -P #       Set number of processors\n\
";

static int	noverlaps;		/* Number of overlaps seen */
static int	overlap_count;		/* Number of overlap pairs seen */

/*
 *  For each unique pair of regions that we find an overlap for
 *  we build up one of these structures.
 *  Note that we could also discriminate at the solid pair level.
 */
struct overlap_list {
	struct overlap_list *next;	/* next one */
	CONST char 	*reg1;		/* overlapping region 1 */
	CONST char	*reg2;		/* overlapping region 2 */
	long	count;			/* number of time reported */
	double	maxdepth;		/* maximum overlap depth */
};
static struct overlap_list *olist=NULL;	/* root of the list */


/*
 *			H I T
 *
 * Null function -- handle a hit
 */
/*ARGSUSED*/
int
hit( ap, PartHeadp )
struct application *ap;
register struct partition *PartHeadp;
{
	return	1;
}

/*
 *			M I S S
 *
 *  Null function -- handle a miss
 */
/*ARGSUSED*/
int
miss( ap )
struct application *ap;
{
	return	0;
}

/*
 *			O V E R L A P
 *
 *  Write end points of partition to the standard output.
 *  If this routine return !0, this partition will be dropped
 *  from the boolean evaluation.
 */
int
overlap( ap, pp, reg1, reg2 )
struct application	*ap;
struct partition	*pp;
struct region		*reg1;
struct region		*reg2;
{	
	register struct xray	*rp = &ap->a_ray;
	register struct hit	*ihitp = pp->pt_inhit;
	register struct hit	*ohitp = pp->pt_outhit;
	vect_t	ihit;
	vect_t	ohit;
	double depth;

	VJOIN1( ihit, rp->r_pt, ihitp->hit_dist, rp->r_dir );
	VJOIN1( ohit, rp->r_pt, ohitp->hit_dist, rp->r_dir );
	depth = ohitp->hit_dist - ihitp->hit_dist;
	if( depth < OVLP_TOL )
		return(0);

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	pdv_3line( outfp, ihit, ohit );
	noverlaps++;
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( !rpt_overlap ) {
		rt_log("OVERLAP %d: %s\nOVERLAP %d: %s\nOVERLAP %d: depth %gmm\nOVERLAP %d: in_hit_point (%g,%g,%g) mm\nOVERLAP %d: out_hit_point (%g,%g,%g) mm\n------------------------------------------------------------\n",
			noverlaps,reg1->reg_name,
			noverlaps,reg2->reg_name,
			noverlaps,depth,
			noverlaps,ihit[X],ihit[Y],ihit[Z],
			noverlaps,ohit[X],ohit[Y],ohit[Z]);

	/* If we report overlaps, don't print if already noted once.
	 * Build up a linked list of known overlapping regions and compare 
	 * againt it.
	 */
	} else {
		struct overlap_list	*prev_ol = (struct overlap_list *)0;
		struct overlap_list	*op;		/* overlap list */
		struct overlap_list     *new_op;
		new_op =(struct overlap_list *)rt_malloc(sizeof(struct overlap_list),"overlap list");

		/* look for it in our list */
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		for( op=olist; op; prev_ol=op,op=op->next ) {
			if( (strcmp(reg1->reg_name,op->reg1) == 0)
			 && (strcmp(reg2->reg_name,op->reg2) == 0) ) {
				op->count++;
				if( depth > op->maxdepth )
					op->maxdepth = depth;
				bu_semaphore_release( BU_SEM_SYSCALL );
				rt_free( (char *) new_op, "overlap list");
				return	0;	/* already on list */
			}
		}

		/* we have a new overlapping region pair */
		overlap_count++;
		op = new_op;
		if( olist )		/* previous entry exists */
			prev_ol->next = op;
		else
			olist = op;	/* finally initialize root */
		op->reg1 = reg1->reg_name;
		op->reg2 = reg2->reg_name;
		op->maxdepth = depth;
		op->next = NULL;
		op->count = 1;
		bu_semaphore_release( BU_SEM_SYSCALL );
	}

	return(0);	/* No further consideration to this partition */
}

/*
 *  			V I E W _ I N I T
 *
 *  Called once for this run.
 */
int
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
int minus_o;
{
	ap->a_hit = hit;
	ap->a_miss = miss;
	ap->a_overlap = overlap;
	ap->a_onehit = 0;
	if( !minus_o)			/* Needs to be set to  stdout */
		outfp = stdout;
	return	0;		/* No framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  Called at the beginning of each frame
 */
void
view_2init( ap )
register struct application *ap;
{
	register struct rt_i *rtip = ap->a_rt_i;
	
	pdv_3space( outfp, rtip->rti_pmin, rtip->rti_pmax );
	noverlaps = 0;
	overlap_count = 0;
}

/*
 *			V I E W _ E N D
 *
 *  Called at the end of each frame
 */
void
view_end() {
	pl_flush(outfp);
	fflush(outfp);
	rt_log("%d overlaps detected\n", noverlaps);
	if( rpt_overlap ) {
		register struct overlap_list *op, *nextop;

		rt_log("%d overlapping region pairs\n", overlap_count);
		op = olist;
		while( op ) {
			rt_log("OVERLAP : %s\nOVERLAP : %s\n%d overlap%c detected, maximum depth is %gmm\n-------------------------------------------\n",
			op->reg1, op->reg2,
			op->count, op->count>1 ? 's' : (char) 0, op->maxdepth);
			/* free struct */
			nextop = op->next;
			rt_free( (char *)op, "overlap_list" );
			op = nextop;
		}
		olist = (struct overlap_list *)NULL;
	}
	rt_log("\n");
}

/*
 *	Stubs
 */
void view_pixel() {}

void view_eol() {}

void view_setup() {}
void view_cleanup() {}
void application_init () {}
