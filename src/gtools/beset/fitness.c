/*                       F I T N E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file fitness.c
 *
 * Compare rays of source and population 
 *
 * Author - Ben Poole
 * 
 */

#include "common.h"
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>                     /* home of INT_MAX aka MAXINT */

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"
#define SEM_WORK RT_SEM_LAST
#define SEM_SHOTS RT_SEM_LAST+1
#define SEM_STATS RT_SEM_LAST+2 /* semaphore for statistics */
#define SEM_LIST RT_SEM_LAST+3
#define TOTAL_SEMAPHORES SEM_LIST+1

#define U_AXIS 0
#define V_AXIS 1
#define I_AXIS 2

char *options = "d:p:?:h:";
char *progname = "(noname)";
char *usage_msg = "Usage: %s database model pop\n";

int ncpu;
int max_cpus;
int verbose;
int debug;
#define DLOG if (debug) bu_log
/*following is questionable*/
#define A_STATE a_uptr 


struct resource resource[MAX_PSW]; /* memory resources for multi-cpup processing */
int row; /*row counter*/
int num_rows; /*number of rows */
int num_cols;
struct rt_i *rtip;


/* linked list of rays shot at model */
struct saved_ray {
    struct bu_list  l;
    struct saved_partition *part;
};
/* linked list of partitions of a ray*/
struct saved_partition {
    struct bu_list  l;
    fastf_t	    inhit_dist;
    fastf_t	    outhit_dist;
};

/* stored ray trace of the model */
static struct saved_ray modelRays = {
    {
	BU_LIST_HEAD_MAGIC,
	(struct bu_list *)&modelRays,
	(struct bu_list *)&modelRays
    },
    (struct saved_partition *)NULL
};

/**
 *	U S A G E --- tell users how to invoke this program, then exit
 */
void
usage(char *s)
{
    if(s) fputs(s,stderr);
    fprintf(stderr, usage_msg, progname);
    exit(1);
}

/**
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int
parse_args(int ac, char *av[])
{
    char c;
    if( ! (progname=strrchr(*av, '/')) )
	progname = *av;
    else
	++progname;

    bu_opterr = 0;

    while((c=bu_getopt(ac,av,options)) != EOF) {
	switch(c){
	    case 'd'	: debug = 1; break;
	    case 'P'	: if((c=atoi(bu_optarg)) > 0 && c <= max_cpus) ncpu = c;
			  break;
	    case '?'	:
	    case 'h'	:
	    default	:
			  fprintf(stderr, "Bad or help flag '%c' specified\n",c);
			  usage("");
			  break;
	}
    }
    return bu_optind;
}

/**
 *	C A P T U R E _ H I T --- called by rt_shootray(), stores a ray that hit the shape
 */
int
hit_capture(register struct application *ap, struct partition *partHeadp, struct seg *segs)
{
    register struct partition *pp; /* pointer to current ray's partitions */

    /* save ray */
    struct saved_partition *add;
    for(pp = partHeadp->pt_forw; pp != partHeadp; pp = pp->pt_forw){
	add = bu_malloc(sizeof(struct saved_partition), "saved_partition");
	add->inhit_dist = pp->pt_inhit->hit_dist;
	add->outhit_dist = pp->pt_outhit->hit_dist;
	bu_semaphore_acquire(SEM_LIST);
	BU_LIST_INSERT(&modelRays.l, &add->l);
	bu_semaphore_release(SEM_LIST);
    }

}

/**
 *	C A P T U R E _ M I S S --- called by rt_shootray(), stores a ray that missed the shape
 */
int 
miss_capture(register struct application *ap)
{
    struct saved_partition *add = bu_malloc(sizeof(struct saved_partition), "saved_partition");
    add->inhit_dist = add->outhit_dist = -1;
    bu_semaphore_acquire(SEM_LIST);
    BU_LIST_INSERT(&modelRays.l, &add->l);
    bu_semaphore_release(SEM_LIST);
}

/**
 *	G E T _ N E X T _ R O W --- grab the next row of rays to be evaluated
 */
int
get_next_row(void)
{
    bu_semaphore_acquire(SEM_WORK);
    if(row < num_rows)
	++row; /* get a row to work on */
    else
	row = 0; /* signal end of work */
    bu_semaphore_release(SEM_WORK);
    
    return row;
}

/**
 *	R T _ R O W S --- parallel ray tracing of rows
 */
void
rt_rows(int cpu, genptr_t ptr)
{
    struct application ap;
    int u, v;
    struct saved_ray *add;
    
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = (struct rt_i *)rtip;
    ap.a_hit = capture_hit;
    ap.a_miss = capture_miss;
    ap.a_resource = &resource[cpu];

    ap.a_ray.r_dir[U_AXIS] = ap.a_ray.r_dist[V_AXIS] = 0.0;
    ap.a_ray.r_dir[I_AXIS] = 1.0;
    ap.A_STATE = ptr; /* necessary? */

    u = -1;
    /* D'OH: linked lists develop race condition which means rays are not in order in linked list
     * --> move from linked list to array, oh well.
     */

    /*
    while((v = get_next_row())) {
	for(u = 1; u < num_cols; u++) {
	    ap.a_ray.r_pt[U_AXIS] = ap.a_rt_i->mdl_min[U_AXIS] + u * gridSpacing;
	    ap.a_ray.r_pt[V_AXIS] = ap.a_rt_i->mdl_min[V_AXIS] + v * gridSpacing;
	    ap.a_ray.r_pt[I_AXIS] = ap.a_rt_i->mdl_min[I_AXIS];
	    ap.a_user = v;

	    add = bu_malloc(sizeof(struct saved_ray), "saved_ray");
	    */
}
	    




    



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
