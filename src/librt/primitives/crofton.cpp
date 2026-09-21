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
 * ray-sampling algorithm.  Its public API provides:
 *
 *   - an estimator for prepared raytrace instances;
 *   - bounded-memory segment and complete-ray visitors;
 *   - reusable parallel worker sessions for repeated samples; and
 *   - surface-area and volume fallbacks for primitive functabs.
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
 * Correctness requires uniform coverage of the invariant measure on
 * oriented lines intersecting the bounding sphere: directions are
 * isotropic and offsets perpendicular to each direction are uniform by
 * disk area.  The legacy PRNG obtains that measure by joining two
 * independent uniform sphere points.
 *
 * When BRLCAD_ENABLE_QMC is enabled, an OpenQMC randomized rank-one
 * lattice samples the line's four natural coordinates directly.  This
 * improves finite-sample coverage without changing the target measure.
 * The independent-PRNG implementation remains intact as a runtime
 * compatibility path and an independent check on QMC correlation.
 */

#include "common.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <random>
#include <stdlib.h>
#include <math.h>
#include <stdexcept>
#include <string.h>
#include <thread>
#include <time.h>
#include <vector>

#ifdef BRLCAD_ENABLE_QMC
#  include <oqmc/lattice.h>
#endif

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/datetime.h"
#include "bu/str.h"
#include "bg/obr.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/overlap.h"


/* ------------------------------------------------------------------ */
/* Default parameters for the functab fallbacks                        */
/* ------------------------------------------------------------------ */

/** Minimum rays per iteration when used as a generic functab fallback.
 *  Kept small so the fallback is fast for interactive use; callers
 *  that need higher accuracy should call rt_crofton_sample() with
 *  appropriate params.                                                */
#define RT_CROFTON_DEFAULT_SAMPLES   2000u
#define RT_CROFTON_VISIT_BATCH_RAYS 8192u
#define RT_CROFTON_FOCUSED_BACKOUT_SCALE 2.0

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

/** An oriented line has two direction and two perpendicular-offset values. */
static constexpr int CROFTON_LINE_DIMENSIONS = 4;

/** Convert the high 53 random bits exactly to a double in [0, 1). */
static constexpr unsigned int CROFTON_RANDOM_SHIFT = 11u;
static constexpr double CROFTON_RANDOM_SCALE =
    1.0 / 9007199254740992.0;

#ifdef BRLCAD_ENABLE_QMC
/** Keep the reference axis safely separated from the sampled direction. */
static constexpr double CROFTON_BASIS_REFERENCE_LIMIT = 0.9;

/** Crofton uses one OpenQMC domain rather than image pixel domains. */
static constexpr int CROFTON_QMC_DOMAIN_COORDINATE = 0;

/** Number of nonnegative values representable by OpenQMC's int frame. */
static constexpr uint64_t CROFTON_QMC_FRAME_COUNT =
    static_cast<uint64_t>(INT_MAX) + 1u;

/** Convert every uint32_t value uniformly and exactly into [0, 1). */
static constexpr double CROFTON_QMC_SCALE =
    1.0 / (static_cast<double>(UINT32_MAX) + 1.0);

static const char CROFTON_PRNG_ENV[] = "LIBRT_CROFTON_USE_PRNG";
#endif

static uint64_t
crofton_mix_seed(uint64_t value)
{
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

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
    size_t  invalid_partition_count;
    size_t  ray_offset;      /* first identifier in this sample stream */
    point_t *points;
    size_t point_count;
    size_t point_capacity;
    struct rt_crofton_segment *segments;
    size_t segment_count;
    size_t segment_capacity;
    rt_crofton_segment_fn visitor;
    rt_crofton_ray_fn ray_visitor;
    rt_crofton_overlap_fn overlap_visitor;
    rt_crofton_invalid_fn invalid_visitor;
    void *visitor_data;
    bool collect_points;
    bool collect_segments;
};

struct crofton_worker_data {
    struct application     ap = {};        /* per-CPU application struct */
    struct crofton_ray    *rays = NULL;    /* shared ray array (read-only) */
    size_t                 start = 0;
    size_t                 end = 0;
    struct crofton_shared *shared = NULL;
    size_t                 local_crossings = 0;
    double                 local_chord = 0.0;
    size_t                 local_rays = 0;
    size_t                 local_invalid_partitions = 0;
    std::vector<struct rt_crofton_invalid_ray> local_invalid;
    std::vector<struct rt_crofton_overlap> local_overlaps;
    point_t               *local_points = NULL;
    size_t                 local_point_count = 0;
    size_t                 local_point_capacity = 0;
    struct rt_crofton_segment *local_segments = NULL;
    size_t                 local_segment_count = 0;
    size_t                 local_segment_capacity = 0;
    size_t                 current_ray = 0;
};


/* ------------------------------------------------------------------ */
/* Hit / miss callbacks                                                 */
/* ------------------------------------------------------------------ */

static int
crofton_overlap(struct application *ap, struct partition *pp,
    struct region *first, struct region *second, struct partition *head)
{
    struct crofton_worker_data *worker =
        (struct crofton_worker_data *)ap->a_uptr;
    if (worker->shared->overlap_visitor && pp && first && second && head) {
        const fastf_t depth = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
        if (!std::isfinite(depth) || depth <= 0.0)
            return rt_defoverlap(ap, pp, first, second, head);
        struct rt_crofton_overlap overlap;
        overlap.ray_id = worker->rays[worker->current_ray].id;
        VMOVE(overlap.origin, ap->a_ray.r_pt);
        VMOVE(overlap.direction, ap->a_ray.r_dir);
        overlap.depth = depth;
        VJOIN1(overlap.in_point, ap->a_ray.r_pt,
            pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
        VJOIN1(overlap.out_point, ap->a_ray.r_pt,
            pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
        overlap.first_region = first;
        overlap.second_region = second;
        worker->local_overlaps.push_back(overlap);
    }
    return rt_defoverlap(ap, pp, first, second, head);
}

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
        if (!std::isfinite(partition->pt_inhit->hit_dist) ||
            !std::isfinite(partition->pt_outhit->hit_dist) ||
            !std::isfinite(thickness) || thickness <= 0.0) {
            worker->local_invalid_partitions++;
            if (worker->shared->invalid_visitor) {
                struct rt_crofton_invalid_ray invalid;
                invalid.ray_id = worker->rays[worker->current_ray].id;
                VMOVE(invalid.origin, ap->a_ray.r_pt);
                VMOVE(invalid.direction, ap->a_ray.r_dir);
                invalid.in_distance = partition->pt_inhit->hit_dist;
                invalid.out_distance = partition->pt_outhit->hit_dist;
                invalid.thickness = thickness;
                invalid.region = partition->pt_regionp;
                invalid.reason = !std::isfinite(thickness) ?
                    "non-finite-thickness" : "non-positive-thickness";
                worker->local_invalid.push_back(invalid);
            }
            continue;
        }
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
            VMOVE(sample->ray_origin, ap->a_ray.r_pt);
            VMOVE(sample->ray_direction, ap->a_ray.r_dir);
            sample->in_distance = partition->pt_inhit->hit_dist;
            sample->out_distance = partition->pt_outhit->hit_dist;
            VJOIN1(sample->in_point, ap->a_ray.r_pt,
                sample->in_distance, ap->a_ray.r_dir);
            VJOIN1(sample->out_point, ap->a_ray.r_pt,
                sample->out_distance, ap->a_ray.r_dir);
            RT_HIT_NORMAL(sample->in_normal, partition->pt_inhit,
                in_solid, &ap->a_ray, partition->pt_inflip);
            RT_HIT_NORMAL(sample->out_normal, partition->pt_outhit,
                out_solid, &ap->a_ray, partition->pt_outflip);
            sample->thickness = thickness;
            sample->ray_id = worker->rays[worker->current_ray].id;
            sample->region = partition->pt_regionp;
            sample->in_solid = in_solid;
            sample->out_solid = out_solid;
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
 * Generate the legacy independent-pseudorandom endpoint chords.  For two
 * uniform sphere endpoints, rotational invariance makes the line direction
 * isotropic.  If alpha is their central angle, cos(alpha) is uniform and the
 * squared distance of their line from the center is
 * R^2(1 + cos(alpha))/2.  The offset is therefore uniform by disk area, as
 * required by the Crofton line measure.
 *
 * Keep this implementation unchanged as the compatibility fallback.  Four
 * PRNG values per ray give ray_offset a fixed deterministic stream stride.
 */
static void
generate_prng_rays(struct crofton_ray *rays, size_t ray_count, size_t first_id,
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

/**
 * Return whether this process should use the QMC generator.  Sampler choice is
 * intentionally not part of rt_crofton_params: that public structure describes
 * accuracy and resource limits, whereas the two generators implement the same
 * mathematical measure.  Keeping the compatibility control in the
 * environment also preserves the historical default API.  Set
 * LIBRT_CROFTON_USE_PRNG to any value accepted by bu_str_true() to recover
 * the original generator exactly.
 */
#ifdef BRLCAD_ENABLE_QMC
static bool
use_qmc_sampler()
{
    return !bu_str_true(getenv(CROFTON_PRNG_ENV));
}
#endif

#ifdef BRLCAD_ENABLE_QMC
/**
 * Construct an oriented line from four uniform coordinates.
 *
 * A line intersecting a sphere has four degrees of freedom.  Its unit
 * direction d contributes two; its closest point p to the sphere center
 * contributes two more because (p-center) is perpendicular to d.  The
 * invariant Crofton measure factorizes as:
 *
 *     uniform solid angle for d  *  uniform disk area for p
 *
 * sample[0:1] map to a uniform sphere direction by making cos(theta), not
 * theta, uniform.  sample[2:3] map to the perpendicular disk.  In particular,
 * offset_radius = R*sqrt(sample[2]) is essential: the probability inside
 * radius r is then r^2/R^2, exactly the fraction of disk area inside r.
 * Sampling the radius linearly would put too many lines near the center and
 * bias both surface-area and volume estimates.
 *
 * The arbitrary perpendicular basis cannot change the distribution because
 * its disk angle is uniform.  Switching reference axes when d approaches Z
 * avoids a nearly zero cross product.  Finally, moving backward from p by the
 * sphere half-chord places the ray origin on the near side of the bounding
 * sphere, matching the numerical setup used by the endpoint generator.
 *
 * This is a reparameterization, not a different integral.  For the legacy
 * endpoint construction, rotational invariance makes d isotropic and the
 * squared center-to-line offset is uniform.  Those are precisely the two
 * distributions constructed directly here.
 */
static void
qmc_line_from_sample(struct crofton_ray *ray, double radius,
    const point_t center, const double sample[CROFTON_LINE_DIMENSIONS])
{
    const double direction_z = 1.0 - 2.0 * sample[0];
    const double direction_radius =
        sqrt(std::max(0.0, 1.0 - direction_z * direction_z));
    const double direction_angle = 2.0 * M_PI * sample[1];
    vect_t direction;
    VSET(direction, direction_radius * cos(direction_angle),
        direction_radius * sin(direction_angle), direction_z);

    vect_t reference;
    if (fabs(direction[Z]) < CROFTON_BASIS_REFERENCE_LIMIT)
        VSET(reference, 0.0, 0.0, 1.0);
    else
        VSET(reference, 0.0, 1.0, 0.0);

    vect_t basis_u;
    vect_t basis_v;
    VCROSS(basis_u, reference, direction);
    VUNITIZE(basis_u);
    VCROSS(basis_v, direction, basis_u);

    const double offset_radius = radius * sqrt(sample[2]);
    const double offset_angle = 2.0 * M_PI * sample[3];
    point_t closest_point;
    VJOIN2(closest_point, center,
        offset_radius * cos(offset_angle), basis_u,
        offset_radius * sin(offset_angle), basis_v);

    const double half_chord = sqrt(std::max(0.0,
        radius * radius - offset_radius * offset_radius));
    VJOIN1(ray->r_pt, closest_point, -half_chord, direction);
    VMOVE(ray->r_dir, direction);
}

/**
 * Generate direct-line samples with OpenQMC's LatticeSampler.
 *
 * OpenQMC uses the rank-one generator from Hickernell et al., "Weighted
 * compound integration rules with higher order convergence for all N", makes
 * it progressive by bit-reversing and shuffling the index, and randomizes it
 * with per-dimension toroidal shifts.  The lattice's even coverage reduces
 * finite-sample clumping and gaps compared with independent PRNG points.  The
 * shifts remove fixed alignment with the origin and give uniform coordinate
 * marginals over the randomization ensemble; they do not make samples IID.
 * A deterministic frame schedule therefore gives reproducible randomized-QMC
 * estimates, while the PRNG override remains available to check an unusual
 * geometry for a lattice-correlation artifact.
 *
 * OpenQMC stores a 16-bit local sample index.  Its normal high-index behavior
 * changes the randomization pattern for each 65,536-sample block.  Mapping the
 * global Crofton ray id to (frame, local index) preserves that behavior across
 * rt_crofton_collect() continuations rather than restarting the lattice.  The
 * frame modulo can repeat only after 2^47 rays, beyond a realizable Crofton
 * run, and avoids implementation-defined conversion to a signed int.
 */
static void
generate_qmc_rays(struct crofton_ray *rays, size_t ray_count, size_t first_id,
    double radius, const point_t center, uint64_t frame_offset)
{
    const uint64_t block_size =
        static_cast<uint64_t>(oqmc::State64Bit::maxIndexSize);

    for (size_t i = 0; i < ray_count; i++) {
        const uint64_t ray_id = static_cast<uint64_t>(first_id) + i;
        const uint64_t block_id = ray_id / block_size;
        const int frame = static_cast<int>(
            (frame_offset + block_id) % CROFTON_QMC_FRAME_COUNT);
        const int local_index = static_cast<int>(ray_id % block_size);
        const oqmc::LatticeSampler sampler(
            CROFTON_QMC_DOMAIN_COORDINATE,
            CROFTON_QMC_DOMAIN_COORDINATE, frame, local_index, NULL);

        uint32_t integer_sample[CROFTON_LINE_DIMENSIONS];
        double sample[CROFTON_LINE_DIMENSIONS];
        sampler.drawSample<CROFTON_LINE_DIMENSIONS>(integer_sample);
        for (int dimension = 0;
             dimension < CROFTON_LINE_DIMENSIONS; dimension++) {
            sample[dimension] =
                static_cast<double>(integer_sample[dimension]) *
                CROFTON_QMC_SCALE;
        }

        qmc_line_from_sample(&rays[i], radius, center, sample);
        rays[i].id = first_id + i;
    }
}
#endif

static void
generate_rays(struct crofton_ray *rays, size_t ray_count, size_t first_id,
    double radius, const point_t center, std::mt19937_64 &rng, bool use_qmc,
    uint64_t qmc_frame_offset)
{
#ifdef BRLCAD_ENABLE_QMC
    if (use_qmc) {
        generate_qmc_rays(rays, ray_count, first_id, radius, center,
            qmc_frame_offset);
        return;
    }
#else
    (void)use_qmc;
    (void)qmc_frame_offset;
#endif
    generate_prng_rays(rays, ray_count, first_id, radius, center, rng);
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
		wd.local_invalid_partitions = 0;
		wd.local_point_count = 0;
		wd.local_segment_count = 0;
		wd.local_invalid.clear();
		wd.local_overlaps.clear();
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

struct rt_crofton_session {
    struct rt_i *rtip = NULL;
    struct resource *resources = NULL;
    size_t resource_count = 0;
    CroftonWorkerPool *pool = NULL;
};

void
rt_crofton_session_destroy(struct rt_crofton_session *session)
{
    if (!session)
        return;
    delete session->pool;
    if (session->resources) {
        for (size_t i = 0; i < session->resource_count; i++) {
            if (session->resources[i].re_magic == RESOURCE_MAGIC) {
                rt_clean_resource_basic(session->rtip,
                    &session->resources[i]);
                BU_PTBL_SET(&session->rtip->rti_resources, i, NULL);
            }
        }
        bu_free(session->resources, "Crofton resources");
    }
    delete session;
}

struct rt_crofton_session *
rt_crofton_session_create(struct rt_i *rtip)
{
    if (!rtip)
        return NULL;

    struct rt_crofton_session *session = NULL;
    try {
        session = new rt_crofton_session;
        session->rtip = rtip;
        session->resource_count = bu_avail_cpus();
        if (session->resource_count < 1)
            session->resource_count = 1;
        if (session->resource_count > MAX_PSW)
            session->resource_count = MAX_PSW;
        session->resources = (struct resource *)bu_calloc(
            session->resource_count, sizeof(struct resource),
            "Crofton resources");
        for (size_t i = 0; i < session->resource_count; i++)
            rt_init_resource(&session->resources[i], i, rtip);

        struct application ap;
        RT_APPLICATION_INIT(&ap);
        ap.a_rt_i = rtip;
        ap.a_hit = crofton_hit;
        ap.a_miss = crofton_miss;
        ap.a_overlap = crofton_overlap;
        ap.a_multioverlap = NULL;
        ap.a_logoverlap = rt_silent_logoverlap;
        ap.a_resource = session->resources;
        ap.a_onehit = 0;
        session->pool = new CroftonWorkerPool(&ap, session->resources,
            session->resource_count);
    } catch (const std::exception &e) {
        bu_log("rt_crofton: unable to start worker session: %s\n", e.what());
        rt_crofton_session_destroy(session);
        return NULL;
    } catch (...) {
        bu_log("rt_crofton: unable to start worker session\n");
        rt_crofton_session_destroy(session);
        return NULL;
    }
    return session;
}


static void
do_one_iteration(CroftonWorkerPool &pool, size_t ray_count, double radius,
    const point_t center, double focused_backout,
    struct crofton_shared *shared, bool use_qmc, uint64_t qmc_frame_offset,
    std::mt19937_64 &rng)
{
    struct crofton_ray *rays = (struct crofton_ray *)bu_calloc(
        ray_count, sizeof(struct crofton_ray), "Crofton rays");
    generate_rays(rays, ray_count, shared->ray_offset + shared->total_rays,
        radius, center, rng, use_qmc, qmc_frame_offset);
    if (focused_backout > 0.0) {
        for (size_t i = 0; i < ray_count; i++)
            VJOIN1(rays[i].r_pt, rays[i].r_pt, -focused_backout,
                rays[i].r_dir);
    }
    pool.run(rays, ray_count, shared);

    for (const struct crofton_worker_data &worker : pool.worker_data()) {
        shared->total_crossings += worker.local_crossings;
        shared->total_chord += worker.local_chord;
        shared->total_rays += worker.local_rays;
        shared->invalid_partition_count += worker.local_invalid_partitions;
        if (shared->invalid_visitor) {
            for (const auto &invalid : worker.local_invalid)
                shared->invalid_visitor(&invalid, shared->visitor_data);
        }
        if (shared->overlap_visitor) {
            for (const auto &overlap : worker.local_overlaps)
                shared->overlap_visitor(&overlap, shared->visitor_data);
        }

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

        if (worker.local_segment_count && shared->visitor) {
	    for (size_t i = 0; i < worker.local_segment_count; i++)
		shared->visitor(&worker.local_segments[i],
		    shared->visitor_data);
            shared->segment_count += worker.local_segment_count;
        } else if (worker.local_segment_count && !shared->ray_visitor) {
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

    if (shared->ray_visitor) {
        for (const struct crofton_worker_data &worker : pool.worker_data()) {
            size_t segment_index = 0;
            for (size_t ray_index = worker.start; ray_index < worker.end; ray_index++) {
                const size_t ray_id = worker.rays[ray_index].id;
                while (segment_index < worker.local_segment_count &&
                       worker.local_segments[segment_index].ray_id < ray_id)
                    segment_index++;
                const size_t first_segment = segment_index;
                while (segment_index < worker.local_segment_count &&
                       worker.local_segments[segment_index].ray_id == ray_id)
                    segment_index++;
                struct rt_crofton_ray ray;
                ray.ray_id = ray_id;
                VMOVE(ray.origin, worker.rays[ray_index].r_pt);
                VMOVE(ray.direction, worker.rays[ray_index].r_dir);
                ray.segments = first_segment < segment_index ?
                    &worker.local_segments[first_segment] : NULL;
                ray.segment_count = segment_index - first_segment;
                shared->ray_visitor(&ray, shared->visitor_data);
            }
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
crofton_shoot_impl(struct rt_crofton_session      *session,
                   struct rt_crofton_result       *out_result,
                   struct rt_crofton_stats          *out_stats,
                   enum rt_crofton_sequence         sequence,
                   uint64_t                         seed,
                   uint64_t                         stream_id,
                   rt_crofton_segment_fn            visitor,
                   rt_crofton_invalid_fn             invalid_visitor,
                   rt_crofton_ray_fn                ray_visitor,
                   rt_crofton_overlap_fn            overlap_visitor,
                   void                             *visitor_data,
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
    if (!rtip || (session && session->rtip != rtip) ||
        (!out_result && !out_stats && !out_surf_area && !out_volume &&
            !out_aabb_min && !out_obb && !out_points) ||
	((out_points == NULL) != (out_point_count == NULL)) ||
	((out_aabb_min == NULL) != (out_aabb_max == NULL)) ||
	((bbox_min == NULL) != (bbox_max == NULL)) ||
	(out_result && out_result->segments) ||
	ray_offset > UINT64_MAX / CROFTON_LINE_DIMENSIONS ||
	(((visitor == NULL) && (ray_visitor == NULL)) != (out_stats == NULL)) ||
	(params && (!std::isfinite(params->stability_mm) ||
	    params->stability_mm < 0.0 || !std::isfinite(params->time_ms) ||
	    params->time_ms < 0.0 ||
	    (params->stability_metrics &
		~static_cast<unsigned int>(RT_CROFTON_STABILITY_ALL)))) ||
	sequence < RT_CROFTON_SEQUENCE_DEFAULT ||
	sequence > RT_CROFTON_SEQUENCE_QMC)
	return -1;
#ifndef BRLCAD_ENABLE_QMC
    if (sequence == RT_CROFTON_SEQUENCE_QMC)
        return -1;
#endif
    if (bbox_min) {
	for (int coordinate = 0; coordinate < 3; coordinate++) {
	    if (!std::isfinite(bbox_min[coordinate]) ||
		!std::isfinite(bbox_max[coordinate]) ||
		bbox_min[coordinate] > bbox_max[coordinate])
		return -1;
	}
    }
    if (out_stats)
	memset(out_stats, 0, sizeof(*out_stats));
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
    point_t tight_min, tight_max;
    bool have_finite_solid = false;
    VSETALL(tight_min,  MAX_FASTF);
    VSETALL(tight_max, -MAX_FASTF);
    {
        struct soltab *stp;
        RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
            /* librt excludes infinite solids from model bounds and keeps them
             * in rti_inf_box so they still participate in Boolean clipping.
             * Mirror that policy here or a half-space makes the sampling
             * sphere non-finite. */
            if (!std::isfinite(stp->st_min[X]) ||
                !std::isfinite(stp->st_min[Y]) ||
                !std::isfinite(stp->st_min[Z]) ||
                !std::isfinite(stp->st_max[X]) ||
                !std::isfinite(stp->st_max[Y]) ||
                !std::isfinite(stp->st_max[Z]))
                continue;
            VMIN(tight_min, stp->st_min);
            VMAX(tight_max, stp->st_max);
            have_finite_solid = true;
        } RT_VISIT_ALL_SOLTABS_END;
    }

    double model_radius;
    point_t model_center;
    if (have_finite_solid) {
        VADD2SCALE(model_center, tight_max, tight_min, 0.5);
        vect_t tight_diag;
        VSUB2(tight_diag, tight_max, tight_min);
        model_radius = 0.5 * MAGNITUDE(tight_diag);
    } else {
        model_radius = rtip->rti_radius;
        VADD2SCALE(model_center, rtip->mdl_max, rtip->mdl_min, 0.5);
    }
    if (model_radius <= 0.0)
        model_radius = rtip->rti_radius;

    double R;
    double focused_backout = 0.0;
    point_t center;
    if (bbox_min && bbox_max) {
	VADD2SCALE(center, bbox_max, bbox_min, 0.5);
	vect_t bbox_diag;
	VSUB2(bbox_diag, bbox_max, bbox_min);
	R = 0.5 * MAGNITUDE(bbox_diag);
        /* Focused sampling changes the line distribution, not the intended
         * ray extent.  Move each origin behind a sphere containing both the
         * model and focused sphere so Boolean evaluation starts outside all
         * selected geometry. */
        focused_backout = RT_CROFTON_FOCUSED_BACKOUT_SCALE *
            (model_radius + DIST_PNT_PNT(center, model_center) + R);
    } else {
        R = model_radius;
        VMOVE(center, model_center);
    }

    if (!std::isfinite(R) || !std::isfinite(center[X]) ||
        !std::isfinite(center[Y]) || !std::isfinite(center[Z])) {
	return -1;
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
    enum rt_crofton_stop_reason stop_reason = RT_CROFTON_STOP_NONE;

    /* Batch size for each iteration.
     * Default: 2 000 (same as before, growth factor applied each round).
     * Explicit n_rays with no other criteria: fire them in one shot.   */
    size_t batch = RT_CROFTON_DEFAULT_SAMPLES;
    if (!use_default && max_rays > 0 && stability_mm <= 0.0 && time_ms <= 0.0)
	batch = max_rays;   /* single-iteration mode */
    if ((visitor || ray_visitor || overlap_visitor || invalid_visitor) &&
        batch > RT_CROFTON_VISIT_BATCH_RAYS)
        batch = RT_CROFTON_VISIT_BATCH_RAYS;  /* bound per-worker event memory */

    /* ---- Shared accumulator ---- */
    struct crofton_shared shared;
    memset(&shared, 0, sizeof(shared));
    shared.ray_offset = ray_offset;
    shared.collect_points =
        out_points != NULL || out_aabb_min != NULL || out_obb != NULL;
    shared.collect_segments = out_result != NULL || visitor != NULL || ray_visitor != NULL;
    shared.visitor = visitor;
    shared.ray_visitor = ray_visitor;
    shared.overlap_visitor = overlap_visitor;
    shared.invalid_visitor = invalid_visitor;
    shared.visitor_data = visitor_data;
    const unsigned int stability_metrics = params ?
        params->stability_metrics :
        static_cast<unsigned int>(RT_CROFTON_STABILITY_DEFAULT);
    const bool infer_stability_metrics =
        stability_metrics == RT_CROFTON_STABILITY_DEFAULT;
    const bool stabilize_surface = infer_stability_metrics ?
        (out_surf_area != NULL || out_result != NULL || out_stats != NULL ||
            shared.collect_points) :
        (stability_metrics & RT_CROFTON_STABILITY_SURFACE_AREA) != 0;
    const bool stabilize_volume = infer_stability_metrics ?
        (out_volume != NULL || out_result != NULL || out_stats != NULL) :
        (stability_metrics & RT_CROFTON_STABILITY_VOLUME) != 0;

    const bool owns_session = session == NULL;
    if (owns_session) {
        session = rt_crofton_session_create(rtip);
        if (!session)
            return -1;
    }
    CroftonWorkerPool *pool = session->pool;

    const bool use_qmc =
        rt_crofton_resolve_sequence(sequence) == RT_CROFTON_SEQUENCE_QMC;
    const uint64_t requested_seed = seed ? seed : RT_CROFTON_RNG_SEED;
    const bool custom_randomization = seed != 0 || stream_id != 0;
    const uint64_t randomized_seed = custom_randomization ?
        crofton_mix_seed(requested_seed ^ crofton_mix_seed(stream_id)) :
        RT_CROFTON_RNG_SEED;
    /*
     * Independent randomized QMC replications support uncertainty estimates;
     * ray offsets only continue one dependent point set.  See A. B. Owen,
     * "Error estimation for quasi-Monte Carlo" (2025), section 5,
     * https://arxiv.org/abs/2501.00150v3.
     */
    uint64_t qmc_frame_offset = 0;
#ifdef BRLCAD_ENABLE_QMC
    if (custom_randomization)
        qmc_frame_offset = randomized_seed % CROFTON_QMC_FRAME_COUNT;
#endif
    std::mt19937_64 rng(randomized_seed);
    if (!use_qmc) {
        rng.discard(static_cast<uint64_t>(ray_offset) *
            CROFTON_LINE_DIMENSIONS);
    }

    const double FOUR_PI    = 4.0 * M_PI;
    const double PI         = M_PI;
    const double INV_4PI    = 1.0 / FOUR_PI;
    const double INV_4PI3   = 3.0 / FOUR_PI;   /* for V → equivalent r */

    double curr_est_sa = 0.0, curr_est_v = 0.0;
    double observed_stability = 0.0;
    bool stability_evaluated = false;
    const int64_t t0 = (time_ms > 0.0 || (params && params->progress)) ?
	bu_gettime() : 0;

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

	    do_one_iteration(*pool, curr_rays, R, center, focused_backout,
		&shared, use_qmc, qmc_frame_offset, rng);
	    iteration++;

	    if (shared.total_rays == 0) break;

	    curr_est_sa = FOUR_PI * R * R
		* (double)shared.total_crossings
		/ (2.0 * (double)shared.total_rays);
	    curr_est_v = PI * R * R
		* shared.total_chord
		/ (double)shared.total_rays;

	    if (params && params->progress) {
		const double progress_elapsed_ms =
		    static_cast<double>(bu_gettime() - t0) / 1000.0;
		params->progress(shared.total_rays,
		    shared.total_crossings, curr_est_sa, curr_est_v, 0.0, 0,
		    progress_elapsed_ms, params->progress_data);
	    }

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
		const bool volume_stable = !stabilize_volume ||
		    (d_v_cur <= thr && d_v_prev <= thr);
		if (sa_stable && volume_stable) {
		    stop_reason = RT_CROFTON_STOP_STABILITY;
		    break;
		}
	    }

	    prev2_est_sa = prev1_est_sa;  prev1_est_sa = curr_est_sa;
	    prev2_est_v  = prev1_est_v;   prev1_est_v  = curr_est_v;

	} while (1);

    } else {
	/* ---- Parametric loop: n_rays / stability_mm / time_ms ---- */
	double prev_r_sa = -1.0, prev_r_v = -1.0;
	size_t total_fired = 0;
	size_t stable_windows = 0;
	size_t next_stability_check = RT_CROFTON_STABILITY_MIN_RAYS;

	for (;;) {
	    /* Time-budget check before firing */
	    if (time_ms > 0.0 && total_fired > 0 &&
		(bu_gettime() - t0) / 1000.0 >= time_ms) {
		stop_reason = RT_CROFTON_STOP_TIME;
		break;
	    }

	    /* Rays-budget: clamp batch to remaining if n_rays is set */
	    size_t fire = batch;
	    if (max_rays > 0) {
		size_t remaining = (total_fired < max_rays)
		    ? (max_rays - total_fired) : 0;
		if (remaining == 0) {
		    stop_reason = RT_CROFTON_STOP_RAYS;
		    break;
		}
		if (fire > remaining) fire = remaining;
	    }

	    do_one_iteration(*pool, fire, R, center, focused_backout,
		&shared, use_qmc, qmc_frame_offset, rng);
	    total_fired += fire;

	    if (shared.total_rays == 0) break;

	    curr_est_sa = FOUR_PI * R * R
		* (double)shared.total_crossings
		/ (2.0 * (double)shared.total_rays);
	    curr_est_v = PI * R * R
		* shared.total_chord
		/ (double)shared.total_rays;

	    bool stop_for_stability = false;
	    /* Compare at doubling checkpoints.  Fixed-size incremental changes
	     * shrink as a mathematical consequence of accumulating more samples
	     * and can falsely suggest convergence. */
	    if (stability_mm > 0.0 &&
		shared.total_rays >= next_stability_check &&
		shared.total_crossings >= RT_CROFTON_STABILITY_MIN_CROSSINGS) {
		double r_sa = (curr_est_sa > 0.0) ?
		    sqrt(curr_est_sa * INV_4PI) : 0.0;
		double r_v = (curr_est_v > 0.0) ?
		    cbrt(curr_est_v * INV_4PI3) : 0.0;
		const bool compare_sa = stabilize_surface && prev_r_sa >= 0.0;
		const bool compare_v = stabilize_volume && prev_r_v >= 0.0;
		if (compare_sa || compare_v) {
		    double change = 0.0;
		    if (compare_sa)
			change = fabs(r_sa - prev_r_sa);
		    if (compare_v)
			change = std::max(change, fabs(r_v - prev_r_v));
		    observed_stability = change;
		    stability_evaluated = true;
		}
		const bool sa_ok = !stabilize_surface || (compare_sa &&
		    fabs(r_sa - prev_r_sa) <= stability_mm);
		const bool v_ok = !stabilize_volume || (compare_v &&
		    fabs(r_v - prev_r_v) <= stability_mm);
		prev_r_sa = r_sa;
		prev_r_v = r_v;
		if (sa_ok && v_ok) {
		    stable_windows++;
		    if (stable_windows >= RT_CROFTON_STABILITY_MIN_WINDOWS)
			stop_for_stability = true;
		} else {
		    stable_windows = 0;
		}
		next_stability_check = shared.total_rays <= SIZE_MAX / 2 ?
		    shared.total_rays * 2 : SIZE_MAX;
	    }

	    if (params && params->progress) {
		const double progress_elapsed_ms =
		    static_cast<double>(bu_gettime() - t0) / 1000.0;
		params->progress(shared.total_rays, shared.total_crossings,
		    curr_est_sa, curr_est_v, observed_stability,
		    stability_evaluated ? 1 : 0, progress_elapsed_ms,
		    params->progress_data);
	    }
	    if (stop_for_stability) {
		stop_reason = RT_CROFTON_STOP_STABILITY;
		break;
	    }

	    /* Time-budget check after firing */
	    if (time_ms > 0.0 &&
		(bu_gettime() - t0) / 1000.0 >= time_ms) {
		stop_reason = RT_CROFTON_STOP_TIME;
		break;
	    }
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
    if (out_stats) {
        out_stats->surface_area = curr_est_sa;
        out_stats->volume = curr_est_v;
        out_stats->ray_count = shared.total_rays;
        out_stats->crossing_count = shared.total_crossings;
        out_stats->stability_mm = observed_stability;
        out_stats->stability_evaluated = stability_evaluated ? 1 : 0;
        out_stats->invalid_partition_count = shared.invalid_partition_count;
        out_stats->stop_reason = stop_reason;
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
    if (owns_session)
        rt_crofton_session_destroy(session);

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
    return crofton_shoot_impl(NULL, NULL, NULL,
        RT_CROFTON_SEQUENCE_DEFAULT, 0, 0,
        NULL, NULL, NULL, NULL, NULL,
        out_points, out_point_count, out_surf_area, out_volume,
        out_aabb_min, out_aabb_max, out_obb,
        rtip, params, 0, bbox_min, bbox_max);
}

int
rt_crofton_collect(struct rt_crofton_result *result, struct rt_i *rtip,
    const struct rt_crofton_params *params, size_t ray_offset,
    const fastf_t *bbox_min, const fastf_t *bbox_max)
{
    if (!result)
        return -1;
    return crofton_shoot_impl(NULL, result, NULL,
        RT_CROFTON_SEQUENCE_DEFAULT, 0, 0,
        NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        rtip, params, ray_offset, bbox_min, bbox_max);
}

int
rt_crofton_qmc_available(void)
{
#ifdef BRLCAD_ENABLE_QMC
    return 1;
#else
    return 0;
#endif
}

enum rt_crofton_sequence
rt_crofton_resolve_sequence(enum rt_crofton_sequence sequence)
{
    if (sequence != RT_CROFTON_SEQUENCE_DEFAULT)
        return sequence;
#ifdef BRLCAD_ENABLE_QMC
    return use_qmc_sampler() ? RT_CROFTON_SEQUENCE_QMC :
        RT_CROFTON_SEQUENCE_RANDOM;
#else
    return RT_CROFTON_SEQUENCE_RANDOM;
#endif
}

int
rt_crofton_visit(struct rt_crofton_stats *stats, struct rt_i *rtip,
    const struct rt_crofton_params *params, size_t ray_offset,
    const fastf_t *bbox_min, const fastf_t *bbox_max,
    enum rt_crofton_sequence sequence, rt_crofton_segment_fn visitor,
    rt_crofton_invalid_fn invalid_visitor, void *data)
{
    return rt_crofton_visit_seeded(stats, rtip, params, ray_offset,
        bbox_min, bbox_max, sequence, 0, 0, visitor, invalid_visitor, data);
}

int
rt_crofton_visit_seeded(struct rt_crofton_stats *stats, struct rt_i *rtip,
    const struct rt_crofton_params *params, size_t ray_offset,
    const fastf_t *bbox_min, const fastf_t *bbox_max,
    enum rt_crofton_sequence sequence, uint64_t seed, uint64_t stream_id,
    rt_crofton_segment_fn visitor, rt_crofton_invalid_fn invalid_visitor,
    void *data)
{
    return crofton_shoot_impl(NULL, NULL, stats, sequence, seed, stream_id,
        visitor, invalid_visitor,
        NULL, NULL, data,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        rtip, params, ray_offset, bbox_min, bbox_max);
}

int
rt_crofton_session_visit_seeded(struct rt_crofton_session *session,
    struct rt_crofton_stats *stats,
    const struct rt_crofton_params *params, size_t ray_offset,
    const fastf_t *bbox_min, const fastf_t *bbox_max,
    enum rt_crofton_sequence sequence, uint64_t seed, uint64_t stream_id,
    rt_crofton_segment_fn visitor, rt_crofton_invalid_fn invalid_visitor,
    void *data)
{
    if (!session)
        return -1;
    return crofton_shoot_impl(session, NULL, stats, sequence, seed, stream_id,
        visitor, invalid_visitor,
        NULL, NULL, data,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        session->rtip, params, ray_offset, bbox_min, bbox_max);
}

int
rt_crofton_visit_rays_ex(struct rt_crofton_stats *stats, struct rt_i *rtip,
    const struct rt_crofton_params *params, size_t ray_offset,
    const fastf_t *bbox_min, const fastf_t *bbox_max,
    enum rt_crofton_sequence sequence, rt_crofton_ray_fn ray_visitor,
    rt_crofton_overlap_fn overlap_visitor,
    rt_crofton_invalid_fn invalid_visitor, void *data)
{
    return rt_crofton_visit_rays_seeded_ex(stats, rtip, params, ray_offset,
        bbox_min, bbox_max, sequence, 0, 0, ray_visitor, overlap_visitor,
        invalid_visitor, data);
}

int
rt_crofton_visit_rays_seeded_ex(struct rt_crofton_stats *stats,
    struct rt_i *rtip, const struct rt_crofton_params *params,
    size_t ray_offset, const fastf_t *bbox_min, const fastf_t *bbox_max,
    enum rt_crofton_sequence sequence, uint64_t seed, uint64_t stream_id,
    rt_crofton_ray_fn ray_visitor, rt_crofton_overlap_fn overlap_visitor,
    rt_crofton_invalid_fn invalid_visitor, void *data)
{
    return crofton_shoot_impl(NULL, NULL, stats, sequence, seed, stream_id,
        NULL, invalid_visitor,
        ray_visitor, overlap_visitor, data,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        rtip, params, ray_offset, bbox_min, bbox_max);
}

int
rt_crofton_visit_rays(struct rt_crofton_stats *stats, struct rt_i *rtip,
    const struct rt_crofton_params *params, size_t ray_offset,
    const fastf_t *bbox_min, const fastf_t *bbox_max,
    enum rt_crofton_sequence sequence, rt_crofton_ray_fn ray_visitor,
    rt_crofton_invalid_fn invalid_visitor, void *data)
{
    return rt_crofton_visit_rays_ex(stats, rtip, params, ray_offset,
        bbox_min, bbox_max, sequence, ray_visitor, NULL,
        invalid_visitor, data);
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

static const struct rt_crofton_params s_default_params  =
    { 0u, 0.0, 0.0, RT_CROFTON_STABILITY_DEFAULT, NULL, NULL };
static const struct rt_crofton_params s_implicit_params =
    { RT_CROFTON_IMPLICIT_SAMPLES, 0.0, 0.0, RT_CROFTON_STABILITY_DEFAULT, NULL, NULL };

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
