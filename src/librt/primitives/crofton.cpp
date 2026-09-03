/*                     C R O F T O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
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
/** @file primitives/crofton.cpp
 *
 * Cauchy-Crofton sampling estimator for surface area and volume.
 *
 * This file is the single authoritative implementation of the Crofton
 * ray-sampling algorithm.  It exposes three public symbols:
 *
 *   rt_crofton_shoot()     -- core estimator given a prepared rt_i;
 *                             shared with libanalyze for code reuse
 *   rt_crofton_surf_area() -- ft_surf_area-compatible fallback for the
 *                             primitive functab (used when a primitive
 *                             lacks an analytic surface-area formula)
 *   rt_crofton_volume()    -- ft_volume-compatible fallback for the
 *                             primitive functab (used when a primitive
 *                             lacks an analytic volume formula)
 *
 * The Cauchy-Crofton integral-geometry formula relates the number of
 * times random lines pierce a surface to its area, and the total
 * length of solid chord segments to its volume:
 *
 *   SA = 4*pi*R^2 * N_crossings / (2 * N_rays)
 *   V  = pi * R^2 * total_chord / N_rays
 *
 * where R is the bounding-sphere radius, N_crossings counts every
 * entry AND exit hit event (2 per solid segment for a non-self-
 * intersecting closed surface), and total_chord is the sum of solid
 * segment lengths.
 *
 * References:
 *   Li et al. (2003), "Using low-discrepancy sequences and the Crofton
 *   formula to compute surface areas of geometric models",
 *   Computer-Aided Design 35, 771-782.
 *
 *   Liu et al. (2010), "Surface area estimation of digitized 3D objects
 *   using quasi-Monte Carlo methods", Pattern Recognition 43, 3900-3909.
 */

#include "common.h"

#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <stdlib.h>
#include <math.h>
#include <random>
#include <stdexcept>
#include <string.h>
#include <thread>
#include <time.h>
#include <vector>

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/datetime.h"
#include "bg/obr.h"
#include "raytrace.h"
#include "rt/geom.h"


/* ------------------------------------------------------------------ */
/* Default parameters for the functab fallbacks                        */
/* ------------------------------------------------------------------ */

/** Minimum rays per iteration when used as a generic functab fallback.
 *  Kept small so the fallback is fast for interactive use; callers
 *  that need higher accuracy should call rt_crofton_sample() with
 *  appropriate params.                                                */
#define RT_CROFTON_DEFAULT_SAMPLES   2000u

/** Rays per iteration for the implicit-primitive functab wrappers
 *  (ARS, EBM, METABALL, EXTRUDE, REVOLVE, HRT).  These primitives
 *  do not implement their own analytic SA/volume formulas so they
 *  rely entirely on Crofton; 50 000 rays are still very fast for a
 *  single primitive and keep typical error well under 2 %.          */
#define RT_CROFTON_IMPLICIT_SAMPLES  50000u

/** Convergence threshold (%) for the functab fallback.               */
#define RT_CROFTON_DEFAULT_THRESHOLD 1.0

/** Minimum evidence before applying stability-based stopping.         */
#define RT_CROFTON_STABILITY_MIN_RAYS      20000u
#define RT_CROFTON_STABILITY_MIN_CROSSINGS 500u
#define RT_CROFTON_STABILITY_MIN_WINDOWS   2u

/** Fixed seed for reproducible estimates in tests and cross-platform runs. */
#define RT_CROFTON_RNG_SEED 0x9e3779b97f4a7c15ULL


/* ------------------------------------------------------------------ */
/* Internal types                                                       */
/* ------------------------------------------------------------------ */

struct crofton_ray {
    point_t r_pt;
    vect_t  r_dir;
};

struct crofton_shared {
    /* Accumulated across all rays/threads */
    size_t  total_crossings; /* in+out hit events */
    double  total_chord;     /* solid segment length sum (mm) */
    size_t  total_rays;      /* rays fired (hits + misses) */
    point_t *points;
    size_t point_count;
    size_t point_capacity;
    bool collect_points;
};

struct crofton_worker_data {
    struct application     ap;        /* per-CPU application struct */
    struct crofton_ray    *rays;      /* shared ray array (read-only) */
    size_t                 start;
    size_t                 end;
    struct crofton_shared *shared;
    size_t                 local_crossings;
    double                 local_chord;
    size_t                 local_rays;
    point_t               *local_points;
    size_t                 local_point_count;
    size_t                 local_point_capacity;
};


/* ------------------------------------------------------------------ */
/* Hit / miss callbacks                                                 */
/* ------------------------------------------------------------------ */

static int
crofton_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    struct crofton_worker_data *wd = (struct crofton_worker_data *)ap->a_uptr;

    size_t crossings = 0;
    double chord     = 0.0;

    struct partition *pp;
    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	/* Each partition contributes an in-hit and an out-hit */
	crossings += 2;
	chord += pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	if (wd->shared->collect_points) {
	    if (wd->local_point_count + 2 > wd->local_point_capacity) {
		const size_t initial_capacity = 1024;
		size_t new_capacity = wd->local_point_capacity ?
		    2 * wd->local_point_capacity : initial_capacity;
		while (new_capacity < wd->local_point_count + 2)
		    new_capacity *= 2;
		wd->local_points = (point_t *)bu_realloc(wd->local_points,
		    new_capacity * sizeof(point_t), "Crofton worker hit points");
		wd->local_point_capacity = new_capacity;
	    }
	    VJOIN1(wd->local_points[wd->local_point_count], ap->a_ray.r_pt,
		pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	    wd->local_point_count++;
	    VJOIN1(wd->local_points[wd->local_point_count], ap->a_ray.r_pt,
		pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	    wd->local_point_count++;
	}
    }

    wd->local_crossings += crossings;
    wd->local_chord     += chord;
    wd->local_rays      += 1;

    return 1;
}


static int
crofton_miss(struct application *ap)
{
    struct crofton_worker_data *wd = (struct crofton_worker_data *)ap->a_uptr;
    wd->local_rays += 1;

    return 0;
}


/* ------------------------------------------------------------------ */
/* Point / ray generation                                               */
/* ------------------------------------------------------------------ */

static double
crofton_rand01(std::mt19937_64 &rng)
{
    return std::generate_canonical<double, 53>(rng);
}


static void
random_point_on_sphere(double radius, const point_t center, point_t out, std::mt19937_64 &rng)
{
    double theta = 2.0 * M_PI * crofton_rand01(rng);
    double phi   = acos(2.0 * crofton_rand01(rng) - 1.0);
    double sp    = sin(phi);
    out[X] = center[X] + radius * sp * cos(theta);
    out[Y] = center[Y] + radius * sp * sin(theta);
    out[Z] = center[Z] + radius * cos(phi);
}


/**
 * Generate @p nrays chord rays from 2*nrays random points on the
 * bounding sphere.  Each ray connects one randomly chosen point to
 * another.
 */
static void
generate_rays(struct crofton_ray *rays, size_t nrays,
	      double radius, const point_t center, std::mt19937_64 &rng)
{
    size_t npts = nrays * 2;

    point_t *pts = (point_t *)bu_calloc(npts, sizeof(point_t), "crofton pts");
    for (size_t i = 0; i < npts; i++)
	random_point_on_sphere(radius, center, pts[i], rng);

    /* Shuffle so pairing is random */
    for (size_t i = npts - 1; i > 0; i--) {
	size_t j = (size_t)(crofton_rand01(rng) * (i + 1));
	if (j > i) j = i;
	point_t tmp;
	VMOVE(tmp, pts[i]);
	VMOVE(pts[i], pts[j]);
	VMOVE(pts[j], tmp);
    }

    for (size_t i = 0; i < nrays; i++) {
	VMOVE(rays[i].r_pt, pts[i * 2]);
	VSUB2(rays[i].r_dir, pts[i * 2 + 1], pts[i * 2]);
	VUNITIZE(rays[i].r_dir);
    }

    bu_free(pts, "crofton pts");
}


/* ------------------------------------------------------------------ */
/* Reusable parallel workers                                            */
/* ------------------------------------------------------------------ */

class CroftonWorkerPool
{
    public:
	CroftonWorkerPool(const struct application *ap_template,
		struct resource *resources, size_t worker_count) :
		count(worker_count), data(worker_count)
	{
	    for (size_t i = 0; i < count; i++) {
		data[i].ap = *ap_template;
		data[i].ap.a_resource = &resources[i];
		data[i].shared = NULL;
		data[i].ap.a_uptr = &data[i];
	    }

	    try {
		executor = std::thread(&CroftonWorkerPool::run_workers, this);
	    } catch (...) {
		free_worker_points();
		throw;
	    }

	    std::unique_lock<std::mutex> lock(mutex);
	    if (!workers_started.wait_for(lock, worker_start_timeout,
		    [this]() { return registered == count; })) {
		stopping = true;
		work_ready.notify_all();
		lock.unlock();
		executor.join();
		free_worker_points();
		throw std::runtime_error("worker startup timed out");
	    }
	}

	~CroftonWorkerPool()
	{
	    stop_and_join();
	    free_worker_points();
	}

	CroftonWorkerPool(const CroftonWorkerPool &) = delete;
	CroftonWorkerPool &operator=(const CroftonWorkerPool &) = delete;

	void run(struct crofton_ray *rays, size_t nrays,
		struct crofton_shared *shared)
	{
	    const size_t rays_per_worker = nrays / count;
	    std::unique_lock<std::mutex> lock(mutex);
	    for (size_t i = 0; i < count; i++) {
		struct crofton_worker_data &wd = data[i];
		wd.rays = rays;
		wd.start = i * rays_per_worker;
		wd.end = (i == count - 1) ? nrays : (i + 1) * rays_per_worker;
		wd.shared = shared;
		wd.local_crossings = 0;
		wd.local_chord = 0.0;
		wd.local_rays = 0;
		wd.local_point_count = 0;
	    }
	    completed = 0;
	    generation++;
	    work_ready.notify_all();
	    work_done.wait(lock, [this]() { return completed == count; });
	}

	const std::vector<struct crofton_worker_data> &worker_data() const
	{
	    return data;
	}

    private:
	/* Native thread creation should complete promptly.  A bounded startup wait
	 * turns partial bu_parallel startup into a reported failure, not a hang. */
	static constexpr std::chrono::seconds worker_start_timeout{5};

	static void worker_entry(int UNUSED(id), void *context)
	{
	    CroftonWorkerPool *pool = static_cast<CroftonWorkerPool *>(context);
	    size_t slot = 0;
	    {
		std::lock_guard<std::mutex> lock(pool->mutex);
		slot = pool->registered++;
		if (pool->registered == pool->count)
		    pool->workers_started.notify_one();
	    }
	    pool->worker(slot);
	}

	void run_workers()
	{
	    bu_parallel(worker_entry, count, this);
	}

	void worker(size_t slot)
	{
	    size_t observed_generation = 0;
	    std::unique_lock<std::mutex> lock(mutex);
	    while (true) {
		work_ready.wait(lock, [this, observed_generation]() {
		    return stopping || generation != observed_generation;
		});
		if (stopping)
		    return;

		observed_generation = generation;
		struct crofton_worker_data *wd = &data[slot];
		lock.unlock();
		for (size_t i = wd->start; i < wd->end; i++) {
		    VMOVE(wd->ap.a_ray.r_pt, wd->rays[i].r_pt);
		    VMOVE(wd->ap.a_ray.r_dir, wd->rays[i].r_dir);
		    rt_shootray(&wd->ap);
		}
		lock.lock();
		completed++;
		if (completed == count)
		    work_done.notify_one();
	    }
	}

	void stop_and_join()
	{
	    {
		std::lock_guard<std::mutex> lock(mutex);
		stopping = true;
		work_ready.notify_all();
	    }
	    if (executor.joinable())
		executor.join();
	}

	void free_worker_points()
	{
	    for (struct crofton_worker_data &wd : data) {
		if (wd.local_points)
		    bu_free(wd.local_points, "Crofton worker hit points");
		wd.local_points = NULL;
	    }
	}

	size_t count;
	std::vector<struct crofton_worker_data> data;
	std::thread executor;
	std::mutex mutex;
	std::condition_variable workers_started;
	std::condition_variable work_ready;
	std::condition_variable work_done;
	size_t registered = 0;
	size_t generation = 0;
	size_t completed = 0;
	bool stopping = false;
};


static void
do_one_iteration(CroftonWorkerPool &pool, size_t nrays, double radius,
	const point_t center, struct crofton_shared *shared,
	std::mt19937_64 &rng)
{
    struct crofton_ray *rays = (struct crofton_ray *)bu_calloc(
	nrays, sizeof(struct crofton_ray), "crofton rays");

    generate_rays(rays, nrays, radius, center, rng);
    pool.run(rays, nrays, shared);

    for (const struct crofton_worker_data &wd : pool.worker_data()) {
	shared->total_crossings += wd.local_crossings;
	shared->total_chord += wd.local_chord;
	shared->total_rays += wd.local_rays;
	if (wd.local_point_count) {
	    const size_t needed = shared->point_count + wd.local_point_count;
	    if (needed > shared->point_capacity) {
		size_t new_capacity = shared->point_capacity ?
		    2 * shared->point_capacity : needed;
		if (new_capacity < needed)
		    new_capacity = needed;
		shared->points = (point_t *)bu_realloc(shared->points,
		    new_capacity * sizeof(point_t), "Crofton hit points");
		shared->point_capacity = new_capacity;
	    }
	    memcpy(&shared->points[shared->point_count], wd.local_points,
		wd.local_point_count * sizeof(point_t));
	    shared->point_count = needed;
	}
    }

    bu_free(rays, "crofton rays");
}


/* ------------------------------------------------------------------ */
/* Public API: rt_crofton_shoot                                         */
/* ------------------------------------------------------------------ */

/**
 * Run the Cauchy-Crofton sampling estimator on an already-prepared
 * raytrace instance @p rtip, using the stopping criteria in @p params.
 *
 * The caller is responsible for creating, preparing (rt_prep_parallel),
 * and freeing (rt_i_destroy) @p rtip.  This function does NOT call
 * rt_i_destroy.
 *
 * @param out_surf_area Receives the estimated surface area (mm^2).
 * @param out_volume    Receives the estimated volume (mm^3).
 * @param out_aabb_min  Optional sampled AABB minimum; must be paired with max.
 * @param out_aabb_max  Optional sampled AABB maximum; must be paired with min.
 * @param out_obb       Optional sampled OBB in ARB8 point ordering.
 * @param out_points    Optional caller-owned sampled surface-point array.
 * @param out_point_count Number of returned points; must accompany out_points.
 * @param rtip         Prepared raytrace instance (rt_prep_parallel must
 *                     have been called before this function).
 * @param params       Stopping criteria (see struct rt_crofton_params).
 *                     NULL or all-zero -> 2 000-ray default behaviour.
 * @param bbox_min     Optional focused sampling bbox minimum, or NULL.
 * @param bbox_max     Optional focused sampling bbox maximum, or NULL.
 * @return  The total number of ray-surface crossings accumulated during
 *          sampling (>= 0) on success; -1 on bad arguments.
 */
static int
crofton_shoot_impl(point_t                        **out_points,
		   size_t                          *out_point_count,
		   double                          *out_surf_area,
		   double                          *out_volume,
		   point_t                         *out_aabb_min,
		   point_t                         *out_aabb_max,
		   point_t                          out_obb[8],
		 struct rt_i                    *rtip,
		 const struct rt_crofton_params *params,
		 const fastf_t                  *bbox_min,
		 const fastf_t                  *bbox_max)
{
    if (!rtip || (!out_surf_area && !out_volume && !out_aabb_min && !out_obb && !out_points) ||
	((out_points == NULL) != (out_point_count == NULL)) ||
	((out_aabb_min == NULL) != (out_aabb_max == NULL)))
	return -1;
    if (out_points) {
	*out_points = NULL;
	*out_point_count = 0;
    }
    if (out_aabb_min) {
	VSETALL(*out_aabb_min, INFINITY);
	VSETALL(*out_aabb_max, -INFINITY);
    }
    if (out_obb) {
	for (int i = 0; i < 8; ++i)
	    VSETALL(out_obb[i], 0.0);
    }

    /* ---- Compute a tight bounding sphere from actual soltab extents ----
     *
     * rt_prep_parallel inflates mdl_min/mdl_max to integer-mm boundaries
     * (floor/ceil in prep.cpp) to prevent edge-grazing artefacts in the ray
     * scheduler.  This is harmless for scene-sized geometry, but for sub-mm
     * primitives (e.g. xyzringtrc.s, diameter ~0.06 mm) the inflation can
     * expand the Crofton bounding sphere by a factor of 10-15×, reducing the
     * fraction of rays that actually pierce the object from ~20 % to ~0.1 %.
     * At 50 000 rays that leaves only ~90 expected crossings, giving ~10 %
     * statistical noise rather than the expected ~1 %.
     *
     * Fix: walk the soltab list (which stores the pre-inflation st_min/st_max)
     * and use their union RPP to build the Crofton sphere.  For large geometry
     * that already spans integer-mm boundaries the result is identical to the
     * old rti_radius / mdl_min / mdl_max path.                              */
    double R;
    point_t center;
    if (bbox_min && bbox_max) {
	VADD2SCALE(center, bbox_max, bbox_min, 0.5);
	vect_t bbox_diag;
	VSUB2(bbox_diag, bbox_max, bbox_min);
	R = 0.5 * MAGNITUDE(bbox_diag);
    } else {
	point_t tight_min, tight_max;
	VSETALL(tight_min,  MAX_FASTF);
	VSETALL(tight_max, -MAX_FASTF);
	{
	    struct soltab *stp;
	    RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
		VMIN(tight_min, stp->st_min);
		VMAX(tight_max, stp->st_max);
	    } RT_VISIT_ALL_SOLTABS_END;
	}

	/* Tight path: use actual object extents */
	if (tight_min[X] < MAX_FASTF) {
	    VADD2SCALE(center, tight_max, tight_min, 0.5);
	    vect_t tight_diag;
	    VSUB2(tight_diag, tight_max, tight_min);
	    R = 0.5 * MAGNITUDE(tight_diag);
	    /* Sanity: fall back if something degenerate slipped through */
	    if (R <= 0.0)
		R = rtip->rti_radius;
	} else {
	    /* No soltabs (unusual): fall back to the inflated rti values */
	    R = rtip->rti_radius;
	    VADD2SCALE(center, rtip->mdl_max, rtip->mdl_min, 0.5);
	}
    }

    if (R <= 0.0) {
	if (out_surf_area) *out_surf_area = 0.0;
	if (out_volume)    *out_volume    = 0.0;
	return 0;
    }

    /* ---- Resolve stopping criteria from params ---- */
    size_t max_rays     = params ? params->n_rays       : 0;
    double stability_mm = params ? params->stability_mm : 0.0;
    double time_ms      = params ? params->time_ms      : 0.0;
    int    use_default  = (!max_rays && stability_mm <= 0.0 && time_ms <= 0.0);

    /* Batch size for each iteration.
     * Default: 2 000 (same as before, growth factor applied each round).
     * Explicit n_rays with no other criteria: fire them in one shot.   */
    size_t batch = RT_CROFTON_DEFAULT_SAMPLES;
    if (!use_default && max_rays > 0 && stability_mm <= 0.0 && time_ms <= 0.0)
	batch = max_rays;   /* single-iteration mode */

    size_t ncpus = bu_avail_cpus();
    if (ncpus < 1) ncpus = 1;
    if (ncpus > MAX_PSW) ncpus = MAX_PSW;

    /* ---- Initialize per-CPU resources ---- */
    struct resource *resources = (struct resource *)bu_calloc(
	ncpus, sizeof(struct resource), "crofton resources");
    for (size_t i = 0; i < ncpus; i++)
	rt_init_resource(&resources[i], i, rtip);

    /* ---- Set up application template ---- */
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i         = rtip;
    ap.a_hit          = crofton_hit;
    ap.a_miss         = crofton_miss;
    ap.a_overlap      = NULL;
    ap.a_multioverlap = NULL;
    ap.a_logoverlap   = rt_silent_logoverlap;
    ap.a_resource     = resources;
    ap.a_onehit       = 0;

    /* ---- Shared accumulator ---- */
    struct crofton_shared shared;
    memset(&shared, 0, sizeof(shared));
    shared.collect_points = out_points != NULL || out_aabb_min != NULL || out_obb != NULL;
    const bool stabilize_surface = out_surf_area != NULL || shared.collect_points;

    CroftonWorkerPool *pool = NULL;
    try {
	pool = new CroftonWorkerPool(&ap, resources, ncpus);
    } catch (const std::exception &e) {
	bu_log("rt_crofton: unable to start worker pool: %s\n", e.what());
	for (size_t i = 0; i < ncpus; i++) {
	    if (resources[i].re_magic == RESOURCE_MAGIC) {
		rt_clean_resource_basic(rtip, &resources[i]);
		BU_PTBL_SET(&rtip->rti_resources, i, NULL);
	    }
	}
	bu_free(resources, "crofton resources");
	return -1;
    }

    std::mt19937_64 rng(RT_CROFTON_RNG_SEED);

    const double FOUR_PI    = 4.0 * M_PI;
    const double PI         = M_PI;
    const double INV_4PI    = 1.0 / FOUR_PI;
    const double INV_4PI3   = 3.0 / FOUR_PI;   /* for V → equivalent r */

    double curr_est_sa = 0.0, curr_est_v = 0.0;

    if (use_default) {
	/* ---- Legacy default: 2 000-ray convergence loop ---- */
	double prev2_est_sa = -2.0, prev1_est_sa = -1.0;
	double prev2_est_v  = -2.0, prev1_est_v  = -1.0;
	size_t iteration = 0;
	size_t curr_rays = batch;

	do {
	    if (iteration > 0) {
		double factor = pow(1.5, (double)iteration);
		curr_rays = (size_t)(batch * factor);
		if (curr_rays < batch)
		    curr_rays = batch;
	    }

	    do_one_iteration(*pool, curr_rays, R, center, &shared, rng);
	    iteration++;

	    if (shared.total_rays == 0) break;

	    curr_est_sa = FOUR_PI * R * R
		* (double)shared.total_crossings
		/ (2.0 * (double)shared.total_rays);
	    curr_est_v = PI * R * R
		* shared.total_chord
		/ (double)shared.total_rays;

	    if (iteration >= 3 &&
		shared.total_rays >= RT_CROFTON_STABILITY_MIN_RAYS &&
		shared.total_crossings >= RT_CROFTON_STABILITY_MIN_CROSSINGS) {
		const double thr = RT_CROFTON_DEFAULT_THRESHOLD;
		double d_sa_cur  = (prev1_est_sa > 0.0)
		    ? fabs(curr_est_sa  - prev1_est_sa) / prev1_est_sa * 100.0 : 999.0;
		double d_sa_prev = (prev2_est_sa > 0.0)
		    ? fabs(prev1_est_sa - prev2_est_sa) / prev2_est_sa * 100.0 : 999.0;
		double d_v_cur   = (prev1_est_v  > 0.0)
		    ? fabs(curr_est_v   - prev1_est_v)  / prev1_est_v  * 100.0 : 999.0;
		double d_v_prev  = (prev2_est_v  > 0.0)
		    ? fabs(prev1_est_v  - prev2_est_v)  / prev2_est_v  * 100.0 : 999.0;

		const bool sa_stable = !stabilize_surface ||
		    (d_sa_cur <= thr && d_sa_prev <= thr);
		const bool volume_stable = !out_volume ||
		    (d_v_cur <= thr && d_v_prev <= thr);
		if (sa_stable && volume_stable)
		    break;
	    }

	    prev2_est_sa = prev1_est_sa;  prev1_est_sa = curr_est_sa;
	    prev2_est_v  = prev1_est_v;   prev1_est_v  = curr_est_v;

	} while (1);

    } else {
	/* ---- Parametric loop: n_rays / stability_mm / time_ms ---- */
	double prev_r_sa = -1.0, prev_r_v = -1.0;
	size_t total_fired = 0;
	size_t stable_windows = 0;
	int64_t t0 = (time_ms > 0.0) ? bu_gettime() : 0;

	for (;;) {
	    /* Time-budget check before firing */
	    if (time_ms > 0.0 && total_fired > 0 &&
		(bu_gettime() - t0) / 1000.0 >= time_ms)
		break;

	    /* Rays-budget: clamp batch to remaining if n_rays is set */
	    size_t fire = batch;
	    if (max_rays > 0) {
		size_t remaining = (total_fired < max_rays)
		    ? (max_rays - total_fired) : 0;
		if (remaining == 0) break;
		if (fire > remaining) fire = remaining;
	    }

	    do_one_iteration(*pool, fire, R, center, &shared, rng);
	    total_fired += fire;

	    if (shared.total_rays == 0) break;

	    curr_est_sa = FOUR_PI * R * R
		* (double)shared.total_crossings
		/ (2.0 * (double)shared.total_rays);
	    curr_est_v = PI * R * R
		* shared.total_chord
		/ (double)shared.total_rays;

	    /* Stability check */
	    if (stability_mm > 0.0 &&
		shared.total_rays >= RT_CROFTON_STABILITY_MIN_RAYS &&
		shared.total_crossings >= RT_CROFTON_STABILITY_MIN_CROSSINGS) {
		double r_sa = (curr_est_sa > 0.0) ? sqrt(curr_est_sa * INV_4PI)  : 0.0;
		double r_v  = (curr_est_v  > 0.0) ? cbrt(curr_est_v  * INV_4PI3) : 0.0;

		if (prev_r_sa >= 0.0) {
		    int sa_ok = (!stabilize_surface) ||
			fabs(r_sa - prev_r_sa) < stability_mm;
		    int v_ok  = (!out_volume) ||
			fabs(r_v  - prev_r_v)  < stability_mm;
		    if (sa_ok && v_ok) {
			stable_windows++;
			if (stable_windows >= RT_CROFTON_STABILITY_MIN_WINDOWS)
			    break;
		    } else {
			stable_windows = 0;
		    }
		}
		prev_r_sa = r_sa;
		prev_r_v  = r_v;
	    }

	    /* Time-budget check after firing */
	    if (time_ms > 0.0 &&
		(bu_gettime() - t0) / 1000.0 >= time_ms)
		break;
	}
    }

    if (out_surf_area) *out_surf_area = curr_est_sa;
    if (out_volume)    *out_volume    = curr_est_v;
    if (out_aabb_min) {
	for (size_t i = 0; i < shared.point_count; ++i)
	    VMINMAX(*out_aabb_min, *out_aabb_max, shared.points[i]);
    }
    int bounds_ret = 0;
    if (out_obb) {
	point_t *corner_ptrs[8];
	for (int i = 0; i < 8; ++i)
	    corner_ptrs[i] = &out_obb[i];
	if (!shared.point_count ||
	    bg_3d_obb(corner_ptrs, &shared.points[0][0],
		static_cast<int>(shared.point_count)))
	    bounds_ret = -1;
    }
    if (out_points && !bounds_ret) {
	*out_points = shared.points;
	*out_point_count = shared.point_count;
    } else if (shared.points) {
	bu_free(shared.points, "Crofton hit points");
    }

    delete pool;

    /* Clean each resource and NULL out its slot in rtip->rti_resources.
     * This is necessary because crofton_from_ip calls rt_i_destroy(rtip)
     * after we return.  rt_i_destroy → rt_clean iterates rti_resources and
     * calls rt_clean_resource (which calls rt_init_resource) on every
     * non-NULL entry.  If we free the resources array first, those entries
     * become dangling pointers and rt_init_resource reads garbage re_cpu
     * values that may exceed MAX_PSW, triggering a BU_ASSERT.
     *
     * By setting the slot to NULL we let rt_i_destroy's cleanup skip it,
     * and then we can safely bu_free the resources array.                */
    for (size_t i = 0; i < ncpus; i++) {
	if (resources[i].re_magic == RESOURCE_MAGIC) {
	    rt_clean_resource_basic(rtip, &resources[i]);
	    BU_PTBL_SET(&rtip->rti_resources, i, NULL);
	}
    }
    bu_free(resources, "crofton resources");

    /* Return the total crossing count so callers can distinguish
     * "zero hits" (return == 0) from "some hits" (return > 0).
     * Clamp to INT_MAX to avoid signed-overflow on pathological inputs. */
    if (bounds_ret)
	return -1;
    return (shared.total_crossings <= INT_MAX)
	? (int)shared.total_crossings : INT_MAX;
}

int
rt_crofton_shoot(double *out_surf_area, double *out_volume,
		 point_t *out_aabb_min, point_t *out_aabb_max, point_t out_obb[8],
		 point_t **out_points, size_t *out_point_count, struct rt_i *rtip,
		 const struct rt_crofton_params *params,
		 const fastf_t *bbox_min, const fastf_t *bbox_max)
{
    return crofton_shoot_impl(out_points, out_point_count, out_surf_area,
	out_volume, out_aabb_min, out_aabb_max, out_obb, rtip, params,
	bbox_min, bbox_max);
}


/* ------------------------------------------------------------------ */
/* Private: build a temp in-memory DB and run Crofton on it           */
/* ------------------------------------------------------------------ */

/**
 * Create a temporary in-memory database containing only the primitive
 * described by @p ip, run the Crofton estimator with the given @p params,
 * and return the results.
 *
 * The caller's @p ip is NOT consumed or freed.
 */
static int
crofton_from_ip_n(const struct rt_db_internal    *ip,
		  double                         *out_sa,
		  double                         *out_vol,
		  const struct rt_crofton_params *params)
{
    if (!ip || (!out_sa && !out_vol))
	return -1;

    /* ---- Open an in-memory database ---- */
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL) {
	bu_log("rt_crofton: db_open_inmem() failed\n");
	return -1;
    }

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	bu_log("rt_crofton: wdb_dbopen() failed\n");
	db_close(dbip);
	return -1;
    }

    /* ---- Serialize ip to bu_external without freeing the caller's data.
     *
     * Build a shallow wrapper around ip so that rt_db_cvt_to_ext5
     * can serialize the primitive data without requiring a full deep copy.
     * We must NOT call rt_db_free_internal on this wrapper because idb_ptr
     * is owned by the caller.                                             */
    const char *scratch = "_crofton_tmp";

    /* ---- DSP special case: also export the referenced binary data object.
     *
     * A DSP primitive with dsp_datasrc == RT_DSP_SRC_OBJ stores its height
     * data in a separate binary-uniform object (dsp_bip) that is looked up by
     * name (dsp_name) during rt_dsp_import.  The in-memory database only
     * receives the DSP primitive itself, so the lookup fails at rt_gettree
     * time unless we also export the binary data object here.              */
    if (ip->idb_minor_type == ID_DSP) {
	struct rt_dsp_internal *dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	if (dsp_ip && dsp_ip->dsp_datasrc == RT_DSP_SRC_OBJ && dsp_ip->dsp_bip) {
	    const char *data_name = bu_vls_cstr(&dsp_ip->dsp_name);
	    struct rt_db_internal *bip = dsp_ip->dsp_bip;
	    struct bu_external bip_ext;
	    BU_EXTERNAL_INIT(&bip_ext);
	    if (rt_db_cvt_to_ext5(&bip_ext, data_name, bip, 1.0,
				       dbip, bip->idb_major_type) == 0) {
		int bip_flags = db_flags_internal(bip);
		if (wdb_export_external(wdbp, &bip_ext, data_name,
					bip_flags,
					(unsigned char)bip->idb_minor_type) < 0)
		    bu_free_external(&bip_ext);
		/* on success ext_buf is stolen; no free needed */
	    } else {
		bu_free_external(&bip_ext);
		bu_log("rt_crofton: failed to export DSP data object '%s'\n",
		       data_name);
	    }
	}
    }

    struct rt_db_internal tmp_intern;
    RT_DB_INTERNAL_INIT(&tmp_intern);
    tmp_intern.idb_major_type = ip->idb_major_type;
    tmp_intern.idb_type       = ip->idb_minor_type;
    tmp_intern.idb_ptr        = ip->idb_ptr;   /* shared, not owned */
    /* Derive idb_meth from the global function table rather than trusting
     * ip->idb_meth: callers that construct a struct rt_db_internal by hand
     * (e.g. unit tests) frequently leave this field uninitialised.
     * rt_db_get_internal always sets it correctly, so for those callers the
     * assignment below is a no-op (same pointer value).                   */
    if (ip->idb_minor_type >= 0 && ip->idb_minor_type < (int)ID_MAXIMUM)
	tmp_intern.idb_meth = &OBJ[ip->idb_minor_type];
    else
	tmp_intern.idb_meth = ip->idb_meth; /* last resort: trust the caller */

    struct bu_external ext;
    BU_EXTERNAL_INIT(&ext);

    if (rt_db_cvt_to_ext5(&ext, scratch, &tmp_intern, 1.0,
				dbip, ip->idb_major_type) < 0) {
	bu_log("rt_crofton: rt_db_cvt_to_ext5() failed\n");
	bu_free_external(&ext);
	db_close(dbip);
	return -1;
    }

    int eflags = db_flags_internal(&tmp_intern);
    if (wdb_export_external(wdbp, &ext, scratch,
			    eflags,
			    (unsigned char)ip->idb_minor_type) < 0) {
	bu_log("rt_crofton: wdb_export_external() failed\n");
	/* ext.ext_buf stolen by db_inmem on success; free any remainder */
	bu_free_external(&ext);
	db_close(dbip);
	return -1;
    }
    /* In the INMEM path ext_buf is stolen; this is safe to call regardless */
    bu_free_external(&ext);

    db_update_nref(dbip);

    /* ---- Build raytrace instance ---- */
    struct rt_i *rtip = rt_i_create(dbip);
    if (!rtip) {
	bu_log("rt_crofton: rt_i_create() failed\n");
	db_close(dbip);
	return -1;
    }

    if (rt_gettree(rtip, scratch) < 0) {
	bu_log("rt_crofton: rt_gettree() failed for '%s'\n", scratch);
	rt_i_destroy(rtip);
	db_close(dbip);
	return -1;
    }

    rt_prep_parallel(rtip, 1);

    /* ---- Run Crofton estimator ---- */
    double sa  = 0.0;
    double vol = 0.0;
    (void)rt_crofton_shoot(&sa, &vol, NULL, NULL, NULL, NULL, NULL,
	rtip, params, NULL, NULL);

    if (out_sa)  *out_sa  = sa;
    if (out_vol) *out_vol = vol;

    /* ---- Clean up ---- */
    rt_i_destroy(rtip);
    /* wdb_dbopen for INMEM returns an embedded pointer inside dbip;
     * do NOT call wdb_close() here, as that would double-free dbip. */
    db_close(dbip);

    return 0;
}


/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/**
 * Cauchy-Crofton estimator with configurable stopping criteria.
 * See struct rt_crofton_params in func.h for full documentation.
 */
void
rt_crofton_sample(fastf_t *area, fastf_t *vol,
		  const struct rt_db_internal *ip,
		  const struct rt_crofton_params *params)
{
    if ((!area && !vol) || !ip)
	return;

    double sa = 0.0, v = 0.0;
    if (crofton_from_ip_n(ip, area ? &sa : NULL, vol ? &v : NULL, params) < 0) {
	sa = 0.0;
	v  = 0.0;
    }

    if (area) *area = (fastf_t)sa;
    if (vol)  *vol  = (fastf_t)v;
}


/* ------------------------------------------------------------------ */
/* Functab callbacks — internal to librt, not exported                 */
/*                                                                      */
/* ft_surf_area / ft_volume require a fixed two-argument signature, so  */
/* each variant below is a minimal wrapper around rt_crofton_sample().  */
/*                                                                      */
/* Default (2 000 rays): BREP, DSP, BSPLINE, HF — where the raytrace   */
/* can be expensive and interactive speed matters more than precision.  */
/*                                                                      */
/* Implicit (50 000 rays): ARS, EBM, METABALL, EXTRUDE, REVOLVE, HRT   */
/* — simple implicit primitives where the extra rays are essentially    */
/* free yet bring typical error well under 2 %.                        */
/* ------------------------------------------------------------------ */

static const struct rt_crofton_params s_default_params  = { 0u,                        0.0, 0.0 };
static const struct rt_crofton_params s_implicit_params = { RT_CROFTON_IMPLICIT_SAMPLES, 0.0, 0.0 };

extern "C" {

RT_EXPORT void
rt_crofton_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    rt_crofton_sample(area, NULL, ip, &s_default_params);
}

RT_EXPORT void
rt_crofton_volume(fastf_t *vol, const struct rt_db_internal *ip)
{
    rt_crofton_sample(NULL, vol, ip, &s_default_params);
}

RT_EXPORT void
rt_crofton_surf_area_implicit(fastf_t *area, const struct rt_db_internal *ip)
{
    rt_crofton_sample(area, NULL, ip, &s_implicit_params);
}

RT_EXPORT void
rt_crofton_volume_implicit(fastf_t *vol, const struct rt_db_internal *ip)
{
    rt_crofton_sample(NULL, vol, ip, &s_implicit_params);
}

} /* extern "C" */


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
