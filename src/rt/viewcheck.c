/*                     V I E W C H E C K . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2007 United States Government as represented by
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
/** @file viewcheck.c
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
 */
#ifndef lint
static const char RCScheckview[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"

#define OVLP_TOL	0.1

extern int	rpt_overlap;		/* report overlapping region names */
int		use_air = 0;		/* Handling of air in librt */
extern int	rt_text_mode;


/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL }
};

extern FILE	*outfp;

char usage[] = "Usage:  rtcheck [options] model.g objects...\n\
Options:\n\
 -s #       Square grid size in pixels (default 512)\n\
 -w # -n #  Grid size width and height in pixels\n\
 -V #       View (pixel) aspect ratio (width/height)\n\
 -a #       Azimuth in degrees\n\
 -e #       Elevation in degrees\n\
 -g #       Grid cell width\n\
 -G #       Grid cell height\n\
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
static int	unique_overlap_count;	/* Number of unique overlap pairs seen */

/*
 *  For each unique pair of regions that we find an overlap for
 *  we build up one of these structures.
 *  Note that we could also discriminate at the solid pair level.
 */
struct overlap_list {
	struct overlap_list *next;	/* next one */
	const char 	*reg1;		/* overlapping region 1 */
	const char	*reg2;		/* overlapping region 2 */
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
hit(struct application *ap, register struct partition *PartHeadp, struct seg *segHeadp)
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
miss(struct application *ap)
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
overlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2)
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
		bu_log("OVERLAP %d: %s\nOVERLAP %d: %s\nOVERLAP %d: depth %gmm\nOVERLAP %d: in_hit_point (%g,%g,%g) mm\nOVERLAP %d: out_hit_point (%g,%g,%g) mm\n------------------------------------------------------------\n",
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
		new_op =(struct overlap_list *)bu_malloc(sizeof(struct overlap_list),"overlap list");

		/* look for it in our list */
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		for( op=olist; op; prev_ol=op, op=op->next ) {

			/* if we already have an entry for this region pair,
			 * we increase the counter and return
			 */
			if( (strcmp(reg1->reg_name,op->reg1) == 0) && (strcmp(reg2->reg_name,op->reg2) == 0) ) {
				op->count++;
				if( depth > op->maxdepth )
					op->maxdepth = depth;
				bu_semaphore_release( BU_SEM_SYSCALL );
				bu_free( (char *) new_op, "overlap list");
				return	0;	/* already on list */
			}
		}

		for( op=olist; op; prev_ol=op, op=op->next ) {
			/* if this pair was seen in reverse, decrease the unique counter */
			if ( (strcmp(reg1->reg_name, op->reg2) == 0) && (strcmp(reg2->reg_name, op->reg1) == 0) ) {
				unique_overlap_count--;
				break;
			}
		}

		/* we have a new overlapping region pair */
		overlap_count++;
		unique_overlap_count++;

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

	/* useful debugging */
	if (0) {
		struct overlap_list	*op;		/* overlap list */
		bu_log("PRINTING LIST::reg1==%s, reg2==%s\n", reg1->reg_name, reg2->reg_name);
		for (op=olist; op; op=op->next) {
			bu_log("\tpair: %s  %s  %d matches\n", op->reg1, op->reg2, op->count);
		}
	}

	return(0);	/* No further consideration to this partition */
}

/*
 *  			V I E W _ I N I T
 *
 *  Called once for this run.
 */
int
view_init(register struct application *ap, char *file, char *obj, int minus_o)
{
	ap->a_hit = hit;
	ap->a_miss = miss;
	ap->a_overlap = overlap;
	ap->a_logoverlap = rt_silent_logoverlap;
	ap->a_onehit = 0;
	if( !minus_o)			/* Needs to be set to  stdout */
		outfp = stdout;

	if (rt_text_mode)
	    pl_setOutputMode(PL_OUTPUT_MODE_TEXT);

	return	0;		/* No framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  Called at the beginning of each frame
 */
void
view_2init(register struct application *ap)
{
	register struct rt_i *rtip = ap->a_rt_i;

	pdv_3space( outfp, rtip->rti_pmin, rtip->rti_pmax );
	noverlaps = 0;
	overlap_count = 0;
	unique_overlap_count = 0;
}


/*
 *	P R I N T _ O V E R L A P _ S U M M A R Y
 *
 *  Print out a summary of the overlaps found
 */
static void print_overlap_summary(void) {
	register struct overlap_list *op=0, *backop=0;
	int object_counter=0;

	/* if there are any overlaps, print out a summary report, otherwise just
	 * print out that there were zero overlaps
	 */
	if (noverlaps) {

		bu_log("==========================================\n");
		bu_log("SUMMARY\n");

		bu_log("\t%d overlap%c detected\n", noverlaps, (noverlaps==1)?(char)NULL:'s');
		bu_log("\t%d unique overlapping pair%c (%d ordered pair%c)\n", unique_overlap_count, (unique_overlap_count==1)?(char)NULL:'s', overlap_count, (overlap_count==1)?(char)NULL:'s');

		if (olist)	{
			bu_log("\tOverlapping objects: ");

			for (op=olist; op ; op=op->next) {

				/* iterate over the list and see if we already printed this one */
				for ( backop=olist; (backop!=op) && (backop); backop=backop->next ) {
					if ((strcmp(op->reg1, backop->reg1) == 0) || (strcmp(op->reg1, backop->reg2) == 0)) break;
				}
				/* if we got to the end of the list (backop points to the match) */
				if (!backop || (backop==op)) {
					bu_log("%s  ", op->reg1);
					object_counter++;
				}

				/* iterate over the list again up to where we are to see if the second
				 * region was already printed */
				for (backop=olist; backop; backop=backop->next) {
					if ((strcmp(op->reg2, backop->reg1) == 0) || (strcmp(op->reg2, backop->reg2) == 0)) break;
				}
				if ( !backop || (backop==op)) {
					bu_log("%s  ", op->reg2);
					object_counter++;
				}
			}
			bu_log("\n\t%d unique overlapping object%c detected\n", object_counter, (object_counter==1)?(char)NULL:'s');
		}
	}
	else {
		bu_log("%d overlap%c detected\n\n", noverlaps, (noverlaps==1)?(char)NULL:'s');
	}
}

/*
 *			V I E W _ E N D
 *
 *  Called at the end of each frame
 */
void
view_end(void) {
	pl_flush(outfp);
	fflush(outfp);
	/*	bu_log("%d overlap%c detected\n\n", noverlaps, (noverlaps==1)?(char)NULL:'s');*/

	/*        bu_log("\nocount==%d, unique_ocount==%d\n\n", overlap_count, unique_overlap_count);*/

	if( rpt_overlap ) {
		/* using counters instead of the actual variables to be able to
		 * summarize after checking for matching pairs
		 */
		int overlap_counter=overlap_count;
		int unique_overlap_counter=unique_overlap_count;
		register struct overlap_list *op=0, *backop=0, *nextop=0;

		/* iterate over the overlap pairs and output one OVERLAP section
		 * per unordered pair.  a summary is output at the end.
		 */
		bu_log("OVERLAP PAIRS\n------------------------------------------\n");
		for ( op=olist; op; op=op->next ) {

			/* !!! would/should not need to do this..  need a doubly-linked
			 * list so we can go backwards.  we look through the list and
			 * see if we hit the reverse previously.
			 */
			for ( backop=olist; (backop!=op) && (backop); backop=backop->next ) {
				if ((strcmp(op->reg2, backop->reg1) == 0) && (strcmp(op->reg1, backop->reg2) == 0)) break;
			}
			if (backop && (backop!=op)) continue;

			bu_log("%s and %s overlap\n", op->reg1, op->reg2);

			nextop=(struct overlap_list *)NULL;
			/* if there are still matching pairs to search for */
			if (overlap_counter > unique_overlap_counter) {

				/* iterate until end of pairs or we find a
				 * reverse matching pair (done inside loop
				 * explicitly)*/
				for ( nextop=op; nextop ; nextop=nextop->next) {
					if ((strcmp(op->reg1, nextop->reg2) == 0) &&
							(strcmp(op->reg2, nextop->reg1) == 0))
						break;
				}
				/* when we leave the loop, nextop is either
				 * null (hit end of list) or the matching
				 * reverse pair */
			}

			bu_log("\t<%s, %s>: %d overlap%c detected, maximum depth is %gmm\n", op->reg1, op->reg2, op->count, op->count>1 ? 's' : (char) 0, op->maxdepth);
			if (nextop) {
				bu_log("\t<%s, %s>: %d overlap%c detected, maximum depth is %gmm\n", nextop->reg1, nextop->reg2, nextop->count, nextop->count>1 ? 's' : (char) 0, nextop->maxdepth);
				/* counter the decrement below to account for
				 * the matched reverse pair
				 */
				unique_overlap_counter++;
			}

			/* decrement so we may stop scanning for unique overlaps asap */
			unique_overlap_counter--;
			overlap_counter--;
		}

		/* print out a summary of the overlaps that were found */
		print_overlap_summary();

		/* free our structures */
		op = olist;
		while( op ) {
			/* free struct */
			nextop = op->next;
			bu_free( (char *)op, "overlap_list" );
			op = nextop;
		}
		olist = (struct overlap_list *)NULL;
	}
	bu_log("\n");
}

/*
 *	Stubs
 */
void view_pixel(void) {}

void view_eol(void) {}

void view_setup(void) {}
void view_cleanup(void) {}
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
