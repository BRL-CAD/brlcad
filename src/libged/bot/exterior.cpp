/*                      E X T E R I O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/bot/exterior.cpp
 *
 * BoT "exterior" subcommand — identify and retain only exterior faces.
 *
 * Two methods are supported, depending on supporting libraries being
 * present and enabled:
 *
 *  Cauchy-Crofton ray sampling (default)
 *    Random chord rays are fired uniformly through the bounding sphere.
 *    For each ray, the first and last surface hits are marked as exterior
 *    faces.  Simultaneously the Cauchy-Crofton surface-area estimate is
 *    tracked; convergence of that estimate (< 1 % change over three
 *    successive iterations) determines when to stop.  This gives thorough,
 *    unbiased coverage of all outward-facing triangles.
 *
 *  Flood fill (--flood-fill, requires OpenVDB)
 *    The model is voxelised via rt_rtip_to_occupancy_grid, then a
 *    BFS flood-fill from the boundary finds all "water-reachable" voxels.
 *    A face is exterior when any of its six face-adjacent voxels is
 *    reachable.
 *
 * Also provides the legacy ged_bot_exterior() entry point, which
 * accepts  [-f] old_bot new_bot  and forwards to _bot_cmd_exterior.
 */

#include "common.h"

#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/parallel.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "rt/application.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "rt/ray_partition.h"
#include "rt/seg.h"
#include "raytrace.h"
#include "ged.h"

#include "./ged_bot.h"
#include "concurrentqueue.h"

/*
 * Offset the probe-ray origin outside the entry point by 10x BN_TOL_DIST.
 * This steps past near-surface numeric jitter while remaining negligible
 * relative to model scale.
 */
#define EXTERIOR_RAY_OFFSET_FACTOR 10.0
/* Work chunk size for dynamic scheduling: small enough for load balancing,
 * large enough to avoid excessive queue overhead. */
#define EXTERIOR_FACE_CHUNK 64

/* Phase 2 time factor */
#define PHASE2_FACTOR 10.0

/* Options bag passed from _bot_cmd_exterior down into the classifier. */
struct bot_exterior_opts {
    double vis_threshold;   /* [0,1] fraction of fired rays that must "see"
			     * a triangle before it is included; 0 = disabled */
    int    vis_strict;      /* also re-check phase-1 triangles against the
			     * visibility threshold (requires vis_threshold > 0) */
    int    verbose;         /* 0 = default output; 1 = extra metrics
			     * (per-phase timings, flip-time stats*/
};


/* ======================================================================
 * Focused second-pass helpers: parallel per-triangle exterior sampling
 *
 * After the global Cauchy-Crofton pass converges, some triangles may still
 * be unclassified.  For each such triangle a time-budgeted focused pass fires
 * random rays from points ON the triangle itself in uniformly random outward
 * directions, shooting the full model.
 *
 * Per-face termination (earliest wins):
 *   (a) target triangle appears as first or last surface hit → exterior, done.
 *   (b) Cauchy-Crofton SA estimate (normalised by the triangle's bounding
 *       sphere area) stable < 1 % over two successive batch deltas → the
 *       solid angle has been adequately sampled; absence of an exterior hit
 *       implies this face is interior.
 *   (c) Per-face time budget (10× initial-pass time / #unclassified faces)
 *       exhausted.
 *
 * Parallelism: all unclassified faces are fed into a ConcurrentQueue;
 * worker threads pull work items and process them independently so that
 * multiple CPUs make progress simultaneously.
 * ====================================================================== */

/* Per-application context (one per in-flight face, owned by its worker). */
struct ext_focused2_ctx {
    int    target_face;
    int    num_faces;
    int   *mask;        /* shared mask — writes are benign races (only 0→1) */
    int    found;
    size_t crossings;
    size_t rays;
    size_t vis_rays;           /* rays where target face appeared as first/last */
    int    disable_opportunistic; /* if 1, skip benign mask writes for non-target faces */
};

static int
ext_focused2_hit(struct application *ap, struct partition *PartHeadp,
		 struct seg *UNUSED(segs))
{
    struct ext_focused2_ctx *ctx = (struct ext_focused2_ctx *)ap->a_uptr;
    int first_face = -1, last_face = -1;
    size_t crossings = 0;

    for (struct partition *pp = PartHeadp->pt_forw;
	 pp != PartHeadp; pp = pp->pt_forw) {
	crossings += 2;
	int isf = pp->pt_inhit->hit_surfno;
	int osf = pp->pt_outhit->hit_surfno;
	if (first_face < 0 && isf >= 0 && isf < ctx->num_faces)
	    first_face = isf;
	if (osf >= 0 && osf < ctx->num_faces)
	    last_face = osf;
    }

    /* Opportunistic exterior marking.  Multiple workers may write 1 to the
     * same slot concurrently; this is a benign race: only 1 is ever written
     * and the mask is read only after all worker threads have joined.
     * When disable_opportunistic is set (visibility-strict mode) we skip
     * this so that only the designated target face is threshold-tested. */
    if (!ctx->disable_opportunistic) {
	if (first_face >= 0) ctx->mask[first_face] = 1;
	if (last_face  >= 0) ctx->mask[last_face]  = 1;
    }

    if (first_face == ctx->target_face || last_face == ctx->target_face) {
	ctx->found = 1;
	ctx->vis_rays++;   /* count every ray that sees the target face */
    }

    ctx->crossings += crossings;
    ctx->rays      += 1;
    return 1;
}

static int
ext_focused2_miss(struct application *ap)
{
    struct ext_focused2_ctx *ctx = (struct ext_focused2_ctx *)ap->a_uptr;
    ctx->rays += 1;
    return 0;
}

/* Portable per-worker pseudo-random [0, 1) via a Knuth LCG. */
static inline uint32_t
ext_focused2_lcg(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static inline double
ext_focused2_rand01(uint32_t *state)
{
    /* Use the upper 24 bits for better distribution. */
    return (double)(ext_focused2_lcg(state) >> 8) / (double)(1u << 24);
}

/* Shared progress counters, written atomically by workers. */
struct ext_focused2_progress {
    std::atomic<size_t>   faces_done;
    size_t                faces_total;
    std::atomic<int64_t>  last_print_us; /* µs since pass2_start of last log */
    std::chrono::steady_clock::time_point pass2_start;
};

struct ext_focused2_worker_data {
    struct rt_i                 *rtip;
    struct rt_bot_internal      *bot;
    int                         *mask;
    moodycamel::ConcurrentQueue<size_t> *face_queue;
    const moodycamel::ProducerToken     *ptok; /* token for the single producer */
    double                       per_face_budget_sec;
    uint32_t                     rand_seed;
    struct resource             *resource; /* one slot from resources[] */
    struct ext_focused2_progress *progress;
    size_t                       n_classified;
    double                       vis_threshold;        /* 0 = disabled */
    int                          disable_opportunistic; /* for strict mode */
    /* Flip-time instrumentation: elapsed seconds from face_start until a
     * face is first confirmed exterior.  Aggregated across all faces this
     * worker processes; combined by the parent after thread join. */
    double                       max_flip_sec;   /* longest flip time seen */
    double                       sum_flip_sec;   /* sum of flip times */
    size_t                       n_flip_timed;   /* number of flips recorded */
};

static void
ext_focused2_worker(struct ext_focused2_worker_data *wd)
{
    const double FOUR_PI = 4.0 * M_PI;
    const size_t BATCH   = 64u; /* rays per sub-batch before time/convergence check */
    const bool threshold_mode = (wd->vis_threshold > 0.0);

    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i         = wd->rtip;
    ap.a_hit          = ext_focused2_hit;
    ap.a_miss         = ext_focused2_miss;
    ap.a_overlap      = NULL;
    ap.a_multioverlap = NULL;
    ap.a_logoverlap   = rt_silent_logoverlap;
    ap.a_resource     = wd->resource;
    ap.a_onehit       = 0;

    uint32_t seed = wd->rand_seed;

    size_t face_idx;
    while (wd->face_queue->try_dequeue_from_producer(*wd->ptok, face_idx)) {
	bool newly_found = false;

	/* Already marked exterior by an opportunistic side-effect write?
	 * In threshold mode we always re-test so that every included face
	 * meets the visibility ratio; skip the shortcut in that case. */
	if (threshold_mode || !wd->mask[face_idx]) {
	    int vi0 = wd->bot->faces[face_idx * 3 + 0];
	    int vi1 = wd->bot->faces[face_idx * 3 + 1];
	    int vi2 = wd->bot->faces[face_idx * 3 + 2];

	    if (vi0 >= 0 && vi1 >= 0 && vi2 >= 0 &&
		(size_t)vi0 < wd->bot->num_vertices &&
		(size_t)vi1 < wd->bot->num_vertices &&
		(size_t)vi2 < wd->bot->num_vertices) {

		point_t tp0, tp1, tp2;
		VMOVE(tp0, &wd->bot->vertices[vi0 * 3]);
		VMOVE(tp1, &wd->bot->vertices[vi1 * 3]);
		VMOVE(tp2, &wd->bot->vertices[vi2 * 3]);

		/* Bounding sphere: centroid + circumradius + 10 % bump */
		point_t tcen;
		VADD3(tcen, tp0, tp1, tp2);
		VSCALE(tcen, tcen, 1.0 / 3.0);

		double d0 = DIST_PNT_PNT(tcen, tp0);
		double d1 = DIST_PNT_PNT(tcen, tp1);
		double d2 = DIST_PNT_PNT(tcen, tp2);
		double tri_r = d0;
		if (d1 > tri_r) tri_r = d1;
		if (d2 > tri_r) tri_r = d2;

		double bump = tri_r * 0.1;
		if (bump < BN_TOL_DIST * EXTERIOR_RAY_OFFSET_FACTOR)
		    bump = BN_TOL_DIST * EXTERIOR_RAY_OFFSET_FACTOR;
		tri_r += bump;

		/* The bounding sphere's area provides the Cauchy-Crofton
		 * normalisation constant for the SA convergence estimate. */
		const double sphere_area = FOUR_PI * tri_r * tri_r;

		/* Per-face ray context */
		struct ext_focused2_ctx fctx;
		fctx.target_face           = (int)face_idx;
		fctx.num_faces             = (int)wd->bot->num_faces;
		fctx.mask                  = wd->mask;
		fctx.found                 = 0;
		fctx.crossings             = 0;
		fctx.rays                  = 0;
		fctx.vis_rays              = 0;
		fctx.disable_opportunistic = wd->disable_opportunistic;
		ap.a_uptr                  = (void *)&fctx;

		double fprev2 = -2.0, fprev1 = -1.0, fcurr = 0.0;
		size_t total_rays = 0;
		bool   face_found = false;
		auto   face_start = std::chrono::steady_clock::now();

		while (true) {
		    /* Time budget (skip check on very first batch so we always
		     * fire at least BATCH rays before giving up). */
		    if (total_rays > 0) {
			double elapsed = std::chrono::duration<double>(
			    std::chrono::steady_clock::now() - face_start).count();
			if (elapsed >= wd->per_face_budget_sec)
			    break;
		    }

		    /* Fire BATCH rays from random points ON the triangle.
		     * Each ray shoots the full model; the first/last surface
		     * hit determines exterior status.  The same crossings count
		     * feeds the Cauchy-Crofton SA estimate used for convergence,
		     * with the triangle's bounding sphere providing the area
		     * normalisation: when the estimate stabilises we have
		     * adequately covered the solid angle around the triangle.
		     *
		     * In threshold mode we do not exit early when the face is
		     * first seen — we fire until the budget is exhausted so the
		     * visibility ratio is based on a representative sample. */
		    for (size_t ri = 0; ri < BATCH; ri++) {
			/* Normal mode: skip remaining rays once face is found. */
			if (!threshold_mode && face_found)
			    break;

			/* Uniform random point on triangle via Shirley warping:
			 *   P = (1-√r1)·v0 + √r1·(1-r2)·v1 + √r1·r2·v2 */
			double r1 = ext_focused2_rand01(&seed);
			double r2 = ext_focused2_rand01(&seed);
			double sq = sqrt(r1);
			double u  = 1.0 - sq;
			double v  = sq * (1.0 - r2);
			double w  = sq * r2;
			point_t tript;
			tript[X] = u*tp0[X] + v*tp1[X] + w*tp2[X];
			tript[Y] = u*tp0[Y] + v*tp1[Y] + w*tp2[Y];
			tript[Z] = u*tp0[Z] + v*tp1[Z] + w*tp2[Z];

			/* Uniformly random direction over the full sphere. */
			double theta = 2.0 * M_PI * ext_focused2_rand01(&seed);
			double phi   = acos(2.0 * ext_focused2_rand01(&seed) - 1.0);
			double sp    = sin(phi);
			vect_t fdir;
			fdir[X] = sp * cos(theta);
			fdir[Y] = sp * sin(theta);
			fdir[Z] = cos(phi);
			/* fdir is already unit length by construction from
			 * spherical coordinates; no VUNITIZE needed. */

			/* Back the origin up to just outside the model bounding
			 * box so rt_shootray traverses the complete model.
			 * rt_in_rpp sets r_min < 0 when the origin is inside the
			 * bbox; stepping back by (r_min - offset) places it just
			 * outside the entry face. */
			VMOVE(ap.a_ray.r_pt,  tript);
			VMOVE(ap.a_ray.r_dir, fdir);
			vect_t finv;
			VINVDIR(finv, fdir);
			if (!rt_in_rpp(&ap.a_ray, finv,
				       ap.a_rt_i->mdl_min,
				       ap.a_rt_i->mdl_max))
			    continue;

			fastf_t foff = ap.a_ray.r_min
			    - (EXTERIOR_RAY_OFFSET_FACTOR * BN_TOL_DIST);
			VJOIN1(ap.a_ray.r_pt, tript, foff, fdir);

			rt_shootray(&ap);
			total_rays++;

			if (fctx.found && !face_found) {
			    face_found = true;
			    /* Record the elapsed time at which this face was
			     * first confirmed exterior.  This "flip time"
			     * feeds the post-phase-2 statistics. */
			    double flip_sec = std::chrono::duration<double>(
				std::chrono::steady_clock::now() - face_start).count();
			    if (flip_sec > wd->max_flip_sec)
				wd->max_flip_sec = flip_sec;
			    wd->sum_flip_sec += flip_sec;
			    wd->n_flip_timed++;
			} else if (fctx.found) {
			    face_found = true;
			}
		    }

		    /* Normal mode: stop as soon as the face is confirmed exterior. */
		    if (!threshold_mode && face_found)
			break;

		    if (fctx.rays == 0)
			break;

		    /* In threshold mode we do not use the SA convergence test —
		     * we always run to the time budget so the ratio estimate is
		     * based on the maximum available sample.  In normal mode the
		     * convergence test lets us conclude "interior" early. */
		    if (!threshold_mode) {
			/* Cauchy-Crofton SA estimate normalised by the triangle's
			 * bounding sphere area.  Convergence (< 1 % change over
			 * two successive batch deltas) means the solid angle has
			 * been adequately sampled; absence of an exterior hit then
			 * implies this face is interior. */
			fcurr = sphere_area
			    * (double)fctx.crossings
			    / (2.0 * (double)fctx.rays);

			if (total_rays >= 3 * BATCH) {
			    double dl = (fprev1 > 0.0)
				? fabs(fcurr  - fprev1) / fprev1 * 100.0 : 999.0;
			    double dp = (fprev2 > 0.0)
				? fabs(fprev1 - fprev2) / fprev2 * 100.0 : 999.0;
			    if (dl <= 1.0 && dp <= 1.0)
				break;
			}

			fprev2 = fprev1;
			fprev1 = fcurr;
		    }
		}

		/* Decide whether to mark this face exterior.
		 *
		 * Normal mode: any confirmed sighting suffices.
		 * Threshold mode: the fraction of rays that saw the face must
		 *   reach or exceed vis_threshold.  A face with zero fired rays
		 *   (degenerate geometry that is always clipped by rt_in_rpp)
		 *   is never included. */
		bool should_mark;
		if (threshold_mode) {
		    should_mark = (fctx.rays > 0 &&
				   (double)fctx.vis_rays / (double)fctx.rays
				   >= wd->vis_threshold);
		} else {
		    should_mark = face_found;
		}

		if (should_mark) {
		    wd->mask[face_idx] = 1;
		    newly_found = true;
		}
	    }
	}

	if (newly_found)
	    wd->n_classified++;

	/* Progress: atomically update counter and print at most every 5 s. */
	struct ext_focused2_progress *prog = wd->progress;
	size_t done = prog->faces_done.fetch_add(1, std::memory_order_relaxed) + 1;
	int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
	    std::chrono::steady_clock::now() - prog->pass2_start).count();
	int64_t prev_us = prog->last_print_us.load(std::memory_order_relaxed);
	const int64_t INTERVAL_US = 5000000LL; /* 5 seconds */
	if (elapsed_us - prev_us >= INTERVAL_US) {
	    if (prog->last_print_us.compare_exchange_weak(
		    prev_us, elapsed_us, std::memory_order_relaxed)) {
		size_t total     = prog->faces_total;
		size_t remaining = (total > done) ? total - done : 0;
		double elapsed_s = elapsed_us * 1e-6;
		double rate      = (elapsed_s > 0.0)
		    ? (double)done / elapsed_s : 0.0;
		double eta_s     = (rate > 0.0)
		    ? (double)remaining / rate : 0.0;
		bu_log("bot exterior [2nd pass]: %zu/%zu faces (%.0f%%), "
		       "elapsed=%.1fs, ETA=%.1fs\n",
		       done, total,
		       100.0 * done / (double)total,
		       elapsed_s, eta_s);
	    }
	}
    }
}


/* Flood-fill classifier lives in bot_flood_exterior.cpp and is only
 * compiled when OpenVDB is available. */
#ifdef HAVE_OPENVDB_BOT_EXTERIOR
extern "C" int bot_flood_exterior_classify(struct rt_i *rtip,
					   struct rt_bot_internal *bot,
					   int *face_exterior,
					   double voxel_size);
#endif


/* ======================================================================
 * Cauchy-Crofton exterior classifier
 *
 * Fire chord rays uniformly distributed over the bounding sphere.  Every
 * ray that hits the model contributes:
 *   - its first inhit surfno  → exterior face (first surface seen from outside)
 *   - its last  outhit surfno → exterior face (last surface seen from outside)
 *
 * Simultaneously the crossing count is accumulated to estimate the total
 * surface area via the Cauchy-Crofton formula; convergence of that estimate
 * (< 1 % change across three successive iterations) is used as the
 * termination criterion.  This gives thorough, unbiased coverage of all
 * outward-facing triangles at the cost of fewer overall raycast operations
 * compared with the per-face voting approach.
 * ====================================================================== */

/* Ray: origin on bounding sphere + unit direction toward a second sphere pt */
struct ext_crofton_ray {
    point_t r_pt;
    vect_t  r_dir;
};

/* Shared SA accumulators (protected by semaphore) */
struct ext_crofton_shared {
    size_t  total_crossings;
    double  total_chord;
    size_t  total_rays;
    int     sem_stats;
};

/* Per-application context passed through a_uptr */
struct ext_crofton_hit_ctx {
    int    *local_mask;   /* per-worker exterior face mask (no lock needed) */
    size_t  num_faces;
    struct ext_crofton_shared *shared;
};

static int
ext_crofton_hit(struct application *ap, struct partition *PartHeadp,
		struct seg *UNUSED(segs))
{
    struct ext_crofton_hit_ctx *ctx =
	(struct ext_crofton_hit_ctx *)ap->a_uptr;
    struct ext_crofton_shared *sh = ctx->shared;

    size_t crossings = 0;
    double chord     = 0.0;
    int first_face   = -1;
    int last_face    = -1;

    for (struct partition *pp = PartHeadp->pt_forw;
	 pp != PartHeadp;
	 pp = pp->pt_forw) {
	crossings += 2;
	chord += pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;

	int isf = pp->pt_inhit->hit_surfno;
	int osf = pp->pt_outhit->hit_surfno;

	if (first_face < 0 && isf >= 0 && (size_t)isf < ctx->num_faces)
	    first_face = isf;
	if (osf >= 0 && (size_t)osf < ctx->num_faces)
	    last_face = osf;
    }

    /* Mark exterior faces; these are per-worker local arrays so no lock. */
    if (first_face >= 0)
	ctx->local_mask[first_face] = 1;
    if (last_face >= 0)
	ctx->local_mask[last_face] = 1;

    bu_semaphore_acquire(sh->sem_stats);
    sh->total_crossings += crossings;
    sh->total_chord     += chord;
    sh->total_rays      += 1;
    bu_semaphore_release(sh->sem_stats);

    return 1;
}

static int
ext_crofton_miss(struct application *ap)
{
    struct ext_crofton_hit_ctx *ctx =
	(struct ext_crofton_hit_ctx *)ap->a_uptr;
    struct ext_crofton_shared *sh = ctx->shared;

    bu_semaphore_acquire(sh->sem_stats);
    sh->total_rays += 1;
    bu_semaphore_release(sh->sem_stats);

    return 0;
}

/* Per-worker state for bu_parallel */
struct ext_crofton_worker_data {
    struct application       *ap;
    struct ext_crofton_ray   *rays;
    size_t                    start;
    size_t                    end;
    struct ext_crofton_hit_ctx ctx;
};

static void
ext_crofton_worker(int id, void *data)
{
    struct ext_crofton_worker_data *wd =
	&((struct ext_crofton_worker_data *)data)[id - 1];
    struct application *ap = wd->ap;

    for (size_t i = wd->start; i < wd->end; i++) {
	VMOVE(ap->a_ray.r_pt,  wd->rays[i].r_pt);
	VMOVE(ap->a_ray.r_dir, wd->rays[i].r_dir);
	rt_shootray(ap);
    }
}

/* Inline LCG (Knuth) providing a uniform double in [0, 1).
 * State is per-caller so the Crofton phase is fully deterministic and
 * independent of any global rand() state. */
static inline double
ext_crofton_rand01(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return (double)(*state >> 8) / (double)(1u << 24);
}

/* Random point uniformly distributed on a sphere of given radius */
static void
ext_crofton_sphere_pt(double radius, const point_t center, point_t out,
		      uint32_t *state)
{
    double theta = 2.0 * M_PI * ext_crofton_rand01(state);
    double phi   = acos(2.0 * ext_crofton_rand01(state) - 1.0);
    double sp    = sin(phi);
    out[X] = center[X] + radius * sp * cos(theta);
    out[Y] = center[Y] + radius * sp * sin(theta);
    out[Z] = center[Z] + radius * cos(phi);
}

/*
 * Generate nrays chord rays: pair 2*nrays random sphere points after
 * shuffling, so each ray connects two statistically independent points.
 */
static void
ext_crofton_gen_rays(struct ext_crofton_ray *rays, size_t nrays,
		     double radius, const point_t center, uint32_t *state)
{
    size_t npts = nrays * 2;
    point_t *pts = (point_t *)bu_calloc(npts, sizeof(point_t),
					"ext crofton pts");

    for (size_t i = 0; i < npts; i++)
	ext_crofton_sphere_pt(radius, center, pts[i], state);

    /* Fisher-Yates shuffle for random pairing */
    for (size_t i = npts - 1; i > 0; i--) {
	size_t j = (size_t)(ext_crofton_rand01(state) * (double)(i + 1));
	if (j > i) j = i;
	point_t tmp;
	VMOVE(tmp, pts[i]);
	VMOVE(pts[i], pts[j]);
	VMOVE(pts[j], tmp);
    }

    for (size_t i = 0; i < nrays; i++) {
	VMOVE(rays[i].r_pt,  pts[i * 2]);
	VSUB2(rays[i].r_dir, pts[i * 2 + 1], pts[i * 2]);
	VUNITIZE(rays[i].r_dir);
    }

    bu_free(pts, "ext crofton pts");
}

/*
 * Fire one batch of nrays, updating shared SA accumulators and per-worker
 * local masks.  After parallel execution the local masks are OR-ed into the
 * caller-supplied global mask.
 */
static void
ext_crofton_do_iteration(struct application      *ap_tmpl,
			 struct resource         *resources,
			 size_t                   nrays,
			 double                   radius,
			 const point_t            center,
			 struct ext_crofton_shared *shared,
			 int                     *mask,
			 size_t                   num_faces,
			 uint32_t                *rand_state)
{
    struct ext_crofton_ray *rays = (struct ext_crofton_ray *)bu_calloc(
	nrays, sizeof(struct ext_crofton_ray), "ext crofton rays");

    ext_crofton_gen_rays(rays, nrays, radius, center, rand_state);

    size_t ncpus = bu_avail_cpus();
    if (ncpus < 1) ncpus = 1;

    struct ext_crofton_worker_data *wdata =
	(struct ext_crofton_worker_data *)bu_calloc(
	    ncpus, sizeof(struct ext_crofton_worker_data),
	    "ext crofton wdata");

    size_t per_cpu = nrays / ncpus;

    for (size_t i = 0; i < ncpus; i++) {
	struct application *a = (struct application *)bu_calloc(
	    1, sizeof(struct application), "ext crofton app");
	*a = *ap_tmpl;
	a->a_resource = &resources[i];

	int *lmask = (int *)bu_calloc(num_faces, sizeof(int),
				      "ext crofton lmask");

	wdata[i].ap            = a;
	wdata[i].rays          = rays;
	wdata[i].start         = i * per_cpu;
	wdata[i].end           = (i == ncpus - 1) ? nrays : (i + 1) * per_cpu;
	wdata[i].ctx.local_mask = lmask;
	wdata[i].ctx.num_faces  = num_faces;
	wdata[i].ctx.shared     = shared;
	a->a_uptr = (void *)&wdata[i].ctx;
    }

    bu_parallel(ext_crofton_worker, (int)ncpus, (void *)wdata);

    /* Merge per-worker local masks into global mask and free workers */
    for (size_t i = 0; i < ncpus; i++) {
	int *lmask = wdata[i].ctx.local_mask;
	for (size_t f = 0; f < num_faces; f++) {
	    if (lmask[f])
		mask[f] = 1;
	}
	bu_free(lmask, "ext crofton lmask");
	bu_free(wdata[i].ap, "ext crofton app");
    }

    bu_free(wdata, "ext crofton wdata");
    bu_free(rays,  "ext crofton rays");
}

/*
 * Classify all faces of @p bot as exterior/interior using Cauchy-Crofton
 * ray sampling on the already-prepared @p rtip.
 *
 * On success allocates *face_exterior_out (caller must bu_free) and returns
 * the count of exterior faces (>= 0).  Returns -1 on error.
 */
static int
bot_exterior_classify_crofton(struct rt_i *rtip,
			      struct rt_bot_internal *bot,
			      int **face_exterior_out,
			      const struct bot_exterior_opts *opts)
{
    if (!rtip || !bot || !face_exterior_out || !opts)
	return -1;

    int *mask = (int *)bu_calloc(bot->num_faces, sizeof(int), "face_exterior");
    if (!mask)
	return -1;

    if (!bot->num_faces) {
	*face_exterior_out = mask;
	return 0;
    }

    /* Tight bounding sphere from actual soltab extents (mirrors crofton.cpp) */
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

    double R;
    point_t center;
    if (tight_min[X] < MAX_FASTF) {
	VADD2SCALE(center, tight_max, tight_min, 0.5);
	vect_t diag;
	VSUB2(diag, tight_max, tight_min);
	R = 0.5 * MAGNITUDE(diag);
	if (R <= 0.0) R = rtip->rti_radius;
    } else {
	R = rtip->rti_radius;
	VADD2SCALE(center, rtip->mdl_max, rtip->mdl_min, 0.5);
    }

    if (R <= 0.0) {
	*face_exterior_out = mask;
	return 0;
    }

    /* Per-CPU resources */
    struct resource *resources = (struct resource *)bu_calloc(
	MAX_PSW, sizeof(struct resource), "ext crofton resources");
    for (int i = 0; i < MAX_PSW; i++)
	rt_init_resource(&resources[i], i, rtip);

    /* Application template */
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i         = rtip;
    ap.a_hit          = ext_crofton_hit;
    ap.a_miss         = ext_crofton_miss;
    ap.a_overlap      = NULL;
    ap.a_multioverlap = NULL;
    ap.a_logoverlap   = rt_silent_logoverlap;
    ap.a_resource     = resources;
    ap.a_onehit       = 0;

    /* Shared SA accumulators */
    struct ext_crofton_shared shared;
    memset(&shared, 0, sizeof(shared));
    shared.sem_stats = bu_semaphore_register("EXT_CROFTON_STATS");

    /* Convergence loop: grow batch by 1.5× each iteration until the SA
     * estimate changes less than 1% in each of two successive deltas
     * (i.e., three consecutive iteration values are all close).
     * BASE_BATCH = 10 000: large enough to sample broad coverage of
     * a complex model in the first iteration, small enough to converge
     * quickly on simple geometry.
     *
     * Fixed-seed LCG: keeps phase-1 ray generation deterministic across
     * runs so iteration counts and SA estimates are reproducible. */
    const double FOUR_PI = 4.0 * M_PI;
    const size_t BASE_BATCH = 10000u;
    double prev2_sa = -2.0, prev1_sa = -1.0, curr_sa = 0.0;
    size_t iteration = 0;
    uint32_t crofton_seed = 0xC0FFEE01u;   /* deterministic; never zero */

    auto pass1_start = std::chrono::steady_clock::now();

    do {
	size_t nrays = BASE_BATCH;
	if (iteration > 0) {
	    double factor = pow(1.5, (double)iteration);
	    size_t scaled  = (size_t)((double)BASE_BATCH * factor);
	    nrays = (scaled > BASE_BATCH) ? scaled : BASE_BATCH;
	}

	ext_crofton_do_iteration(&ap, resources, nrays, R, center,
				 &shared, mask, bot->num_faces, &crofton_seed);
	iteration++;

	if (shared.total_rays == 0) break;

	curr_sa = FOUR_PI * R * R
	    * (double)shared.total_crossings
	    / (2.0 * (double)shared.total_rays);

	bu_log("bot exterior (Crofton): iter %zu, total_rays=%zu, "
	       "SA_est=%.2f mm^2\n",
	       iteration, (size_t)shared.total_rays, curr_sa);

	if (iteration >= 3) {
	    double tpercent = 1.0;
	    /* d_latest: change between current and previous iteration.
	     * d_prior:  change between the iteration before that and the
	     *           one before that.  Both must be < tpercent to converge. */
	    double d_latest = (prev1_sa > 0.0)
		? fabs(curr_sa   - prev1_sa) / prev1_sa * 100.0 : 999.0;
	    double d_prior  = (prev2_sa > 0.0)
		? fabs(prev1_sa  - prev2_sa) / prev2_sa * 100.0 : 999.0;
	    if (d_latest <= tpercent && d_prior <= tpercent)
		break;
	}

	prev2_sa = prev1_sa;
	prev1_sa = curr_sa;

    } while (1);

    double pass1_sec = std::chrono::duration<double>(
	std::chrono::steady_clock::now() - pass1_start).count();

    if (opts->verbose)
	bu_log("bot exterior (Crofton): phase 1 complete — "
	       "%zu iteration(s), %.2fs, SA_est=%.2f mm^2\n",
	       iteration, pass1_sec, curr_sa);

    /* ================================================================
     * Phase 2: parallel focused per-triangle sampling.
     *
     * Each unclassified face is processed by one of ncpus worker threads
     * that pull work from a ConcurrentQueue.  Rays originate from random
     * points ON the triangle in uniformly random outward directions and
     * shoot the complete model.
     * ================================================================ */

    /* Save the post-phase-1 mask for visibility-strict re-checking.
     * Only allocated when both vis_threshold and vis_strict are active. */
    int *phase1_mask = NULL;
    if (opts->vis_strict && opts->vis_threshold > 0.0) {
	phase1_mask = (int *)bu_calloc(bot->num_faces, sizeof(int), "phase1_mask");
	memcpy(phase1_mask, mask, bot->num_faces * sizeof(int));
    }

    {
	std::vector<size_t> unclassified;
	for (size_t i = 0; i < bot->num_faces; i++) {
	    if (!mask[i])
		unclassified.push_back(i);
	}

	if (!unclassified.empty()) {
	    size_t n_unc = unclassified.size();

	    /* Budget: total second-pass wall time ≤ PHASE2_FACTOR × initial pass.
	     * Distribute evenly across unclassified faces.
	     *
	     * No per-face floor is applied here: the worker loop already
	     * guarantees at least BATCH (64) rays per face regardless of the
	     * budget (the time check is gated on total_rays > 0, so it fires
	     * only after the first batch completes). */
	    double total_p2_budget = PHASE2_FACTOR * pass1_sec;
	    double per_face_budget = total_p2_budget / (double)n_unc;

	    if (opts->vis_threshold > 0.0)
		bu_log("bot exterior [2nd pass]: visibility threshold mode "
		       "(threshold=%.4f)\n", opts->vis_threshold);
	    bu_log("bot exterior [2nd pass]: %zu unclassified face(s); "
		   "initial pass %.2fs, budget %.2fs total "
		   "(%.6fs/face)\n",
		   n_unc, pass1_sec, total_p2_budget,
		   per_face_budget);

	    /* Load work queue — use a single ProducerToken so all items land
	     * in one internal sub-queue, letting workers drain it via
	     * try_dequeue_from_producer without scanning other sub-queues. */
	    moodycamel::ConcurrentQueue<size_t> face_queue(n_unc);
	    moodycamel::ProducerToken face_ptok(face_queue);
	    for (size_t fi : unclassified)
		face_queue.enqueue(face_ptok, fi);

	    /* Shared progress state */
	    ext_focused2_progress prog;
	    prog.faces_done.store(0, std::memory_order_relaxed);
	    prog.faces_total    = n_unc;
	    prog.last_print_us.store(0, std::memory_order_relaxed);
	    prog.pass2_start    = std::chrono::steady_clock::now();

	    /* One worker per available CPU; reuse the Phase-1 resource
	     * slots (Phase 1 has completed so they are idle). */
	    size_t ncpus2 = bu_avail_cpus();
	    if (ncpus2 < 1)  ncpus2 = 1;
	    if (ncpus2 > (size_t)MAX_PSW) ncpus2 = (size_t)MAX_PSW;

	    std::vector<struct ext_focused2_worker_data> wdata(ncpus2);
	    /* Seed each worker with a deterministic, worker-index-derived hash
	     * so phase-2 results are reproducible across runs on the same
	     * machine.  A fixed base constant is mixed with the worker index
	     * via the same Murmur3-finalizer mix used previously, replacing
	     * the former nanosecond timestamp. */
	    for (size_t i = 0; i < ncpus2; i++) {
		/* Mix a fixed base with the 1-based worker index */
		uint64_t h = 0x9E3779B97F4A7C15ULL
		    ^ ((uint64_t)(i + 1) * 6364136223846793005ULL);
		h = (h ^ (h >> 33)) * 0xff51afd7ed558ccdULL;
		h = (h ^ (h >> 33)) * 0xc4ceb9fe1a85ec53ULL;
		h ^= (h >> 33);
		wdata[i].rtip                = rtip;
		wdata[i].bot                 = bot;
		wdata[i].mask                = mask;
		wdata[i].face_queue          = &face_queue;
		wdata[i].ptok                = &face_ptok;
		wdata[i].per_face_budget_sec = per_face_budget;
		wdata[i].rand_seed           = (uint32_t)h;
		wdata[i].resource            = &resources[i];
		wdata[i].progress            = &prog;
		wdata[i].n_classified        = 0;
		wdata[i].vis_threshold       = opts->vis_threshold;
		wdata[i].disable_opportunistic = (opts->vis_threshold > 0.0) ? 1 : 0;
		wdata[i].max_flip_sec        = 0.0;
		wdata[i].sum_flip_sec        = 0.0;
		wdata[i].n_flip_timed        = 0;
	    }

	    std::vector<std::thread> threads;
	    threads.reserve(ncpus2);
	    for (size_t i = 0; i < ncpus2; i++)
		threads.emplace_back(ext_focused2_worker, &wdata[i]);
	    for (auto &t : threads)
		t.join();

	    size_t newly_classified = 0;
	    double max_flip = 0.0, sum_flip = 0.0;
	    size_t n_flip = 0;
	    for (size_t i = 0; i < ncpus2; i++) {
		newly_classified += wdata[i].n_classified;
		if (wdata[i].max_flip_sec > max_flip)
		    max_flip = wdata[i].max_flip_sec;
		sum_flip += wdata[i].sum_flip_sec;
		n_flip   += wdata[i].n_flip_timed;
	    }

	    double pass2_sec = std::chrono::duration<double>(
		std::chrono::steady_clock::now() - prog.pass2_start).count();

	    bu_log("bot exterior [2nd pass]: done in %.2fs, "
		   "%zu/%zu faces newly classified as exterior\n",
		   pass2_sec, newly_classified, n_unc);

	    /* Report flip-time statistics under -v/--verbose.
	     * These tell the caller how long per-face sampling actually
	     * needed to run before exterior faces were confirmed. */
	    if (opts->verbose) {
		if (n_flip > 0 && pass1_sec > 0.0) {
		    double avg_flip = sum_flip / (double)n_flip;
		    bu_log("bot exterior [2nd pass]: flip-time stats over %zu "
			   "confirmed exterior face(s): max=%.4fs avg=%.4fs\n",
			   n_flip, max_flip, avg_flip);
		} else if (n_flip == 0 && newly_classified == 0) {
		    bu_log("bot exterior [2nd pass]: no faces flipped to "
			   "exterior (all unclassified faces appear "
			   "interior)\n");
		}
	    }
	}
    }   /* end anonymous block (unclassified vector) */

    /* ================================================================
     * Visibility-strict phase: re-check phase-1 triangles.
     *
     * When --visibility-threshold AND --visibility-strict are both set,
     * every triangle that was classified in phase 1 (the broad Crofton
     * sweep) is re-sampled with the same per-triangle ray budget used
     * in phase 2 and must meet the visibility threshold to be retained.
     * Opportunistic marking is disabled here: each face is evaluated
     * independently so that no face bypasses the threshold test.
     * ================================================================ */
    if (phase1_mask && opts->vis_threshold > 0.0) {
	std::vector<size_t> p1faces;
	for (size_t i = 0; i < bot->num_faces; i++) {
	    if (phase1_mask[i])
		p1faces.push_back(i);
	}
	bu_free(phase1_mask, "phase1_mask");
	phase1_mask = NULL;

	if (!p1faces.empty()) {
	    size_t n_p1 = p1faces.size();

	    /* Per-face budget for the strict re-verification phase (phase 3):
	     * same derivation as phase 2; no floor applied for the same reason. */
	    double total_strict_phase_budget = PHASE2_FACTOR * pass1_sec;
	    double per_face_strict_budget    = total_strict_phase_budget / (double)n_p1;

	    bu_log("bot exterior [strict check]: %zu phase-1 face(s) to re-verify; "
		   "budget %.1fs total (%.4fs/face)\n",
		   n_p1, total_strict_phase_budget, per_face_strict_budget);

	    /* Strict mask: workers write 1 only if threshold is met. */
	    int *strict_mask = (int *)bu_calloc(bot->num_faces, sizeof(int),
					       "strict_mask");

	    moodycamel::ConcurrentQueue<size_t> strict_queue(n_p1);
	    moodycamel::ProducerToken strict_ptok(strict_queue);
	    for (size_t fi : p1faces)
		strict_queue.enqueue(strict_ptok, fi);

	    ext_focused2_progress sprog;
	    sprog.faces_done.store(0, std::memory_order_relaxed);
	    sprog.faces_total   = n_p1;
	    sprog.last_print_us.store(0, std::memory_order_relaxed);
	    sprog.pass2_start   = std::chrono::steady_clock::now();

	    size_t ncpus_s = bu_avail_cpus();
	    if (ncpus_s < 1)  ncpus_s = 1;
	    if (ncpus_s > (size_t)MAX_PSW) ncpus_s = (size_t)MAX_PSW;

	    std::vector<struct ext_focused2_worker_data> swdata(ncpus_s);
	    /* Deterministic seeds for strict-phase workers; use a different
	     * base constant from phase 2 to avoid correlated streams. */
	    for (size_t i = 0; i < ncpus_s; i++) {
		uint64_t h = 0x6C62272E07BB0142ULL
		    ^ ((uint64_t)(i + 7) * 6364136223846793005ULL);
		h = (h ^ (h >> 33)) * 0xff51afd7ed558ccdULL;
		h = (h ^ (h >> 33)) * 0xc4ceb9fe1a85ec53ULL;
		h ^= (h >> 33);
		swdata[i].rtip                  = rtip;
		swdata[i].bot                   = bot;
		swdata[i].mask                  = strict_mask;
		swdata[i].face_queue            = &strict_queue;
		swdata[i].ptok                  = &strict_ptok;
		swdata[i].per_face_budget_sec   = per_face_strict_budget;
		swdata[i].rand_seed             = (uint32_t)h;
		swdata[i].resource              = &resources[i];
		swdata[i].progress              = &sprog;
		swdata[i].n_classified          = 0;
		swdata[i].vis_threshold         = opts->vis_threshold;
		swdata[i].disable_opportunistic = 1; /* strict: no free rides */
	    }

	    std::vector<std::thread> sthreads;
	    sthreads.reserve(ncpus_s);
	    for (size_t i = 0; i < ncpus_s; i++)
		sthreads.emplace_back(ext_focused2_worker, &swdata[i]);
	    for (auto &st : sthreads)
		st.join();

	    /* Clear mask entries for phase-1 faces that failed the threshold. */
	    size_t rejected = 0;
	    for (size_t fi : p1faces) {
		if (!strict_mask[fi]) {
		    mask[fi] = 0;
		    rejected++;
		}
	    }
	    bu_free(strict_mask, "strict_mask");

	    double strict_sec = std::chrono::duration<double>(
		std::chrono::steady_clock::now() - sprog.pass2_start).count();
	    bu_log("bot exterior [strict check]: done in %.1fs; "
		   "%zu/%zu phase-1 face(s) rejected by visibility threshold\n",
		   strict_sec, rejected, n_p1);
	}
    } else if (phase1_mask) {
	bu_free(phase1_mask, "phase1_mask");
	phase1_mask = NULL;
    }

    /* Count exterior faces */
    size_t n_ext = 0;
    for (size_t i = 0; i < bot->num_faces; i++)
	if (mask[i]) n_ext++;

    bu_log("bot exterior (Crofton): %zu/%zu faces exterior "
	   "(SA_est=%.2f mm^2)\n",
	   n_ext, bot->num_faces, curr_sa);

    /* Clean up resources: null out rti_resources slots first so that the
     * caller's rt_free_rti() does not try to re-clean already-freed memory
     * (mirrors the pattern in src/librt/primitives/crofton.cpp). */
    for (int i = 0; i < MAX_PSW; i++) {
	if (resources[i].re_magic == RESOURCE_MAGIC) {
	    rt_clean_resource_basic(rtip, &resources[i]);
	    BU_PTBL_SET(&rtip->rti_resources, i, NULL);
	}
    }
    bu_free(resources, "ext crofton resources");

    if (n_ext > (size_t)INT_MAX) {
	bu_log("bot exterior: exterior face count exceeds int range\n");
	bu_free(mask, "face_exterior");
	return -1;
    }

    *face_exterior_out = mask;
    return (int)n_ext;
}


/* ======================================================================
 * Shared face-rebuild helper
 * ====================================================================== */

/*
 * Replace bot->faces in-place with only the faces whose mask entry is 1.
 * num_exterior must equal the count of set entries in face_exterior[].
 * Returns the number of faces removed.
 */
static size_t
bot_apply_exterior_mask(struct rt_bot_internal *bot,
			const int *face_exterior, size_t num_exterior)
{
    if (!bot || !bot->faces || !face_exterior || !bot->num_faces)
	return 0;

    size_t actual_exterior = 0;
    for (size_t i = 0; i < bot->num_faces; i++) {
	if (face_exterior[i])
	    actual_exterior++;
    }

    if (num_exterior != actual_exterior) {
	bu_log("bot exterior: warning - classifier reported %zu exterior faces, "
	       "mask contains %zu; using mask count\n",
	       num_exterior, actual_exterior);
    }

    size_t removed  = bot->num_faces - actual_exterior;
    int   *newfaces = (int *)bu_calloc(actual_exterior, 3 * sizeof(int),
				       "bot exterior new faces");
    size_t j = 0;
    for (size_t i = 0; i < bot->num_faces; i++) {
	if (face_exterior[i]) {
	    newfaces[j * 3 + 0] = bot->faces[i * 3 + 0];
	    newfaces[j * 3 + 1] = bot->faces[i * 3 + 1];
	    newfaces[j * 3 + 2] = bot->faces[i * 3 + 2];
	    j++;
	}
    }

    bu_free(bot->faces, "bot exterior old faces");
    bot->faces     = newfaces;
    bot->num_faces = actual_exterior;
    return removed;
}


/* ======================================================================
 * Usage helper
 * ====================================================================== */

static void
exterior_usage(struct bu_vls *str, const char *cmd, struct bu_opt_desc *d)
{
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
    bu_vls_printf(str,
	"If output_name is omitted, the result is written to input_bot_exterior.\n"
	"Use -i/--in-place to overwrite the input BoT directly.\n"
	"Default method is Cauchy-Crofton ray sampling.\n");
}


/* ======================================================================
 * _bot_cmd_exterior — new-style subcommand handler
 * ====================================================================== */

extern "C" int
_bot_cmd_exterior(void *bs, int argc, const char **argv)
{
    const char *usage_string  = "bot exterior [options] <objname> [output_name]";
    const char *purpose_string =
	"reduce BoT to exterior (outward-facing / water-reachable) faces";

    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    /* Consume the subcommand name token. */
    argc--; argv++;

    int       print_help  = 0;
    int       in_place    = 0;
    int       flood_fill  = 0;
    int       verbose     = 0;
    fastf_t   voxel_size  = 0.0;
    fastf_t   vis_threshold = 0.0;
    int       vis_strict  = 0;

    struct bu_opt_desc d[8];
    BU_OPT(d[0], "h",       "help",                "",      NULL,            &print_help,
	   "Print help");
    BU_OPT(d[1], "i",   "in-place",                "",      NULL,            &in_place,
	   "Overwrite input BoT in-place (no output_name needed)");
    BU_OPT(d[2], "",  "visibility-threshold", "ratio",  &bu_opt_fastf_t, &vis_threshold,
	   "Minimum fraction [0,1] of phase-2 rays that must see a face to include it "
	   "(default 0 = disabled; enabling this is slower.  no-op with -f)");
    BU_OPT(d[3], "",  "visibility-strict",       "",      NULL,            &vis_strict,
	   "Verify phase-1 faces satisfy the threshold "
	   "(requires --visibility-threshold; potentially very slow. no-op with -f)");
    BU_OPT(d[4], "v",    "verbose",                "",      NULL,            &verbose,
	   "Print extra metrics: per-phase timings, flip-time stats");
    BU_OPT(d[5], "f", "flood-fill",                "",      NULL,            &flood_fill,
	   "Use voxel flood-fill / water-filling method (requires OpenVDB)");
    BU_OPT(d[6], "",  "voxel-size",            "size", &bu_opt_fastf_t,  &voxel_size,
	   "Voxel edge length for flood-fill (default: auto-computed)");
    BU_OPT_NULL(d[7]);

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	exterior_usage(gb->gedp->ged_result_str, "bot exterior", d);
	return GED_HELP;
    }

    if (in_place && argc != 1) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "--in-place requires exactly one object name\n%s\n",
		      usage_string);
	return BRLCAD_ERROR;
    }

    if (argc > 2) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    /* Validate option combinations. */
    if (vis_threshold < 0.0 || vis_threshold > 1.0) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "--visibility-threshold must be in [0,1] (got %g)\n",
		      (double)vis_threshold);
	return BRLCAD_ERROR;
    }
    if (vis_strict && vis_threshold <= 0.0) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "--visibility-strict requires --visibility-threshold > 0\n");
	return BRLCAD_ERROR;
    }

    const char *input_name = argv[0];

    /* Determine output name. */
    struct bu_vls output_name = BU_VLS_INIT_ZERO;
    if (in_place) {
	bu_vls_sprintf(&output_name, "%s", input_name);
    } else if (argc == 2) {
	bu_vls_sprintf(&output_name, "%s", argv[1]);
    } else {
	bu_vls_sprintf(&output_name, "%s_exterior", input_name);
    }

    /* For non-in-place writes, verify the output name is free. */
    if (!in_place &&
	db_lookup(gb->gedp->dbip, bu_vls_cstr(&output_name), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "Object %s already exists\n", bu_vls_cstr(&output_name));
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }

    /* Load the input BoT. */
    if (_bot_obj_setup(gb, input_name) & BRLCAD_ERROR) {
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    RT_BOT_CK_MAGIC(bot);

    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "%s is a PLATE MODE BoT; exterior classification is unsupported\n",
		      input_name);
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }

    /* Build the raytrace context. */
    struct application  ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rt_new_rti(gb->gedp->dbip);

    if (rt_gettree(ap.a_rt_i, input_name)) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "Unable to load %s for raytracing\n", input_name);
	rt_free_rti(ap.a_rt_i);
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }
    rt_prep(ap.a_rt_i);

    /* ----------------------------------------------------------------
     * Classify faces.
     * ---------------------------------------------------------------- */
    struct bot_exterior_opts opts;
    opts.vis_threshold  = (double)vis_threshold;
    opts.vis_strict     = vis_strict;
    opts.verbose        = verbose;

    int *face_exterior = NULL;
    int  n_ext;

    if (flood_fill) {
#ifdef HAVE_OPENVDB_BOT_EXTERIOR
	bu_log("bot exterior: using flood-fill (water-filling) method\n");
	face_exterior = (int *)bu_calloc(bot->num_faces, sizeof(int), "face_exterior");
	n_ext = bot_flood_exterior_classify(ap.a_rt_i, bot, face_exterior,
					    (double)voxel_size);
	if (n_ext < 0) {
	    bu_free(face_exterior, "face_exterior");
	    rt_free_rti(ap.a_rt_i);
	    bu_vls_free(&output_name);
	    bu_vls_printf(gb->gedp->ged_result_str,
			  "Flood-fill exterior classification failed\n");
	    return BRLCAD_ERROR;
	}
#else
	bu_vls_printf(gb->gedp->ged_result_str,
		      "Note: --flood-fill requires OpenVDB (not compiled in); "
		      "falling back to Crofton ray-sampling\n");
	bu_log("bot exterior: falling back to Crofton ray-sampling method\n");
	n_ext = bot_exterior_classify_crofton(ap.a_rt_i, bot, &face_exterior, &opts);
#endif
    } else {
	bu_log("bot exterior: using Crofton ray-sampling method\n");
	n_ext = bot_exterior_classify_crofton(ap.a_rt_i, bot, &face_exterior, &opts);
    }

    rt_free_rti(ap.a_rt_i);

    /* ----------------------------------------------------------------
     * Handle degenerate classification results.
     * ---------------------------------------------------------------- */
    if (!face_exterior || n_ext < 0) {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "Exterior face classification failed\n");
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }

    if (n_ext == 0) {
	bu_free(face_exterior, "face_exterior");
	bu_vls_printf(gb->gedp->ged_result_str,
		      "No exterior faces identified; "
		      "verify the mesh is a closed solid\n");
	bu_vls_free(&output_name);
	return BRLCAD_ERROR;
    }

    /* ----------------------------------------------------------------
     * Rebuild the face list and condense vertices.
     * ---------------------------------------------------------------- */
    size_t removed = 0;
    if ((size_t)n_ext < bot->num_faces) {
	removed = bot_apply_exterior_mask(bot, face_exterior, (size_t)n_ext);
    } else {
	bu_vls_printf(gb->gedp->ged_result_str,
		      "All %zu faces are exterior; no interior faces to remove\n",
		      bot->num_faces);
    }
    bu_free(face_exterior, "face_exterior");

    size_t vcount = rt_bot_condense(bot);

    bu_vls_printf(gb->gedp->ged_result_str,
		  "Removed %zu interior face(s) and %zu unreferenced vertex/vertices\n",
		  removed, vcount);

    /* ----------------------------------------------------------------
     * Write the result.
     * ---------------------------------------------------------------- */
    int ret = BRLCAD_OK;

    if (in_place) {
	if (rt_db_put_internal(gb->dp, gb->gedp->dbip, gb->intern,
			       &rt_uniresource) < 0) {
	    bu_vls_printf(gb->gedp->ged_result_str,
			  "Failed to write %s\n", input_name);
	    ret = BRLCAD_ERROR;
	}
    } else {
	struct directory *outdp =
	    db_diradd(gb->gedp->dbip, bu_vls_cstr(&output_name),
		      RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID,
		      (void *)&gb->intern->idb_type);
	if (outdp == RT_DIR_NULL) {
	    bu_vls_printf(gb->gedp->ged_result_str,
			  "Cannot add %s to directory\n", bu_vls_cstr(&output_name));
	    ret = BRLCAD_ERROR;
	} else if (rt_db_put_internal(outdp, gb->gedp->dbip, gb->intern,
				      &rt_uniresource) < 0) {
	    bu_vls_printf(gb->gedp->ged_result_str,
			  "Failed to write %s\n", bu_vls_cstr(&output_name));
	    ret = BRLCAD_ERROR;
	}
    }

    bu_vls_free(&output_name);
    return ret;
}


/* ======================================================================
 * ged_bot_exterior — legacy top-level GED command
 *
 * Accepts:  bot_exterior [-f] old_bot new_bot
 *
 * Forwards to _bot_cmd_exterior via a minimal _ged_bot_info wrapper.
 * argv[0] ("bot_exterior") plays the role of the subcommand token that
 * _bot_cmd_exterior skips on entry.
 * ====================================================================== */

extern "C" int
ged_bot_exterior(struct ged *gedp, int argc, const char *argv[])
{
    struct _ged_bot_info gb;
    gb.gedp      = gedp;
    gb.intern    = NULL;
    gb.dp        = NULL;
    gb.vbp       = NULL;
    gb.color     = NULL;
    gb.verbosity = 0;
    gb.visualize = 0;
    gb.vlfree    = &rt_vlfree;
    gb.cmds      = NULL;
    gb.gopts     = NULL;

    return _bot_cmd_exterior(&gb, argc, argv);
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
