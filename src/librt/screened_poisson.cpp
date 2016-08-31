/*            S C R E E N E D _ P O I S S O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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
/** @file screened_poisson.cpp
 *
 * Brief description
 *
 */
#include "common.h"

#ifdef ENABLE_SPR

#include "vmath.h"
#include "raytrace.h"
#include "../other/PoissonRecon/Src/SPR.h"

#define DEFAULT_FULL_DEPTH 5
struct rt_vert {
    point_t p;
    vect_t n;
    char *solid_name;
    long solid_surfno;
    int is_set;
};

struct npoints {
    struct rt_vert in;  /* IN */
    struct rt_vert out; /* OUT */
};

struct rt_ray_container {
    point_t p;  /* ray starting point */
    vect_t d;   /* direction vector */
    int x, y;  /* indexes into 2D array of the current view */

    struct npoints *pts;  /* list of points along this ray */
    int pnt_cnt;
    int pnt_capacity;
};

struct rt_point_container {
    struct rt_ray_container *rays;  /* list of rays for this cpu
				     * along the current view */
    int ray_cnt;
    int ray_capacity;

    struct npoints *pts;  /* overall point array for this cpu */
    int pnt_cnt;
    int pnt_capacity;
};

struct rt_parallel_container {
    struct rt_i *rtip;
    struct resource *resp;
    struct bu_vls *logs;
    struct rt_point_container *npts;
    int ray_dir;

    /* stuff needed for subdividing among cpus */
    fastf_t spacing[3];
    point_t model_min;
    point_t model_max;
    fastf_t model_span[3];
    fastf_t per_cpu_slab_width[3];

    /* edge sampling flags */
    int do_subsample;
    int sub_division;
};

/* add all hit point info to info list */
HIDDEN int
add_hit_pnts(struct application *app, struct partition *partH, struct seg *UNUSED(segs))
{
    /* iterating over partitions, this will keep track of the current
     * partition we're working on. */
    struct partition *pp;

    /* will serve as a pointer for the entry and exit hitpoints */
    struct hit *hitp;

    /* will serve as a pointer to the solid primitive we hit */
    struct soltab *stp;

    /* ray container and current points for this segment */
    struct rt_ray_container *c = (struct rt_ray_container *)(app->a_uptr);
    struct npoints *npt;

    RT_CK_APPLICATION(app);

    /* iterate over each partition until we get back to the head.
     * each partition corresponds to a specific homogeneous region of
     * material. */
    for (pp = partH->pt_forw; pp != partH; pp = pp->pt_forw) {

	/* need to see if enough space is allocate to hold the data */
	if (c->pnt_cnt > c->pnt_capacity-1) {
	    c->pnt_capacity *= 4;
	    c->pts = (struct npoints *)bu_realloc((char *)c->pts, c->pnt_capacity * sizeof(struct npoints), "enlarge results array");
	}

	/* pointer to the next set of in/out to save */
	npt = &(c->pts[c->pnt_cnt]);

	/* entry hit point, so we type less */
	hitp = pp->pt_inhit;

	/* primitive we encountered on entry */
	stp = pp->pt_inseg->seg_stp;

	/* hack fix for bad tgc surfaces */
	if (bu_strncmp("rec", stp->st_meth->ft_label, 3) == 0 || bu_strncmp("tgc", stp->st_meth->ft_label, 3) == 0) {

	    /* correct invalid surface number */
	    if (pp->pt_inhit->hit_surfno < 1 || pp->pt_inhit->hit_surfno > 3) {
		pp->pt_inhit->hit_surfno = 2;
	    }
	    if (pp->pt_outhit->hit_surfno < 1 || pp->pt_outhit->hit_surfno > 3) {
		pp->pt_outhit->hit_surfno = 2;
	    }
	}

	/* record IN hit point */
	VJOIN1(npt->in.p, app->a_ray.r_pt, hitp->hit_dist, app->a_ray.r_dir);

	/* calculate IN normal vector */
	RT_HIT_NORMAL(npt->in.n, hitp, stp, &(app->a_ray), pp->pt_inflip);

	/* save IN solid and surface number */
	npt->in.solid_name = bu_strdup(stp->st_dp->d_namep);
	npt->in.solid_surfno = hitp->hit_surfno;

	/* flag IN point as set */
	npt->in.is_set = 1;

	//bu_vls_printf(fp, "%f %f %f %f %f %f\n", hit_pnt[0], hit_pnt[1], hit_pnt[2], hit_normal[0], hit_normal[1], hit_normal[2]);

	/* exit point, so we type less */
	hitp = pp->pt_outhit;

	/* primitive we exited from */
	stp = pp->pt_outseg->seg_stp;

	/* record OUT hit point */
	VJOIN1(npt->out.p, app->a_ray.r_pt, hitp->hit_dist, app->a_ray.r_dir);

	/* calculate OUT normal vector */
	RT_HIT_NORMAL(npt->out.n, hitp, stp, &(app->a_ray), pp->pt_outflip);

	/* save OUT solid and surface number */
	npt->out.solid_name = bu_strdup(stp->st_dp->d_namep);
	npt->out.solid_surfno = hitp->hit_surfno;

	/* flag OUT point as set */
	npt->out.is_set = 1;

	c->pnt_cnt++;
    }
    return 1;
}

/* don't care about misses */
HIDDEN int
ignore_miss(struct application *app)
{
    RT_CK_APPLICATION(app);
    //bu_log("miss!\n");
    return 0;
}

void
copy_ray_points_to_list(struct rt_point_container *cont,
			struct rt_ray_container *ray)
{
    int i;

    for (i = 0; i < ray->pnt_cnt; i++) {
	/* increase space if necessary */
	if (cont->pnt_cnt > cont->pnt_capacity-1) {
	    cont->pnt_capacity *= 4;
	    cont->pts = (struct npoints *)bu_realloc((char *)cont->pts, cont->pnt_capacity * sizeof(struct npoints), "enlarge cpu npoints array");
	}

	/* copy from ray into overall results array */
	memcpy(&(cont->pts[cont->pnt_cnt]), &(ray->pts[i]), sizeof(struct npoints));
	cont->pnt_cnt++;
    }
}

/* checks to see if the results of 2 rays are different */
/*  1 = different, 0 = same */
int
ray_results_differ(struct rt_ray_container *prevRay,
		   struct rt_ray_container *currRay)
{
    /* if the list sizes differ that is a dead give-away */
    if (prevRay->pnt_cnt != currRay->pnt_cnt)
	return 1;

    /* they are the same size, walk them */
    for (int i=0; i < prevRay->pnt_cnt; i++) {
	struct npoints *prevPt = &(prevRay->pts[i]);
	struct npoints *currPt = &(currRay->pts[i]);

	if ((prevPt->in.is_set != currPt->in.is_set) ||
	    (prevPt->out.is_set != currPt->out.is_set))
	    return 1;

	if ((prevPt->in.solid_surfno != currPt->in.solid_surfno) ||
	    (prevPt->out.solid_surfno != currPt->out.solid_surfno))
	    return 1;

	if (bu_strcmp(prevPt->in.solid_name, currPt->in.solid_name) ||
	    bu_strcmp(prevPt->out.solid_name, currPt->out.solid_name))
	    return 1;
    }

    return 0;
}

void
sub_sample(struct rt_parallel_container *state,
	   struct rt_ray_container *prevRay,
	   struct rt_ray_container *currRay,
	   struct rt_point_container *pcont,
	   const int depth, int cpu)
{
    struct application ap;
    struct rt_ray_container middle_ray;

    if (depth >= state->sub_division) return;

    /* init raytracer */
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = state->rtip;
    ap.a_hit = add_hit_pnts;
    ap.a_miss = ignore_miss;
    ap.a_onehit = 0;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_resource = &state->resp[cpu-1];

    /* init ray */
    VADD2(middle_ray.p, currRay->p, prevRay->p);
    VSCALE(middle_ray.p, middle_ray.p, 0.5);
    VMOVE(middle_ray.d, currRay->d);
    middle_ray.x = -1;  /* note that this is a sub-sample ray */
    middle_ray.y = -1;
    VMOVE(ap.a_ray.r_pt, middle_ray.p);
    VMOVE(ap.a_ray.r_dir, middle_ray.d);
    int total_pts = 1e3;  	    /* allocate enough space to hold 1e3 segments along this ray
				     * (probably overkill) */
    middle_ray.pts = (struct npoints *)bu_calloc(total_pts, sizeof(struct npoints), "subray npoints array");
    middle_ray.pnt_cnt = 0;
    middle_ray.pnt_capacity = total_pts;
    ap.a_uptr = (void *) &middle_ray;

    rt_shootray(&ap);  /* shoot the ray */

    int diffLeft = ray_results_differ(prevRay, &middle_ray);
    int diffRight = ray_results_differ(&middle_ray, currRay);

    /* NOTE: There should be a way to check and see if we do or don't need points for this ray
     *  In previous PointSampler code of GCT we wouldn't keep points that did not provide
     *  new value as to limit the # of points we were dealing with.  Right now we go ahead and
     *  save them because it is not hurting us and is easier */
#if 0
    if (diffLeft && diffRight) {
	/* sample the the left and right of the middle ray */
	sub_sample(state, prevRay, &middle_ray, pcont, depth+1, cpu);
	sub_sample(state, &middle_ray, currRay, pcont, depth+1, cpu);
	copy_ray_points_to_list(pcont, &middle_ray);  /* store these points right to the main list */
    } else if (diffLeft) {
	sub_sample(state, prevRay, &middle_ray, pcont, depth+1, cpu);
	/*XXX need to to match checking for unneeded points */
	copy_ray_points_to_list(pcont, &middle_ray);  /* store these points right to the main list */
    } else if (diffRight) {
	sub_sample(state, &middle_ray, currRay, pcont, depth+1, cpu);
	/*XXX need to to match checking for unneeded points */
	copy_ray_points_to_list(pcont, &middle_ray);  /* store these points right to the main list */
    }
#else
    copy_ray_points_to_list(pcont, &middle_ray);  /* store these points right to the main list */

    if (diffLeft) {
	/* if the previous ray differs from this "middle" ray then look between it and the middle */
	sub_sample(state, prevRay, &middle_ray, pcont, depth+1, cpu);
    }

    if (diffRight) {
	/* if the current ray differs from this "middle" ray then look between it and the middle */
	sub_sample(state, &middle_ray, currRay, pcont, depth+1, cpu);
    }
#endif

    bu_free(middle_ray.pts, "subray npoints array");
}

void
check_subsample(int cpu, int j,
		struct rt_parallel_container *state,
		struct rt_ray_container *prevRay,
		struct rt_ray_container *currRay,
		struct rt_point_container *pcont)
{
    /* look to see if we crossed a physical boundary
     * along the i_axis and should sub-sample */
    if (ray_results_differ(prevRay, currRay)) {
	sub_sample(state, prevRay, currRay, pcont, 0, cpu);
    }

    /* look to see if we crossed a physical boundary
     * along the j_axis and should sub-sample */
    if (j == 0) return;

    /* look backwards for matching x,y-1 entry */
    for (int l = pcont->ray_cnt-1; l >= 0; l--) {
	struct rt_ray_container *lower = &(pcont->rays[l]);
	if (lower->x == currRay->x && lower->y == (currRay->y -1)) {
	    /* found it */
	    if (ray_results_differ(lower, currRay)) {
		sub_sample(state, lower, currRay, pcont, 0, cpu);
	    }

	    /* sample diagonally */
	    if (ray_results_differ(prevRay, lower)) {
		sub_sample(state, prevRay, lower, pcont, 0, cpu);
	    }

	    /* now go looking back for the x-1,y-1 entry
	     * so we can sample along the other diagonal */
	    for (int d = pcont->ray_cnt-1; d >= 0; d--) {
		struct rt_ray_container *diag = &(pcont->rays[d]);
		if (diag->x == (currRay->x -1) && diag->y == (currRay->y -1)) {
		    /* found it */
		    if (ray_results_differ(diag, currRay)) {
			sub_sample(state, diag, currRay, pcont, 0, cpu);
		    }

		    break;
		} /* found x-1, y-1 */
	    }
	    break;
	} /* found x, y-1 */
    }
}

void
_rt_gen_worker(int cpu, void *ptr)
{
    int i, j;
    struct application ap;
    struct rt_parallel_container *state = (struct rt_parallel_container *)ptr;
    struct rt_point_container *pcont = (struct rt_point_container *)(&state->npts[cpu-1]);
    struct rt_ray_container *prevRay;
    struct rt_ray_container *currRay;
    int dir1, dir2, dir3;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = state->rtip;
    ap.a_hit = add_hit_pnts;
    ap.a_miss = ignore_miss;
    ap.a_onehit = 0;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_resource = &state->resp[cpu-1];

    /* allocate enough space for this view to hold 1e4 number of rays
     * (probably overkill) */
    int total_rays = 1e4;
    pcont->rays = (struct rt_ray_container *)bu_calloc(total_rays, sizeof(struct rt_ray_container), "cpu ray container array");
    pcont->ray_cnt = 0;
    pcont->ray_capacity = total_rays;

    dir1 = state->ray_dir;
    dir2 = (state->ray_dir+1)%3;  /* axis we step between rays */
    dir3 = (state->ray_dir+2)%3;  /* the slab axis */

    /* set ray direction */
    ap.a_ray.r_dir[dir1] = 1;
    ap.a_ray.r_dir[dir2] = 0;
    ap.a_ray.r_dir[dir3] = 0;

    ap.a_ray.r_pt[dir1] = state->model_min[dir1] -100;
    double slabBottom = (cpu - 1) * state->per_cpu_slab_width[dir3];
    double slabTop = cpu * state->per_cpu_slab_width[dir3];

    int slabStartLN = ceil(slabBottom / state->spacing[dir3]);
    int slabEndLN = ceil(slabTop / state->spacing[dir3]);

    for (j = slabStartLN; j <= slabEndLN; j++) {
	ap.a_ray.r_pt[dir3] = state->model_min[dir3] + (j * state->spacing[dir3]);

	prevRay = NULL;  /* reset previous ray */

	int iAxisSteps = ceil(state->model_span[dir2] / state->spacing[dir2]);
	for (i = 0; i <= iAxisSteps; i++) {
	    ap.a_ray.r_pt[dir2] = state->model_min[dir2] + (i * state->spacing[dir2]);

	    /* expand ray array if necessary */
	    if (pcont->ray_cnt > pcont->ray_capacity-1) {
		pcont->ray_capacity *= 4;
		pcont->rays = (struct rt_ray_container *)bu_realloc((char *)pcont->rays, pcont->ray_capacity * sizeof(struct rt_ray_container), "enlarge ray array");
	    }

	    /* set current ray info */
	    currRay = &(pcont->rays[pcont->ray_cnt]);
	    VMOVE(currRay->p, ap.a_ray.r_pt);
	    VMOVE(currRay->d, ap.a_ray.r_dir);
	    currRay->x = i;
	    currRay->y = j;

	    /* allocate enough space to hold 1e3 segments along this ray
	     * (probably overkill) */
	    int total_pts = 1e3;
	    currRay->pts = (struct npoints *)bu_calloc(total_pts, sizeof(struct npoints), "ray npoints array");
	    currRay->pnt_cnt = 0;
	    currRay->pnt_capacity = total_pts;
	    ap.a_uptr = (void *)currRay;

	    /* fire the ray */
	    rt_shootray(&ap);
	    pcont->ray_cnt++;

	    if (state->do_subsample && state->sub_division > 0) {
		if (i > 0) {
		    check_subsample(cpu, j, state, currRay, prevRay, pcont);
		}
	    }

	    prevRay = currRay;
	}
    }

    /* done all the rays for this view ... need to copy all the results
     * to the main storage array */
    for (i = 0; i < pcont->ray_cnt; i++) {
	currRay = &(pcont->rays[i]);
	copy_ray_points_to_list(pcont, currRay);

	/* we can free the current ray's points because they've been copied */
	/* NOTE: no need to free the 'solid_name' for each point because the memory
	 *   address was copied to the main result array and will be freed later */
	bu_free(currRay->pts, "ray npoints array");
    }

    /* free memory for the rays of this view */
    bu_free(pcont->rays, "cpu ray container array");
    pcont->rays = NULL;
    pcont->ray_cnt = 0;
}


HIDDEN int
_rt_generate_points(int **faces, int *num_faces, point_t **points, int *num_pnts, struct bu_ptbl *hit_pnts, struct db_i *dbip, const char *obj, int fidelity)
{
    int i, dir1, j;
    int ncpus = bu_avail_cpus();
    struct rt_parallel_container *state;
    struct bu_vls vlsstr;
    bu_vls_init(&vlsstr);

    if (!hit_pnts || !dbip || !obj) return -1;

    BU_GET(state, struct rt_parallel_container);

    state->rtip = rt_new_rti(dbip);

    /* load geometry object */
    if (rt_gettree(state->rtip, obj) < 0) return -1;
    rt_prep_parallel(state->rtip, 1);

    /* initialize all the per-CPU resources that are going to be used */
    state->resp = (struct resource *)bu_calloc(ncpus, sizeof(struct resource), "resources");
    for (i = 0; i < ncpus; i++) {
	rt_init_resource(&(state->resp[i]), i, state->rtip);
    }

    /* allocate storage for each of the cpus */
    state->npts = (struct rt_point_container *)bu_calloc(ncpus, sizeof(struct rt_point_container), "cpu point container arrays");

    /* get min and max points of bounding box */
    VMOVE(state->model_min, state->rtip->mdl_min);
    VMOVE(state->model_max, state->rtip->mdl_max);
    VSUB2(state->model_span, state->model_max, state->model_min);

    state->do_subsample = 1;  /* flag to turn on sub-sampling */

    /* set spacing based on element width & fidelity */
    /* sub_division = # of recursive jumps */
    switch (fidelity) {
    case 0:
	state->sub_division = 2;
	for (i = 0; i < 3; i++) {
	    state->spacing[i] = state->model_span[i] / 10.0;  /* 10 sections on each side */
	}
	break;

    case 1:
	state->sub_division = 4;
	for (i = 0; i < 3; i++) {
	    state->spacing[i] = state->model_span[i] / 12.0;  /* 12 sections on each side */
	}
	break;

    case 2:
        {
	    fastf_t width = INFINITY;

	    state->sub_division = 6;
	    for (i = 0; i < 3; i++) {
		if (state->model_span[i] < width)
		    width = state->model_span[i];
	    }
	    for (i = 0; i < 3; i++) {
		state->spacing[i] = width / 15.0;  /* at least 15 sections based on the SMALLEST side */
	    }
	}
	break;

    default:
	bu_log("Unknown fidelity: %d\n", fidelity);
	/*XXX should probably free memory */
	return 1;
	break;
    }

    for (i = 0; i < 3; i++) {
	state->per_cpu_slab_width[i] = state->model_span[i] / ncpus;
    }


    /* allocate enough space for each cpu to hold 1e6 number of points
     * (probably overkill) */
    int total_pts = 1e6;
    for (i = 0; i < ncpus; i++) {
	state->npts[i].pts = (struct npoints *)bu_calloc(total_pts, sizeof(struct npoints), "cpu npoints array");
	state->npts[i].pnt_cnt = 0;
	state->npts[i].pnt_capacity = total_pts;
    }

    /* perform raytracing from the 3 views */
    for (dir1 = 0; dir1 < 3; dir1++) {
	state->ray_dir = dir1;
	bu_parallel(_rt_gen_worker, ncpus, (void *)state);
    }

    /* count the number of points */
    int out_cnt = 0;
    for (i = 0; i < ncpus; i++) {
	bu_log("%d, pnt_cnt: %d\n", i, state->npts[i].pnt_cnt);
	for (j = 0; j < state->npts[i].pnt_cnt; j++) {
	    struct npoints *npt = &(state->npts[i].pts[j]);
	    if (npt->in.is_set) out_cnt++;
	    if (npt->out.is_set) out_cnt++;
	}
    }

    if (!out_cnt) return 1;

    /* store points */
    struct rt_vert **rt_verts = (struct rt_vert **)bu_calloc(out_cnt, sizeof(struct rt_vert *), "output array");
    int curr_ind = 0;
    for (i = 0; i < ncpus; i++) {
	for (j = 0; j < state->npts[i].pnt_cnt; j++) {
	    struct npoints *npt = &(state->npts[i].pts[j]);
	    if (npt->in.is_set) {
		rt_verts[curr_ind] = &(npt->in);
		curr_ind++;
	    }
	    if (npt->out.is_set) {
		rt_verts[curr_ind] = &(npt->out);
		curr_ind++;
	    }
	}
    }

    /* perform SPR algorithm */
    (void)spr_surface_build(faces, num_faces, (double **)points, num_pnts,
			    (const struct cvertex **)rt_verts, out_cnt, fidelity);

    /* free memory */
    bu_free(rt_verts, "output array");
    rt_verts = NULL;
    for (i = 0; i < ncpus; i++) {
	for (j = 0; j < state->npts[i].pnt_cnt; j++) {
	    struct npoints *npt = &(state->npts[i].pts[j]);
	    if (npt->in.is_set) bu_free(npt->in.solid_name, "solid name");
	    if (npt->out.is_set) bu_free(npt->out.solid_name, "solid number");
	}
	bu_free(state->npts[i].pts, "npoints arrays");
    }
    bu_free(state->npts, "point container arrays");
    bu_free(state->resp, "resources");
    BU_PUT(state, struct rt_parallel_container);
    state = NULL;

    return 0;
}

extern "C" void
rt_generate_mesh(int **faces, int *num_faces, point_t **points, int *num_pnts,
		 struct db_i *dbip, const char *obj, int fidelity)
{
    struct bu_ptbl *hit_pnts;
    if (!faces || !num_faces || !points || !num_pnts) return;
    if (!dbip || !obj) return;
    BU_GET(hit_pnts, struct bu_ptbl);
    bu_ptbl_init(hit_pnts, 64, "hit pnts");
    if (_rt_generate_points(faces, num_faces, points, num_pnts, hit_pnts, dbip, obj, fidelity)) {
	(*num_faces) = 0;
	(*num_pnts) = 0;
	return;
    }
}

#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

