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
 * ray-sampling and deterministic comparison algorithms.
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
 * Chord endpoints are independent pseudorandom points on the bounding
 * sphere.  Reinitializing the generator with a fixed seed makes sampling
 * reproducible without changing the ordinary Monte Carlo convergence
 * behavior on which the estimator's stability checks rely.
 */

#include "common.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <mutex>
#include <random>
#include <stdlib.h>
#include <math.h>
#include <stdexcept>
#include <string.h>
#include <thread>
#include <time.h>
#include <utility>
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
#define RT_CROFTON_RNG_SEED UINT64_C(0x9e3779b97f4a7c15)

/** Two sphere coordinates for each of two chord endpoints. */
#define RT_CROFTON_RANDOM_VALUES_PER_RAY 4u

/** Convert the high 53 random bits exactly to a double in [0, 1). */
static constexpr unsigned int CROFTON_RANDOM_SHIFT = 11u;
static constexpr double CROFTON_RANDOM_SCALE =
    1.0 / 9007199254740992.0;

/* ------------------------------------------------------------------ */
/* Internal types                                                       */
/* ------------------------------------------------------------------ */

struct crofton_ray {
    point_t r_pt;
    vect_t  r_dir;
    size_t  id;
};

struct crofton_shared {
    /* Accumulated across all rays/threads */
    size_t  total_crossings; /* in+out hit events */
    double  total_chord;     /* solid segment length sum (mm) */
    size_t  total_rays;      /* rays fired (hits + misses) */
    size_t  ray_offset;      /* first identifier in this sample stream */
    point_t *points;
    size_t point_count;
    size_t point_capacity;
    struct rt_crofton_segment *segments;
    size_t segment_count;
    size_t segment_capacity;
    bool collect_points;
    bool collect_segments;
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
    struct rt_crofton_segment *local_segments;
    size_t                 local_segment_count;
    size_t                 local_segment_capacity;
    size_t                 current_ray;
};


/* ------------------------------------------------------------------ */
/* Hit / miss callbacks                                                 */
/* ------------------------------------------------------------------ */

static int
crofton_hit(struct application *ap, struct partition *PartHeadp,
    struct seg *UNUSED(segs))
{
    struct crofton_worker_data *worker =
        (struct crofton_worker_data *)ap->a_uptr;
    size_t crossings = 0;
    double chord = 0.0;

    struct partition *partition;
    for (partition = PartHeadp->pt_forw; partition != PartHeadp;
         partition = partition->pt_forw) {
        fastf_t thickness = partition->pt_outhit->hit_dist -
            partition->pt_inhit->hit_dist;
        crossings += 2;
        chord += thickness;

        if (worker->shared->collect_points) {
            if (worker->local_point_count + 2 >
                worker->local_point_capacity) {
                const size_t initial_capacity = 1024;
                size_t new_capacity = worker->local_point_capacity ?
                    2 * worker->local_point_capacity : initial_capacity;
                while (new_capacity < worker->local_point_count + 2)
                    new_capacity *= 2;
                worker->local_points = (point_t *)bu_realloc(
                    worker->local_points,
                    new_capacity * sizeof(point_t),
                    "Crofton worker hit points");
                worker->local_point_capacity = new_capacity;
            }
            VJOIN1(worker->local_points[worker->local_point_count],
                ap->a_ray.r_pt, partition->pt_inhit->hit_dist,
                ap->a_ray.r_dir);
            worker->local_point_count++;
            VJOIN1(worker->local_points[worker->local_point_count],
                ap->a_ray.r_pt, partition->pt_outhit->hit_dist,
                ap->a_ray.r_dir);
            worker->local_point_count++;
        }

        if (worker->shared->collect_segments) {
            if (worker->local_segment_count + 1 >
                worker->local_segment_capacity) {
                const size_t initial_capacity = 512;
                size_t new_capacity = worker->local_segment_capacity ?
                    2 * worker->local_segment_capacity : initial_capacity;
                worker->local_segments =
                    (struct rt_crofton_segment *)bu_realloc(
                        worker->local_segments,
                        new_capacity * sizeof(struct rt_crofton_segment),
                        "Crofton worker segments");
                worker->local_segment_capacity = new_capacity;
            }

            struct rt_crofton_segment *sample =
                &worker->local_segments[worker->local_segment_count];
            struct soltab *in_solid =
                partition->pt_inseg->seg_stp;
            struct soltab *out_solid =
                partition->pt_outseg->seg_stp;
            VJOIN1(sample->in_point, ap->a_ray.r_pt,
                partition->pt_inhit->hit_dist, ap->a_ray.r_dir);
            VJOIN1(sample->out_point, ap->a_ray.r_pt,
                partition->pt_outhit->hit_dist, ap->a_ray.r_dir);
            RT_HIT_NORMAL(sample->in_normal, partition->pt_inhit,
                in_solid, &ap->a_ray, partition->pt_inflip);
            RT_HIT_NORMAL(sample->out_normal, partition->pt_outhit,
                out_solid, &ap->a_ray, partition->pt_outflip);
            sample->thickness = thickness;
            sample->ray_id = worker->rays[worker->current_ray].id;
            worker->local_segment_count++;
        }
    }

    worker->local_crossings += crossings;
    worker->local_chord += chord;
    worker->local_rays++;
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
crofton_random01(std::mt19937_64 &rng)
{
    return static_cast<double>(rng() >> CROFTON_RANDOM_SHIFT) *
        CROFTON_RANDOM_SCALE;
}

static void
random_point_on_sphere(double radius, const point_t center, double u,
    double v, point_t point)
{
    double z = 1.0 - 2.0 * u;
    double radial = sqrt(std::max(0.0, 1.0 - z * z));
    double angle = 2.0 * M_PI * v;
    point[X] = center[X] + radius * radial * cos(angle);
    point[Y] = center[Y] + radius * radial * sin(angle);
    point[Z] = center[Z] + radius * z;
}

/**
 * Generate independent random chord endpoints.  Pairing consecutive IID
 * sphere points has the same distribution as shuffling a point array before
 * pairing, but consumes exactly four random values per ray.  That fixed
 * stride lets callers continue the deterministic stream with ray_offset.
 */
static void
generate_rays(struct crofton_ray *rays, size_t ray_count, size_t first_id,
    double radius, const point_t center, std::mt19937_64 &rng)
{
    for (size_t i = 0; i < ray_count; i++) {
        point_t endpoint;
        random_point_on_sphere(radius, center, crofton_random01(rng),
            crofton_random01(rng), rays[i].r_pt);
        random_point_on_sphere(radius, center, crofton_random01(rng),
            crofton_random01(rng), endpoint);
        VSUB2(rays[i].r_dir, endpoint, rays[i].r_pt);
        VUNITIZE(rays[i].r_dir);
        rays[i].id = first_id + i;
    }
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
		wd.local_segment_count = 0;
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
		    wd->current_ray = i;
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
		if (wd.local_segments)
		    bu_free(wd.local_segments, "Crofton worker segments");
		wd.local_points = NULL;
		wd.local_segments = NULL;
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
do_one_iteration(CroftonWorkerPool &pool, size_t ray_count, double radius,
    const point_t center, struct crofton_shared *shared,
    std::mt19937_64 &rng)
{
    struct crofton_ray *rays = (struct crofton_ray *)bu_calloc(
        ray_count, sizeof(struct crofton_ray), "Crofton rays");
    generate_rays(rays, ray_count, shared->ray_offset + shared->total_rays,
        radius, center, rng);
    pool.run(rays, ray_count, shared);

    for (const struct crofton_worker_data &worker : pool.worker_data()) {
        shared->total_crossings += worker.local_crossings;
        shared->total_chord += worker.local_chord;
        shared->total_rays += worker.local_rays;

        if (worker.local_point_count) {
            const size_t needed =
                shared->point_count + worker.local_point_count;
            if (needed > shared->point_capacity) {
                size_t new_capacity = shared->point_capacity ?
                    2 * shared->point_capacity : needed;
                if (new_capacity < needed)
                    new_capacity = needed;
                shared->points = (point_t *)bu_realloc(shared->points,
                    new_capacity * sizeof(point_t),
                    "Crofton hit points");
                shared->point_capacity = new_capacity;
            }
            memcpy(&shared->points[shared->point_count],
                worker.local_points,
                worker.local_point_count * sizeof(point_t));
            shared->point_count = needed;
        }

        if (worker.local_segment_count) {
            const size_t needed =
                shared->segment_count + worker.local_segment_count;
            if (needed > shared->segment_capacity) {
                size_t new_capacity = shared->segment_capacity ?
                    2 * shared->segment_capacity : needed;
                if (new_capacity < needed)
                    new_capacity = needed;
                shared->segments =
                    (struct rt_crofton_segment *)bu_realloc(
                        shared->segments,
                        new_capacity * sizeof(struct rt_crofton_segment),
                        "Crofton segments");
                shared->segment_capacity = new_capacity;
            }
            memcpy(&shared->segments[shared->segment_count],
                worker.local_segments,
                worker.local_segment_count *
                    sizeof(struct rt_crofton_segment));
            shared->segment_count = needed;
        }
    }

    bu_free(rays, "Crofton rays");
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
crofton_shoot_impl(struct rt_crofton_result       *out_result,
                   point_t                        **out_points,
		   size_t                          *out_point_count,
		   double                          *out_surf_area,
		   double                          *out_volume,
		   point_t                         *out_aabb_min,
		   point_t                         *out_aabb_max,
		   point_t                          out_obb[8],
		 struct rt_i                    *rtip,
		 const struct rt_crofton_params *params,
		 size_t                         ray_offset,
		 const fastf_t                  *bbox_min,
		 const fastf_t                  *bbox_max)
{
    if (!rtip ||
        (!out_result && !out_surf_area && !out_volume && !out_aabb_min &&
            !out_obb && !out_points) ||
        ((out_points == NULL) != (out_point_count == NULL)) ||
        ((out_aabb_min == NULL) != (out_aabb_max == NULL)) ||
        (out_result && out_result->segments) ||
        ray_offset > UINT64_MAX / RT_CROFTON_RANDOM_VALUES_PER_RAY)
        return -1;
    if (out_result) {
        struct rt_crofton_result empty = RT_CROFTON_RESULT_INIT;
        *out_result = empty;
    }
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
    shared.ray_offset = ray_offset;
    shared.collect_points =
        out_points != NULL || out_aabb_min != NULL || out_obb != NULL;
    shared.collect_segments = out_result != NULL;
    const bool stabilize_surface =
        out_surf_area != NULL || out_result != NULL ||
        shared.collect_points;

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
    rng.discard(static_cast<uint64_t>(ray_offset) *
        RT_CROFTON_RANDOM_VALUES_PER_RAY);

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
	double prev_sa = -1.0, prev_v = -1.0;
	size_t total_fired = 0;
	size_t stable_windows = 0;
	int64_t t0 = (time_ms > 0.0) ? bu_gettime() : 0;
	const bool use_relative_stability = out_result && time_ms > 0.0 &&
	    stability_mm <= 0.0;

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
	    if ((stability_mm > 0.0 || use_relative_stability) &&
		shared.total_rays >= RT_CROFTON_STABILITY_MIN_RAYS &&
		shared.total_crossings >= RT_CROFTON_STABILITY_MIN_CROSSINGS) {
		bool sa_ok = false;
		bool v_ok = false;
		if (use_relative_stability) {
		    sa_ok = !stabilize_surface || (prev_sa > 0.0 &&
			fabs(curr_est_sa - prev_sa) / prev_sa * 100.0 <=
			RT_CROFTON_DEFAULT_THRESHOLD);
		    v_ok = !out_volume || (prev_v > 0.0 &&
			fabs(curr_est_v - prev_v) / prev_v * 100.0 <=
			RT_CROFTON_DEFAULT_THRESHOLD);
		    prev_sa = curr_est_sa;
		    prev_v = curr_est_v;
		} else {
		    double r_sa = (curr_est_sa > 0.0) ?
			sqrt(curr_est_sa * INV_4PI) : 0.0;
		    double r_v = (curr_est_v > 0.0) ?
			cbrt(curr_est_v * INV_4PI3) : 0.0;
		    sa_ok = !stabilize_surface || (prev_r_sa >= 0.0 &&
			fabs(r_sa - prev_r_sa) < stability_mm);
		    v_ok = !out_volume || (prev_r_v >= 0.0 &&
			fabs(r_v - prev_r_v) < stability_mm);
		    prev_r_sa = r_sa;
		    prev_r_v = r_v;
		}
		if (sa_ok && v_ok) {
		    stable_windows++;
		    if (stable_windows >= RT_CROFTON_STABILITY_MIN_WINDOWS)
			break;
		} else {
		    stable_windows = 0;
		}
	    }

	    /* Time-budget check after firing */
	    if (time_ms > 0.0 &&
		(bu_gettime() - t0) / 1000.0 >= time_ms)
		break;
	}
    }

    if (out_surf_area)
        *out_surf_area = curr_est_sa;
    if (out_volume)
        *out_volume = curr_est_v;
    if (out_result) {
        out_result->surface_area = curr_est_sa;
        out_result->volume = curr_est_v;
        out_result->ray_count = shared.total_rays;
        out_result->crossing_count = shared.total_crossings;
    }
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
    if (out_result && !bounds_ret) {
        out_result->segments = shared.segments;
        out_result->segment_count = shared.segment_count;
    } else if (shared.segments) {
        bu_free(shared.segments, "Crofton segments");
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
    return crofton_shoot_impl(NULL, out_points, out_point_count,
        out_surf_area, out_volume, out_aabb_min, out_aabb_max, out_obb,
        rtip, params, 0, bbox_min, bbox_max);
}

int
rt_crofton_collect(struct rt_crofton_result *result, struct rt_i *rtip,
    const struct rt_crofton_params *params, size_t ray_offset,
    const fastf_t *bbox_min, const fastf_t *bbox_max)
{
    if (!result)
        return -1;
    return crofton_shoot_impl(result, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, rtip, params, ray_offset, bbox_min, bbox_max);
}

void
rt_crofton_result_free(struct rt_crofton_result *result)
{
    if (!result)
        return;
    if (result->segments)
        bu_free(result->segments, "Crofton segments");
    struct rt_crofton_result empty = RT_CROFTON_RESULT_INIT;
    *result = empty;
}


namespace {

using CroftonSegmentList =
    std::vector<const struct rt_crofton_segment *>;
using CroftonSegmentMap = std::map<size_t, CroftonSegmentList>;

struct CroftonBoundary {
    fastf_t distance;
    const fastf_t *point;
    const fastf_t *normal;
};

struct CroftonInterval {
    CroftonBoundary entry;
    CroftonBoundary exit;
};

constexpr fastf_t CROFTON_COMPARE_TANGENT_DOT_LIMIT = 0.05;
/* This is a floating-point cancellation guard, not a modeling tolerance. */
constexpr fastf_t CROFTON_COMPARE_ROUNDOFF_ULPS = 64.0;


static bool
crofton_prepared_bounds(point_t minimum, point_t maximum,
    struct rt_i *rtip)
{
    VSETALL(minimum, MAX_FASTF);
    VSETALL(maximum, -MAX_FASTF);
    struct soltab *solid;
    RT_VISIT_ALL_SOLTABS_START(solid, rtip) {
	VMIN(minimum, solid->st_min);
	VMAX(maximum, solid->st_max);
    } RT_VISIT_ALL_SOLTABS_END;
    return minimum[X] < MAX_FASTF && minimum[X] <= maximum[X] &&
	minimum[Y] <= maximum[Y] && minimum[Z] <= maximum[Z];
}


static double
crofton_relative_error(double source, double candidate)
{
    if (source <= SMALL_FASTF) {
	return candidate <= SMALL_FASTF ? 0.0 :
	    std::numeric_limits<double>::infinity();
    }
    return std::fabs(candidate - source) / source;
}


static double
crofton_percentile(std::vector<fastf_t> &values, double fraction)
{
    if (values.empty())
	return 0.0;
    std::sort(values.begin(), values.end());
    size_t index = static_cast<size_t>(
	std::ceil(fraction * static_cast<double>(values.size())));
    index = index ? index - 1 : 0;
    return values[std::min(index, values.size() - 1)];
}


static void
crofton_map_segments(CroftonSegmentMap &mapped,
    const struct rt_crofton_result &result)
{
    for (size_t i = 0; i < result.segment_count; ++i)
	mapped[result.segments[i].ray_id].push_back(&result.segments[i]);
}


static fastf_t
crofton_segment_roundoff_tolerance(const struct rt_crofton_segment &segment)
{
    fastf_t coordinate_scale = 1.0;
    for (size_t coordinate = 0; coordinate < 3; ++coordinate) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(segment.in_point[coordinate]));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(segment.out_point[coordinate]));
    }
    return CROFTON_COMPARE_ROUNDOFF_ULPS *
	std::numeric_limits<fastf_t>::epsilon() * coordinate_scale;
}


static bool
crofton_segment_is_numerically_degenerate(
    const struct rt_crofton_segment &segment, fastf_t *measured_length = NULL)
{
    const fastf_t length = DIST_PNT_PNT(segment.in_point,
	segment.out_point);
    if (measured_length != NULL)
	*measured_length = length;
    return std::isfinite(length) &&
	length <= crofton_segment_roundoff_tolerance(segment);
}


static bool
crofton_ray_direction(vect_t direction,
    const CroftonSegmentList &source,
    const CroftonSegmentList &candidate)
{
    const CroftonSegmentList *lists[] = {&source, &candidate};
    for (const CroftonSegmentList *segments : lists) {
	for (const struct rt_crofton_segment *segment : *segments) {
	    if (crofton_segment_is_numerically_degenerate(*segment))
		continue;
	    VSUB2(direction, segment->out_point, segment->in_point);
	    if (!VNEAR_ZERO(direction, VUNITIZE_TOL)) {
		VUNITIZE(direction);
		return true;
	    }
	}
    }
    VSET(direction, 1.0, 0.0, 0.0);
    return false;
}


static std::vector<CroftonInterval>
crofton_merged_intervals(const CroftonSegmentList &segments,
    const vect_t direction, fastf_t tolerance,
    size_t &numerically_degenerate_count,
    double &maximum_numerically_degenerate_length)
{
    std::vector<CroftonInterval> intervals;
    intervals.reserve(segments.size());
    for (const struct rt_crofton_segment *segment : segments) {
	fastf_t segment_length = 0.0;
	if (crofton_segment_is_numerically_degenerate(*segment,
		&segment_length))
	{
	    ++numerically_degenerate_count;
	    maximum_numerically_degenerate_length = std::max(
		maximum_numerically_degenerate_length,
		static_cast<double>(segment_length));
	    continue;
	}
	CroftonInterval interval = {
	    {VDOT(segment->in_point, direction), segment->in_point,
		segment->in_normal},
	    {VDOT(segment->out_point, direction), segment->out_point,
		segment->out_normal}
	};
	if (interval.entry.distance > interval.exit.distance)
	    std::swap(interval.entry, interval.exit);
	intervals.push_back(interval);
    }
    std::sort(intervals.begin(), intervals.end(),
	[](const CroftonInterval &left, const CroftonInterval &right) {
	    return left.entry.distance < right.entry.distance;
	});

    std::vector<CroftonInterval> merged;
    for (const CroftonInterval &interval : intervals) {
	if (merged.empty() || interval.entry.distance >
		merged.back().exit.distance + tolerance)
	{
	    merged.push_back(interval);
	} else if (interval.exit.distance > merged.back().exit.distance) {
	    merged.back().exit = interval.exit;
	}
    }
    return merged;
}


static double
crofton_interval_length(const std::vector<CroftonInterval> &intervals)
{
    double length = 0.0;
    for (const CroftonInterval &interval : intervals)
	length += interval.exit.distance - interval.entry.distance;
    return length;
}


static double
crofton_interval_disagreement(const std::vector<CroftonInterval> &source,
    const std::vector<CroftonInterval> &candidate,
    const vect_t direction, double &maximum_span,
    double &maximum_normal_separation)
{
    struct BoundaryEvent {
	fastf_t distance;
	int source_delta;
	int candidate_delta;
	const fastf_t *normal;
    };
    std::vector<BoundaryEvent> events;
    events.reserve(2 * (source.size() + candidate.size()));
    for (const CroftonInterval &interval : source) {
	events.push_back({interval.entry.distance, 1, 0,
	    interval.entry.normal});
	events.push_back({interval.exit.distance, -1, 0,
	    interval.exit.normal});
    }
    for (const CroftonInterval &interval : candidate) {
	events.push_back({interval.entry.distance, 0, 1,
	    interval.entry.normal});
	events.push_back({interval.exit.distance, 0, -1,
	    interval.exit.normal});
    }
    std::sort(events.begin(), events.end(),
	[](const BoundaryEvent &left, const BoundaryEvent &right) {
	    return left.distance < right.distance;
	});

    int source_depth = 0;
    int candidate_depth = 0;
    fastf_t previous = events.empty() ? 0.0 : events.front().distance;
    double previous_incidence = 0.0;
    for (size_t event_index = 0; event_index < events.size();) {
	const fastf_t current = events[event_index].distance;
	const size_t group_begin = event_index;
	double current_incidence = 0.0;
	while (event_index < events.size() &&
	    events[event_index].distance == current)
	{
	    const fastf_t *normal = events[event_index].normal;
	    const double magnitude = normal ? MAGNITUDE(normal) : 0.0;
	    if (magnitude > VUNITIZE_TOL && std::isfinite(magnitude)) {
		current_incidence = std::max(current_incidence,
		    std::fabs(VDOT(normal, direction)) / magnitude);
	    }
	    ++event_index;
	}
	if ((source_depth > 0) != (candidate_depth > 0)) {
	    const double span = current - previous;
	    maximum_span = std::max(maximum_span,
		span);
	    maximum_normal_separation = std::max(
		maximum_normal_separation, span * std::max(previous_incidence,
		    current_incidence));
	}
	for (size_t grouped_index = group_begin;
	     grouped_index < event_index; ++grouped_index)
	{
	    source_depth += events[grouped_index].source_delta;
	    candidate_depth += events[grouped_index].candidate_delta;
	}
	previous = current;
	previous_incidence = current_incidence;
    }

    size_t source_index = 0;
    size_t candidate_index = 0;
    double intersection = 0.0;
    while (source_index < source.size() && candidate_index < candidate.size())
    {
	const CroftonInterval &source_interval = source[source_index];
	const CroftonInterval &candidate_interval = candidate[candidate_index];
	const fastf_t entry = std::max(source_interval.entry.distance,
	    candidate_interval.entry.distance);
	const fastf_t exit = std::min(source_interval.exit.distance,
	    candidate_interval.exit.distance);
	if (exit > entry)
	    intersection += exit - entry;
	if (source_interval.exit.distance < candidate_interval.exit.distance)
	    ++source_index;
	else
	    ++candidate_index;
    }
    return crofton_interval_length(source) + crofton_interval_length(candidate) -
	2.0 * intersection;
}

} // namespace


int
rt_crofton_compare(struct rt_crofton_comparison *comparison,
    struct rt_i *source, struct rt_i *candidate,
    const struct rt_crofton_params *params, size_t ray_offset,
    const fastf_t *bbox_min, const fastf_t *bbox_max)
{
    if (!comparison || !source || !candidate ||
	((bbox_min == NULL) != (bbox_max == NULL)))
    {
	return -1;
    }
    struct rt_crofton_comparison empty = RT_CROFTON_COMPARISON_INIT;
    *comparison = empty;

    point_t common_min;
    point_t common_max;
    if (bbox_min != NULL) {
	VMOVE(common_min, bbox_min);
	VMOVE(common_max, bbox_max);
    } else {
	point_t source_min;
	point_t source_max;
	point_t candidate_min;
	point_t candidate_max;
	if (!crofton_prepared_bounds(source_min, source_max, source) ||
	    !crofton_prepared_bounds(candidate_min, candidate_max, candidate))
	{
	    return -1;
	}
	VMOVE(common_min, source_min);
	VMOVE(common_max, source_max);
	VMIN(common_min, candidate_min);
	VMAX(common_max, candidate_max);
    }
    if (!std::isfinite(common_min[X]) || !std::isfinite(common_min[Y]) ||
	!std::isfinite(common_min[Z]) || !std::isfinite(common_max[X]) ||
	!std::isfinite(common_max[Y]) || !std::isfinite(common_max[Z]) ||
	common_min[X] > common_max[X] || common_min[Y] > common_max[Y] ||
	common_min[Z] > common_max[Z])
    {
	return -1;
    }

    struct rt_crofton_result source_result = RT_CROFTON_RESULT_INIT;
    struct rt_crofton_result candidate_result = RT_CROFTON_RESULT_INIT;
    const int source_status = rt_crofton_collect(&source_result, source,
	params, ray_offset, common_min, common_max);
    const int candidate_status = rt_crofton_collect(&candidate_result,
	candidate, params, ray_offset, common_min, common_max);
    if (source_status < 0 || candidate_status < 0 ||
	source_result.ray_count != candidate_result.ray_count)
    {
	rt_crofton_result_free(&source_result);
	rt_crofton_result_free(&candidate_result);
	return -1;
    }

    comparison->ray_count = source_result.ray_count;
    comparison->source_segment_count = source_result.segment_count;
    comparison->candidate_segment_count = candidate_result.segment_count;

    CroftonSegmentMap source_rays;
    CroftonSegmentMap candidate_rays;
    crofton_map_segments(source_rays, source_result);
    crofton_map_segments(candidate_rays, candidate_result);

    std::vector<size_t> ray_ids;
    ray_ids.reserve(source_rays.size() + candidate_rays.size());
    for (const auto &entry : source_rays)
	ray_ids.push_back(entry.first);
    for (const auto &entry : candidate_rays)
	ray_ids.push_back(entry.first);
    std::sort(ray_ids.begin(), ray_ids.end());
    ray_ids.erase(std::unique(ray_ids.begin(), ray_ids.end()), ray_ids.end());

    std::vector<fastf_t> endpoint_errors;
    size_t source_crossings = 0;
    size_t candidate_crossings = 0;
    double candidate_chord = 0.0;
    const fastf_t interval_tolerance = std::max(source->rti_tol.dist,
	candidate->rti_tol.dist);
    for (size_t ray_id : ray_ids) {
	const CroftonSegmentList &source_segments = source_rays[ray_id];
	const CroftonSegmentList &candidate_segments = candidate_rays[ray_id];

	vect_t direction;
	const bool have_direction = crofton_ray_direction(direction,
	    source_segments, candidate_segments);
	size_t source_degenerate_count = 0;
	size_t candidate_degenerate_count = 0;
	const std::vector<CroftonInterval> source_intervals =
	    crofton_merged_intervals(source_segments, direction,
		interval_tolerance, source_degenerate_count,
		comparison->source_max_numerically_degenerate_length);
	const std::vector<CroftonInterval> candidate_intervals =
	    crofton_merged_intervals(candidate_segments, direction,
		interval_tolerance, candidate_degenerate_count,
		comparison->candidate_max_numerically_degenerate_length);
	comparison->source_numerically_degenerate_segments +=
	    source_degenerate_count;
	comparison->candidate_numerically_degenerate_segments +=
	    candidate_degenerate_count;
	if (source_degenerate_count || candidate_degenerate_count)
	    ++comparison->numerically_degenerate_rays;
	if (!have_direction)
	    continue;
	if (!source_intervals.empty())
	    ++comparison->source_hit_rays;
	if (!candidate_intervals.empty())
	    ++comparison->candidate_hit_rays;
	double ray_maximum_disagreement = 0.0;
	double ray_maximum_normal_disagreement = 0.0;
	const double ray_disagreement = crofton_interval_disagreement(
	    source_intervals, candidate_intervals, direction,
	    ray_maximum_disagreement, ray_maximum_normal_disagreement);
	if (source_intervals.size() != candidate_intervals.size()) {
	    ++comparison->partition_mismatch_rays;
	    comparison->maximum_partition_disagreement = std::max(
		comparison->maximum_partition_disagreement,
		ray_maximum_disagreement);
	    comparison->maximum_normal_partition_disagreement = std::max(
		comparison->maximum_normal_partition_disagreement,
		ray_maximum_normal_disagreement);
	    if (ray_maximum_normal_disagreement <= interval_tolerance)
		++comparison->sub_tolerance_partition_mismatch_rays;
	    else
		++comparison->material_partition_mismatch_rays;
	}
	source_crossings += 2 * source_intervals.size();
	candidate_crossings += 2 * candidate_intervals.size();
	comparison->source_chord += crofton_interval_length(source_intervals);
	candidate_chord += crofton_interval_length(candidate_intervals);
	comparison->chord_disagreement += ray_disagreement;

	if (source_intervals.size() != candidate_intervals.size())
	    continue;
	for (size_t i = 0; i < source_intervals.size(); ++i) {
	    const CroftonInterval &source_interval = source_intervals[i];
	    const CroftonInterval &candidate_interval = candidate_intervals[i];
	    if (std::fabs(VDOT(source_interval.entry.normal, direction)) >=
		    CROFTON_COMPARE_TANGENT_DOT_LIMIT)
	    {
		endpoint_errors.push_back(DIST_PNT_PNT(
		    source_interval.entry.point, candidate_interval.entry.point));
	    }
	    if (std::fabs(VDOT(source_interval.exit.normal, direction)) >=
		    CROFTON_COMPARE_TANGENT_DOT_LIMIT)
	    {
		endpoint_errors.push_back(DIST_PNT_PNT(
		    source_interval.exit.point, candidate_interval.exit.point));
	    }
	}
    }

    vect_t common_diagonal;
    VSUB2(common_diagonal, common_max, common_min);
    const double sampling_radius = 0.5 * MAGNITUDE(common_diagonal);
    if (comparison->ray_count) {
	const double ray_count = static_cast<double>(comparison->ray_count);
	const double disk_area = M_PI * sampling_radius * sampling_radius;
	comparison->source_surface_area = 2.0 * disk_area *
	    static_cast<double>(source_crossings) / ray_count;
	comparison->candidate_surface_area = 2.0 * disk_area *
	    static_cast<double>(candidate_crossings) / ray_count;
	comparison->source_volume = disk_area * comparison->source_chord /
	    ray_count;
	comparison->candidate_volume = disk_area * candidate_chord / ray_count;
    }
    comparison->surface_area_error_fraction = crofton_relative_error(
	comparison->source_surface_area, comparison->candidate_surface_area);
    comparison->volume_error_fraction = crofton_relative_error(
	comparison->source_volume, comparison->candidate_volume);
    comparison->partition_mismatch_fraction = comparison->ray_count ?
	static_cast<double>(comparison->partition_mismatch_rays) /
	    static_cast<double>(comparison->ray_count) : 0.0;
    comparison->chord_error_fraction = comparison->source_chord >
	SMALL_FASTF ? comparison->chord_disagreement /
	comparison->source_chord : (comparison->chord_disagreement <=
	SMALL_FASTF ? 0.0 : std::numeric_limits<double>::infinity());
    comparison->compared_endpoint_count = endpoint_errors.size();
    comparison->endpoint_error_p95 = crofton_percentile(endpoint_errors, 0.95);
    comparison->endpoint_error_p99 = crofton_percentile(endpoint_errors, 0.99);
    if (!endpoint_errors.empty()) {
	comparison->endpoint_error_max = *std::max_element(
	    endpoint_errors.begin(), endpoint_errors.end());
    }

    rt_crofton_result_free(&source_result);
    rt_crofton_result_free(&candidate_result);
    return 0;
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
