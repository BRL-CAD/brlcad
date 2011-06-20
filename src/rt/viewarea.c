/*                      V I E W A R E A . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file rt/viewarea.c
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
#include "plot3.h"

#include "./rtuif.h"
#include "./ext.h"


extern int	npsw;			/* number of worker PSWs to run */

extern int 	 rpt_overlap;

/* from opt.c */
extern double units;
extern int default_units;
extern int model_units;

static long hit_count=0;
static fastf_t cell_area=0.0;

/* holds total exposed values */
static unsigned long int exposed_hit_sum=0;
static fastf_t exposed_hit_x_sum=0.0;
static fastf_t exposed_hit_y_sum=0.0;
static fastf_t exposed_hit_z_sum=0.0;

/* Flag to enable/disable center computations, 0=disable, 1=enable */
int rtarea_compute_centers = 0;

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"%d", 1, "compute_centers", bu_byteoffset(rtarea_compute_centers), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "cc", bu_byteoffset(rtarea_compute_centers), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};


const char title[] = "RT Area";

void
usage(const char *argv0)
{
    bu_log("Usage:  %s [options] model.g objects...\n", argv0);
    bu_log("Options:\n");
    bu_log(" -s #		Grid size in pixels, default 512\n");
    bu_log(" -a Az		Azimuth in degrees	(conflicts with -M)\n");
    bu_log(" -e Elev	Elevation in degrees	(conflicts with -M)\n");
    bu_log(" -M		Read model2view matrix on stdin (conflicts with -a, -e)\n");
    bu_log(" -g #		Grid cell width in millimeters (conflicts with -s)\n");
    bu_log(" -G #		Grid cell height in millimeters (conflicts with -s)\n");
    bu_log(" -J #		Jitter.  Default is off.  Any non-zero number is on\n");
    bu_log(" -U #		Set use_air boolean to # (default=1)\n");
    bu_log(" -u units	Set the display units (default=mm)\n");
    bu_log(" -x #		Set librt debug flags\n");
    bu_log(" -c             Auxillary commands (see man page)\n");
    bu_log("\n");
    bu_log("WARNING: rtarea may not correctly report area or center when instancing is\n");
    bu_log("done at the group level. Using 'xpush' can be a workaround for this problem.\n");
    bu_log("\n");
    bu_log("WARNING: rtarea presently outputs in storage units (mm) by default but will\n");
    bu_log("be changed to output in local units in the near future.\n");
    bu_log("\n");
}


struct point_list {
    struct point_list *   next;
    point_t               pt_cell;
};

struct area {
    struct bu_list        l;                           /**< @brief magic # and doubly linked list */
    struct area *         assembly;                    /* pointer to a bu_list of assemblies */
    long                  hits;	                       /* presented hits count */
    long                  exposures;                   /* exposed hits count */
    int                   seen;	                       /* book-keeping for exposure */
    int                   depth;                       /* assembly depth */
    const char *          name;	                       /* assembly name */
    struct point_list *   hit_points;                  /* list of points that define the presented area */
    struct point_list *   exp_points;                  /* list of points that define the exposed area */
    int                   num_hit_points;              /* number of points present in hit_points */
    int                   num_exp_points;              /* number of points present in exp_points */
    fastf_t               group_exposed_hit_x_sum;     /* running x total of exposed hit points on group */
    fastf_t               group_exposed_hit_y_sum;     /* running y total of exposed hit points on group */
    fastf_t               group_exposed_hit_z_sum;     /* running z total of exposed hit points on group */
    fastf_t               group_presented_hit_x_sum;   /* running x total of presented hit points on group */
    fastf_t               group_presented_hit_y_sum;   /* running y total of presented hit points on group */
    fastf_t               group_presented_hit_z_sum;   /* running z total of presented hit points on group */
};

struct area_list {
    struct area *         cell;
    struct area_list *    next;
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
view_init(struct application *ap, char *UNUSED(file), char *UNUSED(obj), int UNUSED(minus_o), int UNUSED(minus_F))
{
    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 0;

    use_air = 1;

    if ( !rpt_overlap )
	ap->a_logoverlap = rt_silent_logoverlap;

    output_is_binary = 0;		/* output is printable ascii */

    hit_count = 0;

    return 0;		/* No framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  View_2init is called by do_frame(), which in turn is called by
 *  main().
 */
void
view_2init( struct application *ap, char *UNUSED(framename) )
{
    register struct region *rp;
    register struct rt_i *rtip = ap->a_rt_i;

    /* initial empty parent assembly */
    struct area *assembly;

    /* cell_width and cell_height are global variables */
    cell_area = cell_width * cell_height;

    /* create first parent-area record */
    BU_GETSTRUCT(assembly, area);

    /* initialize linked-list pointers for first parent-area record */
    BU_LIST_INIT(&(assembly->l));

    /* initialize first parent-area record */
    assembly->assembly = (struct area *) NULL;
    assembly->hits = 0;
    assembly->exposures = 0;
    assembly->seen = 0;
    assembly->depth = 0;
    assembly->name = (char *) NULL;
    assembly->hit_points = (struct point_list *) NULL;
    assembly->exp_points = (struct point_list *) NULL;
    assembly->num_hit_points = 0;
    assembly->num_exp_points = 0;
    assembly->group_exposed_hit_x_sum = 0.0;
    assembly->group_exposed_hit_y_sum = 0.0;
    assembly->group_exposed_hit_z_sum = 0.0;
    assembly->group_presented_hit_x_sum = 0.0;
    assembly->group_presented_hit_y_sum = 0.0;
    assembly->group_presented_hit_z_sum = 0.0;

    bu_semaphore_acquire( RT_SEM_RESULTS );

    /* traverse the regions linked-list, create and assign an empty area record
     * to each region record.
     */
    for ( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	struct area *cell;

        /* create region-area record */
        BU_GETSTRUCT(cell, area);

        /* initialize linked-list pointers for region-area record */
        BU_LIST_INIT(&(cell->l));

        /* initialize region-area record */
	cell->assembly = assembly;
        cell->hits = 0;
        cell->exposures = 0;
        cell->seen = 0;
        cell->depth = 0;
        cell->name = (char *) NULL;
        cell->hit_points = (struct point_list *) NULL;
        cell->exp_points = (struct point_list *) NULL;
        cell->num_hit_points = 0;
        cell->num_exp_points = 0;
        cell->group_exposed_hit_x_sum = 0.0;
        cell->group_exposed_hit_y_sum = 0.0;
        cell->group_exposed_hit_z_sum = 0.0;
        cell->group_presented_hit_x_sum = 0.0;
        cell->group_presented_hit_y_sum = 0.0;
        cell->group_presented_hit_z_sum = 0.0;

        /* place new region-area record into current region record */
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
raymiss(struct application *UNUSED(ap))
{
    return 0;
}

/*
 *			V I E W _ P I X E L
 *
 *  This routine is called from do_run(), and in this case does nothing.
 */
void
view_pixel(struct application *UNUSED(ap))
{
    return;
}


/* keeps track of how many times we encounter an assembly by
 * maintaining a simple linked list of them using the same structure
 * used for regions.
 */
static void
increment_assembly_counter(register struct area *cell, const char *path, area_type_t type, point_t *hit_point)
{
    size_t l = 0;
    char *buffer = (char *) NULL;
    int depth = 0;
    int parent_found = 0;
    register struct area *assembly_head_ptr = cell->assembly;
    register struct area *area_record_ptr = (struct area *) NULL;

    if (!cell) {
        bu_log("ERROR: Function 'increment_assembly_counter' received a NULL pointer to an 'area' structure. Function aborted.\n");
	return;
    }
    if (!path) {
        bu_log("ERROR: Function 'increment_assembly_counter' received a NULL pointer to a region path. Function aborted.\n");
	return;
    }

    l = strlen(path);
    buffer = bu_calloc((unsigned int)l+1, sizeof(char), "increment_assembly_counter buffer allocation");
    bu_strlcpy(buffer, path, l+1);

    /* trim off the region name */
    while (l > 0) {
	if (buffer[l-1] == '/') {
	    break;
	}
	l--;
    }
    buffer[l-1] = '\0';

    /* traverse region path name, character-by-character to
     * extract and process each parent name of the current region.
     */
    while (l > 0) {
        /* when the current character in the path name is forward-slash, this
         * indicates the characters to the right of the forward-slash are the next
         * parent in the path, use this name. when done find next name up the
         * path until the path is empty.
         */
	if (buffer[l-1] == '/') {
            parent_found=0;

            /* traverse the assembly linked-list */
            for (BU_LIST_FOR(area_record_ptr, area, &(assembly_head_ptr->l)))  {
                /* if the 'name' of the current parent/group of the
                 * current region matches the current 'name' in the area
                 * structure, then within the current linked-list area
                 * structure, increment number of exposures & hits and
                 * increment seen, then exit for-loop.
                 */
		if ( (BU_STR_EQUAL(area_record_ptr->name, &buffer[l])) ) {
		    if (type == EXPOSED_AREA) {
			if (!area_record_ptr->seen) {
			    area_record_ptr->exposures++;
			    area_record_ptr->seen++;
			    area_record_ptr->hits++;

                            /* total x,y,z for exposed & presented hits */
                            if (rtarea_compute_centers) {
                                area_record_ptr->group_exposed_hit_x_sum += (fastf_t)((*hit_point)[X]);
                                area_record_ptr->group_exposed_hit_y_sum += (fastf_t)((*hit_point)[Y]); 
                                area_record_ptr->group_exposed_hit_z_sum += (fastf_t)((*hit_point)[Z]);
                                area_record_ptr->group_presented_hit_x_sum += (fastf_t)((*hit_point)[X]);
                                area_record_ptr->group_presented_hit_y_sum += (fastf_t)((*hit_point)[Y]);
                                area_record_ptr->group_presented_hit_z_sum += (fastf_t)((*hit_point)[Z]); 
                            }

			    if (depth > area_record_ptr->depth) {
				area_record_ptr->depth = depth;
			    }
			}
		    }
		    if (type == PRESENTED_AREA) {
			if (!area_record_ptr->seen) {
			    area_record_ptr->hits++;
			    area_record_ptr->seen++;

                            /* total x,y,z for presented hits */
                            if (rtarea_compute_centers) {
                                area_record_ptr->group_presented_hit_x_sum += (fastf_t)((*hit_point)[X]);
                                area_record_ptr->group_presented_hit_y_sum += (fastf_t)((*hit_point)[Y]);
                                area_record_ptr->group_presented_hit_z_sum += (fastf_t)((*hit_point)[Z]); 
                            }

			    if (depth > area_record_ptr->depth) {
			        area_record_ptr->depth = depth;
			    }
			}
		    }
                    parent_found=1;
		    break;
		}
            }

	    /* if a parent of the current region does not already exist in the
             * assembly linked-list then create, populate, insert new area record 
             * into the assembly linked-list.
             */
	    if (!parent_found) {
		char *name;
		size_t len;

                /* create new area record */
                BU_GETSTRUCT(area_record_ptr, area);

                /* initialize new parent-area record (i.e. assembly-area record) */
                area_record_ptr->assembly = (struct area *) NULL;
                area_record_ptr->hits = 0;
                area_record_ptr->exposures = 0;
                area_record_ptr->seen = 0;
                area_record_ptr->depth = 0;
                area_record_ptr->name = (char *) NULL;
                area_record_ptr->hit_points = (struct point_list *) NULL;
                area_record_ptr->exp_points = (struct point_list *) NULL;
                area_record_ptr->num_hit_points = 0;
                area_record_ptr->num_exp_points = 0;
                area_record_ptr->group_exposed_hit_x_sum = 0.0;
                area_record_ptr->group_exposed_hit_y_sum = 0.0;
                area_record_ptr->group_exposed_hit_z_sum = 0.0;
                area_record_ptr->group_presented_hit_x_sum = 0.0;
                area_record_ptr->group_presented_hit_y_sum = 0.0;
                area_record_ptr->group_presented_hit_z_sum = 0.0;

                /* allocate string to contain the object/parent/group name, copy the
                 * name to the new string, assign the string to the parent-area record.
                 */
		len = strlen(&buffer[l])+1;
		name = (char *)bu_malloc(len, "increment_assembly_counter assembly name allocation");
		bu_strlcpy(name, &buffer[l], len);

                /* populate parent-area record */
		area_record_ptr->name = name;

		if (type == EXPOSED_AREA) {
		    area_record_ptr->exposures++;
		    area_record_ptr->hits++;
                    /* total x,y,z for exposed & presented hits */
                    if (rtarea_compute_centers) {
                        area_record_ptr->group_exposed_hit_x_sum += (fastf_t)((*hit_point)[X]);
                        area_record_ptr->group_exposed_hit_y_sum += (fastf_t)((*hit_point)[Y]); 
                        area_record_ptr->group_exposed_hit_z_sum += (fastf_t)((*hit_point)[Z]);
                        area_record_ptr->group_presented_hit_x_sum += (fastf_t)((*hit_point)[X]);
                        area_record_ptr->group_presented_hit_y_sum += (fastf_t)((*hit_point)[Y]);
                        area_record_ptr->group_presented_hit_z_sum += (fastf_t)((*hit_point)[Z]); 
                    }
		}
		if (type == PRESENTED_AREA) {
		    area_record_ptr->hits++;
                    /* total x,y,z for presented hits */
                    if (rtarea_compute_centers) {
                        area_record_ptr->group_presented_hit_x_sum += (fastf_t)((*hit_point)[X]);
                        area_record_ptr->group_presented_hit_y_sum += (fastf_t)((*hit_point)[Y]);
                        area_record_ptr->group_presented_hit_z_sum += (fastf_t)((*hit_point)[Z]); 
                    }
		}

                /* always mark the current parent as seen since this is a new
                 * parent-area record and it is being processed now.
                 */
		area_record_ptr->seen++;

		if (depth > area_record_ptr->depth) {
		    area_record_ptr->depth = depth;
		}
                /* attach new area record to assembly linked-list */
                BU_LIST_PUSH(&(assembly_head_ptr->l), &(area_record_ptr->l));
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
rayhit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segHeadp))
{
    struct point_list * temp_point_list;           /* to contain exposed points */
    struct point_list * temp_presented_point_list; /* to contain presented points */
    register struct region *rp;
    struct rt_i *rtip = ap->a_rt_i;
    register struct partition *pp = PartHeadp->pt_forw;
    register struct area *cell;
    struct area *cellp;

/* exposed hit sums */
    extern unsigned long int exposed_hit_sum;
    extern fastf_t exposed_hit_x_sum;
    extern fastf_t exposed_hit_y_sum;
    extern fastf_t exposed_hit_z_sum;

    if ( pp == PartHeadp )
	return 0;		/* nothing was actually hit?? */

    /* ugh, horrible block */
    bu_semaphore_acquire( RT_SEM_RESULTS );

    hit_count++;

    /* traverse all regions in the model, clear seen in the 'area' record
     * associated with each region.
     */
    for ( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	cell = (struct area *)rp->reg_udata;
	cell->seen = 0;
    }

    /* clear seen for all 'area' records associated with groups */
    rp = BU_LIST_FIRST(region, &(rtip->HeadRegion));
    cell = (struct area *)rp->reg_udata;
    for ( BU_LIST_FOR(cellp, area, &(cell->assembly->l) ) )  {
        cellp->seen = 0;
    }

    /* get the exposed areas (i.e. first hits), traverse linked-list of
     * partition records.
     */
    for ( BU_LIST_FOR( pp, partition, (struct bu_list *)PartHeadp ) )  {
	struct region  *reg = pp->pt_regionp;

        /* obtain the pointer to the area record associated with the current
         * region being processed.
         */
	cell = (struct area *)reg->reg_udata;

        /* ignore air. if the current region is of type 'air' skip this region
         * and continue with the next region hit by the ray.
         */
	if (reg->reg_aircode) {
	    continue;
	}

        /* only process this hit if the region has not already been hit by this
         * ray. this allows only exposed regions to be processed. (i.e. 1st hit)
         */
	if (!cell->seen) {  /* for exposed areas */
            if (rtarea_compute_centers) {

                /* create a new 'exposed' point_list record */
                temp_point_list = (struct point_list *) bu_malloc(sizeof(struct point_list), "Point list allocation");

                /* create a new 'presented' point_list record */
                temp_presented_point_list = (struct point_list *) bu_malloc(sizeof(struct point_list), "Presented Point list allocation");

                /* initialize new 'exposed' point_list record */
                temp_point_list->next = (struct point_list *) NULL;
                (temp_point_list->pt_cell)[X] = 0.0;
                (temp_point_list->pt_cell)[Y] = 0.0;
                (temp_point_list->pt_cell)[Z] = 0.0;

                /* initialize new 'presented' point_list record */
                temp_presented_point_list->next = (struct point_list *) NULL;
                (temp_presented_point_list->pt_cell)[X] = 0.0;
                (temp_presented_point_list->pt_cell)[Y] = 0.0;
                (temp_presented_point_list->pt_cell)[Z] = 0.0;

                /* compute hit_point and store in pt_inhit record within the
                 * current partition record
                 */
                VJOIN1(pp->pt_inhit->hit_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);

                /* place hit_point in new 'exposed' point_list record */
                (temp_point_list->pt_cell)[X] = (pp->pt_inhit->hit_point)[X];
                (temp_point_list->pt_cell)[Y] = (pp->pt_inhit->hit_point)[Y];
                (temp_point_list->pt_cell)[Z] = (pp->pt_inhit->hit_point)[Z];

                /* place hit_point in new 'presented' point_list record */
                (temp_presented_point_list->pt_cell)[X] = (pp->pt_inhit->hit_point)[X];
                (temp_presented_point_list->pt_cell)[Y] = (pp->pt_inhit->hit_point)[Y];
                (temp_presented_point_list->pt_cell)[Z] = (pp->pt_inhit->hit_point)[Z];

                /* for computing exposed area center, count hits and sum x,y,z
                 * values of hits on exposed regions
                 */
                exposed_hit_sum++;
                exposed_hit_x_sum = exposed_hit_x_sum + (fastf_t)((pp->pt_inhit->hit_point)[X]);
                exposed_hit_y_sum = exposed_hit_y_sum + (fastf_t)((pp->pt_inhit->hit_point)[Y]);
                exposed_hit_z_sum = exposed_hit_z_sum + (fastf_t)((pp->pt_inhit->hit_point)[Z]);

                /* if current area record contains no point_list linked-list then
                 * place the new and first point_list record in the area record.
                 * if the current area record already has a point_list record
                 * linked-list, then add the new point_list record to the head of
                 * the linked-list.
                 */
                if (!cell->exp_points) {
                    cell->exp_points = temp_point_list;
                } else {
                    temp_point_list->next = cell->exp_points;
                    cell->exp_points = temp_point_list;
                }

                /* if current area record contains no 'presented' point_list linked-list
                 * then place the new and first point_list record in the area record.
                 * if the current area record already has a point_list record
                 * linked-list, then add the new point_list record to the head of
                 * the linked-list.
                 */
                if (!cell->hit_points) {
                    cell->hit_points = temp_presented_point_list;
                } else {
                    temp_presented_point_list->next = cell->hit_points;
                    cell->hit_points = temp_presented_point_list;
                }

                /* for the current area record, increment num_exp_points which
                 * maintains a count of the number of records within the point_list
                 * linked-list and also the total of exposed hits on the current
                 * region.
                 */
                cell->num_exp_points++;

                /* for the current area record, increment num_hit_points which
                 * maintains a count of the number of records within the
                 * 'presented' point_list linked-list and also the total of
                 * presented hits on the current region.
                 */
                cell->num_hit_points++;
            }

            /* for the current area structure, increment exposures and seen */
	    cell->exposures++;
	    cell->seen++;

            /* increment 'presented hits' for current region since all hits on a
             * region are 'presented hits' even those that are a first-hit on a ray.
             */
	    cell->hits++;

            /* if the current area record does not contain the region name that
             * it represents then extract the region name from the object path
             * then assign the name within the area record.
             */
	    if (!cell->name) {
		size_t l = strlen(reg->reg_name);
		while (l > 0) {
		    if (reg->reg_name[l-1] == '/') {
			break;
		    }
		    l--;
		}
		cell->name = &reg->reg_name[l];
	    }

	    /* record the parent assemblies */
	    increment_assembly_counter(cell, reg->reg_name, EXPOSED_AREA, &(pp->pt_inhit->hit_point));
	}

	/* halt after first non-air */
	break;
    } /* end of for-loop to 'get the exposed areas (i.e. first hits)' */

    /* get the presented areas */
    for ( BU_LIST_FOR( pp, partition, (struct bu_list *)PartHeadp ) )  {
	struct region  *reg = pp->pt_regionp;
	cell = (struct area *)reg->reg_udata;

	/* ignore air */
	if (reg->reg_aircode) {
	    continue;
	}

        /* makes sure that a region already processed is not processed again */
	if (!cell->seen) {

            /* marks the current region being processed as 'seen'='yes' so that
             * additional hits of this region by this ray are not processed. 
             */
	    cell->seen++;

            /* increments the presented hits on the current region because all
             * hits on a region are considered part of presented area of that region.
             *
             * do not increment exposed hits here because all exposed hit on all
             * regions have already been processed in the previous loop used to
             * process the first hit of the current ray.
             */
	    cell->hits++;

            /* enable/disable center point processing */
            if (rtarea_compute_centers) {
	        cell->num_hit_points++;

	        /* allocate memory for point_list structure */
	        temp_point_list = (struct point_list *) bu_malloc(sizeof(struct point_list), "Point list allocation");

                /* compute hit point */
                VJOIN1(pp->pt_inhit->hit_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);

	        /* record the points in the area */
	        (temp_point_list->pt_cell)[X] = (pp->pt_inhit->hit_point)[X];
	        (temp_point_list->pt_cell)[Y] = (pp->pt_inhit->hit_point)[Y];
	        (temp_point_list->pt_cell)[Z] = (pp->pt_inhit->hit_point)[Z];
                temp_point_list->next = (struct point_list *) NULL;

	        if (!cell->hit_points) {
		        cell->hit_points = temp_point_list;
	        } else {
                temp_point_list->next = cell->hit_points;
                cell->hit_points = temp_point_list;
	        }
            }

	    if (!cell->name) {
	        /* get the region name */
	        size_t l = strlen(reg->reg_name);
	        while (l > 0) {
		    if (reg->reg_name[l-1] == '/') {
		        break;
		    }
		    l--;
	        }
	        cell->name = &reg->reg_name[l];
	    }

	    /* record the parent assemblies */
	    increment_assembly_counter(cell, reg->reg_name, PRESENTED_AREA, &(pp->pt_inhit->hit_point));
        }
    }
    bu_semaphore_release( RT_SEM_RESULTS );

    return 0;
}


/*
 *			V I E W _ E O L
 *
 *  View_eol() is called by rt_shootray() in do_run().  In this case,
 *  it does nothing.
 */
void view_eol(struct application *UNUSED(ap))
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
		    if (!listp->cell || (bu_strcmp(cell->name, listp->cell->name) < 0)) {
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
	double factor = 1.0; /* show mm in parens by default */

	/* show some common larger units in parens otherwise default to mm^2*/
	if (ZERO(units - 1.0)) {
	    factor = bu_units_conversion("m");
	} else if (ZERO(units - 10.0)) {
	    factor = bu_units_conversion("m");
	} else if (ZERO(units - 100.0)) {
	    factor = bu_units_conversion("m");
	} else if (ZERO(units - 1000.0)) {
	    factor = bu_units_conversion("km");
	} else if (ZERO(units - 25.4)) {
	    factor = bu_units_conversion("ft");
	} else if (ZERO(units - 304.8)) {
	    factor = bu_units_conversion("yd");
	} else {
		factor = bu_units_conversion("mm");
	}
	cell = listp->cell;

	if (type == PRESENTED_AREA) {
	    bu_log("Region %s\t(%ld hits)\t= %18.4lf square %s\t(%.4lf %s^2)",
		   cell->name,
		   cell->hits,
		   cell_area * (fastf_t)cell->hits / (units*units),
		   bu_units_string(units),
		   cell_area * (fastf_t)cell->hits / (factor*factor),
		   bu_units_string(factor)
		);
            if (rtarea_compute_centers) {
                point_t temp;
                if (area_center((cell->hit_points), (cell->num_hit_points), &temp)) {
		    bu_log("\tcenter at (%.4lf, %.4lf, %.4lf) %s",
			temp[X]/units,
			temp[Y]/units,
			temp[Z]/units,
			bu_units_string(units)
		    );
                }
            }
	}
	if (type == EXPOSED_AREA) {
	    bu_log("Region %s\t(%ld hits)\t= %18.4lf square %s\t(%.4lf %s^2)",
		   cell->name,
		   cell->exposures,
		   cell_area * (fastf_t)cell->exposures / (units*units),
		   bu_units_string(units),
		   cell_area * (fastf_t)cell->exposures / (factor*factor),
		   bu_units_string(factor)
		   );
            if (rtarea_compute_centers) {
                point_t temp;
                if (area_center((cell->exp_points), (cell->num_exp_points), &temp)) {
		    bu_log("\tcenter at (%.4lf, %.4lf, %.4lf) %s",
			temp[X]/units,
			temp[Y]/units,
			temp[Z]/units,
			bu_units_string(units)
		    );
                }
            }
	}

        bu_log("\n");
	fflush(stdout); fflush(stderr);

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
    register struct area *cell = (struct area *)NULL;
    long int count = 0;

    struct area_list *listHead = (struct area_list *)bu_malloc(sizeof(struct area_list), "print_area_list area list node allocation");
    struct area_list *listp;
    listHead->cell = (struct area *)NULL;
    listHead->next = (struct area_list *)NULL;

    /* insertion sort based on depth and case */
    rp = BU_LIST_FIRST(region, &(rtip->HeadRegion));
    cell = (struct area *)rp->reg_udata;

    for (BU_LIST_FOR(cellp, area, &(cell->assembly->l)))  {
	int counted_items = 0;
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
		if ((cellp->depth == listp->cell->depth) && (bu_strcmp(cellp->name, listp->cell->name) < 0)) {
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
	double factor = 1.0; /* show mm in parens by default */

	/* show some common larger units in parens otherwise default to mm^2*/
	if (ZERO(units - 1.0)) {
	    factor = bu_units_conversion("m");
	} else if (ZERO(units - 10.0)) {
	    factor = bu_units_conversion("m");
	} else if (ZERO(units - 100.0)) {
	    factor = bu_units_conversion("m");
	} else if (ZERO(units - 1000.0)) {
	    factor = bu_units_conversion("km");
	} else if (ZERO(units - 25.4)) {
	    factor = bu_units_conversion("ft");
	} else if (ZERO(units - 304.8)) {
	    factor = bu_units_conversion("yd");
	} else {
		factor = bu_units_conversion("mm");
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
	    bu_log("Assembly %s\t(%ld hits)\t= %18.4lf square %s\t(%.4lf %s^2)",
		   cell->name,
		   cell->hits,
		   cell_area * (fastf_t)cell->hits / (units*units),
		   bu_units_string(units),
		   cell_area * (fastf_t)cell->hits / (factor*factor),
		   bu_units_string(factor)
		);
            if (rtarea_compute_centers) {
                if (cell->hits) {
                    bu_log("\tcenter at (%.4lf, %.4lf, %.4lf) %s",
                	cell->group_presented_hit_x_sum / (fastf_t)cell->hits / units,
                	cell->group_presented_hit_y_sum / (fastf_t)cell->hits / units,
                	cell->group_presented_hit_z_sum / (fastf_t)cell->hits / units,
			bu_units_string(units)
                    );
                }
            }
	}
	if (type == EXPOSED_AREA) {
	    bu_log("Assembly %s\t(%ld hits)\t= %18.4lf square %s\t(%.4lf %s^2)",
		   cell->name,
		   cell->exposures,
		   cell_area * (fastf_t)cell->exposures / (units*units),
		   bu_units_string(units),
		   cell_area * (fastf_t)cell->exposures / (factor*factor),
		   bu_units_string(factor)
		);
            if (rtarea_compute_centers) {
                if (cell->exposures) {
                    bu_log("\tcenter at (%.4lf, %.4lf, %.4lf) %s",
                    cell->group_exposed_hit_x_sum / (fastf_t)cell->exposures / units,
                    cell->group_exposed_hit_y_sum / (fastf_t)cell->exposures / units,
                    cell->group_exposed_hit_z_sum / (fastf_t)cell->exposures / units,
		    bu_units_string(units)
                    );
                }
            }
	}

	bu_log("\n");
	fflush(stdout); fflush(stderr);
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

    double factor = 1.0; /* show local database units in parens by default */

    /* if not specified by user use local database units for summary units */
    if (model_units) {
    	units = rtip->rti_dbip->dbi_local2base;
    	bu_log("WARNING: using current model working units of (%s)\n", bu_units_string(units));
    }

    /* show some common larger units in parens otherwise default to mm^2*/
    if (ZERO(units - 1.0)) {
	factor = bu_units_conversion("m");
    } else if (ZERO(units - 10.0)) {
	factor = bu_units_conversion("m");
    } else if (ZERO(units - 100.0)) {
	factor = bu_units_conversion("m");
    } else if (ZERO(units - 1000.0)) {
	factor = bu_units_conversion("km");
    } else if (ZERO(units - 25.4)) {
	factor = bu_units_conversion("ft");
    } else if (ZERO(units - 304.8)) {
	factor = bu_units_conversion("yd");
    } else {
	factor = bu_units_conversion("mm");
    }
    bu_log("\n"
	   "********************************************************************\n"
	   "WARNING: The terminology and output format of 'rtarea' is deprecated\n"
	   "         and subject to change in a future release of BRL-CAD.\n"
	   "********************************************************************\n"
	   "\n");
    if (default_units) {
	bu_log("\n"
	       "WARNING: Default units of measurement is presently storage units (%s).\n"
	       "         Future output default will be local units (%s)\n"
	       "\n",
	       bu_units_string(units),
	       bu_units_string(ap->a_rt_i->rti_dbip->dbi_local2base)
	    );
	    
    }

    cumulative = print_region_area_list(&presented_region_count, rtip, PRESENTED_AREA);
    (void) print_region_area_list(&exposed_region_count, rtip, EXPOSED_AREA);

    /* find the maximum assembly depth */
    rp = BU_LIST_FIRST(region, &(rtip->HeadRegion));
    cell = (struct area *)rp->reg_udata;

    for (BU_LIST_FOR(cellp, area, &(cell->assembly->l)))  {
	if (cellp->depth > max_depth) {
	    max_depth = cellp->depth;
	}
    }

    presented_assembly_count = print_assembly_area_list(rtip, max_depth, PRESENTED_AREA);

    exposed_assembly_count = print_assembly_area_list(rtip, max_depth, EXPOSED_AREA);

    bu_log("\nSummary\n=======\n");
    total_area = cell_area * (fastf_t)hit_count;
    bu_log("Cumulative Presented Areas (%ld hits) = %18.4lf square %s\t(%.4lf %s^2)\n",
	   cumulative,
	   cell_area * (fastf_t)cumulative / (units*units),
	   bu_units_string(units),
	   cell_area * (fastf_t)cumulative / (factor*factor),
	   bu_units_string(factor)
	);
    bu_log("Total Exposed Area         (%ld hits) = %18.4lf square %s\t(%.4lf %s^2)\n",
	   hit_count,
	   total_area / (units*units),
	   bu_units_string(units),
	   total_area / (factor*factor),
	   bu_units_string(factor)
	);
    /* output of center of exposed area */
    if (rtarea_compute_centers) {
        bu_log("Center of Exposed Area     (%lu hits) = (%.4lf, %.4lf, %.4lf) %s\n",
	       (long unsigned)exposed_hit_sum,
	       exposed_hit_x_sum / (fastf_t)exposed_hit_sum / units,
	       exposed_hit_y_sum / (fastf_t)exposed_hit_sum / units,
	       exposed_hit_z_sum / (fastf_t)exposed_hit_sum / units,
	       bu_units_string(units)
	);
    }
    bu_log("Number of Presented Regions:    %8d\n", presented_region_count);
    bu_log("Number of Presented Assemblies: %8d\n", presented_assembly_count);
    bu_log("Number of Exposed Regions:    %8d\n", exposed_region_count);
    bu_log("Number of Exposed Assemblies: %8d\n", exposed_assembly_count);
    bu_log("\n"
	   "********************************************************************\n"
	   "WARNING: The terminology and output format of 'rtarea' is deprecated\n"
	   "         and subject to change in a future release of BRL-CAD.\n"
	   "********************************************************************\n"
	   "\n");

    /* free the assembly areas */
    cell = (struct area *)rp->reg_udata;
    if (cell) {
        while (BU_LIST_WHILE(cellp, area, &(cell->assembly->l))) {
	    if (cellp->name) {
		bu_free((char *)cellp->name, "view_end assembly name free");
	    }
            if (rtarea_compute_centers) {
                if (cellp->exp_points) {
                    struct point_list * point, * next_point;
                    point = cellp->exp_points;
                    while (point) {
                        /*next_point = BU_LIST_NEXT(point_list, &(point->l));*/
                        next_point = point->next;
                        bu_free(point, "exposed point_list free");
                        point = next_point;
                    }
                }
                if (cellp->hit_points) {
                struct point_list * point, * next_point;
                point = cellp->hit_points;
                    while (point) {
                        /*next_point = BU_LIST_NEXT(point_list, &(point->l));*/
                        next_point = point->next;
                        bu_free(point, "presented point_list free");
                        point = next_point;
                    }
                }
            }
            BU_LIST_DEQUEUE(&(cellp->l));
            bu_free(cellp, "free assembly-area entry");
	}  
        bu_free(cell->assembly, "free assembly list head");
    }

    /* free the region areas */
    for ( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	cell = (struct area *)rp->reg_udata;
	if (cell) {
            if (rtarea_compute_centers) {
                if (cell->exp_points) {
                    struct point_list * point, * next_point;
                    point = cell->exp_points;
                    while (point) {
                        /*next_point = BU_LIST_NEXT(point_list, &(point->l));*/
                        next_point = point->next;
                        bu_free(point, "exposed point_list free");
                        point = next_point;
                    }
                }
                if (cell->hit_points) {
                struct point_list * point, * next_point;
                point = cell->hit_points;
                    while (point) {
                        /*next_point = BU_LIST_NEXT(point_list, &(point->l));*/
                        next_point = point->next;
                        bu_free(point, "presented point_list free");
                        point = next_point;
                    }
                }
            }
	    bu_free(cell, "view_end area free");
	    cell = (genptr_t)NULL;
	}
    }

    /* flush for good measure */
    fflush(stdout); fflush(stderr);
    return;
}

void view_setup(struct rt_i *UNUSED(rtip)) {}
void view_cleanup(struct rt_i *UNUSED(rtip)) {}
void application_init () {}

/*
 *		 A R E A _ C E N T E R
 *
 *	This function receives a pointer to a point_list
 *	structure, the number of points to calculate, and
 *  the point address of where to record the information.
 */

int
area_center(struct point_list * ptlist, int number, point_t *center)
{
	struct point_list *point_it;
    point_t temp_point;

    if (!ptlist) {
        return 0;
    }
    temp_point[X] = 0;
    temp_point[Y] = 0;
    temp_point[Z] = 0;

    point_it = ptlist;
    while (point_it) {
        temp_point[X] += point_it->pt_cell[X];
        temp_point[Y] += point_it->pt_cell[Y];
        temp_point[Z] += point_it->pt_cell[Z];
        point_it = point_it->next;
    }

    (*center)[X] = temp_point[X] / number;
	(*center)[Y] = temp_point[Y] / number;
	(*center)[Z] = temp_point[Z] / number;

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
