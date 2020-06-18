/*                       F I T N E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2020 United States Government as represented by
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
 * Compare rays of source and population.
 *
 * Usage: global variable struct fitness_state *fstate must exist
 *	fit_prep(db, rows, cols);
 *	fit_store(source_object);
 *	int linear_difference = fit_diff(test_object);
 *	fit_clear();
 *
 */

#include "common.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>                     /* home of INT_MAX aka MAXINT */

#include "bu/malloc.h"
#include "bu/parallel.h"
#include "vmath.h"
#include "raytrace.h"
#include "bn/plot3.h"
#include "rt/geom.h"

#include "fitness.h"


/**
 * store an object as the "source" to compare with
 */
void
fit_store (char *obj, char *dbname, struct fitness_state *fstate)
{
    struct db_i *db;

    if ( (db=db_open(dbname, DB_OPEN_READONLY)) == DBI_NULL)
	bu_exit(EXIT_FAILURE, "Failed to open geometry database file");
    if (db_dirbuild(db) < 0)
	bu_exit(EXIT_FAILURE, "Failed to build directory structure");

    fstate->capture = 1;
    fit_rt(obj, db, fstate);
    db_close(db);
    fstate->capture = 0;

}


/**
 * called by rt_shootray(), stores a ray that hit the shape
 */
int
capture_hit(register struct application *ap, struct partition *partHeadp, struct seg *UNUSED(segs))
{
    register struct partition *pp;
    struct part *add;

    /* initialize list of partitions */
    BU_ALLOC(((struct fitness_state *)ap->a_uptr)->ray[ap->a_user], struct part);
    BU_LIST_INIT(&((struct fitness_state *)ap->a_uptr)->ray[ap->a_user]->l);

    /* save ray */
    for (pp = partHeadp->pt_forw; pp != partHeadp; pp = pp->pt_forw) {
	BU_ALLOC(add, struct part);
	add->inhit_dist = pp->pt_inhit->hit_dist;
	add->outhit_dist = pp->pt_outhit->hit_dist;
	BU_LIST_INSERT(&((struct fitness_state *)ap->a_uptr)->ray[ap->a_user]->l, &add->l);
    }
    return 1;
}


/**
 * called by rt_shootray(), stores a ray that missed the shape
 */
int
capture_miss(register struct application *ap)
{
    ((struct fitness_state *)ap->a_uptr)->ray[ap->a_user] = NULL;
    return 0;
}


/**
 * compare a ray that hit to source
 */
int
compare_hit(register struct application *ap, struct partition *partHeadp, struct seg *UNUSED(segs))
{
    register struct partition *pp=NULL;
    register struct part *mp=NULL;
    struct fitness_state *fstate = (struct fitness_state *) ap->a_uptr;
    fastf_t xp, yp, lastpt=0.0;
    int status = 0;

    if (partHeadp == NULL && fstate->ray[ap->a_user] == NULL) {
	bu_semaphore_acquire(fstate->sem_same);
	fstate->same += fstate->a_len;
	bu_semaphore_release(fstate->sem_same);
	return 0;
    }


    /* move from head */
    if (partHeadp!=NULL)
	pp = partHeadp->pt_forw;
    if (fstate->ray[ap->a_user] !=NULL)
	mp = BU_LIST_FORW(part, &fstate->ray[ap->a_user]->l);

    /* if both rays missed, count this as the same.
     * no need to evaluate further*/

    bu_semaphore_acquire(fstate->sem_same);
    bu_semaphore_acquire(fstate->sem_diff);

    while (pp != partHeadp && mp != fstate->ray[ap->a_user]) {
	if (status & STATUS_PP)
	    xp = pp->pt_outhit->hit_dist;
	else
	    xp = pp->pt_inhit->hit_dist;
	if (status & STATUS_MP)
	    yp = mp->outhit_dist;
	else
	    yp = mp->inhit_dist;

	V_MAX(xp, 0);
	V_MAX(yp, 0);

	if (status==STATUS_EMPTY) {
	    if (NEAR_EQUAL(xp, yp, 1.0e-5)) {
		fstate->same += xp;
		status = (STATUS_PP | STATUS_MP);
		lastpt = xp;
	    } else if (xp < yp) {
		fstate->same+= xp;

		lastpt = xp;
		status = STATUS_PP;
	    } else if (yp < xp) {
		fstate->same+= yp;
		lastpt = yp;
		status = STATUS_MP;
	    }
	} else if (status == (STATUS_MP | STATUS_PP)) {
	    if (NEAR_EQUAL(xp, yp, 1.0e-5)) {
		fstate->same += xp - lastpt;
		status = STATUS_EMPTY;
		pp = pp->pt_forw;
		mp = BU_LIST_FORW(part, &mp->l);
		lastpt = xp;
	    } else if (xp < yp) {
		fstate->same += xp - lastpt;
		lastpt = xp;
		status = STATUS_MP;
		pp=pp->pt_forw;
	    } else if (yp < xp) {
		fstate->same += yp - lastpt;
		lastpt = yp;
		status = STATUS_PP;
		mp = BU_LIST_FORW(part, &mp->l);
	    }

	}

	else if (status == STATUS_PP) {
	    if (NEAR_EQUAL(xp, yp, 1.0e-5)) {
		fstate->diff += xp - lastpt;
		status = STATUS_MP;
		lastpt = yp;
		pp = pp->pt_forw;
	    } else if (xp > yp) {
		fstate->diff += yp - lastpt;
		lastpt = yp;
		status = STATUS_PP | STATUS_MP;
	    } else if (xp < yp) {
		fstate->diff += xp - lastpt;
		status = STATUS_EMPTY;
		pp = pp ->pt_forw;
		lastpt = xp;
	    }
	}
	else if (status == STATUS_MP) {
	    if (NEAR_EQUAL(xp, yp, 1.0e-5)) {
		fstate->diff += yp - lastpt;
		status = STATUS_PP;
		lastpt = xp;
		mp = BU_LIST_FORW(part, &mp->l);
	    } else if (xp < yp) {
		fstate->diff += xp - lastpt;
		lastpt = xp;
		status = STATUS_PP | STATUS_MP;
	    } else if (xp > yp) {
		fstate->diff += yp - lastpt;
		status = STATUS_EMPTY;
		mp = BU_LIST_FORW(part, &mp->l);
		lastpt = yp;
	    }
	}
    }

    /* we could be halfway through evaluating a partition
     * finish evaluating it before proceeding */
    if (status == STATUS_PP) {
	if (pp->pt_outhit->hit_dist > fstate->a_len) {
	    /* trim ray */
	    fstate->diff += fstate->a_len - lastpt;
	    lastpt = fstate->a_len;
	} else {
	    fstate->diff+= pp->pt_outhit->hit_dist - lastpt;
	    lastpt = pp->pt_outhit->hit_dist;
	}
	pp = pp->pt_forw;
    } else if (status == STATUS_MP) {
	fstate->diff += mp->outhit_dist - lastpt;
	lastpt = mp->outhit_dist;
	mp = BU_LIST_FORW(part, &mp->l);
    }

    /* if there are a different # of partitions in source and individual */
    while (mp != fstate->ray[ap->a_user]) {
	fstate->diff += mp->outhit_dist - mp->inhit_dist;
	lastpt = mp->outhit_dist;
	mp = BU_LIST_FORW(part, &mp->l);
    }
    while (pp != partHeadp && pp->pt_inhit->hit_dist < fstate->a_len) {
	if (pp->pt_outhit->hit_dist > fstate->a_len) {
	    /* trim bounding box */
	    fstate->diff += fstate->a_len - pp->pt_inhit->hit_dist;
	    lastpt = fstate->a_len;
	} else {
	    fstate->diff += pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	    lastpt = pp->pt_outhit->hit_dist;
	}
	pp = pp->pt_forw;
    }

    /* include trailing empty space as similar */
    fstate->same += fstate->a_len - lastpt;

    bu_semaphore_release(fstate->sem_same);
    bu_semaphore_release(fstate->sem_diff);

    return 1;
}


/**
 * compares missed ray to real ray
 */
int
compare_miss(register struct application *ap)
{
    compare_hit(ap, NULL, NULL);
    return 0;
}


/**
 * grab the next row of rays to be evaluated
 */
int
get_next_row(struct fitness_state *fstate)
{
    int r;
    bu_semaphore_acquire(fstate->sem_work);
    if (fstate->row < fstate->res[Y])
	r = ++fstate->row; /* get a row to work on */
    else
	r = 0; /* signal end of work */
    bu_semaphore_release(fstate->sem_work);

    return r;
}


/**
 * raytraces an object in parallel and stores or compares it to source
 */
void
rt_worker(int UNUSED(cpu), void *g)
{
    struct application ap;
    struct fitness_state *fstate = (struct fitness_state *)g;
    int u, v;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = fstate->rtip;
    if (fstate->capture) {
	ap.a_hit = capture_hit;
	ap.a_miss = capture_miss;
    } else {
	ap.a_hit = compare_hit;
	ap.a_miss = compare_miss;
    }

    ap.a_resource = &rt_uniresource;/*fstate->resource[cpu];*/

    ap.a_ray.r_dir[X] = ap.a_ray.r_dir[Y] = 0.0;
    ap.a_ray.r_dir[Z] = 1.0;
    ap.a_uptr = (void *) g;

    u = -1;

    while ((v = get_next_row(fstate))) {
	for (u = 1; u <= fstate->res[X]; u++) {
	    ap.a_ray.r_pt[X] = fstate->min[X] + u * fstate->gridSpacing[X];
	    ap.a_ray.r_pt[Y] = fstate->min[Y] + v * fstate->gridSpacing[Y];
	    ap.a_ray.r_pt[Z] = fstate->min[Z];
	    ap.a_user = (v-1)*(fstate->res[X]) + u-1;

	    rt_shootray(&ap);


	}
    }
}


/**
 * raytrace an object optionally storing the rays
 */
void
fit_rt(char *obj, struct db_i *db, struct fitness_state *fstate)
{
    int i;
    fastf_t diff[3], tmp;
    fastf_t min[3], max[3];


    /*
     * uncomment to calculate # of nodes
     * and use in fitness calculation
     *
     struct directory *dp
     struct rt_db_internal in;
     int n_leaves;
     if (!rt_db_lookup_internal(db, obj, &dp, &in, LOOKUP_NOISY, &rt_uniresource))
     bu_exit(EXIT_FAILURE, "Failed to read object to raytrace");
     n_leaves = db_count_tree_nodes(((struct rt_comb_internal *)in.idb_ptr)->tree, 0);
     rt_db_free_internal(&in);
    */

    fstate->rtip = rt_new_rti(db);
    fstate->row = 0;

    if (rt_gettree(fstate->rtip, obj) < 0)
	bu_exit(EXIT_FAILURE, "rt_gettree failed");

    /* stash bounding box and if comparing to source
     * calculate the difference between the bounding boxes */
    if (fstate->capture) {
	VMOVE(fstate->min, fstate->rtip->mdl_min);
	VMOVE(fstate->max, fstate->rtip->mdl_max);
    }
    /*else {
     * instead of storing min and max, just compute
     * what we're going to need later
     for (i = 0; i < 3; i++) {
     diff[i] = 0;
     if (fstate->min[i] > fstate->rtip->mdl_min[i])
     diff[i] += fstate->min[i] - fstate->rtip->mdl_min[i];
     if (fstate->max[i] < fstate->rtip->mdl_max[i])
     diff[i] += fstate->rtip->mdl_max[i] - fstate->max[i];
     if (fstate->min[i]  < fstate->rtip->mdl_min[i])
     min[i] = fstate->min[i];
     else
     min[i] = fstate->rtip->mdl_min[i];
     if (fstate->max[i] > fstate->rtip->mdl_max[i])
     max[i] = fstate->max[i];
     else
     max[i] = fstate->rtip->mdl_max[i];
     diff[i] = max[i] - min[i];
     }
     fastf_t tmp = (diff[X]/fstate->gridSpacing[X]-1) * (diff[Y]/fstate->gridSpacing[Y] - 1);
     fstate->volume = (fstate->a_len + (max[Z] - fstate->max[Z])) * tmp;
     }
    */



    rt_prep(fstate->rtip);
    if (fstate->capture) {
	/* Store bounding box of voxel data -- fixed bounding box for fitness */
	fstate->gridSpacing[X] = (fstate->rtip->mdl_max[X] - fstate->rtip->mdl_min[X]) / (fstate->res[X] + 1);
	fstate->gridSpacing[Y] = (fstate->rtip->mdl_max[Y] - fstate->rtip->mdl_min[Y]) / (fstate->res[Y] + 1);
	fstate->a_len = fstate->max[Z]-fstate->rtip->mdl_min[Z]; /* maximum ray length (z-dist of bounding box) */
	fstate->volume = fstate->a_len * fstate->res[X] * fstate->res[Y]; /* volume of bounding box */

	/* allocate storage for saved rays */
	fstate->ray = (struct part **)bu_malloc(sizeof(struct part *) * fstate->res[X] * fstate->res[Y], "ray");
	VMOVE(fstate->min, fstate->rtip->mdl_min);
	VMOVE(fstate->max, fstate->rtip->mdl_max);


    } else {
	/* instead of storing min and max, just compute
	 * what we're going to need later */
	for (i = 0; i < 3; i++) {
	    diff[i] = 0;
	    if (fstate->min[i]  < fstate->rtip->mdl_min[i])
		min[i] = fstate->min[i];
	    else
		min[i] = fstate->rtip->mdl_min[i];
	    if (fstate->max[i] > fstate->rtip->mdl_max[i])
		max[i] = fstate->max[i];
	    else
		max[i] = fstate->rtip->mdl_max[i];
	    diff[i] = max[i] - min[i];
	}
	tmp = (diff[X]/fstate->gridSpacing[X]-1) * (diff[Y]/fstate->gridSpacing[Y] - 1);
	fstate->volume = (fstate->a_len + (max[Z] - fstate->max[Z])) * tmp;
	/* scale fitness to the unon of the sources and individual's bounding boxes */
	/* FIXME: sloppy
	   fastf_t tmp = (diff[X]/fstate->gridSpacing[X]-1) * (diff[Y]/fstate->gridSpacing[Y] * diff[Z] - 1);
	   V_MAX(tmp, 0);
	*/
    }


    rt_worker(0, (void *)fstate);

    /* normalize fitness if we aren't just saving the source */
    if (!fstate->capture) {
	fstate->fitness = fstate->same / (fstate->volume );
	/* reset counters for future comparisons */
	fstate->diff = fstate->same = 0.0;
    }

    /* clean up resources and rtip */
    /*
      for (i = 0; i < fstate->max_cpus; i++)
      rt_clean_resource(fstate->rtip, &fstate->resource[i]);
    */
    rt_free_rti(fstate->rtip);


}


/**
 * returns the difference between input geometry and individual  (compares rays)
 */
void
fit_diff(char *obj, struct db_i *db, struct fitness_state *fstate)
{
    fit_rt(obj, db, fstate);
}


/**
 * load database and prepare for raytracing
 */
void
fit_prep(struct fitness_state *fstate, int rows, int cols)
{
    fstate->sem_work = bu_semaphore_register("beset_sem_work");
    fstate->sem_diff = bu_semaphore_register("beset_sem_diff");
    fstate->sem_same = bu_semaphore_register("beset_sem_same");
    fstate->max_cpus = fstate->ncpu = 1;/*bu_avail_cpus();*/
    fstate->capture = 0;
    fstate->res[X] = rows;
    fstate->res[Y] = cols;
    fstate->ray = NULL;
}


/**
 * cleanup
 */
void
fit_clean(struct fitness_state *fstate)
{
    free_rays(fstate);
}


/**
 * free saved rays
 */
void
free_rays(struct fitness_state *fstate)
{
    int i;
    struct part *p;
    for (i = 0; i < fstate->res[X] * fstate->res[Y]; i++) {
	if (fstate->ray[i] == NULL)
	    continue;
	while (BU_LIST_WHILE(p, part, &fstate->ray[i]->l)) {
	    BU_LIST_DEQUEUE(&p->l);
	    bu_free(p, "part");
	}
	bu_free(fstate->ray[i], "part");
    }
    bu_free(fstate->ray, "fstate->ray");
}


/**
 * change ray grid resolution.
 *
 * Note: currently not in use, will be used to refine grid as fitness
 * increases
 */
/*
  void
  fit_updateRes(int rows, int cols, struct fitness_state *fstate) {
  if ( fstate->ray != NULL) {
  free_ray(fstate);
  }
  fstate->res[X] = rows;
  fstate->res[Y] = cols;
  fit_store(fstate->name, fstate);

  }
*/


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
