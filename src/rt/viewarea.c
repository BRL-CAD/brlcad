/*                      V I E W A R E A . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
/** @file viewarea.c
 *
 * Computes the exposed and presented surface area projections of
 * specified geometry.  Exposed area is the occluded 2D projection,
 * presented is the unoccluded 2D projection.  That is to say that if
 * there is an object in front, it will reduce the exposed area, but
 * not the presented area.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "./ext.h"
#include "plot3.h"
#include "rtprivate.h"


extern int	npsw;			/* number of worker PSWs to run */

int		use_air = 1;		/* Handling of air in librt */

extern int 	 rpt_overlap;

extern fastf_t  rt_cline_radius;        /* from g_cline.c */

extern double units; /* from opt.c */

static long hit_count=0;
static fastf_t cell_area=0.0;


/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL }
};


const char title[] = "RT Area";
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
 -u units	Set the display units (default=mm)\n\
 -x #		Set librt debug flags\n\
";

struct point_list {
    struct bu_list l;
    point_t pt_cell;
};

struct area {
    struct area *assembly;	/* pointer to a linked list of assemblies */
    long hits;			/* presented hits count */
    long exposures;		/* exposed hits count */
    int seen;			/* book-keeping for exposure */
    int depth;			/* assembly depth */
    const char *name;		/* assembly name */
    struct point_list *hit_points;	/* list of points that define the presented area */
    struct point_list *exp_points;	/* list of points that define the exposed area */
    int num_hit_points;	/* number of points present in hit_points */
    int num_exp_points;	/* number of points present in exp_points */
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
int area_center(struct point_list *ptlist, int number, point_t *center);

/*
 *  			V I E W _ I N I T
 */
int
view_init( register struct application *ap, char *file, char *obj )
{
    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 0;

    if ( !rpt_overlap )
	ap->a_logoverlap = rt_silent_logoverlap;

    output_is_binary = 0;		/* output is printable ascii */

    hit_count = 0;

    return(0);		/* No framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  View_2init is called by do_frame(), which in turn is called by
 *  main().
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
    bu_semaphore_acquire( RT_SEM_RESULTS );
    for ( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	struct area *cell;
	/* allocate memory first time through */
	cell = (struct area *)bu_calloc(1, sizeof(struct area), "view_2init area allocation");
	cell->assembly = assembly;
    cell->num_exp_points = cell->num_hit_points = 0;

    BU_GETSTRUCT(cell->hit_points, point_list);
    BU_LIST_INIT(&(cell->hit_points->l));
    BU_GETSTRUCT(cell->exp_points, point_list);
    BU_LIST_INIT(&(cell->exp_points->l));

    rp->reg_udata = (genptr_t)cell;
    } 
    bu_semaphore_release( RT_SEM_RESULTS );


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
    bu_strlcpy(buffer, path, l+1);

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
		int len;

		/* sanity check */
		if (cellp->assembly) {
		    bu_log("Inconsistent assembly list detected\n");
		    break;
		}

		len = strlen(&buffer[l])+1;
		name = (char *)bu_malloc(len, "increment_assembly_counter assembly name allocation");
		bu_strlcpy(name, &buffer[l], len);
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
    struct point_list * temp_point_list;
    register struct region *rp;
    struct rt_i *rtip = ap->a_rt_i;
    register struct partition *pp = PartHeadp->pt_forw;
    register struct area *cell;
    register int l;


    if ( pp == PartHeadp )
	return(0);		/* nothing was actually hit?? */

    /* ugh, horrible block */
    bu_semaphore_acquire( RT_SEM_RESULTS );

    hit_count++;

    /* clear the list of visited regions */
    for ( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
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
        /* record the exposed points in the area */
        temp_point_list = (struct point_list *) bu_malloc(sizeof(struct point_list), "Point list allocation");
        VMOVE(temp_point_list->pt_cell, pp->pt_inhit->hit_point);

        BU_LIST_INSERT(&(cell->exp_points->l), &(temp_point_list->l));

        cell->num_exp_points++;
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
	cell->num_hit_points++;
	/* record the presented points (hit_points) in the area */
	temp_point_list = (struct point_list *) bu_malloc(sizeof(struct point_list), "Point list allocation");
    VMOVE(temp_point_list->pt_cell, pp->pt_inhit->hit_point);

    BU_LIST_INSERT(&(cell->hit_points->l), &(temp_point_list->l));

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
    for ( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
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
	double factor = 1.0; // show mm in parens by default

	// if millimeters, show meters in parens
	if (NEAR_ZERO(units - 1.0, SMALL_FASTF)) {
	    factor = bu_units_conversion("m");
	}

	cell = listp->cell;
	if (type == PRESENTED_AREA) {
        point_t temp;
	    bu_log("Region %s\t(%ld hits)\t= %18.4lf square %s\t(%.4lf square %s)",
		   cell->name,
		   cell->hits,
		   cell_area * (fastf_t)cell->hits / (units*units),
		   bu_units_string(units),
		   cell_area * (fastf_t)cell->hits / (factor*factor),
		   bu_units_string(factor)
		);
        if (area_center((cell->hit_points), (cell->num_hit_points), &temp)) {
            bu_log("\tcenter at (%.4lf, %.4lf, %.4lf)\n",
                temp[X],
                temp[Y],
                temp[Z]
            );
        } else {
            bu_log("\n");
        }
	    fflush(stdout); fflush(stderr);
	}
	if (type == EXPOSED_AREA) {
        point_t temp;
	    bu_log("Region %s\t(%ld hits)\t= %18.4lf square %s\t(%.4lf square %s)",
		   cell->name,
		   cell->exposures,
		   cell_area * (fastf_t)cell->exposures / (units*units),
		   bu_units_string(units),
		   cell_area * (fastf_t)cell->exposures / (factor*factor),
		   bu_units_string(factor)
		   );
        if (area_center((cell->exp_points), (cell->num_exp_points), &temp)) {
            bu_log("\tcenter at (%.4lf, %.4lf, %.4lf)\n",
                temp[X],
                temp[Y],
                temp[Z]
            );
        } else {
            bu_log("\n");
        }
	    fflush(stdout); fflush(stderr);
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
	double factor = 1.0; // show mm in parens by default

	// if millimeters, show meters in parens
	if (NEAR_ZERO(units - 1.0, SMALL_FASTF)) {
	    factor = bu_units_conversion("m");
	}

	cell = listp->cell;

	while (indents-- > 0) {
	    if (indents % 2) {
		bu_log("++");
	    } else {
		bu_log("--");
	    }
	}
    
	if (type == PRESENTED_AREA) {
        
	    bu_log("Assembly %s\t(%ld hits)\t= %18.4lf square %s\t(%.4lf square %s)\n",
		   cell->name,
		   cell->hits,
		   cell_area * (fastf_t)cell->hits / (units*units),
		   bu_units_string(units),
		   cell_area * (fastf_t)cell->hits / (factor*factor),
		   bu_units_string(factor)
		);
	    fflush(stdout); fflush(stderr);
	}
	if (type == EXPOSED_AREA) {

	    bu_log("Assembly %s\t(%ld hits)\t= %18.4lf square %s\t(%.4lf square %s)\n",
		   cell->name,
		   cell->exposures,
		   cell_area * (fastf_t)cell->exposures / (units*units),
		   bu_units_string(units),
		   cell_area * (fastf_t)cell->exposures / (factor*factor),
		   bu_units_string(factor)
		);
	    fflush(stdout); fflush(stderr);
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

    double factor = 1.0; // show mm in parens by default

    // if millimeters, show meters in parens
    if (NEAR_ZERO(units - 1.0, SMALL_FASTF)) {
	factor = bu_units_conversion("m");
    }

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
    bu_log("Cumulative Presented Areas (%ld hits) = %18.4lf square %s\t(%.4lf square %s)\n",
	   cumulative,
	   cell_area * (fastf_t)cumulative / (units*units),
	   bu_units_string(units),
	   cell_area * (fastf_t)cumulative / (factor*factor),
	   bu_units_string(factor)
	);
    bu_log("Total Exposed Area         (%ld hits) = %18.4lf square %s\t(%.4lf square %s)\n",
	   hit_count,
	   total_area / (units*units),
	   bu_units_string(units),
	   total_area / (factor*factor),
	   bu_units_string(factor)
	);
    bu_log("Number of Presented Regions:    %8d\n", presented_region_count);
    bu_log("Number of Presented Assemblies: %8d\n", presented_assembly_count);
    bu_log("Number of Exposed Regions:    %8d\n", exposed_region_count);
    bu_log("Number of Exposed Assemblies: %8d\n", exposed_assembly_count);
    bu_log("\n"
	   "WARNING: The terminology and output format of 'rtarea' is deprecated\n"
	   "         and subject to change in a future release of BRL-CAD.\n"
	   "\n");

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
        if (cell->exp_points) {
            bu_list_free(&(cell->exp_points->l));
        }
        if (cell->hit_points) {
            bu_list_free(&(cell->hit_points->l));
        }
	    bu_free(cell, "view_end assembly free");
	    cell = next_cell;
	}
    }

    /* free the region areas */
    for ( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	cell = (struct area *)rp->reg_udata;

	if (cell) {
	    bu_free(cell, "view_end area free");
	    cell = (genptr_t)NULL;
	    rp->reg_name = (genptr_t)NULL;
	}
    }

    /* flush for good measure */
    fflush(stdout); fflush(stderr);

    return;
}

void view_setup() {}
void view_cleanup() {}
void application_init () {}

/*
 *		 A R E A _ C E N T E R
 *
 *	This function receives a pointer to a point_list
 *	structure, the number of points to calculate, and
 *  the point address of where to record the information.
 *
 *  Since the structure of a vector and of a point is pretty
 *  similar, some vector macros can be used to handle points.
 *  The macros used in the calculation of the area center are
 *  VSET and VSETALL. In the rayhit() function, VMOVE is also
 *  used.
 */
int
area_center(struct point_list * ptlist, int number, point_t *center)
{
	struct point_list *point_it;
    point_t temp_point;

    if (BU_LIST_IS_EMPTY(&(ptlist->l))) {
        return 0;
    }

    VSETALL(temp_point, 0);

    /*  This block of code could be replaced with a
     *  BU_LIST_FOR(), but the first element of the list,
     *  the one pointed by the exp_points and hit_points
     *  point_lists (in struct area), are initialized with
     *  no data, and using the BU_LIST_FOR macro would hurt
     *  precision.
     *
     *  This for() iterates from the second element of the list
     *  to the last one that is not head.
     *
     *  In order to use BU_LIST_FOR, the rayhit function must
     *  be modified so that when the point lists are formed,
     *  checks are made to ensure that the head element is not
     *  empty.
     */
    for ((point_it) = BU_LIST_PNEXT(point_list,
                    BU_LIST_FIRST(point_list, &(ptlist->l))); 
	     (point_it) && BU_LIST_NOT_HEAD(point_it, &(ptlist->l)); 
         (point_it) = BU_LIST_PNEXT(point_list, point_it)) {

        VSET(temp_point,
            temp_point[X] + point_it->pt_cell[X],
            temp_point[Y] + point_it->pt_cell[Y],
            temp_point[Z] + point_it->pt_cell[Z]);
	}
    VSET(*center,
        temp_point[X] / number,
        temp_point[Y] / number,
        temp_point[Z] / number);

    return 1;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
