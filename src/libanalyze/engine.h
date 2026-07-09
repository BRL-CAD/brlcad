/*                       E N G I N E . H
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
/** @file libanalyze/engine.h
 *
 * Internal C++17 engine layer for libanalyze.
 *
 * This header is NOT part of the public libanalyze API.  It is included
 * only within libanalyze implementation files.
 *
 * Layer summary:
 *   Phase A — AnalyzeRequest marshalling + unified analyze::run() entry point.
 *   Phase B — ISampler strategy abstraction (TripleGrid, Rotated, Crofton, ViewPlane).
 *   Phase C — IAnalyzer plugin abstraction (one per ANALYZE_* event type).
 *   Phase D — RAII ownership for current_state and analyze_results.
 *   Phase E — Remove duplicate cleanup code now handled by RAII.
 */

#ifndef LIBANALYZE_ENGINE_H
#define LIBANALYZE_ENGINE_H

#include "common.h"
#include "raytrace.h"
#include "analyze.h"

__BEGIN_DECLS

/**
 * analyze_engine_run - unified analysis execution entry point (C linkage).
 *
 * Marshals @p cfg into an AnalyzeRequest, invokes perform_raytracing(),
 * and harvests results into a freshly-allocated struct analyze_results.
 *
 * Both analyze_run() and (in a later phase) the legacy
 * perform_raytracing() compatibility path converge on this function.
 */
extern struct analyze_results *
analyze_engine_run(const struct analyze_config *cfg, struct db_i *dbip,
   char *names[], int num_names, int flags);

__END_DECLS


/* ======================================================================
 * C++17 internal types — compiled only in C++ translation units.
 * ====================================================================== */
#ifdef __cplusplus

#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>

/* Opaque C types used by value or pointer in the C++ interfaces below. */
struct current_state;
struct bu_vls;

/* Forward declaration of the shared capture context (defined in engine.cpp).
 * Visible to both ISampler and IAnalyzer without exposing implementation. */
struct ar_capture_ctx;

namespace analyze {

/* ======================================================================
 * Phase A: AnalyzeRequest — typed, immutable config snapshot.
 * ====================================================================== */

/**
 * AnalyzeRequest — immutable, validated snapshot of one analysis session.
 *
 * Built once from an analyze_config (or, in the legacy-compat path, from a
 * current_state) and passed by const-reference throughout the engine.
 *
 * Default member-initializer values mirror analyze_current_state_init() so
 * that NULL-config callers get the same results as before Phase A.
 */
struct AnalyzeRequest {
    /* ---- Sampling ---- */
    int    sampler           = ANALYZE_SAMPLER_TRIPLE_GRID;
    int    num_views         = 3;
    double azimuth_deg       = 35.0;
    double elevation_deg     = 25.0;
    double grid_spacing      = 50.0;
    double grid_spacing_min  = 0.5;
    double aspect            = 1.0;
    size_t n_crofton_rays    = 0;

    /* ---- Grid size override ---- */
    int    grid_width  = 0;
    int    grid_height = 0;

    /* ---- Sampling detail ---- */
    int    quiet_missed           = 0;
    double samples_per_model_axis = 2.0;

    /* ---- VIEW_PLANE parameters ---- */
    double  view_size    = 0.0;
    point_t view_eye     = {};
    quat_t  view_quat    = {};

    /* ---- Convergence tolerances ---- */
    double overlap_tol   = 0.0;
    double volume_tol    = -1.0;
    double mass_tol      = -1.0;
    double surf_area_tol = -1.0;

    /* ---- Material densities ---- */
    const char *density_file = nullptr;

    /* ---- Execution ---- */
    int    use_air        = 1;
    int    ncpu           = 0;    /* 0 -> all available */
    size_t required_hits  = 1;

    /* ---- Output ---- */
    int         verbose = 0;
    struct bu_vls *log_str = nullptr;

    /* ---- Runtime limits ---- */
    long   timeout_ms      = 0;
    double required_digits = 0.0;

    /* ---- Per-pass volume plot file ---- */
    FILE *volume_plot_file = nullptr;

    /* ---- Presentation-layer render hooks ---- */
    analyze_overlap_render_fn    overlap_render      = nullptr;
    void                        *overlap_render_data = nullptr;
    analyze_gap_render_fn        gap_render          = nullptr;
    void                        *gap_render_data     = nullptr;
    analyze_adj_air_render_fn    adj_air_render      = nullptr;
    void                        *adj_air_render_data = nullptr;
    analyze_exp_air_render_fn    exp_air_render      = nullptr;
    void                        *exp_air_render_data = nullptr;
    analyze_unconf_air_render_fn unconf_air_render      = nullptr;
    void                        *unconf_air_render_data = nullptr;

    /* ---- Analysis flags (ANALYZE_* bitmask) ---- */
    int flags = 0;

    /**
     * Build an AnalyzeRequest from a public analyze_config.
     * When @p cfg is NULL all fields remain at their default values.
     */
    static AnalyzeRequest from_config(const struct analyze_config *cfg,
      int analysis_flags);
};


/* ======================================================================
 * Phase B: ISampler — sampler strategy interface.
 *
 * Each concrete ISampler encapsulates the sampler-specific configuration
 * and (for future phases) the actual ray-firing loop.  For Phase B the
 * actual ray firing still lives inside perform_raytracing(); ISampler
 * controls what happen before (configure()) and after (finalize()).
 *
 * Concrete implementations live in engine.cpp:
 *   TripleGridSampler  — axis-aligned rectangular grid (default)
 *   RotatedGridSampler — grid along arbitrary az/el direction
 *   CroftonSampler     — isotropic random rays + Cauchy-Crofton SA formula
 *   ViewPlaneSampler   — single-plane grid from explicit eye/view data
 * ====================================================================== */
struct ISampler {
    virtual ~ISampler() = default;

    /**
     * Apply sampler-specific configuration to @p state before
     * perform_raytracing() is called.  The base implementation is a no-op
     * (the sampler type is already written by apply_request_to_state()).
     * Override to add sampler-specific extra setup.
     */
    virtual void configure(const AnalyzeRequest &req,
   struct current_state *state) const;

    /**
     * Post-process state/results after perform_raytracing() returns.
     * Called unconditionally so each strategy can clean up its own
     * temporary state.  Default: no-op.
     */
    virtual void finalize(struct current_state *state,
  struct analyze_results *res) const;

    /**
     * Factory: create the sampler matching @p sampler_type
     * (an ANALYZE_SAMPLER_* constant).  Returns TripleGridSampler for
     * unknown values.
     */
    static std::unique_ptr<ISampler> create(int sampler_type);
};


/* ======================================================================
 * Phase C: IAnalyzer — analyzer plugin interface.
 *
 * Each concrete IAnalyzer registers exactly the event callback(s) it needs
 * on current_state so that perform_raytracing() routes events to it.  The
 * callback writes results directly into the shared ar_capture_ctx (and thus
 * into struct analyze_results) — no separate harvest step is needed.
 *
 * Concrete implementations live in engine.cpp:
 *   OverlapAnalyzer   — registers ar_overlap_cb
 *   GapAnalyzer       — registers ar_gap_cb
 *   ExpAirAnalyzer    — registers ar_exp_air_cb
 *   AdjAirAnalyzer    — registers ar_adj_air_cb
 *   FirstAirAnalyzer  — registers ar_first_air_cb
 *   LastAirAnalyzer   — registers ar_last_air_cb
 *   UnconfAirAnalyzer — registers ar_unconf_air_cb
 * ====================================================================== */
struct IAnalyzer {
    virtual ~IAnalyzer() = default;

    /**
     * Register this analyzer's event callback(s) on @p state.
     * @p ctx is the shared capture context (semaphore + result pointer +
     * render hooks).  It remains valid for the lifetime of
     * perform_raytracing().
     */
    virtual void register_callbacks(struct current_state *state,
    struct ar_capture_ctx *ctx) const = 0;

    /**
     * Factory: create all analyzers needed for the given ANALYZE_* @p flags.
     * Called once per analyze::run() invocation; returns an empty vector when
     * no issue-detection flags are set (metric-only runs).
     */
    static std::vector<std::unique_ptr<IAnalyzer>> from_flags(int flags);
};


/* ======================================================================
 * Phase A–D: engine execution functions.
 * ====================================================================== */

/**
 * Apply an AnalyzeRequest to an already-initialised current_state.
 *
 * Single authoritative mapping: AnalyzeRequest -> current_state.
 * Called by analyze::run() and (in a later phase) the legacy-compat path.
 */
void apply_request_to_state(const AnalyzeRequest &req,
    struct current_state *state);

/**
 * Run the full analysis pipeline from a typed AnalyzeRequest.
 *
 * Phase A+B+C+D implementation:
 *   1. RAII-allocate struct analyze_results (Phase D).
 *   2. RAII-create current_state + apply_request_to_state() (Phase D).
 *   3. Create ISampler and configure state (Phase B).
 *   4. Build ar_capture_ctx; create and register IAnalyzer set (Phase C).
 *   5. Invoke perform_raytracing().
 *   6. Call ISampler::finalize() for sampler post-processing (Phase B).
 *   7. Harvest scalar totals and per-object/per-region arrays.
 *   8. Transfer result ownership to caller; RAII cleans up state (Phase D+E).
 *
 * Returns NULL on error.  On success returns a heap-allocated result
 * that must be freed with analyze_results_free().
 */
struct analyze_results *run(const AnalyzeRequest &req,
    struct db_i *dbip,
    char *names[], int num_names);

} /* namespace analyze */

#endif /* __cplusplus */

#endif /* LIBANALYZE_ENGINE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
