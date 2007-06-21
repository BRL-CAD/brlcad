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
 * PROGRAM MODEL
 * 1) read source  and convert to struct part 
 * 2) when done, call function to compare a given population object to source
 *    and return a number representative of the fitness (in terms of closeness
 *    to the source)
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

char *options = "c:r:d:p:t:?:h:";
char *progname = "(noname)";
char *usage_msg = "Usage: %s database model pop\n";


//struct resource resource[MAX_PSW]; /* memory resources for multi-cpup processing */


/* linked list of partitions of a ray*/
struct part {
    struct bu_list  l;
    fastf_t	    inhit_dist;
    fastf_t	    outhit_dist;
};

/* stored model ray */
struct fitness_state {
    char *name;
    struct part **rays; /* internal representation of raytraced source */
    struct db_i *db; /* the database the source and population are a part of */
    struct rt_i *rtip; /* current objects to be raytraced */

    struct resource resource[MAX_PSW]; /* memory resource for multi-cpu processing */
    int ncpu;
    int max_cpus;

    int res[2]; /*  ray resolution on u and v axes */
    double gridSpacing[2]; /* grid spacing on u and v axes */
    int row; /* current v axis index *///IS IT?
    
    int capture; /* flags whether to store the object */
    fastf_t diff; /* linear difference between source and object */
};

struct fitness_state *fstate;





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
	    case 'c'	: fstate->res[U_AXIS] = atoi(bu_optarg);break;
	    case 'r'	: fstate->res[V_AXIS] = atoi(bu_optarg);break;
	    case 'p'	: if((c=atoi(bu_optarg)) > 0 && c <= fstate->max_cpus) fstate->ncpu = c;
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
capture_hit(register struct application *ap, struct partition *partHeadp, struct seg *segs)
{
    register struct partition *pp; /* pointer to current ray's partitions */

    /* save ray */
    struct part *add;
    for(pp = partHeadp->pt_forw; pp != partHeadp; pp = pp->pt_forw){
	add = bu_malloc(sizeof(struct part), "part");
	add->inhit_dist = pp->pt_inhit->hit_dist;
	add->outhit_dist = pp->pt_outhit->hit_dist;
	bu_semaphore_acquire(SEM_LIST);
	BU_LIST_INSERT(&fstate->rays[ap->a_user]->l, &add->l);
	bu_semaphore_release(SEM_LIST);
    }

}

/**
 *	C A P T U R E _ M I S S --- called by rt_shootray(), stores a ray that missed the shape
 */
int 
capture_miss(register struct application *ap)
{
    struct part *add = bu_malloc(sizeof(struct part), "part");
    add->inhit_dist = add->outhit_dist = 0;
    bu_semaphore_acquire(SEM_LIST);
    BU_LIST_INSERT(&fstate->rays[ap->a_user]->l, &add->l);
    bu_semaphore_release(SEM_LIST);
}

/**
 *	C O M P A R E _ H I T -- compare a ray that hit to source
 */
int
compare_hit(register struct application *ap, struct partition *partHeadp, struct seg *segs)
{
    register struct partition *pp=NULL;
    register struct part *mp;
    fastf_t xp, yp, lastpt;
    int status = 0;
    //some if(partHeadp == NULL)...
    
    
    if(partHeadp!=NULL)
    pp = partHeadp->pt_forw;
    mp = BU_LIST_FORW(part, &fstate->rays[ap->a_user]->l);
#define STATUS_PP 1
#define STATUS_MP 2
#define STATUS_EMPTY 0


    while(pp != partHeadp && mp != fstate->rays[ap->a_user]) {
	if(status & STATUS_PP)	xp = pp->pt_outhit->hit_dist;
	else			xp = pp->pt_inhit->hit_dist;
	if(status & STATUS_MP)	yp = mp->outhit_dist;
	else			yp = mp->inhit_dist;
    	
	if(status==STATUS_EMPTY){ //neither
	    if(xp < yp){
		lastpt = xp;
		status = STATUS_PP;
	    }
	    else if(yp < xp){
		lastpt = yp;
		status = STATUS_MP;
	    }
	    else{
		status = (STATUS_PP | STATUS_MP);
	    }
	}
	else if(status == (STATUS_MP | STATUS_PP)){
	    if(xp < yp){
		lastpt = xp;
		status = STATUS_MP;
		pp=pp->pt_forw;
	    }
	    else if(yp < xp){
		lastpt = yp;
		status = STATUS_PP;
		mp = BU_LIST_FORW(part, &mp->l);
	    }
	    else{
		status = STATUS_EMPTY;
		pp = pp->pt_forw;
		mp = BU_LIST_FORW(part, &mp->l);
	    }
	}
	else if(status == STATUS_PP){
	    if(xp < yp){
		fstate->diff += xp - lastpt;
		status = STATUS_EMPTY;
		pp = pp ->pt_forw;
	    }
	    if(yp < xp || yp == xp){
		fstate->diff += yp - lastpt;
		status = STATUS_PP | STATUS_MP;
		lastpt = xp;
	    }
	}
	else if(status == STATUS_MP){
	    if(yp < xp){
		fstate->diff += yp - lastpt;
		status = STATUS_EMPTY;
		mp = BU_LIST_FORW(part, &mp->l);
	    }
	    if(xp < yp || xp == yp){
		fstate->diff += xp - lastpt;
		status = STATUS_PP | STATUS_MP;
		lastpt = yp;
	    }
	}
    }
    if(status == STATUS_PP){
	fstate->diff+= pp->pt_outhit->hit_dist - lastpt;
	pp = pp->pt_forw;
    }
    if(status == STATUS_MP){
	fstate->diff += mp->outhit_dist - lastpt;
	mp = BU_LIST_FORW(part, &mp->l);
    }
	/* if there are a different # of partitions in modelHeadp and partHeadp*/
    if(mp != fstate->rays[ap->a_user]){
	while(mp != fstate->rays[ap->a_user]){
	    fstate->diff += mp->outhit_dist - mp->inhit_dist;
	    mp = BU_LIST_FORW(part, &mp->l);
	}
    }
    else if (pp != partHeadp){
	while(pp != partHeadp){
	    fstate->diff += pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	    pp = pp->pt_forw;
	}
    }
    return 1;
}
		
	


/**
 *	C O M P A R E _ M I S S --- compares missed ray to real ray
 */
int
compare_miss(register struct application *ap)
{
    compare_hit(ap, NULL, NULL);
    return 1;
}

/**
 *	G E T _ N E X T _ R O W --- grab the next row of rays to be evaluated
 */
int
get_next_row(void)
{
    int r;
    bu_semaphore_acquire(SEM_WORK);
    if(fstate->row < fstate->res[V_AXIS])
	r = ++fstate->row; /* get a row to work on */
    else
	r = 0; /* signal end of work */
    bu_semaphore_release(SEM_WORK);
    
    return r;
}

/**
 *	C A P T U R E _ R T _  P L A N E --- capture and store raytraced object
 *
 */

void
store_source_rt(int cpu, genptr_t g)
{
    struct application ap;
    int u, v;
    
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = fstate->rtip;
    if(fstate->capture){
	ap.a_hit = capture_hit;
	ap.a_miss = capture_miss;
    } else {
	ap.a_hit = compare_hit;
	ap.a_miss = compare_miss;
    }

    ap.a_resource = &fstate->resource[cpu];

    ap.a_ray.r_dir[U_AXIS] = ap.a_ray.r_dir[V_AXIS] = 0.0;
    ap.a_ray.r_dir[I_AXIS] = 1.0;

    u = -1;

    while((v = get_next_row())) {
	for(u = 1; u <= fstate->res[U_AXIS]; u++) {
	    ap.a_ray.r_pt[U_AXIS] = ap.a_rt_i->mdl_min[U_AXIS] + u * fstate->gridSpacing[U_AXIS];
	    ap.a_ray.r_pt[V_AXIS] = ap.a_rt_i->mdl_min[V_AXIS] + v * fstate->gridSpacing[V_AXIS];
	    ap.a_ray.r_pt[I_AXIS] = ap.a_rt_i->mdl_min[I_AXIS];
	    ap.a_user = (v-1)*(fstate->res[U_AXIS]) + u-1;
	    
	    /* initialize stored partition */
	    if(fstate->capture){
		fstate->rays[ap.a_user] = bu_malloc(sizeof(struct part), "part");
		BU_LIST_INIT(&fstate->rays[ap.a_user]->l);
	    }
	    rt_shootray(&ap);
	}
    }
}

/**
 *	C O M P A R E _ R T _ S O U R C E --- compare raytrace of object to source
 */
void
compare_rt_source(int cpu, genptr_t g)
{

}
/**
 *	R T _ O B J --- raytrace an object optionally storing the rays
 *
 */
int
rt_obj(char *obj) {//use function pointers?

    int i;
    
    fstate->rtip = rt_new_rti(fstate->db);
    if(rt_gettree(fstate->rtip, obj) < 0){
	fprintf(stderr, "%s: rt_gettree failed to read %s\n", progname, obj);
	exit(2);
    }

    
    for(i = 0; i < fstate->max_cpus; i++) {
	rt_init_resource(&fstate->resource[i], i, fstate->rtip);
	bn_rand_init(fstate->resource[i].re_randptr, i);
    }
    rt_prep_parallel(fstate->rtip,fstate->ncpu);
    
    double span[3];
    VSUB2(span, fstate->rtip->mdl_max, fstate->rtip->mdl_min);
    fstate->gridSpacing[U_AXIS] = span[U_AXIS] / (fstate->res[U_AXIS] + 1);
    fstate->gridSpacing[V_AXIS] = span[V_AXIS] / (fstate->res[V_AXIS] + 1 );
    fstate->row = 0;

    if(fstate->capture){
	fstate->name = obj;
	fstate->rays = bu_malloc(sizeof(struct part *) * fstate->res[U_AXIS] * fstate->res[V_AXIS], "rays");
	bu_parallel(store_source_rt, fstate->ncpu, NULL);
    }
    else{
	bu_parallel(store_source_rt, fstate->ncpu, NULL);
    }

    /* cleanup */
    rt_clean(fstate->rtip);
}


int main(int ac, char *av[])
{
    fstate = bu_malloc(sizeof(struct fitness_state), "fstate");
    int arg_count;
    //struct rt_i *rtip; /* reset for each object */
#define IDBUFSIZE 2048
    char idbuf[IDBUFSIZE];
    int i;
    int start_objs; /*index in command line args where geom object list starts */
    int num_objects; 
    struct part *p; /* used to free stored source */

    fstate->max_cpus = fstate->ncpu = bu_avail_cpus();

    /*parse command line arguments*/
    arg_count = parse_args(ac, av);
    if((ac - arg_count) < 2) {
	usage("Error: Must specify model and objects on command line\n");
    }

    bu_semaphore_init(TOTAL_SEMAPHORES);

    rt_init_resource(&rt_uniresource, fstate->max_cpus, NULL);
    bn_rand_init(rt_uniresource.re_randptr, 0);

    /* 
     * Load databse into db_i 
     */
    if( (fstate->db = db_open(av[arg_count], "r")) == DBI_NULL) {
	fprintf(stderr, "%s: db_open failure on %s\n", progname, av[arg_count]);
	exit(2);
    }
    RT_CK_DBI(fstate->db);

    if( db_dirbuild(fstate->db) < 0) {
	db_close(fstate->db);
	fprintf(stderr, "db_dirbuild failed on %s\n", av[arg_count]);
	exit(2);
    }

    start_objs = ++arg_count;
    num_objects = ac - arg_count;

    /* 
     * capture source ray trace
     */
    fstate->capture = 1;
    rt_obj(av[arg_count]); 

    /*
     * compare source ray trace to an object
     */
    fstate->capture = 0;
    rt_obj(av[arg_count+1]);

    printf("Total linear difference between %s and %s: %g\n", av[arg_count], av[arg_count+1], fstate->diff);

    db_close(fstate->db);
    for(i = 0; i < fstate->res[U_AXIS] * fstate->res[V_AXIS]; i++){
	while(BU_LIST_WHILE(p, saved_partitition, &fstate->rays[i]->l)) {
	    BU_LIST_DEQUEUE(&p->l);
//	    printf("[%g %g]\n", p->inhit_dist, p->outhit_dist);
	    bu_free(p, "part");
	}
	bu_free(fstate->rays[i], "part");
    }
    bu_free(fstate->rays, "fstate->rays");
    return 0;

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
