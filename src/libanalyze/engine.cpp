/*                      E N G I N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file libanalyze/engine.cpp
 *
 * Internal C++17 engine layer for libanalyze (Phases A-E).
 *
 * Implements the types and functions declared in engine.h.  The layer
 * structure mirrors the planned architecture:
 *
 *   Sections:
 *     1. Capture context + ar_*_cb callbacks (Phase A — moved from api.c)
 *     2. ISampler concrete classes (Phase B)
 *     3. IAnalyzer concrete classes (Phase C)
 *     4. namespace analyze: AnalyzeRequest::from_config(),
 *                           apply_request_to_state(),
 *                           run()  <-- uses RAII + ISampler + IAnalyzer (D+E)
 *     5. extern "C" analyze_engine_run() wrapper
 *
 * perform_raytracing(), the worker functions, the convergence loop, and
 * the sampler execution loops remain in api.c (geometry execution adapter
 * layer) and are left unchanged by Phases A-E.
 */

#include "common.h"

#include <cstring>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <vector>

#include "raytrace.h"
#include "vmath.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/parallel.h"

#include "analyze.h"
#include "./analyze_private.h"
#include "./engine.h"


/* ======================================================================
 * Section 1: Capture context and per-event callbacks (Phase A).
 *
 * These are registered with current_state before calling
 * perform_raytracing() and accumulate detected issues into
 * struct analyze_results.  They may be invoked from multiple worker
 * threads concurrently; all mutations of the shared result tables are
 * serialised under ctx->sem.
 *
 * Moved verbatim from api.c in Phase A — no algorithm change.
 * ====================================================================== */

struct ar_capture_ctx {
    struct analyze_results *res;
    int sem; /**< bu_semaphore protecting all list mutations */

    /** Back-pointer to the current_state so callbacks can read
     *  state->current_cell_area for overlap volume accumulation.
     *  Set once in analyze::run() before perform_raytracing(). */
    struct current_state *state;

    /* Presentation-layer render hooks copied from AnalyzeRequest. */
    analyze_overlap_render_fn    overlap_render;
    void                        *overlap_render_data;
    analyze_gap_render_fn        gap_render;
    void                        *gap_render_data;
    analyze_adj_air_render_fn    adj_air_render;
    void                        *adj_air_render_data;
    analyze_exp_air_render_fn    exp_air_render;
    void                        *exp_air_render_data;
    analyze_unconf_air_render_fn unconf_air_render;
    void                        *unconf_air_render_data;
};


/**
 * ar_find_or_insert — locate an existing record by region name(s) or append
 * a new one.  For single-region tables pass name2 == NULL.
 */
static struct analyze_overlap_record *
ar_find_or_insert(struct bu_ptbl *tbl, const char *name1, const char *name2)
{
    size_t i;
    struct analyze_overlap_record *rec;

    for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
rec = (struct analyze_overlap_record *)BU_PTBL_GET(tbl, i);
if (!BU_STR_EQUAL(rec->region1, name1))
    continue;
if (!name2 && !rec->region2)
    return rec;
if (name2 && rec->region2 && BU_STR_EQUAL(rec->region2, name2))
    return rec;
    }

    BU_ALLOC(rec, struct analyze_overlap_record);
    rec->region1  = bu_strdup(name1);
    rec->region2  = name2 ? bu_strdup(name2) : NULL;
    rec->count    = 0;
    rec->max_dist = 0.0;
    rec->estimated_volume = 0.0;
    VSETALL(rec->coord, 0.0);
    bu_ptbl_ins(tbl, (long *)rec);
    return rec;
}

static int
ar_find_object_index(const struct current_state *state, const char *name)
{
    for (int i = 0; i < state->num_objects; i++) {
	if (BU_STR_EQUAL(state->objs[i].o_name, name))
	    return i;
    }
    return -1;
}

static double
ar_total_volume_calc(const struct current_state *state)
{
    double avg_vol = 0.0;
    for (int view = 0; view < state->num_views; view++) {
	avg_vol += state->m_len[view] * (state->area[view] / state->shots[view]);
    }
    return avg_vol / state->num_views;
}

static double
ar_total_mass_calc(const struct current_state *state)
{
    double avg_mass = 0.0;
    for (int view = 0; view < state->num_views; view++) {
	avg_mass += state->m_lenDensity[view] * (state->area[view] / state->shots[view]);
    }
    return avg_mass / state->num_views;
}

static double
ar_object_volume_calc(const struct current_state *state, int obj)
{
    double volume = 0.0;
    for (int view = 0; view < state->num_views; view++) {
	volume += state->objs[obj].o_volume[view];
    }
    return volume / state->num_views;
}

static double
ar_object_mass_calc(const struct current_state *state, int obj)
{
    double mass = 0.0;
    for (int view = 0; view < state->num_views; view++) {
	mass += state->objs[obj].o_mass[view];
    }
    return mass / state->num_views;
}

static double
ar_object_surf_area_calc(const struct current_state *state, int obj)
{
    double area = 0.0;
    double limit = 0.0;
    int mean_count = 0;

    for (int view = 0; view < state->num_views; view++)
	V_MAX(limit, state->objs[obj].o_surf_area[view]);

    limit *= 0.8;
    for (int view = 0; view < state->num_views; view++) {
	if (state->objs[obj].o_surf_area[view] >= limit) {
	    area += state->objs[obj].o_surf_area[view];
	    mean_count++;
	}
    }

    return area / mean_count;
}

static double
ar_total_surf_area_calc(const struct current_state *state)
{
    double total = 0.0;
    for (int i = 0; i < state->num_objects; i++) {
	total += ar_object_surf_area_calc(state, i);
    }
    return total;
}

static void
ar_total_centroid_calc(const struct current_state *state, point_t cent)
{
    vect_t centroid = VINIT_ZERO;
    const fastf_t inv_total_mass = 1 / ar_total_mass_calc(state);
    for (int view = 0; view < state->num_views; view++) {
	vect_t torque;
	fastf_t cell_area = state->area[view] / state->shots[view];
	VSCALE(torque, &state->m_lenTorque[view*3], cell_area);
	VADD2(centroid, centroid, torque);
    }
    VSCALE(centroid, centroid, 1.0/(fastf_t)state->num_views);
    VSCALE(centroid, centroid, inv_total_mass);
    VMOVE(cent, centroid);
}

static void
ar_object_centroid_calc(const struct current_state *state, int obj, point_t cent)
{
    const double avg_mass = ar_object_mass_calc(state, obj);
    VSETALL(cent, 0.0);
    if (ZERO(avg_mass))
	return;

    point_t centroid = VINIT_ZERO;
    const fastf_t inv_total_mass = 1.0/avg_mass;
    for (int view = 0; view < state->num_views; view++) {
	vect_t torque;
	fastf_t cell_area = state->area[view] / state->shots[view];
	VSCALE(torque, &state->objs[obj].o_lenTorque[view*3], cell_area);
	VADD2(centroid, centroid, torque);
    }
    VSCALE(centroid, centroid, 1.0/(fastf_t)state->num_views);
    VSCALE(centroid, centroid, inv_total_mass);
    VMOVE(cent, centroid);
}

static void
ar_total_moments_calc(const struct current_state *state, mat_t moments)
{
    point_t centroid;
    ar_total_centroid_calc(state, centroid);
    const double avg_mass = ar_total_mass_calc(state);
    MAT_ZERO(moments);
    for (int view = 0; view < state->num_views; view++) {
	vectp_t moi = &state->m_moi[view*3];
	vectp_t poi = &state->m_poi[view*3];
	moments[MSX] += moi[X];
	moments[MSY] += moi[Y];
	moments[MSZ] += moi[Z];
	moments[1] += poi[X];
	moments[2] += poi[Y];
	moments[6] += poi[Z];
    }
    moments[MSX] /= (fastf_t)state->num_views;
    moments[MSY] /= (fastf_t)state->num_views;
    moments[MSZ] /= (fastf_t)state->num_views;
    moments[1] /= (fastf_t)state->num_views;
    moments[2] /= (fastf_t)state->num_views;
    moments[6] /= (fastf_t)state->num_views;

    const fastf_t Dx_sq = centroid[X] * centroid[X];
    const fastf_t Dy_sq = centroid[Y] * centroid[Y];
    const fastf_t Dz_sq = centroid[Z] * centroid[Z];
    moments[MSX] -= avg_mass * (Dy_sq + Dz_sq);
    moments[MSY] -= avg_mass * (Dx_sq + Dz_sq);
    moments[MSZ] -= avg_mass * (Dx_sq + Dy_sq);
    moments[1] += avg_mass * centroid[X] * centroid[Y];
    moments[2] += avg_mass * centroid[X] * centroid[Z];
    moments[6] += avg_mass * centroid[Y] * centroid[Z];
    moments[4] = moments[1];
    moments[8] = moments[2];
    moments[9] = moments[6];
}

static void
ar_object_moments_calc(const struct current_state *state, int obj, mat_t moments)
{
    point_t centroid;
    ar_object_centroid_calc(state, obj, centroid);
    const double avg_mass = ar_object_mass_calc(state, obj);
    MAT_ZERO(moments);
    for (int view = 0; view < state->num_views; view++) {
	vectp_t moi = &state->objs[obj].o_moi[view*3];
	vectp_t poi = &state->objs[obj].o_poi[view*3];
	moments[MSX] += moi[X];
	moments[MSY] += moi[Y];
	moments[MSZ] += moi[Z];
	moments[1] += poi[X];
	moments[2] += poi[Y];
	moments[6] += poi[Z];
    }
    moments[MSX] /= (fastf_t)state->num_views;
    moments[MSY] /= (fastf_t)state->num_views;
    moments[MSZ] /= (fastf_t)state->num_views;
    moments[1] /= (fastf_t)state->num_views;
    moments[2] /= (fastf_t)state->num_views;
    moments[6] /= (fastf_t)state->num_views;

    const fastf_t Dx_sq = centroid[X] * centroid[X];
    const fastf_t Dy_sq = centroid[Y] * centroid[Y];
    const fastf_t Dz_sq = centroid[Z] * centroid[Z];
    moments[MSX] -= avg_mass * (Dy_sq + Dz_sq);
    moments[MSY] -= avg_mass * (Dx_sq + Dz_sq);
    moments[MSZ] -= avg_mass * (Dx_sq + Dy_sq);
    moments[1] += avg_mass * centroid[X] * centroid[Y];
    moments[2] += avg_mass * centroid[X] * centroid[Z];
    moments[6] += avg_mass * centroid[Y] * centroid[Z];
    moments[4] = moments[1];
    moments[8] = moments[2];
    moments[9] = moments[6];
}

static double
ar_region_volume_calc(struct current_state *state, int ridx)
{
    double avg_vol = 0.0;
    for (int view = 0; view < state->num_views; view++) {
	double *vv = &state->reg_tbl[ridx].r_volume[view];
	*vv = state->reg_tbl[ridx].r_len[view] * (state->area[view] / state->shots[view]);
	avg_vol += *vv;
    }
    return avg_vol / state->num_views;
}

static double
ar_region_mass_calc(struct current_state *state, int ridx)
{
    double avg_mass = 0.0;
    for (int view = 0; view < state->num_views; view++) {
	double *mm = &state->reg_tbl[ridx].r_mass[view];
	*mm = state->reg_tbl[ridx].r_lenDensity[view] * (state->area[view] / state->shots[view]);
	avg_mass += *mm;
    }
    return avg_mass / state->num_views;
}

static double
ar_region_surf_area_calc(const struct current_state *state, int ridx)
{
    double hi = -INFINITY;
    for (int view = 0; view < state->num_views; view++) {
	V_MAX(hi, state->reg_tbl[ridx].r_surf_area[view]);
    }
    double sa = 0.0;
    int mean_count = 0;
    const double limit = hi * 0.8;
    for (int view = 0; view < state->num_views; view++) {
	if (state->reg_tbl[ridx].r_surf_area[view] >= limit) {
	    sa += state->reg_tbl[ridx].r_surf_area[view];
	    mean_count++;
	}
    }
    return sa / mean_count;
}


static void
ar_overlap_cb(const struct xray *ray, const struct partition *pp,
      const struct region *reg1, const struct region *reg2,
      double depth, void *data)
{
    struct ar_capture_ctx *ctx = (struct ar_capture_ctx *)data;
    struct analyze_overlap_record *rec;
    point_t ihit, ohit;

    VJOIN1(ihit, ray->r_pt, pp->pt_inhit->hit_dist,  ray->r_dir);
    VJOIN1(ohit, ray->r_pt, pp->pt_outhit->hit_dist, ray->r_dir);

    if (ctx->overlap_render)
ctx->overlap_render(reg1->reg_name, reg2->reg_name,
    depth, ihit, ohit, ctx->overlap_render_data);

    bu_semaphore_acquire(ctx->sem);
    rec = ar_find_or_insert(&ctx->res->overlaps,
    reg1->reg_name, reg2->reg_name);
    rec->count++;
    if (depth > rec->max_dist) {
rec->max_dist = depth;
VMOVE(rec->coord, ihit);
    }
    /* Accumulate estimated overlap volume: depth × cell_area.
     * current_cell_area is set by the main thread before each bu_parallel()
     * call and never written by worker threads, so reading it here is safe
     * without holding the semaphore — but we are under ctx->sem already.
     * ctx->state is always non-NULL (set in analyze::run() before callbacks
     * are registered). */
    if (ctx->state->current_cell_area > 0.0)
rec->estimated_volume += depth * ctx->state->current_cell_area;
    bu_semaphore_release(ctx->sem);
}


static void
ar_gap_cb(const struct xray *ray, const struct partition *pp,
  double gap_dist, point_t pt, void *data)
{
    struct ar_capture_ctx *ctx = (struct ar_capture_ctx *)data;
    struct analyze_overlap_record *rec;
    const char *name_after  = (pp && pp->pt_regionp) ? pp->pt_regionp->reg_name : "";
    const char *name_before = (pp && pp->pt_back && pp->pt_back->pt_regionp)
? pp->pt_back->pt_regionp->reg_name : "";

    if (ctx->gap_render) {
point_t gap_start;
VJOIN1(gap_start, pt, -gap_dist, ray->r_dir);
ctx->gap_render(name_after, name_before, gap_dist,
gap_start, pt, ctx->gap_render_data);
    }

    bu_semaphore_acquire(ctx->sem);
    rec = ar_find_or_insert(&ctx->res->gaps, name_after, name_before);
    rec->count++;
    if (gap_dist > rec->max_dist) {
rec->max_dist = gap_dist;
VMOVE(rec->coord, pt);
    }
    bu_semaphore_release(ctx->sem);
}


static void
ar_exp_air_cb(const struct partition *pp, point_t last_out,
      point_t pt, point_t opt, void *data)
{
    struct ar_capture_ctx *ctx = (struct ar_capture_ctx *)data;
    struct analyze_overlap_record *rec;
    const char *name = (pp && pp->pt_regionp) ? pp->pt_regionp->reg_name : "";
    double thickness = (pp) ? pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist : 0.0;

    if (ctx->exp_air_render)
ctx->exp_air_render(name, pt, opt, ctx->exp_air_render_data);

    bu_semaphore_acquire(ctx->sem);
    rec = ar_find_or_insert(&ctx->res->exp_air, name, NULL);
    rec->count++;
    if (thickness > rec->max_dist) {
rec->max_dist = thickness;
VMOVE(rec->coord, last_out);
    }
    bu_semaphore_release(ctx->sem);
}


static void
ar_adj_air_cb(const struct xray *ray, const struct partition *pp,
      point_t pt, void *data)
{
    struct ar_capture_ctx *ctx = (struct ar_capture_ctx *)data;
    struct analyze_overlap_record *rec;
    const char *name_air   = (pp && pp->pt_regionp) ? pp->pt_regionp->reg_name : "";
    const char *name_solid = (pp && pp->pt_back && pp->pt_back->pt_regionp)
? pp->pt_back->pt_regionp->reg_name : "";

    if (ctx->adj_air_render) {
double thickness = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
point_t out_pt;
VJOIN1(out_pt, pt, thickness * 0.25, ray->r_dir);
ctx->adj_air_render(name_solid, name_air, pt, out_pt,
    ctx->adj_air_render_data);
    }

    bu_semaphore_acquire(ctx->sem);
    rec = ar_find_or_insert(&ctx->res->adj_air, name_solid, name_air);
    rec->count++;
    VMOVE(rec->coord, pt);
    bu_semaphore_release(ctx->sem);
}


static void
ar_first_air_cb(const struct xray *UNUSED(ray), const struct partition *pp,
void *data)
{
    struct ar_capture_ctx *ctx = (struct ar_capture_ctx *)data;
    struct analyze_overlap_record *rec;
    const char *name = (pp && pp->pt_regionp) ? pp->pt_regionp->reg_name : "";

    bu_semaphore_acquire(ctx->sem);
    rec = ar_find_or_insert(&ctx->res->first_air, name, NULL);
    rec->count++;
    bu_semaphore_release(ctx->sem);
}


static void
ar_last_air_cb(const struct xray *UNUSED(ray), const struct partition *pp,
       void *data)
{
    struct ar_capture_ctx *ctx = (struct ar_capture_ctx *)data;
    struct analyze_overlap_record *rec;
    const char *name = (pp && pp->pt_regionp) ? pp->pt_regionp->reg_name : "";

    bu_semaphore_acquire(ctx->sem);
    rec = ar_find_or_insert(&ctx->res->last_air, name, NULL);
    rec->count++;
    bu_semaphore_release(ctx->sem);
}


static void
ar_unconf_air_cb(const struct xray *ray,
 const struct partition *in_p, const struct partition *out_p,
 void *data)
{
    struct ar_capture_ctx *ctx = (struct ar_capture_ctx *)data;
    struct analyze_overlap_record *rec;
    const char *name_in  = (in_p  && in_p->pt_regionp)  ? in_p->pt_regionp->reg_name  : "";
    const char *name_out = (out_p && out_p->pt_regionp)  ? out_p->pt_regionp->reg_name : "";
    double depth = (in_p && out_p) ?
in_p->pt_inhit->hit_dist - out_p->pt_outhit->hit_dist : 0.0;

    if (ctx->unconf_air_render && ray) {
point_t ihit, ohit;
VJOIN1(ihit, ray->r_pt, in_p->pt_inhit->hit_dist,   ray->r_dir);
VJOIN1(ohit, ray->r_pt, out_p->pt_outhit->hit_dist, ray->r_dir);
ctx->unconf_air_render(name_in, name_out, ihit, ohit,
       ctx->unconf_air_render_data);
    }

    bu_semaphore_acquire(ctx->sem);
    rec = ar_find_or_insert(&ctx->res->unconf_air, name_in, name_out);
    rec->count++;
    if (depth > rec->max_dist) {
rec->max_dist = depth;
if (ray && in_p)
    VJOIN1(rec->coord, ray->r_pt, in_p->pt_inhit->hit_dist, ray->r_dir);
    }
    bu_semaphore_release(ctx->sem);
}


/* ======================================================================
 * Section 2: ISampler concrete classes (Phase B).
 *
 * Each class encapsulates one sampler strategy.  For Phase B the actual
 * ray-firing loop stays in perform_raytracing(); the concrete class only
 * sets the state->sampler discriminator (and any sampler-specific extra
 * fields) that perform_raytracing() reads at dispatch time.
 *
 * finalize() hooks exist for post-processing that cannot live inside the
 * firing loop; currently all backends do their post-processing inside
 * api.c, so all finalize() implementations are no-ops in Phase B.
 * ====================================================================== */

namespace {

/** Triple-grid sampler: axis-aligned rectangular grid (default). */
class TripleGridSampler : public analyze::ISampler {
public:
    void configure(const analyze::AnalyzeRequest & /*req*/,
   struct current_state *state) const override
    {
state->sampler = ANALYZE_SAMPLER_TRIPLE_GRID;
    }
};

/** Rotated-grid sampler: grid along arbitrary az/el direction. */
class RotatedGridSampler : public analyze::ISampler {
public:
    void configure(const analyze::AnalyzeRequest & /*req*/,
   struct current_state *state) const override
    {
state->sampler = ANALYZE_SAMPLER_ROTATED;
/* az/el already written by apply_request_to_state(). */
    }
};

/** Crofton sampler: isotropic random rays + Cauchy-Crofton SA formula. */
class CroftonSampler : public analyze::ISampler {
public:
    void configure(const analyze::AnalyzeRequest &req,
   struct current_state *state) const override
    {
state->sampler = ANALYZE_SAMPLER_CROFTON;
/* ray count already written by apply_request_to_state();
 * reinforce here in case caller bypasses that path. */
if (req.n_crofton_rays > 0)
    state->crofton_n_rays = req.n_crofton_rays;
    }
    /* Crofton SA post-processing lives inside shoot_rays_crofton()
     * in api.c and fires automatically — no extra finalize() needed. */
};

/** View-plane sampler: single-plane grid from explicit eye/view data. */
class ViewPlaneSampler : public analyze::ISampler {
public:
    void configure(const analyze::AnalyzeRequest & /*req*/,
   struct current_state *state) const override
    {
state->sampler = ANALYZE_SAMPLER_VIEW_PLANE;
/* view_size / eye / quat already written by apply_request_to_state(). */
    }
};

/* ======================================================================
 * Section 3: IAnalyzer concrete classes (Phase C).
 *
 * Each class registers exactly the callback it needs on current_state.
 * All callbacks write directly into the shared ar_capture_ctx, which
 * holds the result pointer and the protecting semaphore.  No separate
 * harvest step is required.
 * ====================================================================== */

class OverlapAnalyzer : public analyze::IAnalyzer {
public:
    void register_callbacks(struct current_state *state,
    struct ar_capture_ctx *ctx) const override
    {
state->overlaps_callback = ar_overlap_cb;
state->overlaps_callback_data = ctx;
    }
};

class GapAnalyzer : public analyze::IAnalyzer {
public:
    void register_callbacks(struct current_state *state,
    struct ar_capture_ctx *ctx) const override
    {
state->gaps_callback = ar_gap_cb;
state->gaps_callback_data = ctx;
    }
};

class ExpAirAnalyzer : public analyze::IAnalyzer {
public:
    void register_callbacks(struct current_state *state,
    struct ar_capture_ctx *ctx) const override
    {
state->exp_air_callback = ar_exp_air_cb;
state->exp_air_callback_data = ctx;
    }
};

class AdjAirAnalyzer : public analyze::IAnalyzer {
public:
    void register_callbacks(struct current_state *state,
    struct ar_capture_ctx *ctx) const override
    {
state->adj_air_callback = ar_adj_air_cb;
state->adj_air_callback_data = ctx;
    }
};

class FirstAirAnalyzer : public analyze::IAnalyzer {
public:
    void register_callbacks(struct current_state *state,
    struct ar_capture_ctx *ctx) const override
    {
state->first_air_callback = ar_first_air_cb;
state->first_air_callback_data = ctx;
    }
};

class LastAirAnalyzer : public analyze::IAnalyzer {
public:
    void register_callbacks(struct current_state *state,
    struct ar_capture_ctx *ctx) const override
    {
state->last_air_callback = ar_last_air_cb;
state->last_air_callback_data = ctx;
    }
};

class UnconfAirAnalyzer : public analyze::IAnalyzer {
public:
    void register_callbacks(struct current_state *state,
    struct ar_capture_ctx *ctx) const override
    {
state->unconf_air_callback = ar_unconf_air_cb;
state->unconf_air_callback_data = ctx;
    }
};

} /* anonymous namespace */


/* ======================================================================
 * ISampler and IAnalyzer factory + default implementations (Phase B+C).
 * ====================================================================== */

namespace analyze {

/* ISampler base default implementations */
void
ISampler::configure(const AnalyzeRequest & /*req*/,
    struct current_state * /*state*/) const
{
    /* Base default: no extra configuration needed.
     * apply_request_to_state() already set state->sampler. */
}

void
ISampler::finalize(struct current_state * /*state*/,
   struct analyze_results * /*res*/) const
{
    /* Base default: no post-processing. */
}

std::unique_ptr<ISampler>
ISampler::create(int sampler_type)
{
    switch (sampler_type) {
case ANALYZE_SAMPLER_ROTATED:    return std::make_unique<RotatedGridSampler>();
case ANALYZE_SAMPLER_CROFTON:    return std::make_unique<CroftonSampler>();
case ANALYZE_SAMPLER_VIEW_PLANE: return std::make_unique<ViewPlaneSampler>();
default:                         return std::make_unique<TripleGridSampler>();
    }
}

/* IAnalyzer factory */
std::vector<std::unique_ptr<IAnalyzer>>
IAnalyzer::from_flags(int flags)
{
    std::vector<std::unique_ptr<IAnalyzer>> result;
    if (flags & ANALYZE_OVERLAPS)   result.push_back(std::make_unique<OverlapAnalyzer>());
    if (flags & ANALYZE_GAP)        result.push_back(std::make_unique<GapAnalyzer>());
    if (flags & ANALYZE_EXP_AIR)    result.push_back(std::make_unique<ExpAirAnalyzer>());
    if (flags & ANALYZE_ADJ_AIR)    result.push_back(std::make_unique<AdjAirAnalyzer>());
    if (flags & ANALYZE_FIRST_AIR)  result.push_back(std::make_unique<FirstAirAnalyzer>());
    if (flags & ANALYZE_LAST_AIR)   result.push_back(std::make_unique<LastAirAnalyzer>());
    if (flags & ANALYZE_UNCONF_AIR) result.push_back(std::make_unique<UnconfAirAnalyzer>());
    return result;
}


/* ======================================================================
 * Section 4: Core C++17 engine (namespace analyze).
 *
 * AnalyzeRequest::from_config()  — Phase A: config -> typed request
 * apply_request_to_state()       — Phase A: request -> current_state
 * run()                          — Phase A-E: unified execution entry point
 * ====================================================================== */

/**
 * AnalyzeRequest::from_config — build from a public analyze_config.
 *
 * Consolidates the mapping logic that was previously inlined inside
 * analyze_run() in api.c.  When cfg == NULL all fields keep their C++17
 * default values (which mirror analyze_current_state_init() defaults).
 */
AnalyzeRequest
AnalyzeRequest::from_config(const struct analyze_config *cfg, int analysis_flags)
{
    AnalyzeRequest req;
    req.flags = analysis_flags;

    if (!cfg)
return req; /* all defaults */

    req.sampler = cfg->sampler;
    if (cfg->num_views > 0)
req.num_views = cfg->num_views;
    req.azimuth_deg   = cfg->azimuth_deg;
    req.elevation_deg = cfg->elevation_deg;
    if (cfg->grid_spacing > 0.0)
req.grid_spacing = cfg->grid_spacing;
    if (cfg->grid_spacing_min > 0.0)
req.grid_spacing_min = cfg->grid_spacing_min;
    if (cfg->aspect > 0.0)
req.aspect = cfg->aspect;

    req.grid_width  = cfg->grid_width;
    req.grid_height = cfg->grid_height;

    req.quiet_missed = cfg->quiet_missed;
    if (cfg->samples_per_model_axis > 0.0)
req.samples_per_model_axis = cfg->samples_per_model_axis;

    req.view_size = cfg->view_size;
    VMOVE(req.view_eye,  cfg->view_eye);
    HMOVE(req.view_quat, cfg->view_quat);

    req.overlap_tol = cfg->overlap_tol;
    if (cfg->volume_tol >= 0.0)
req.volume_tol = cfg->volume_tol;
    if (cfg->mass_tol >= 0.0)
req.mass_tol = cfg->mass_tol;
    if (cfg->surf_area_tol >= 0.0)
req.surf_area_tol = cfg->surf_area_tol;

    req.density_file = cfg->density_file;

    req.use_air = cfg->use_air;
    if (cfg->ncpu > 0)
req.ncpu = cfg->ncpu;
    if (cfg->required_hits > 0)
req.required_hits = cfg->required_hits;

    req.verbose = cfg->verbose;
    req.log_str = cfg->log_str;

    if (cfg->timeout_ms > 0)
req.timeout_ms = cfg->timeout_ms;
    req.required_digits = cfg->required_digits;

    if (cfg->n_crofton_rays > 0)
req.n_crofton_rays = cfg->n_crofton_rays;

    req.volume_plot_file = cfg->volume_plot_file;

    req.overlap_render      = cfg->overlap_render;
    req.overlap_render_data = cfg->overlap_render_data;
    req.gap_render          = cfg->gap_render;
    req.gap_render_data     = cfg->gap_render_data;
    req.adj_air_render      = cfg->adj_air_render;
    req.adj_air_render_data = cfg->adj_air_render_data;
    req.exp_air_render      = cfg->exp_air_render;
    req.exp_air_render_data = cfg->exp_air_render_data;
    req.unconf_air_render      = cfg->unconf_air_render;
    req.unconf_air_render_data = cfg->unconf_air_render_data;

    return req;
}


/**
 * apply_request_to_state — single authoritative mapping AnalyzeRequest -> current_state.
 *
 * Phase B will shrink this function as sampler strategies assume responsibility
 * for their own state fields.  For Phase A-B it is an exact equivalent of the
 * mapping code that was inlined in the original analyze_run().
 */
void
apply_request_to_state(const AnalyzeRequest &req, struct current_state *state)
{
    state->sampler = req.sampler;
    if (req.num_views > 0)
state->num_views = req.num_views;
    state->azimuth_deg   = req.azimuth_deg;
    state->elevation_deg = req.elevation_deg;
    if (req.grid_spacing > 0.0)
state->gridSpacing = req.grid_spacing;
    if (req.grid_spacing_min > 0.0)
state->gridSpacingLimit = req.grid_spacing_min;
    if (req.aspect > 0.0)
state->aspect = req.aspect;
    if (req.grid_width > 0 || req.grid_height > 0) {
state->grid_size_flag = 1;
state->grid_width  = (fastf_t)req.grid_width;
state->grid_height = (fastf_t)(req.grid_height > 0 ? req.grid_height
   : req.grid_width);
    }
    if (req.quiet_missed)
state->quiet_missed_report = 1;
    if (req.samples_per_model_axis > 0.0)
state->samples_per_model_axis = req.samples_per_model_axis;

    if (req.sampler == ANALYZE_SAMPLER_VIEW_PLANE && req.view_size > 0.0) {
	VMOVE(state->eye_model, req.view_eye);
	state->viewsize = req.view_size;
	quat_quat2mat(state->Viewrotscale, req.view_quat);
	state->use_view_information = 1;
	state->use_single_grid = 1;
    }

    state->overlap_tolerance = req.overlap_tol;
    if (req.volume_tol >= 0.0)
state->volume_tolerance = req.volume_tol;
    if (req.mass_tol >= 0.0)
state->mass_tolerance = req.mass_tol;
    if (req.surf_area_tol >= 0.0)
state->sa_tolerance = req.surf_area_tol;

    if (req.density_file)
state->densityFileName = (char *)req.density_file;

    state->use_air = req.use_air;
    if (req.ncpu > 0)
state->ncpu = req.ncpu;
    if (req.required_hits > 0)
state->required_number_hits = req.required_hits;

    state->verbose = req.verbose;
    if (req.log_str) {
	if (req.verbose) {
	    state->verbose = 1;
	    state->verbose_str = req.log_str;
	} else {
	    state->debug = 1;
	    state->debug_str = req.log_str;
	}
    }

    if (req.timeout_ms > 0)
state->timeout_ms = req.timeout_ms;
    state->required_digits = req.required_digits;

    if (req.n_crofton_rays > 0)
state->crofton_n_rays = req.n_crofton_rays;

    if (req.volume_plot_file)
	state->plot_volume = req.volume_plot_file;
}


/**
 * run — unified analysis pipeline execution (Phases A-E).
 *
 * Phase D: Both current_state and analyze_results are managed by
 * std::unique_ptr so that every return path (including error paths) frees
 * them correctly without manual cleanup code (Phase E simplification).
 *
 * Phase B: An ISampler is created and configure() is called so that
 * sampler-specific configuration happens through the strategy interface.
 *
 * Phase C: IAnalyzer instances are created from the flag bitmask; each
 * registers its callback on state through the plugin interface, keeping
 * callback wiring decoupled from the main loop.
 */
struct analyze_results *
run(const AnalyzeRequest &req, struct db_i *dbip, char *names[], int num_names)
{
    const int flags = req.flags;

    /* ------------------------------------------------------------------
     * Phase D: RAII result container.
     *
     * Allocate and initialise all bu_ptbl fields before creating the
     * unique_ptr so the deleter (analyze_results_free) is always safe.
     * ------------------------------------------------------------------ */
    struct analyze_results *raw_res =
(struct analyze_results *)bu_calloc(1, sizeof(*raw_res), "analyze_results");
    bu_ptbl_init(&raw_res->overlaps,   8, "ar overlaps");
    bu_ptbl_init(&raw_res->gaps,       8, "ar gaps");
    bu_ptbl_init(&raw_res->adj_air,    8, "ar adj_air");
    bu_ptbl_init(&raw_res->exp_air,    8, "ar exp_air");
    bu_ptbl_init(&raw_res->first_air,  8, "ar first_air");
    bu_ptbl_init(&raw_res->last_air,   8, "ar last_air");
    bu_ptbl_init(&raw_res->unconf_air, 8, "ar unconf_air");

    std::unique_ptr<struct analyze_results,
    void(*)(struct analyze_results *)>
res_owner(raw_res, analyze_results_free);

    /* ------------------------------------------------------------------
     * Phase D: RAII current_state.
     * ------------------------------------------------------------------ */
    std::unique_ptr<struct current_state,
    void(*)(struct current_state *)>
state_owner(analyze_current_state_init(), analyze_free_current_state);
    struct current_state *state = state_owner.get();

    apply_request_to_state(req, state);

    /* ------------------------------------------------------------------
     * Phase B: sampler strategy — configure state for the chosen backend.
     *
     * apply_request_to_state() already wrote state->sampler; configure()
     * is the hook for sampler-specific extra setup that cannot be
     * expressed through the generic field mapping.
     * ------------------------------------------------------------------ */
    auto sampler = ISampler::create(req.sampler);
    sampler->configure(req, state);

    /* ------------------------------------------------------------------
     * Phase C: build capture context and register analyzer callbacks.
     *
     * The ar_capture_ctx carries the shared result pointer, the protecting
     * semaphore, and the presentation-layer render hooks.  Each IAnalyzer
     * registers its callback with a pointer to this shared context.
     * ------------------------------------------------------------------ */
    struct ar_capture_ctx ctx;
    ctx.res                    = raw_res;
    ctx.sem                    = bu_semaphore_register("analyze_run_results_sem");
    ctx.state                  = state;
    ctx.overlap_render         = req.overlap_render;
    ctx.overlap_render_data    = req.overlap_render_data;
    ctx.gap_render             = req.gap_render;
    ctx.gap_render_data        = req.gap_render_data;
    ctx.adj_air_render         = req.adj_air_render;
    ctx.adj_air_render_data    = req.adj_air_render_data;
    ctx.exp_air_render         = req.exp_air_render;
    ctx.exp_air_render_data    = req.exp_air_render_data;
    ctx.unconf_air_render      = req.unconf_air_render;
    ctx.unconf_air_render_data = req.unconf_air_render_data;

    auto analyzers = IAnalyzer::from_flags(flags);
    for (auto const &a : analyzers)
a->register_callbacks(state, &ctx);

    /* ------------------------------------------------------------------
     * Execute analysis (geometry execution adapter layer).
     *
     * Phase E: there is no explicit "free on error" here; the unique_ptrs
     * handle cleanup automatically on every return path.
     * ------------------------------------------------------------------ */
    int ret = perform_raytracing(state, dbip, names, num_names, flags);
    if (ret != ANALYZE_OK)
return nullptr; /* res_owner and state_owner freed automatically */

    /* ------------------------------------------------------------------
     * Phase B: sampler finalisation.
     *
     * For Phase B all samplers do their post-processing inside api.c
     * (e.g., Crofton SA formula is applied in shoot_rays_crofton()).
     * finalize() is a no-op for all current backends; the hook is
     * provided for future phases that move post-processing here.
     * ------------------------------------------------------------------ */
    sampler->finalize(state, raw_res);

    /* ------------------------------------------------------------------
     * Record analysis metadata.
     * ------------------------------------------------------------------ */
    raw_res->final_grid_spacing = state->last_sampled_grid_spacing;
    raw_res->sampler_type  = state->sampler;
    raw_res->is_stochastic = (state->sampler == ANALYZE_SAMPLER_CROFTON) ? 1 : 0;

    if (flags & ANALYZE_BOX)
analyze_bbox(dbip, names, num_names, raw_res->bbox_min, raw_res->bbox_max);

    /* ------------------------------------------------------------------
     * Sort overlaps by estimated volume (largest first).
     *
     * This makes the output immediately useful: the most severe overlaps
     * (by volume) appear at the top of the list, even when found by the
     * cluster-focused sampler with varying per-cluster cell sizes.
     * ------------------------------------------------------------------ */
    if (BU_PTBL_LEN(&raw_res->overlaps) > 1) {
std::sort(raw_res->overlaps.buffer,
  raw_res->overlaps.buffer + BU_PTBL_LEN(&raw_res->overlaps),
  [](const long *a, const long *b) {
      const struct analyze_overlap_record *ra =
          (const struct analyze_overlap_record *)a;
      const struct analyze_overlap_record *rb =
          (const struct analyze_overlap_record *)b;
      return ra->estimated_volume > rb->estimated_volume;
  });
    }

    /* ------------------------------------------------------------------
     * Harvest scalar totals.
     * ------------------------------------------------------------------ */
    if (flags & ANALYZE_VOLUME)
	raw_res->total_volume    = ar_total_volume_calc(state);
    if (flags & ANALYZE_MASS)
	raw_res->total_mass      = ar_total_mass_calc(state);
    if (flags & ANALYZE_SURF_AREA)
	raw_res->total_surf_area = ar_total_surf_area_calc(state);
    if (flags & ANALYZE_CENTROIDS)
	ar_total_centroid_calc(state, raw_res->centroid);
    if (flags & ANALYZE_MOMENTS)
	ar_total_moments_calc(state, raw_res->moments_of_inertia);

    /* ------------------------------------------------------------------
     * Harvest per-input-object results.
     * ------------------------------------------------------------------ */
    if (num_names > 0) {
raw_res->objects = (struct analyze_object_result *)bu_calloc(
(size_t)num_names,
sizeof(struct analyze_object_result),
"ar object results");
raw_res->n_objects = (size_t)num_names;

for (int i = 0; i < num_names; i++) {
    raw_res->objects[i].name = bu_strdup(names[i]);
    int obj = ar_find_object_index(state, names[i]);
    if (obj < 0)
	continue;
    if (flags & ANALYZE_VOLUME)
	raw_res->objects[i].volume    = ar_object_volume_calc(state, obj);
    if (flags & ANALYZE_MASS)
	raw_res->objects[i].mass      = ar_object_mass_calc(state, obj);
    if (flags & ANALYZE_SURF_AREA)
	raw_res->objects[i].surf_area = ar_object_surf_area_calc(state, obj);
    if (flags & ANALYZE_CENTROIDS)
	ar_object_centroid_calc(state, obj, raw_res->objects[i].centroid);
    if (flags & ANALYZE_MOMENTS)
	ar_object_moments_calc(state, obj, raw_res->objects[i].moments_of_inertia);
    if (flags & ANALYZE_BOX) {
char *single[2];
single[0] = names[i];
single[1] = NULL;
analyze_bbox(dbip, single, 1,
     raw_res->objects[i].bbox_min,
     raw_res->objects[i].bbox_max);
    }
}
    }

    /* ------------------------------------------------------------------
     * Harvest per-region results.
     * ------------------------------------------------------------------ */
    {
	int num_regions = state->num_regions;
if (num_regions > 0) {
    raw_res->regions = (struct analyze_region_result *)bu_calloc(
    (size_t)num_regions,
    sizeof(struct analyze_region_result),
    "ar region results");
    raw_res->n_regions = (size_t)num_regions;

	for (int i = 0; i < num_regions; i++) {
char  *rname    = NULL;
double vol      = 0.0, mass = 0.0, sa = 0.0;

if (flags & ANALYZE_VOLUME)
	    vol = ar_region_volume_calc(state, i);
if (flags & ANALYZE_MASS)
	    mass = ar_region_mass_calc(state, i);
if (flags & ANALYZE_SURF_AREA)
	    sa = ar_region_surf_area_calc(state, i);
	rname = state->reg_tbl[i].r_name;

raw_res->regions[i].name      = rname ? bu_strdup(rname)
       : bu_strdup("");
raw_res->regions[i].volume    = vol;
raw_res->regions[i].mass      = mass;
raw_res->regions[i].surf_area = sa;
raw_res->regions[i].hits      = state->reg_tbl[i].hits;
    }
}
    }

    /* ------------------------------------------------------------------
     * Phase D+E: transfer ownership to caller.
     *
     * state_owner destructs here, calling analyze_free_current_state().
     * res_owner::release() detaches the pointer so it is NOT freed; the
     * caller is now responsible (typically via analyze_results_free()).
     * ------------------------------------------------------------------ */
    return res_owner.release();
}

} /* namespace analyze */


/* ======================================================================
 * Section 5: C linkage wrapper (callable from api.c and other C files).
 * ====================================================================== */

extern "C" struct analyze_results *
analyze_engine_run(const struct analyze_config *cfg, struct db_i *dbip,
   char *names[], int num_names, int flags)
{
    analyze::AnalyzeRequest req =
analyze::AnalyzeRequest::from_config(cfg, flags);
    return analyze::run(req, dbip, names, num_names);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
