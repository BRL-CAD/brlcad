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
 * usage: global variable struct fitness_state *fstate must exist
 *	fit_prep(db, rows, cols);
 *	fit_store(source_object);
 *	int linear_difference = fit_linDiff(test_object);
 *	fit_clear();
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
#include "rtgeom.h"

#define SEM_WORK RT_SEM_LAST
#define TOTAL_SEMAPHORES SEM_WORK+1

#include "fitness.h"



void
rays_clear(struct fitness_state *fstate)
{
    int i;
    struct part *p;
    for(i = 0; i < fstate->res[U_AXIS] * fstate->res[V_AXIS]; i++){
	while(BU_LIST_WHILE(p, part, &fstate->rays[i]->l)) {
	    BU_LIST_DEQUEUE(&p->l);
	    //	    printf("[%g %g]\n", p->inhit_dist, p->outhit_dist);
	    bu_free(p, "part");
	}
	bu_free(fstate->rays[i], "part");
    }
    bu_free(fstate->rays, "fstate->rays");
}

	


/**
 *	F I T _ S T O R E  --- store an object as the "source" to compare with
 */

void
fit_store (char *obj, char *dbname, struct fitness_state *fstate)
{
    struct db_i *db;

    if( (db=db_open(dbname, "r")) == DBI_NULL)
	bu_bomb("Failed to open model database");
    if(db_dirbuild(db) < 0)
	bu_bomb("Failed to build directory sturcutre");


    fstate->capture = 1;
 
    fit_rt(obj, db, fstate);
    fstate->capture = 0;
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
	add->inhit_dist =   ((((struct fitness_state *)ap->a_uptr)->bbox[0])+ pp->pt_inhit->hit_dist) / (((struct fitness_state *)ap->a_uptr)->bbox[1]);
	add->outhit_dist =   ((((struct fitness_state *)ap->a_uptr)->bbox[0])+ pp->pt_outhit->hit_dist) / (((struct fitness_state *)ap->a_uptr)->bbox[1]);
	BU_LIST_INSERT(&((struct fitness_state *)ap->a_uptr)->rays[ap->a_user]->l, &add->l);
    }
    return 1;
}

/**
 *	C A P T U R E _ M I S S --- called by rt_shootray(), stores a ray that missed the shape
 */
int 
capture_miss(register struct application *ap)
{
    struct part *add = bu_malloc(sizeof(struct part), "part");
    add->inhit_dist = add->outhit_dist = 0;
    BU_LIST_INSERT(&((struct fitness_state *)ap->a_uptr)->rays[ap->a_user]->l, &add->l);
    return 0;
}

/**
 *	C O M P A R E _ H I T -- compare a ray that hit to source
 */
int
compare_hit(register struct application *ap, struct partition *partHeadp, struct seg *segs)
{
    register struct partition *pp=NULL;
    register struct part *mp;
    struct fitness_state *fstate = (struct fitness_state *) ap->a_uptr;
    fastf_t xp, yp, lastpt=0.0;
    int status = 0;
    
    
    if(partHeadp!=NULL)
	pp = partHeadp->pt_forw;

    mp = BU_LIST_FORW(part, &fstate->rays[ap->a_user]->l);


    while(pp != partHeadp && mp != fstate->rays[ap->a_user]) {
	if(status & STATUS_PP)	xp = (fstate->bbox[0] + pp->pt_outhit->hit_dist)/(fstate->bbox[1]);
	else			xp = (fstate->bbox[0] + pp->pt_inhit->hit_dist)/(fstate->bbox[1]);
	if(status & STATUS_MP)	yp = mp->outhit_dist;
	else			yp = mp->inhit_dist;
    	
	if(status==STATUS_EMPTY){ //neither
	    if(NEAR_ZERO(xp-yp, 1.0e-5))
		status = (STATUS_PP | STATUS_MP);
	    else if(xp < yp){
		lastpt = xp;
		status = STATUS_PP;
	    }
	    else if(yp < xp){
		lastpt = yp;
		status = STATUS_MP;
	    }
	}
	else if(status == (STATUS_MP | STATUS_PP)){
	    if(NEAR_ZERO(xp-yp, 1.0e-5)){
		status = STATUS_EMPTY;
		pp = pp->pt_forw;
		mp = BU_LIST_FORW(part, &mp->l);
	    }

	    else if(xp < yp){
		lastpt = xp;
		status = STATUS_MP;
		pp=pp->pt_forw;
	    }
	    else if(yp < xp){
		lastpt = yp;
		status = STATUS_PP;
		mp = BU_LIST_FORW(part, &mp->l);
	    }
	}
	else if(status == STATUS_PP){
	    if(NEAR_ZERO(xp-yp, 1.0e-5) || yp < xp){
		fstate->diff += yp - lastpt;
		status = STATUS_PP | STATUS_MP;
		lastpt = xp;
	    }
	    if(xp < yp){
		fstate->diff += xp - lastpt;
		status = STATUS_EMPTY;
		pp = pp ->pt_forw;
	    }
	}
	else if(status == STATUS_MP){
	    if(NEAR_ZERO(xp-yp, 1.0e-5) || xp < yp){
		fstate->diff += xp - lastpt;
		status = STATUS_PP | STATUS_MP;
		lastpt = yp;
	    }
	    if(yp < xp){
		fstate->diff += yp - lastpt;
		status = STATUS_EMPTY;
		mp = BU_LIST_FORW(part, &mp->l);
	    }
	}
    }
    if(status == STATUS_PP){
	fstate->diff+= (fstate->bbox[0] +  pp->pt_outhit->hit_dist)/fstate->bbox[1] - lastpt;
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
	    fstate->diff += (pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist) / (fstate->bbox[1]);
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
    return 0;
}

/**
 *	G E T _ N E X T _ R O W --- grab the next row of rays to be evaluated
 */
int
get_next_row(struct fitness_state *fstate)
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
 *	R T _ W O R K E R --- raytraces an object in parallel and stores or compares it to source
 *
 */
void
rt_worker(int cpu, genptr_t g)
{
    struct application ap;
    struct fitness_state *fstate = (struct fitness_state *)g;
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
    ap.a_uptr = (void *) g;

    u = -1;

    while((v = get_next_row(fstate))) {
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
 *	F I T _ R T --- raytrace an object optionally storing the rays
 *
 */
void
fit_rt(char *obj, struct db_i *db, struct fitness_state *fstate)
{
    int i;
    double span[3];
    
    fstate->rtip = rt_new_rti(db);



    if(rt_gettree(fstate->rtip, obj) < 0){
	fprintf(stderr, "rt_gettree failed to read %s\n", obj);
	exit(2);
    }
    
    for(i = 0; i < fstate->max_cpus; i++) {
	rt_init_resource(&fstate->resource[i], i, fstate->rtip);
	bn_rand_init(fstate->resource[i].re_randptr, i);
    }

    
    if(fstate->capture){
	fstate->bbox[0] = fstate->rtip->mdl_min[Z];
	fstate->bbox[1] = fstate->rtip->mdl_max[Z] - fstate->bbox[0];
	rt_prep_parallel(fstate->rtip,fstate->ncpu);
	fstate->bbox[0] = fstate->rtip->mdl_min[Z] - fstate->bbox[0];
 
	VSUB2(span, fstate->rtip->mdl_max, fstate->rtip->mdl_min);
	fstate->gridSpacing[U_AXIS] = (fstate->rtip->mdl_max[U_AXIS]-fstate->rtip->mdl_min[U_AXIS])/ (fstate->res[U_AXIS] + 1);
	fstate->gridSpacing[V_AXIS] = (fstate->rtip->mdl_max[V_AXIS] - fstate->rtip->mdl_min[V_AXIS]) / (fstate->res[V_AXIS] + 1 );
    } else {
	rt_prep_parallel(fstate->rtip, fstate->ncpu);
    }
    fstate->row = 0;
    fstate->diff = 0;


    if(fstate->capture){
	fstate->name = obj;
	fstate->rays = bu_malloc(sizeof(struct part *) * fstate->res[U_AXIS] * fstate->res[V_AXIS], "rays");
	bu_parallel(rt_worker, fstate->ncpu, (genptr_t)fstate);
    }
    else{
	bu_parallel(rt_worker, fstate->ncpu, (genptr_t)fstate);
	//fstate->diff /= fstate->res[U_AXIS]*fstate->res[V_AXIS];
    }

    rt_clean(fstate->rtip);
}

/**
 *	F I T _ L I N D I F F --- returns the total linear difference between the rays of obj and source
 */
fastf_t
fit_linDiff(char *obj, struct db_i *db, struct fitness_state *fstate)
{
    fit_rt(obj, db, fstate);
    return fstate->diff;
}

/**
 *	F I T _ U P D A T E R E S --- change ray grid resolution
 *	Note: currently not in use, will be used to refine grid as 
 *	fitness increases
 */
/*
void
fit_updateRes(int rows, int cols, struct fitness_state *fstate){
    if( fstate->rays != NULL){
	rays_clear(fstate);
    }
    fstate->res[U_AXIS] = rows;
    fstate->res[V_AXIS] = cols;
    fit_store(fstate->name, fstate);

}
*/


/**
 *	F I T _ P R E P --- load database and prepare for raytracing
 */
struct fitness_state * 
fit_prep(int rows, int cols)
{

    struct fitness_state *fstate = bu_malloc(sizeof(struct fitness_state), "fstate");
    fstate->max_cpus = fstate->ncpu = 1;//bu_avail_cpus();

    bu_semaphore_init(TOTAL_SEMAPHORES);

    rt_init_resource(&rt_uniresource, fstate->max_cpus, NULL);
    bn_rand_init(rt_uniresource.re_randptr, 0);


    /* 
     * Load databse into db_i 
     */
    /*
    if( (fstate->db = db_open(db, "r+w")) == DBI_NULL) {
	bu_free(fstate, "fstate");
	fprintf(stderr, "Failed to open database %s\n", db);
	exit(1); 
    }
    RT_CK_DBI(fstate->db);

    if( db_dirbuild(fstate->db) < 0) {
	db_close(fstate->db);
	bu_free(fstate, "fstate");
	fprintf(stderr, "Failed to build directory structure on %s\n", db);
	exit(1);
    }
    */

    fstate->capture = 0;
    fstate->res[U_AXIS] = rows;
    fstate->res[V_AXIS] = cols;
    fstate->rays = NULL;
    return fstate;
}

/**
 *	F I T _ C L E A N --- cleanup
 */
void
fit_clean(struct fitness_state *fstate)
{
    //db_close(fstate->db); 
    rays_clear(fstate);
    bu_free(fstate, "fstate");
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
