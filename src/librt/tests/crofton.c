/*                      C R O F T O N . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file librt/tests/crofton.c
 *
 * Stand-alone tests for Crofton-based area/volume estimation.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/datetime.h"
#include "raytrace.h"
#include "wdb.h"


struct region_visit_counts {
    size_t segments;
    double chord;
};

struct ray_visit_counts {
    size_t rays;
    size_t segments;
    int valid;
};

struct progress_counts {
    size_t calls;
    size_t last_rays;
    int valid;
};

static void
visit_progress(size_t ray_count, size_t UNUSED(crossing_count),
    double UNUSED(surface_area), double UNUSED(volume),
    double UNUSED(stability_mm), int UNUSED(stability_evaluated),
    double elapsed_ms, void *data)
{
    struct progress_counts *counts = (struct progress_counts *)data;
    counts->calls++;
    counts->last_rays = ray_count;
    if (ray_count == 0 || elapsed_ms < 0.0)
        counts->valid = 0;
}

static void
visit_ray(const struct rt_crofton_ray *ray, void *data)
{
    struct ray_visit_counts *counts = (struct ray_visit_counts *)data;
    counts->rays++;
    for (size_t i = 0; i < ray->segment_count; i++) {
	const struct rt_crofton_segment *segment = &ray->segments[i];
	counts->segments++;
	if (segment->ray_id != ray->ray_id ||
	    !VNEAR_EQUAL(segment->ray_origin, ray->origin, SMALL_FASTF) ||
	    !VNEAR_EQUAL(segment->ray_direction, ray->direction, SMALL_FASTF) ||
	    !NEAR_EQUAL(segment->out_distance - segment->in_distance,
		segment->thickness, SMALL_FASTF))
	    counts->valid = 0;
    }
}

static void
visit_region_segment(const struct rt_crofton_segment *segment, void *data)
{
    struct region_visit_counts *counts = (struct region_visit_counts *)data;
    const struct region *region = segment->region;
    if (!region || !region->reg_name) return;
    counts->segments++;
    counts->chord += segment->thickness;
}

static int
check_seeded_streams(struct rt_i *rtip, const char *label,
    enum rt_crofton_sequence sequence)
{
    struct progress_counts progress = {0, 0, 1};
    struct rt_crofton_params params = {
        2048u, 0.0, 0.0, RT_CROFTON_STABILITY_DEFAULT, visit_progress, &progress};
    struct rt_crofton_stats first_stats, repeated_stats, other_stats;
    struct rt_crofton_stats session_stats = {0};
    struct region_visit_counts first_counts = {0, 0.0};
    struct region_visit_counts repeated_counts = {0, 0.0};
    struct region_visit_counts other_counts = {0, 0.0};
    struct region_visit_counts session_counts = {0, 0.0};
    int first_ret = rt_crofton_visit_seeded(&first_stats, rtip,
	&params, 0, NULL, NULL, sequence,
	1234u, 7u, visit_region_segment, NULL, &first_counts);
    int repeated_ret = rt_crofton_visit_seeded(&repeated_stats, rtip,
	&params, 0, NULL, NULL, sequence,
	1234u, 7u, visit_region_segment, NULL, &repeated_counts);
    int other_ret = rt_crofton_visit_seeded(&other_stats, rtip,
	&params, 0, NULL, NULL, sequence,
	1234u, 8u, visit_region_segment, NULL, &other_counts);
    struct rt_crofton_session *session = rt_crofton_session_create(rtip);
    int session_ret = session ?
        rt_crofton_session_visit_seeded(session, &session_stats, &params,
            0, NULL, NULL, sequence, 1234u, 7u, visit_region_segment,
            NULL, &session_counts) : -1;
    rt_crofton_session_destroy(session);
    const int session_match = session_ret == first_ret &&
        session_stats.ray_count == first_stats.ray_count &&
        session_stats.crossing_count == first_stats.crossing_count &&
        EQUAL(session_stats.volume, first_stats.volume) &&
        EQUAL(session_stats.surface_area, first_stats.surface_area) &&
        session_counts.segments == first_counts.segments &&
        EQUAL(session_counts.chord, first_counts.chord);
    const int repeated_match = first_ret == repeated_ret &&
	first_stats.ray_count == repeated_stats.ray_count &&
	first_stats.crossing_count == repeated_stats.crossing_count &&
	EQUAL(first_stats.volume, repeated_stats.volume) &&
	EQUAL(first_stats.surface_area, repeated_stats.surface_area) &&
	first_counts.segments == repeated_counts.segments &&
	EQUAL(first_counts.chord, repeated_counts.chord);
    const int independent_stream = other_ret >= 0 &&
	(!EQUAL(first_stats.volume, other_stats.volume) ||
	 !EQUAL(first_stats.surface_area, other_stats.surface_area));
    const int stopped_at_limit =
        first_stats.stop_reason == RT_CROFTON_STOP_RAYS &&
        repeated_stats.stop_reason == RT_CROFTON_STOP_RAYS &&
        other_stats.stop_reason == RT_CROFTON_STOP_RAYS &&
        session_stats.stop_reason == RT_CROFTON_STOP_RAYS;
    if (first_ret < 0 || !session_match || !repeated_match ||
        !independent_stream || !stopped_at_limit || progress.calls != 4 ||
        progress.last_rays != params.n_rays || !progress.valid) {
	printf("  %-24s  invalid %s seeded Crofton streams\n", label,
	    sequence == RT_CROFTON_SEQUENCE_QMC ? "QMC" : "random");
	return 1;
    }
    return 0;
}

static int
check_invalid_inputs(struct rt_i *rtip, const char *label)
{
    struct rt_crofton_stats stats = {0};
    struct region_visit_counts counts = {0, 0.0};
    struct rt_crofton_params params = {
        1u, NAN, 0.0, RT_CROFTON_STABILITY_DEFAULT, NULL, NULL};
    point_t bbox_min, bbox_max;
    VSETALL(bbox_min, 1.0);
    VSETALL(bbox_max, 0.0);

    const int nonfinite = rt_crofton_visit(&stats, rtip, &params,
        0, NULL, NULL, RT_CROFTON_SEQUENCE_RANDOM,
        visit_region_segment, NULL, &counts);
    params.stability_mm = 0.0;
    const int missing_bound = rt_crofton_visit(&stats, rtip, &params,
        0, bbox_min, NULL, RT_CROFTON_SEQUENCE_RANDOM,
        visit_region_segment, NULL, &counts);
    const int reversed_bounds = rt_crofton_visit(&stats, rtip, &params,
        0, bbox_min, bbox_max, RT_CROFTON_SEQUENCE_RANDOM,
        visit_region_segment, NULL, &counts);
    params.stability_metrics = 1u << 31;
    const int unknown_metric = rt_crofton_visit(&stats, rtip, &params,
        0, NULL, NULL, RT_CROFTON_SEQUENCE_RANDOM,
        visit_region_segment, NULL, &counts);
    if (nonfinite != -1 || missing_bound != -1 || reversed_bounds != -1 ||
        unknown_metric != -1) {
        printf("  %-24s  accepted invalid Crofton controls\n", label);
        return 1;
    }
    return 0;
}

static int
check_stopping_controls(struct rt_i *rtip, const char *label)
{
    const size_t expected_stability_rays = 80000u;
    struct region_visit_counts counts = {0, 0.0};
    struct rt_crofton_stats stats = {0};
    struct rt_crofton_params params = {
        100000u, 0.0, 10000.0, RT_CROFTON_STABILITY_VOLUME, NULL, NULL};
    int ret = rt_crofton_visit(&stats, rtip, &params, 0, NULL, NULL,
        RT_CROFTON_SEQUENCE_RANDOM, visit_region_segment, NULL, &counts);
    if (ret <= 0 || stats.stop_reason != RT_CROFTON_STOP_RAYS ||
        stats.ray_count != params.n_rays) {
        printf("  %-24s  time-only run did not honor its ray limit\n", label);
        return 1;
    }

    params.n_rays = 0;
    params.stability_mm = MAX_FASTF;
    params.time_ms = 1000.0;
    counts.segments = 0;
    counts.chord = 0.0;
    ret = rt_crofton_visit(&stats, rtip, &params, 0, NULL, NULL,
        RT_CROFTON_SEQUENCE_RANDOM, visit_region_segment, NULL, &counts);
    if (ret <= 0 || stats.stop_reason != RT_CROFTON_STOP_STABILITY ||
        stats.ray_count != expected_stability_rays) {
        printf("  %-24s  invalid doubling-checkpoint stop (%zu rays)\n",
            label, stats.ray_count);
        return 1;
    }
    return check_invalid_inputs(rtip, label);
}

static double
rel_err(double estimated, double exact)
{
    if (fabs(exact) < SMALL_FASTF)
	return (fabs(estimated) < SMALL_FASTF) ? 0.0 : 1.0;
    return fabs(estimated - exact) / fabs(exact);
}


static int
crofton_segments_equal(const struct rt_crofton_segment *left,
		       const struct rt_crofton_segment *right)
{
    return left->ray_id == right->ray_id &&
	left->region == right->region &&
	left->in_solid == right->in_solid &&
	left->out_solid == right->out_solid &&
	NEAR_EQUAL(left->in_distance, right->in_distance, SMALL_FASTF) &&
	NEAR_EQUAL(left->out_distance, right->out_distance, SMALL_FASTF) &&
	NEAR_EQUAL(left->thickness, right->thickness, SMALL_FASTF) &&
	VNEAR_EQUAL(left->ray_origin, right->ray_origin, SMALL_FASTF) &&
	VNEAR_EQUAL(left->ray_direction, right->ray_direction, SMALL_FASTF) &&
	VNEAR_EQUAL(left->in_point, right->in_point, SMALL_FASTF) &&
	VNEAR_EQUAL(left->in_normal, right->in_normal, SMALL_FASTF) &&
	VNEAR_EQUAL(left->out_point, right->out_point, SMALL_FASTF) &&
	VNEAR_EQUAL(left->out_normal, right->out_normal, SMALL_FASTF);
}

static int
verify_crofton_estimates(void)
{
    int failures = 0;
    const double tol_pct = 5.0;  /* ±5% acceptable */

    printf("\n--- Crofton estimator verification ---\n");

    struct rt_crofton_params cparams = {0, 0.0, 0.0, RT_CROFTON_STABILITY_DEFAULT, NULL, NULL};

#define CROFTON_CHECK(label, ip_ptr, analytic_sa, analytic_vol) \
    do { \
	struct rt_db_internal *_ip = (ip_ptr); \
	fastf_t _csa = 0.0, _cvol = 0.0; \
	rt_crofton_sample(&_csa, &_cvol, _ip, &cparams); \
	double _sa_err  = fabs(_csa  - (analytic_sa))  / ((analytic_sa)  > 0 ? (analytic_sa)  : 1.0) * 100.0; \
	double _vol_err = fabs(_cvol - (analytic_vol)) / ((analytic_vol) > 0 ? (analytic_vol) : 1.0) * 100.0; \
	const char *_sa_tag  = (_sa_err  <= tol_pct) ? "SA-OK"  : "SA-FAIL"; \
	const char *_vol_tag = (_vol_err <= tol_pct) ? "VOL-OK" : "VOL-FAIL"; \
	printf("  %-42s  SA_err=%.1f%%[%s]  VOL_err=%.1f%%[%s]\n", \
	       (label), _sa_err, _sa_tag, _vol_err, _vol_tag); \
	fflush(stdout); \
	if (_sa_err  > tol_pct) failures++; \
	if (_vol_err > tol_pct) failures++; \
    } while (0)

    {
	struct rt_ell_internal ell;
	memset(&ell, 0, sizeof(ell));
	ell.magic = RT_ELL_INTERNAL_MAGIC;
	VSET(ell.v, 0, 0, 0);
	VSET(ell.a, 10, 0, 0);
	VSET(ell.b, 0, 10, 0);
	VSET(ell.c, 0, 0, 10);

	struct rt_db_internal ip;
	RT_DB_INTERNAL_INIT(&ip);
	ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip.idb_minor_type = ID_ELL;
	ip.idb_type       = ID_ELL;
	ip.idb_ptr        = &ell;
	ip.idb_meth       = &OBJ[ID_ELL];

	double analytic_sa  = 4.0 * M_PI * 10.0 * 10.0;
	double analytic_vol = (4.0 / 3.0) * M_PI * 10.0 * 10.0 * 10.0;
	CROFTON_CHECK("sphere r=10", &ip, analytic_sa, analytic_vol);
    }

    {
	const double full_height = 20.0;
	const double major_radius = 5.0;
	const double minor_radius = 3.0;
	const double neck_base_ratio = 0.4;

	struct rt_hyp_internal hyp;
	memset(&hyp, 0, sizeof(hyp));
	hyp.hyp_magic = RT_HYP_INTERNAL_MAGIC;
	VSET(hyp.hyp_Vi, 0, 0, 0);
	VSET(hyp.hyp_Hi, 0, 0, full_height);
	VSET(hyp.hyp_A, major_radius, 0, 0);
	hyp.hyp_b = minor_radius;
	hyp.hyp_bnr = neck_base_ratio;

	struct rt_db_internal ip;
	RT_DB_INTERNAL_INIT(&ip);
	ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip.idb_minor_type = ID_HYP;
	ip.idb_type = ID_HYP;
	ip.idb_ptr = &hyp;
	ip.idb_meth = &OBJ[ID_HYP];

	double exact_volume = M_PI * major_radius * minor_radius * full_height *
	    (1.0 + 2.0 * neck_base_ratio * neck_base_ratio) / 3.0;
	fastf_t reported_volume = 0.0;
	ip.idb_meth->ft_volume(&reported_volume, &ip);
	double volume_error = rel_err(reported_volume, exact_volume) * 100.0;
	printf("  %-42s  analytic_formula_err=%.12f%%  [%s]\n",
	       "HYP (rt_hyp_volume)", volume_error,
	       (volume_error <= 1.0e-9) ? "OK" : "FORMULA-FAIL");
	if (volume_error > 1.0e-9)
	    failures++;
    }

    {
	plane_t planes[7] = {
	    {2.0, 0.0, 0.0, 2.0},
	    {-3.0, 0.0, 0.0, 0.0},
	    {0.0, 4.0, 0.0, 4.0},
	    {0.0, -5.0, 0.0, 0.0},
	    {0.0, 0.0, 6.0, 6.0},
	    {0.0, 0.0, -7.0, 0.0},
	    {2.0, 2.0, 2.0, 5.0}
	};
	struct rt_arbn_internal arbn;
	memset(&arbn, 0, sizeof(arbn));
	arbn.magic = RT_ARBN_INTERNAL_MAGIC;
	arbn.neqn = 7;
	arbn.eqn = planes;

	struct rt_db_internal ip;
	RT_DB_INTERNAL_INIT(&ip);
	ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip.idb_minor_type = ID_ARBN;
	ip.idb_type = ID_ARBN;
	ip.idb_ptr = &arbn;
	ip.idb_meth = &OBJ[ID_ARBN];

	const double exact_volume = 47.0 / 48.0;
	const double exact_area = 45.0 / 8.0 + sqrt(3.0) / 8.0;
	fastf_t reported_volume = 0.0;
	fastf_t reported_area = 0.0;
	ip.idb_meth->ft_volume(&reported_volume, &ip);
	ip.idb_meth->ft_surf_area(&reported_area, &ip);
	double volume_error = rel_err(reported_volume, exact_volume) * 100.0;
	double area_error = rel_err(reported_area, exact_area) * 100.0;
	printf("  %-42s  SA_err=%.12f%%  V_err=%.12f%%  [%s]\n",
	       "ARBN clipped cube", area_error, volume_error,
	       (area_error <= 1.0e-9 && volume_error <= 1.0e-9) ?
	       "OK" : "FORMULA-FAIL");
	if (area_error > 1.0e-9 || volume_error > 1.0e-9)
	    failures++;
    }

    {
	struct rt_tgc_internal tgc;
	memset(&tgc, 0, sizeof(tgc));
	tgc.magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc.v, 0, 0, 0);
	VSET(tgc.h, 0, 0, 20);
	VSET(tgc.a, 5, 0, 0);
	VSET(tgc.b, 0, 5, 0);
	VSET(tgc.c, 5, 0, 0);
	VSET(tgc.d, 0, 5, 0);

	struct rt_db_internal ip;
	RT_DB_INTERNAL_INIT(&ip);
	ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip.idb_minor_type = ID_TGC;
	ip.idb_type       = ID_TGC;
	ip.idb_ptr        = &tgc;
	ip.idb_meth       = &OBJ[ID_TGC];

	double analytic_sa  = 2.0 * M_PI * 5.0 * (5.0 + 20.0);
	double analytic_vol = M_PI * 5.0 * 5.0 * 20.0;
	CROFTON_CHECK("RCC r=5 h=20", &ip, analytic_sa, analytic_vol);

	fastf_t tgc_vol = 0.0;
	ip.idb_meth->ft_volume(&tgc_vol, &ip);
	double tgc_vol_err = fabs(tgc_vol - analytic_vol) / analytic_vol * 100.0;
	printf("  %-42s  analytic_formula_err=%.2f%%  [%s]\n",
	       "RCC r=5 h=20 (rt_tgc_volume)",
	       tgc_vol_err,
	       (tgc_vol_err <= 0.01) ? "OK" : "FORMULA-FAIL");
	if (tgc_vol_err > 0.01) failures++;
    }

    {
	const double sin30 = 0.5;
	const double cos30 = sqrt(3.0) / 2.0;
	const double h_len = 20.0;
	const double r_cyl = 5.0;
	const double Hx    = h_len * sin30;
	const double Hz    = h_len * cos30;

	struct rt_tgc_internal tgc;
	memset(&tgc, 0, sizeof(tgc));
	tgc.magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc.v, 0, 0, 0);
	VSET(tgc.h, Hx, 0, Hz);
	VSET(tgc.a, r_cyl, 0, 0);
	VSET(tgc.b, 0, r_cyl, 0);
	VSET(tgc.c, r_cyl, 0, 0);
	VSET(tgc.d, 0, r_cyl, 0);

	struct rt_db_internal ip;
	RT_DB_INTERNAL_INIT(&ip);
	ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip.idb_minor_type = ID_TGC;
	ip.idb_type       = ID_TGC;
	ip.idb_ptr        = &tgc;
	ip.idb_meth       = &OBJ[ID_TGC];

	double h_perp       = Hz;
	double analytic_vol = M_PI * r_cyl * r_cyl * h_perp;

	{
	    const int N = 10000;
	    double sum = 0.0;
	    for (int k = 0; k < N; k++) {
		double phi = 2.0 * M_PI * k / N;
		sum += sqrt(Hz * Hz + Hx * Hx * cos(phi) * cos(phi));
	    }
	    double lateral_sa = r_cyl * sum * (2.0 * M_PI / N);
	    double end_caps   = 2.0 * M_PI * r_cyl * r_cyl;
	    double analytic_sa = lateral_sa + end_caps;
	    CROFTON_CHECK("oblique RCC 30deg tilt (Crofton)", &ip, analytic_sa, analytic_vol);
	}

	fastf_t tgc_vol = 0.0;
	ip.idb_meth->ft_volume(&tgc_vol, &ip);
	double tgc_vol_err = fabs(tgc_vol - analytic_vol) / analytic_vol * 100.0;
	double old_vol     = M_PI * r_cyl * r_cyl * h_len;
	double old_err     = fabs(old_vol - analytic_vol) / analytic_vol * 100.0;
	printf("  %-42s  analytic_formula_err=%.2f%%  [%s]  (old_err=%.1f%%)\n",
	       "oblique RCC 30deg (rt_tgc_volume)",
	       tgc_vol_err,
	       (tgc_vol_err <= 0.1) ? "OK" : "FORMULA-FAIL",
	       old_err);
	if (tgc_vol_err > 0.1) failures++;
    }

    /* Sub-mm TRC: reproduces the xyzringtrc.s class of geometry that
     * previously failed because rt_prep_parallel inflates mdl_min/mdl_max to
     * integer-mm boundaries (floor/ceil), blowing up the Crofton bounding
     * sphere and giving only ~90 expected crossings per 50 000 rays instead
     * of ~20 000.  The fix in rt_crofton_shoot uses tight soltab bounding
     * boxes instead of the inflated rti_radius.
     *
     * Geometry: right circular cone, base r=0.030 mm, apex r=0.0003 mm,
     *           height h=0.052 mm (100:1 taper, centered around origin).
     * Default cparams (zero n_rays → convergence-loop mode).              */
    {
	const double r1   = 0.030;    /* base radius   [mm] */
	const double r2   = 0.0003;   /* apex radius   [mm] */
	const double h    = 0.052;    /* height        [mm] */

	struct rt_tgc_internal tgc;
	memset(&tgc, 0, sizeof(tgc));
	tgc.magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc.v, 0, 0, 0);
	VSET(tgc.h, 0, 0, h);
	VSET(tgc.a, r1, 0, 0);
	VSET(tgc.b, 0, r1, 0);
	VSET(tgc.c, r2, 0, 0);
	VSET(tgc.d, 0, r2, 0);

	struct rt_db_internal ip;
	RT_DB_INTERNAL_INIT(&ip);
	ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip.idb_minor_type = ID_TGC;
	ip.idb_type       = ID_TGC;
	ip.idb_ptr        = &tgc;
	ip.idb_meth       = &OBJ[ID_TGC];

	double analytic_sa = M_PI * ((r1 + r2) * sqrt((r1-r2)*(r1-r2) + h*h)
				     + r1*r1 + r2*r2);
	double analytic_vol = M_PI * h * (r1*r1 + r1*r2 + r2*r2) / 3.0;
	CROFTON_CHECK("sub-mm TRC (r1=0.030,r2=0.0003,h=0.052)", &ip,
		      analytic_sa, analytic_vol);
    }

#undef CROFTON_CHECK

    printf("  Crofton verification: %d failure(s)\n", failures);
    return failures;
}


static int
build_convergence_db(struct db_i **dbip_out)
{
    struct db_i *dbip = db_open_inmem();
    if (!dbip) return -1;

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return -1;
    }

    point_t center = VINIT_ZERO;
    if (mk_sph(wdbp, "cvg_sphere.s", center, 25.0) < 0) {
	db_close(dbip);
	return -1;
    }

    point_t base = {0.0, 0.0, -20.0};
    vect_t h = {0.0, 0.0, 40.0};
    if (mk_rcc(wdbp, "cvg_rcc.s", base, h, 10.0) < 0) {
	db_close(dbip);
	return -1;
    }

    db_update_nref(dbip);
    *dbip_out = dbip;
    return 0;
}


static int
run_convergence_case(struct db_i *dbip,
		     const char *objname,
		     const char *label,
		     double sa_exact,
		     double v_exact,
		     const double *targets,
		     size_t ntargets,
		     double *elapsed_total_sec)
{
    int failures = 0;

    for (size_t i = 0; i < ntargets; i++) {
	double target_pct = targets[i];
	double sa = 0.0, vol = 0.0;
	double run_sec = 0.0;
	int cr = -1;

	struct rt_i *rtip = rt_i_create(dbip);
	if (!rtip) {
	    printf("  %-24s  target=%.1f%%  init-fail\n", label, target_pct);
	    failures++;
	    continue;
	}

	if (rt_gettree(rtip, objname) != 0) {
	    printf("  %-24s  target=%.1f%%  gettree-fail\n", label, target_pct);
	    rt_i_destroy(rtip);
	    failures++;
	    continue;
	}
	rt_prep_parallel(rtip, 1);

	/* Convergence-based stopping with a per-target wall-clock budget. */
	struct rt_crofton_params p = { 0u, 0.05, 1000.0, RT_CROFTON_STABILITY_DEFAULT, NULL, NULL };
	if (target_pct <= 7.0)
	    p.time_ms = 1500.0;
	if (target_pct <= 5.0)
	    p.time_ms = 2000.0;
	point_t aabb_min, aabb_max, obb[8];
	point_t *sample_points = NULL;
	size_t sample_count = 0;
	int64_t t0 = bu_gettime();
	cr = rt_crofton_shoot(&sa, &vol, &aabb_min, &aabb_max, obb,
	    &sample_points, &sample_count,
	    rtip, &p, NULL, NULL);
	run_sec = (double)(bu_gettime() - t0) / 1000000.0;

	if (i == 0) {
	    struct rt_crofton_result samples = RT_CROFTON_RESULT_INIT;
	    const size_t ray_offset = 4096u;
	    const size_t continuation_rays = 256u;
	    struct rt_crofton_params sample_params =
		{ray_offset + continuation_rays, 0.0, 0.0, RT_CROFTON_STABILITY_DEFAULT, NULL, NULL};
	    int sample_ret = rt_crofton_collect(&samples, rtip,
		&sample_params, 0, NULL, NULL);
	    int full_samples_valid = sample_ret > 0 && samples.segments &&
		samples.segment_count * 2 == samples.crossing_count &&
		samples.ray_count == sample_params.n_rays;
	    if (!full_samples_valid) {
		printf("  %-24s  invalid structured Crofton output\\n", label);
		failures++;
	    } else {
		for (size_t sample_index = 0;
		     sample_index < samples.segment_count; sample_index++) {
		    struct rt_crofton_segment *segment =
			&samples.segments[sample_index];
		    if (segment->thickness <= 0.0 ||
			!NEAR_EQUAL(DIST_PNT_PNT(segment->in_point,
				segment->out_point), segment->thickness, RT_LEN_TOL) ||
			!NEAR_EQUAL(MAGNITUDE(segment->in_normal), 1.0,
				VUNITIZE_TOL) ||
			!NEAR_EQUAL(MAGNITUDE(segment->out_normal), 1.0,
				VUNITIZE_TOL) ||
			segment->ray_id >= samples.ray_count) {
			printf("  %-24s  invalid structured segment\\n", label);
			failures++;
			break;
		    }
		}
	    }

	    {
		struct rt_crofton_stats stats;
		struct region_visit_counts counts = {0, 0.0};
		int visit_ret = rt_crofton_visit(&stats, rtip, &sample_params,
		    0, NULL, NULL, RT_CROFTON_SEQUENCE_RANDOM,
		    visit_region_segment, NULL, &counts);
		if (visit_ret <= 0 || counts.segments * 2 != stats.crossing_count ||
		    stats.ray_count != sample_params.n_rays ||
		    counts.chord <= 0.0 || stats.volume <= 0.0) {
		    printf("  %-24s  invalid streaming Crofton output\n", label);
		    failures++;
		}

		struct ray_visit_counts ray_counts = {0, 0, 1};
		visit_ret = rt_crofton_visit_rays(&stats, rtip, &sample_params,
		    0, NULL, NULL, RT_CROFTON_SEQUENCE_RANDOM,
		    visit_ray, NULL, &ray_counts);
		if (visit_ret <= 0 || ray_counts.rays != sample_params.n_rays ||
		    ray_counts.segments * 2 != stats.crossing_count ||
		    !ray_counts.valid) {
		    printf("  %-24s  invalid ray streaming Crofton output\n",
			label);
		    failures++;
		}
	    }

	    failures += check_seeded_streams(rtip, label,
		RT_CROFTON_SEQUENCE_RANDOM);
	    if (rt_crofton_qmc_available())
		failures += check_seeded_streams(rtip, label,
		    RT_CROFTON_SEQUENCE_QMC);
	    failures += check_stopping_controls(rtip, label);

	    struct rt_crofton_result offset_samples =
		RT_CROFTON_RESULT_INIT;
	    struct rt_crofton_params offset_params =
		{continuation_rays, 0.0, 0.0, RT_CROFTON_STABILITY_DEFAULT, NULL, NULL};
	    sample_ret = rt_crofton_collect(&offset_samples, rtip,
		&offset_params, ray_offset, NULL, NULL);
	    int offset_samples_valid = sample_ret > 0 &&
		offset_samples.ray_count == offset_params.n_rays;
	    if (!offset_samples_valid) {
		printf("  %-24s  invalid offset Crofton output\\n", label);
		failures++;
	    } else {
		for (size_t sample_index = 0;
		     sample_index < offset_samples.segment_count;
		     sample_index++) {
		    size_t ray_id = offset_samples.segments[sample_index].ray_id;
		    if (ray_id < ray_offset ||
			    ray_id >= ray_offset + offset_params.n_rays) {
			printf("  %-24s  invalid offset ray identifier\\n",
			    label);
			failures++;
			break;
		    }
		}
	    }

	    if (full_samples_valid && offset_samples_valid) {
		size_t suffix_start = 0;
		while (suffix_start < samples.segment_count &&
		       samples.segments[suffix_start].ray_id < ray_offset)
		    suffix_start++;
		size_t suffix_count = samples.segment_count - suffix_start;

		if (suffix_count != offset_samples.segment_count) {
		    printf("  %-24s  random stream continuation count mismatch\\n",
			label);
		    failures++;
		} else {
		    for (size_t sample_index = 0;
			 sample_index < suffix_count; sample_index++) {
			const struct rt_crofton_segment *full_segment =
			    &samples.segments[suffix_start + sample_index];
			const struct rt_crofton_segment *offset_segment =
			    &offset_samples.segments[sample_index];
			if (!crofton_segments_equal(full_segment, offset_segment)) {
			    printf("  %-24s  random stream continuation mismatch\\n",
				label);
			    failures++;
			    break;
			}
		    }
		}
	    }

	    rt_crofton_result_free(&samples);
	    if (samples.segments || samples.segment_count)
		failures++;
	    rt_crofton_result_free(&offset_samples);
	    if (offset_samples.segments || offset_samples.segment_count)
		failures++;
	}
	rt_i_destroy(rtip);

	if (elapsed_total_sec)
	    *elapsed_total_sec += run_sec;

	if (cr < 0) {
	    printf("  %-24s  target=%.1f%%  crofton-fail (ret=%d)\n",
		   label, target_pct, cr);
	    failures++;
	    if (sample_points)
		bu_free(sample_points, "Crofton test points");
	    continue;
	}
	if (!sample_points || sample_count != (size_t)cr ||
	    aabb_min[X] > aabb_max[X] || !isfinite(obb[0][X])) {
	    printf("  %-24s  invalid optional bounds/point outputs\n", label);
	    failures++;
	}
	if (sample_points)
	    bu_free(sample_points, "Crofton test points");

	double sa_err_pct = rel_err(sa, sa_exact) * 100.0;
	double v_err_pct = rel_err(vol, v_exact) * 100.0;
	double max_err_pct = (target_pct < 6.0) ? 10.0 : 12.0;

	printf("  %-24s  target=%.1f%%  time=%.3fs  SA_err=%.2f%%  V_err=%.2f%%\n",
	       label, target_pct, run_sec, sa_err_pct, v_err_pct);
	if (sa_err_pct > max_err_pct || v_err_pct > max_err_pct)
	    failures++;
    }

    return failures;
}


static int
test_crofton_convergence_timing(void)
{
    int failures = 0;
    struct db_i *dbip = DBI_NULL;
    const double targets[] = {10.0, 7.0, 5.0};
    double elapsed_total = 0.0;
    double target_total = 10.0;

    printf("\n--- Crofton convergence timing ---\n");

    if (build_convergence_db(&dbip) != 0 || !dbip) {
	printf("  FAIL: could not build in-memory convergence database\n");
	return 1;
    }

    failures += run_convergence_case(
	dbip, "cvg_sphere.s", "sphere r=25",
	4.0 * M_PI * 25.0 * 25.0,
	(4.0 / 3.0) * M_PI * 25.0 * 25.0 * 25.0,
	targets, 3, &elapsed_total);

    failures += run_convergence_case(
	dbip, "cvg_rcc.s", "rcc r=10 h=40",
	2.0 * M_PI * 10.0 * (10.0 + 40.0),
	M_PI * 10.0 * 10.0 * 40.0,
	targets, 3, &elapsed_total);

    printf("  total convergence runtime: %.3fs\n", elapsed_total);
    if (elapsed_total > target_total) {
	printf("  FAIL: convergence runtime %.3fs exceeds %.3fs target\n", elapsed_total, target_total);
	failures++;
    }

    db_close(dbip);
    printf("  Crofton convergence timing: %d failure(s)\n", failures);
    return failures;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc > 1)
	bu_exit(1, "Usage: %s\n", argv[0]);

    int failures = 0;
    failures += verify_crofton_estimates();
    failures += test_crofton_convergence_timing();

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return (failures > 0) ? 1 : 0;
}
