/*                      V I E W A R E A . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 */
/** @file viewarea.c
 *
 *  Computes the exposed and presented surface area projections of
 *  specified geometry.  Exposed area is the occluded 2D projection,
 *  presented is the unoccluded 2D projection.  That is to say that if
 *  there is an object in front, it will reduce the exposed area, but
 *  not the presented area.
 *
 *  Authors -
 *    Christopher Sean Morrison
 *    John R. Anderson
 *
 *  Source -
 *    The U. S. Army Research Laboratory
 *    Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSrayg3[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ext.h"
#include "../librt/debug.h"
#include "plot3.h"
#include "rtprivate.h"

#define	MM2IN	0.03937008		/* mm times MM2IN gives inches */
#define TOL 0.01/MM2IN			/* GIFT has a 0.01 inch tolerance */

extern int	npsw;			/* number of worker PSWs to run */

int		use_air = 1;		/* Handling of air in librt */

extern int 	 rpt_overlap;

extern fastf_t  rt_cline_radius;        /* from g_cline.c */

static long hit_count=0;
static fastf_t cell_area=0.0;


/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL }
};

const char usage[] = "\
Usage:  rtarea [options] model.g objects...\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -g #		Grid cell width in millimeters (conflicts with -s)\n\
 -G #		Grid cell height in millimeters (conflicts with -s)\n\
 -J #		Jitter.  Default is off.  Any non-zero number is on\n\
 -U #		Set use_air boolean to # (default=1)\n\
 -c \"set rt_cline_radius=radius\"      Additional radius to be added to CLINE solids\n\
 -x #		Set librt debug flags\n\
";

struct area {
    struct area *assembly;	/* pointer to a linked list of assemblies */
    long hits;			/* presented hits count */
    long exposures;		/* exposed hits count */
    int seen;			/* book-keeping for exposure */
    int depth;			/* assembly depth */
    const char *name;		/* assembly name */
};

struct area_list {
    struct area *cell;
    struct area_list *next;
};

typedef enum area_type {
    PRESENTED_AREA,
    EXPOSED_AREA
} area_type_t;


int rayhit(struct application *ap, struct partition *PartHeadp, struct seg *segHeadp);
int raymiss(register struct application *ap);

/*
 *  			V I E W _ I N I T
 *
 */
int
view_init( register struct application *ap, char *file, char *obj )
{
#if defined(USE_FORKED_THREADS)
    printf("\n*** WARNING *** WARNING *** WARNING *** WARNING *** WARNING ***\nLimiting the number of processors to 1 for proper area calculations\n\n");
    npsw = 1;
    rt_g.rtg_parallel = 0;
#endif
	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 0;

	if( !rpt_overlap )
		 ap->a_logoverlap = rt_silent_logoverlap;

	output_is_binary = 0;		/* output is printable ascii */

	hit_count = 0;

	return(0);		/* No framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  View_2init is called by do_frame(), which in turn is called by
 *  main().  It writes the view-specific COVART header.
 *
 */
void
view_2init( struct application *ap )
{
    register struct region *rp;
    register struct rt_i *rtip = ap->a_rt_i;

    /* initial empty parent assembly */
    struct area *assembly;

    cell_area = cell_width * cell_height;

    assembly = (struct area *)bu_calloc(1, sizeof(struct area), "view_2init assembly allocation");

    /* allocate the initial areas and point them all to the same (empty) starting assembly list */
    bu_semaphore_acquire( BU_SEM_SYSCALL );
    for( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	struct area *cell;
	/* allocate memory first time through */
	cell = (struct area *)bu_calloc(1, sizeof(struct area), "view_2init area allocation");
	cell->assembly = assembly;
	rp->reg_udata = (genptr_t)cell;
    }
    bu_semaphore_release( BU_SEM_SYSCALL );

    return;
}

/*
 *			R A Y M I S S
 *
 *  Null function -- handle a miss
 *  This function is called by rt_shootray(), which is called by
 *  do_frame().
 */
int
raymiss(register struct application *ap)
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


/* keeps track of how many times we encounter an assembly by
 * maintaining a simple linked list of them using the same structure
 * used for regions.
 */
static void
increment_assembly_counter(register struct area *cell, const char *path, area_type_t type)
{
    int l;
    char *buffer;
    int depth;

    if (!cell || !path) {
	return;
    }

    l = strlen(path);
    buffer = bu_calloc(l+1, sizeof(char), "increment_assembly_counter buffer allocation");
    strncpy(buffer, path, l);

    /* trim off the region name */
    while (l > 0) {
	if (buffer[l-1] == '/') {
	    break;
	}
	l--;
    }
    buffer[l-1] = '\0';

    /* get the region names, one at a time */
    depth = 0;
    while (l > 0) {
	if (buffer[l-1] == '/') {
	    register struct area *cellp;

	    /* scan the assembly list */
	    cellp = cell->assembly;
	    while (cellp->name) {
		if ( (strcmp(cellp->name, &buffer[l])==0) ) {
		    if (type == EXPOSED_AREA) {
			if (!cellp->seen) {
			    cellp->exposures++;
			    cellp->seen++;
			    if (depth > cellp->depth) {
				cellp->depth = depth;
			    }
			}
		    }
		    if (type == PRESENTED_AREA) {
			cellp->hits++;
			cellp->seen++;
			if (depth > cellp->depth) {
			    cellp->depth = depth;
			}
		    }
		    break;
		}
		cellp = cellp->assembly;
	    }

	    /* insert a new assembly? */
	    if (!cellp->name) {
		char *name;

		/* sanity check */
		if (cellp->assembly) {
		    bu_log("Inconsistent assembly list detected\n");
		    break;
		}

		name = (char *)bu_malloc(strlen(&buffer[l])+1, "increment_assembly_counter assembly name allocation");
		strcpy(name, &buffer[l]);
		cellp->name = name;
		if (type == EXPOSED_AREA) {
		    cellp->exposures++;
		}
		if (type == PRESENTED_AREA) {
		    cellp->hits++;
		}
		cellp->seen++;
		if (depth > cellp->depth) {
		    cellp->depth = depth;
		}

		/* allocate space for the next assembly */
		cellp->assembly = (struct area *)bu_calloc(1, sizeof(struct area), "increment_assembly_counter assembly allocation");
	    }

	    buffer[l-1] = '\0';
	    depth++;
	}
	l--;
    }

    bu_free(buffer, "increment_assembly_counter buffer free");

    return;
}


/*
 *			R A Y H I T
 *
 */
int
rayhit(struct application *ap, struct partition *PartHeadp, struct seg *segHeadp)
{
    register struct region *rp;
    struct rt_i *rtip = ap->a_rt_i;
    register struct partition *pp = PartHeadp->pt_forw;
    register struct area *cell;
    register int l;


    if( pp == PartHeadp )
	return(0);		/* nothing was actually hit?? */

    bu_semaphore_acquire( RT_SEM_RESULTS );

    hit_count++;

    /* clear the list of visited regions */
    for( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	struct area *cellp;
	cell = (struct area *)rp->reg_udata;
	cell->seen = 0;

	for (cellp = cell->assembly; cellp; cellp = cellp->assembly) {
	    cellp->seen = 0;
	}
    }

    /* get the exposed areas (i.e. first hits) */
    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	struct region  *reg = pp->pt_regionp;
	cell = (struct area *)reg->reg_udata;

	/* ignore air */
	if (reg->reg_aircode) {
	    continue;
	}

	/* have not visited this region yet, so increment the count */
	if (!cell->seen) {
	    cell->exposures++;
	    cell->seen++;

	    if (!cell->name) {
		/* get the region name */
		int l = strlen(reg->reg_name);
		while (l > 0) {
		    if (reg->reg_name[l-1] == '/') {
			break;
		    }
		    l--;
		}
		cell->name = &reg->reg_name[l];
	    }

	    /* record the parent assemblies */
	    increment_assembly_counter(cell, reg->reg_name, EXPOSED_AREA);
	}

	/* halt after first non-air */
	break;
    }

    /* get the presented areas */
    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	struct region  *reg = pp->pt_regionp;
	cell = (struct area *)reg->reg_udata;

	/* ignore air */
	if (reg->reg_aircode) {
	    continue;
	}

	cell->hits++;

	if (!cell->name) {
	    /* get the region name */
	    int l = strlen(reg->reg_name);
	    while (l > 0) {
		if (reg->reg_name[l-1] == '/') {
		    break;
		}
		l--;
	    }
	    cell->name = &reg->reg_name[l];
	}

	/* record the parent assemblies */
	increment_assembly_counter(cell, reg->reg_name, PRESENTED_AREA);
    }
    bu_semaphore_release( RT_SEM_RESULTS );

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


/*  p r i n t _ r e g i o n _ a r e a _ l i s t
 *
 * Prints a list of region areas sorted alphabetically reporting
 * either the presented or exposed area 'type' and keeping track of
 * the 'count' of regions printed.  this routine returns the number of
 * ray hits across all regions.
 */
static long int
print_region_area_list(long int *count, struct rt_i *rtip, area_type_t type)
{
    struct region *rp;
    register struct area *cell = (struct area *)NULL;
    long int cumulative = 0;

    struct area_list *listHead = (struct area_list *)bu_malloc(sizeof(struct area_list), "print_area_list area list node allocation");
    struct area_list *listp;
    listHead->cell = (struct area *)NULL;
    listHead->next = (struct area_list *)NULL;

    /* sort the cell entries alphabetically */
    for( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	cell = (struct area *)rp->reg_udata;
	listp = listHead;

	if (cell) {
	    if (cell->hits > 0) {
		struct area_list *prev = (struct area_list *)NULL;
		struct area_list *newNode;

		while (listp->next) {
		    if (!listp->cell || (strcmp(cell->name, listp->cell->name) < 0)) {
			break;
		    }
		    prev = listp;
		    listp = listp->next;
		}

		newNode = (struct area_list *)bu_malloc(sizeof(struct area_list), "print_area_list area list node allocation");
		newNode->cell = cell;
		newNode->next = (struct area_list *)NULL;

		if (!prev) {
		    listHead = newNode;
		    newNode->next = listp;
		} else {
		    newNode->next = prev->next;
		    prev->next = newNode;
		}

		cumulative += cell->hits;
		if (count) {
		    (*count)++;
		}
	    }
	}
    }

    if (type == PRESENTED_AREA) {
	bu_log("\nPresented Region Areas\n======================\n");
    }
    if (type == EXPOSED_AREA) {
	bu_log("\nExposed Region Areas\n====================\n");
    }
    for (listp = listHead; listp->cell != NULL;) {
	struct area_list *prev = listp;
	cell = listp->cell;
	if (type == PRESENTED_AREA) {
	    bu_log("Region %s\t(%ld hits)\t= %18.4lf square mm\t(%.4lf square meters)\n", cell->name, cell->hits, cell_area * (fastf_t)cell->hits, cell_area * (fastf_t)cell->hits / 1000000.0);
	}
	if (type == EXPOSED_AREA) {
	    bu_log("Region %s\t(%ld hits)\t= %18.4lf square mm\t(%.4lf square meters)\n", cell->name, cell->exposures, cell_area * (fastf_t)cell->exposures, cell_area * (fastf_t)cell->exposures / 1000000.0);
	}
	listp = listp->next;
	bu_free(prev, "print_area_list area list node free");
    }
    bu_free(listp, "print_area_list area list node free");

    return cumulative;
}


/*  p r i n t _ a s s e m b l y _ a r e a _ l i s t
 *
 * Prints a list of region areas sorted alphabetically reporting
 * either the presented or exposed area 'type' and keeping track of
 * the 'count' of regions printed.  this routine returns the number of
 * ray hits across all regions.
 */
static long int
print_assembly_area_list(struct rt_i *rtip, long int max_depth, area_type_t type)
{
    struct region *rp;
    struct area *cellp;
    int depth;
    register struct area *cell = (struct area *)NULL;
    long int count = 0;

    struct area_list *listHead = (struct area_list *)bu_malloc(sizeof(struct area_list), "print_area_list area list node allocation");
    struct area_list *listp;
    listHead->cell = (struct area *)NULL;
    listHead->next = (struct area_list *)NULL;

    /* insertion sort based on depth and case */
    rp = BU_LIST_FIRST(region, &(rtip->HeadRegion));
    cell = (struct area *)rp->reg_udata;
    for (cellp = cell->assembly; cellp; cellp = cellp->assembly) {
	int counted_items;
	listp = listHead;

	if (type == PRESENTED_AREA) {
	    counted_items = cellp->hits;
	}
	if (type == EXPOSED_AREA) {
	    counted_items = cellp->exposures;
	}
	if (counted_items) {
	    struct area_list *prev = (struct area_list *)NULL;
	    struct area_list *newNode;

	    while (listp->next) {
		if ((!listp->cell) || (cellp->depth > listp->cell->depth)) {
		    break;
		}
		if ((cellp->depth == listp->cell->depth) && (strcmp(cellp->name, listp->cell->name) < 0)) {
		    break;
		}

		prev = listp;
		listp = listp->next;
	    }

	    newNode = (struct area_list *)bu_malloc(sizeof(struct area_list), "print_area_list area list node allocation");
	    newNode->cell = cellp;
	    newNode->next = (struct area_list *)NULL;

	    if (!prev) {
		listHead = newNode;
		newNode->next = listp;
	    } else {
		newNode->next = prev->next;
		prev->next = newNode;
	    }

	    count++;
	}
    }


    if (type == PRESENTED_AREA) {
	bu_log("\nPresented Assembly Areas\n========================\n");
    }
    if (type == EXPOSED_AREA) {
	bu_log("\nExposed Assembly Areas\n======================\n");
    }
    for (listp = listHead; listp->cell != NULL;) {
	int indents = max_depth - listp->cell->depth;
	struct area_list *prev = listp;
	cell = listp->cell;

	while (indents-- > 0) {
	    if (indents % 2) {
		bu_log("++");
	    } else {
		bu_log("--");
	    }
	}

	if (type == PRESENTED_AREA) {
	    bu_log("Assembly %s\t(%ld hits)\t= %18.4lf square mm\t(%.4lf square meters)\n", cell->name, cell->hits, cell_area * (fastf_t)cell->hits, cell_area * (fastf_t)cell->hits / 1000000.0);
	}
	if (type == EXPOSED_AREA) {
	    bu_log("Assembly %s\t(%ld hits)\t= %18.4lf square mm\t(%.4lf square meters)\n", cell->name, cell->exposures, cell_area * (fastf_t)cell->exposures, cell_area * (fastf_t)cell->exposures / 1000000.0);
	}

	listp = listp->next;
	bu_free(prev, "print_area_list area list node free");
    }
    bu_free(listp, "print_area_list area list node free");

    return count;
}


/*
 *			V I E W _ E N D
 *
 */
void
view_end(struct application *ap)
{
    register struct region *rp;
    struct rt_i *rtip = ap->a_rt_i;
    fastf_t total_area=0.0;
    long cumulative = 0;
    register struct area *cell = (struct area *)NULL;

    int max_depth = 0;
    struct area *cellp;

    long int presented_region_count = 0;
    long int presented_assembly_count = 0;
    long int exposed_region_count = 0;
    long int exposed_assembly_count = 0;

    cumulative = print_region_area_list(&presented_region_count, rtip, PRESENTED_AREA);
    (void) print_region_area_list(&exposed_region_count, rtip, EXPOSED_AREA);

    /* find the maximum assembly depth */
    rp = BU_LIST_FIRST(region, &(rtip->HeadRegion));
    cell = (struct area *)rp->reg_udata;
    for (cellp = cell->assembly; cellp; cellp = cellp->assembly) {
	if (cellp->depth > max_depth) {
	    max_depth = cellp->depth;
	}
    }

    presented_assembly_count = print_assembly_area_list(rtip, max_depth, PRESENTED_AREA);

    exposed_assembly_count = print_assembly_area_list(rtip, max_depth, EXPOSED_AREA);

    bu_log("\nSummary\n=======\n");
    total_area = cell_area * (fastf_t)hit_count;
    bu_log("Cumulative Presented Areas (%ld hits) = %18.4lf square mm\t(%.4lf square meters)\n", cumulative, cell_area * (fastf_t)cumulative, cell_area * (fastf_t)cumulative / 1000000.0);
    bu_log("Total Exposed Area         (%ld hits) = %18.4lf square mm\t(%.4lf square meters)\n", hit_count, total_area, total_area / 1000000.0);
    bu_log("Number of Presented Regions:    %8d\n", presented_region_count);
    bu_log("Number of Presented Assemblies: %8d\n", presented_assembly_count);
    bu_log("Number of Exposed Regions:    %8d\n", exposed_region_count);
    bu_log("Number of Exposed Assemblies: %8d\n", exposed_assembly_count);
    bu_log("\n");

    /* free the assembly areas */
    cell = (struct area *)rp->reg_udata;
    if (cell) {
	struct area *next_cell;
	cell = cell->assembly;
	while (cell) {
	    next_cell = cell->assembly;
	    if (cell->name) {
		bu_free((char *)cell->name, "view_end assembly name free");
	    }
	    bu_free(cell, "view_end assembly free");
	    cell = next_cell;
	}
    }

    /* free the region areas */
    for( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	cell = (struct area *)rp->reg_udata;

	if (cell) {
	    bu_free(cell, "view_end area free");
	    cell = (genptr_t)NULL;
	    rp->reg_name = (genptr_t)NULL;
	}
    }

    return;
}

void view_setup() {}
void view_cleanup() {}
void application_init () {}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
