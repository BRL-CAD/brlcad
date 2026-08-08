/*                       B R E P _ R A Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file brep_ray.cpp
 *
 * Directed comparisons of analytic primitive and converted-BREP ray hits.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "bu/app.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"
#include "wdb.h"


struct ray_result {
    int shot_hits = 0;
    int segments = 0;
    double in_dist = 0.0;
    double out_dist = 0.0;
    vect_t in_normal = VINIT_ZERO;
    vect_t out_normal = VINIT_ZERO;
};


static const size_t MAX_TEST_PARTITIONS = 8;


struct partition_interval {
    double in_dist = 0.0;
    double out_dist = 0.0;
    vect_t in_normal = VINIT_ZERO;
    vect_t out_normal = VINIT_ZERO;
};


struct partition_result {
    size_t partitions = 0;
    bool overflow = false;
    partition_interval intervals[MAX_TEST_PARTITIONS];
};


struct prepared_model {
    struct db_i *dbip = NULL;
    struct rt_i *rtip = NULL;
    struct resource resp = {};
    bool resource_initialized = false;
};


struct sampled_ray {
    point_t origin = VINIT_ZERO;
    vect_t direction = VINIT_ZERO;
};


struct directed_partition_ray {
    const char *name;
    point_t origin;
    vect_t direction;
    size_t partitions;
    double distances[2 * MAX_TEST_PARTITIONS];
};


static struct soltab *
prep_solid(struct rt_i *rtip, struct rt_db_internal *intern, int type)
{
    struct soltab *stp = (struct soltab *)bu_calloc(1, sizeof(struct soltab),
	"direct ray test soltab");
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;
    stp->st_id = type;
    stp->st_meth = &OBJ[type];

    if (OBJ[type].ft_prep(stp, intern, rtip)) {
	if (stp->st_specific && stp->st_meth && stp->st_meth->ft_free)
	    stp->st_meth->ft_free(stp);
	bu_free(stp, "direct ray test soltab");
	return NULL;
    }
    return stp;
}


static void
free_solid(struct soltab *stp)
{
    if (!stp)
	return;
    if (stp->st_meth && stp->st_meth->ft_free)
	stp->st_meth->ft_free(stp);
    bu_free(stp, "direct ray test soltab");
}


static void
free_prepared_model(prepared_model &model)
{
    if (model.resource_initialized && model.rtip) {
	rt_clean_resource_basic(model.rtip, &model.resp);
	BU_PTBL_SET(&model.rtip->rti_resources, 0, NULL);
	model.resource_initialized = false;
    }
    if (model.rtip) {
	rt_i_destroy(model.rtip);
	model.rtip = NULL;
    }
    if (model.dbip) {
	db_close(model.dbip);
	model.dbip = NULL;
    }
}


static bool
prep_partition_model(prepared_model &model,
    const struct rt_db_internal *intern, const char *name,
    const struct bn_tol *tol)
{
    model.dbip = db_open_inmem();
    if (!model.dbip)
	return false;

    struct rt_wdb *wdbp = wdb_dbopen(model.dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	free_prepared_model(model);
	return false;
    }

    struct rt_db_internal tmp_intern;
    RT_DB_INTERNAL_INIT(&tmp_intern);
    tmp_intern.idb_major_type = intern->idb_major_type;
    tmp_intern.idb_type = intern->idb_minor_type;
    tmp_intern.idb_meth = &OBJ[intern->idb_minor_type];
    tmp_intern.idb_ptr = intern->idb_ptr;

    struct bu_external ext;
    BU_EXTERNAL_INIT(&ext);
    if (rt_db_cvt_to_ext5(&ext, name, &tmp_intern, 1.0, model.dbip,
	    intern->idb_major_type) < 0) {
	bu_free_external(&ext);
	free_prepared_model(model);
	return false;
    }

    if (wdb_export_external(wdbp, &ext, name,
	    db_flags_internal(&tmp_intern),
	    (unsigned char)intern->idb_minor_type) < 0) {
	bu_free_external(&ext);
	free_prepared_model(model);
	return false;
    }
    bu_free_external(&ext);
    db_update_nref(model.dbip);

    model.rtip = rt_i_create(model.dbip);
    if (!model.rtip) {
	free_prepared_model(model);
	return false;
    }
    model.rtip->rti_tol = *tol;
    if (rt_gettree(model.rtip, name) < 0) {
	free_prepared_model(model);
	return false;
    }
    rt_prep_parallel(model.rtip, 1);
    rt_init_resource(&model.resp, 0, model.rtip);
    model.resource_initialized = true;
    return true;
}


static bool
export_internal_object(struct db_i *dbip, struct rt_wdb *wdbp,
    const struct rt_db_internal *intern, const char *name)
{
    struct rt_db_internal tmp_intern;
    RT_DB_INTERNAL_INIT(&tmp_intern);
    tmp_intern.idb_major_type = intern->idb_major_type;
    tmp_intern.idb_type = intern->idb_minor_type;
    tmp_intern.idb_meth = &OBJ[intern->idb_minor_type];
    tmp_intern.idb_ptr = intern->idb_ptr;

    struct bu_external ext;
    BU_EXTERNAL_INIT(&ext);
    if (rt_db_cvt_to_ext5(&ext, name, &tmp_intern, 1.0, dbip,
	    intern->idb_major_type) < 0) {
	bu_free_external(&ext);
	return false;
    }
    if (wdb_export_external(wdbp, &ext, name,
	    db_flags_internal(&tmp_intern),
	    (unsigned char)intern->idb_minor_type) < 0) {
	bu_free_external(&ext);
	return false;
    }
    bu_free_external(&ext);
    return true;
}


static bool
export_brep_conversion(struct db_i *dbip, struct rt_wdb *wdbp,
    struct rt_db_internal *intern, const char *name,
    const struct bn_tol *tol)
{
    ON_Brep *brep = ON_Brep::New();
    OBJ[intern->idb_minor_type].ft_brep(&brep, intern, tol);
    if (!brep)
	return false;
    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal brep_intern;
    RT_DB_INTERNAL_INIT(&brep_intern);
    brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_intern.idb_type = ID_BREP;
    brep_intern.idb_meth = &OBJ[ID_BREP];
    brep_intern.idb_ptr = &brep_internal;
    const bool result = export_internal_object(dbip, wdbp, &brep_intern,
	name);
    delete brep;
    return result;
}


static bool
prep_region_model(prepared_model &model, const char *region_name,
    const struct bn_tol *tol)
{
    if (!model.dbip)
	return false;
    db_update_nref(model.dbip);
    model.rtip = rt_i_create(model.dbip);
    if (!model.rtip)
	return false;
    model.rtip->rti_tol = *tol;
    if (rt_gettree(model.rtip, region_name) < 0)
	return false;
    rt_prep_parallel(model.rtip, 1);
    rt_init_resource(&model.resp, 0, model.rtip);
    model.resource_initialized = true;
    return true;
}


static bool
prep_binary_csg_model(prepared_model &model,
    struct rt_db_internal *left_intern,
    struct rt_db_internal *right_intern, int member_operation,
    const struct bn_tol *tol, bool brep_leaves)
{
    model.dbip = db_open_inmem();
    if (!model.dbip)
	return false;
    struct rt_wdb *wdbp = wdb_dbopen(model.dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp)
	return false;

    const bool left_ok = brep_leaves ?
	export_brep_conversion(model.dbip, wdbp, left_intern, "left.s", tol) :
	export_internal_object(model.dbip, wdbp, left_intern, "left.s");
    const bool right_ok = brep_leaves ?
	export_brep_conversion(model.dbip, wdbp, right_intern, "right.s", tol) :
	export_internal_object(model.dbip, wdbp, right_intern, "right.s");
    struct wmember members;
    BU_LIST_INIT(&members.l);
    if (!left_ok || !right_ok ||
	    !mk_addmember("left.s", &members.l, NULL, WMOP_UNION) ||
	    !mk_addmember("right.s", &members.l, NULL, member_operation) ||
	    mk_lcomb(wdbp, "oracle.r", &members, 1, NULL, NULL, NULL, 0))
	return false;
    return prep_region_model(model, "oracle.r", tol);
}


static int
partition_hit(struct application *ap, struct partition *head,
    struct seg *UNUSED(segs))
{
    partition_result *result = static_cast<partition_result *>(ap->a_uptr);
    struct partition *pp;
    for (pp = head->pt_forw; pp != head; pp = pp->pt_forw) {
	if (result->partitions >= MAX_TEST_PARTITIONS) {
	    result->overflow = true;
	    continue;
	}
	partition_interval &interval = result->intervals[result->partitions++];
	interval.in_dist = pp->pt_inhit->hit_dist;
	interval.out_dist = pp->pt_outhit->hit_dist;
	RT_HIT_NORMAL(interval.in_normal, pp->pt_inhit,
	    pp->pt_inseg->seg_stp, &ap->a_ray, pp->pt_inflip);
	RT_HIT_NORMAL(interval.out_normal, pp->pt_outhit,
	    pp->pt_outseg->seg_stp, &ap->a_ray, pp->pt_outflip);
    }
    return 1;
}


static int
partition_miss(struct application *UNUSED(ap))
{
    return 0;
}


static partition_result
shoot_partitions(prepared_model &model, const sampled_ray &ray)
{
    partition_result result;
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = model.rtip;
    ap.a_resource = &model.resp;
    ap.a_hit = partition_hit;
    ap.a_miss = partition_miss;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_onehit = 0;
    ap.a_uptr = &result;
    VMOVE(ap.a_ray.r_pt, ray.origin);
    VMOVE(ap.a_ray.r_dir, ray.direction);
    ap.a_ray.magic = RT_RAY_MAGIC;
    rt_shootray(&ap);
    return result;
}


static ray_result
shoot_solid(struct soltab *stp, struct rt_i *rtip, struct resource *resp,
    const point_t origin, const vect_t direction)
{
    ray_result result;
    struct application ap;
    struct seg seghead;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = resp;
    ap.a_onehit = 0;
    VMOVE(ap.a_ray.r_pt, origin);
    VMOVE(ap.a_ray.r_dir, direction);
    VUNITIZE(ap.a_ray.r_dir);
    ap.a_ray.magic = RT_RAY_MAGIC;
    BU_LIST_INIT(&seghead.l);

    result.shot_hits = rt_obj_shot(stp, &ap.a_ray, &ap, &seghead);
    struct seg *segp;
    for (BU_LIST_FOR(segp, seg, &seghead.l)) {
	result.segments++;
	if (result.segments != 1)
	    continue;
	result.in_dist = segp->seg_in.hit_dist;
	result.out_dist = segp->seg_out.hit_dist;
	struct hit in = segp->seg_in;
	struct hit out = segp->seg_out;
	rt_obj_norm(&in, stp, &ap.a_ray);
	rt_obj_norm(&out, stp, &ap.a_ray);
	VMOVE(result.in_normal, in.hit_normal);
	VMOVE(result.out_normal, out.hit_normal);
    }

    while (BU_LIST_WHILE(segp, seg, &seghead.l)) {
	BU_LIST_DEQUEUE(&segp->l);
	RT_FREE_SEG(segp, resp);
    }
    return result;
}


static bool
finite_unit_vector(const vect_t value)
{
    return std::isfinite(value[X]) && std::isfinite(value[Y]) &&
	std::isfinite(value[Z]) && fabs(MAGNITUDE(value) - 1.0) < 1.0e-7;
}


static int
check_ray(const char *label, struct soltab *implicit_stp,
    struct soltab *brep_stp, struct rt_i *rtip, struct resource *resp,
    const point_t origin, const vect_t direction, double expected_in,
    double expected_out)
{
    const double distance_tolerance = std::max(1.0e-9,
	rtip->rti_tol.dist);
    int failures = 0;
    ray_result implicit_result = shoot_solid(implicit_stp, rtip, resp,
	origin, direction);
    ray_result brep_result = shoot_solid(brep_stp, rtip, resp, origin,
	direction);

    if (implicit_result.segments != 1 || brep_result.segments != 1) {
	std::printf("FAIL: %-18s segments implicit=%d BREP=%d\n", label,
	    implicit_result.segments, brep_result.segments);
	return 1;
    }

    if (fabs(implicit_result.in_dist - expected_in) > distance_tolerance ||
	    fabs(implicit_result.out_dist - expected_out) > distance_tolerance ||
	    fabs(brep_result.in_dist - expected_in) > distance_tolerance ||
	    fabs(brep_result.out_dist - expected_out) > distance_tolerance) {
	std::printf("FAIL: %-18s distances implicit=[%.17g %.17g] "
	    "BREP=[%.17g %.17g] expected=[%.17g %.17g]\n", label,
	    implicit_result.in_dist, implicit_result.out_dist,
	    brep_result.in_dist, brep_result.out_dist, expected_in,
	    expected_out);
	failures++;
    }

    if (fabs(implicit_result.in_dist - brep_result.in_dist) >
	    distance_tolerance ||
	    fabs(implicit_result.out_dist - brep_result.out_dist) >
	    distance_tolerance) {
	std::printf("FAIL: %-18s implicit/BREP distances differ\n", label);
	failures++;
    }

    if (!finite_unit_vector(brep_result.in_normal) ||
	    !finite_unit_vector(brep_result.out_normal) ||
	    VDOT(brep_result.in_normal, direction) >= 0.0 ||
	    VDOT(brep_result.out_normal, direction) <= 0.0) {
	std::printf("FAIL: %-18s invalid BREP normals in=(%.17g %.17g %.17g) "
	    "out=(%.17g %.17g %.17g)\n", label,
	    V3ARGS(brep_result.in_normal), V3ARGS(brep_result.out_normal));
	failures++;
    }

    return failures;
}


static const char *
ray_class(const ray_result &result, double tolerance)
{
    if (result.segments == 0)
	return "MISS";
    if (result.segments != 1)
	return "MULTI";
    if (fabs(result.out_dist - result.in_dist) <= tolerance)
	return "CONTACT";
    return "INTERVAL";
}


static std::vector<double>
grazing_clearances(double radius, double distance_tolerance)
{
    std::vector<double> clearances;
    for (int exponent = 1; exponent <= 50; ++exponent)
	clearances.push_back(std::ldexp(radius, -exponent));

    const double chord_ratios[] = {100.0, 10.0, 2.0, 1.1, 1.0, 0.9,
	0.5, 0.1, 0.01};
    for (size_t i = 0; i < sizeof(chord_ratios) / sizeof(chord_ratios[0]);
	    ++i) {
	const double half_chord = chord_ratios[i] * distance_tolerance * 0.5;
	const double root = sqrt(std::max(0.0,
	    radius * radius - half_chord * half_chord));
	clearances.push_back((half_chord * half_chord) / (radius + root));
    }

    std::sort(clearances.begin(), clearances.end(), std::greater<double>());
    clearances.erase(std::unique(clearances.begin(), clearances.end()),
	clearances.end());
    return clearances;
}


static void
grazing_report(struct soltab *implicit_stp, struct soltab *brep_stp,
    struct rt_i *rtip, struct resource *resp, double radius)
{
    std::vector<double> clearances = grazing_clearances(radius,
	rtip->rti_tol.dist);

    std::printf("# signed sphere grazing report\n");
    std::printf("# h/R chord/tol implicit implicit_chord BREP BREP_chord "
	"BREP_endpoint_error\n");

    vect_t direction = {1.0, 0.0, 0.0};
    for (int sign_value = 1; sign_value >= -1; --sign_value) {
	if (sign_value == 0) {
	    point_t origin = {-2.0 * radius, radius, 0.0};
	    ray_result implicit_result = shoot_solid(implicit_stp, rtip, resp,
		origin, direction);
	    ray_result brep_result = shoot_solid(brep_stp, rtip, resp, origin,
		direction);
	    std::printf("% .17g % .17g %-8s % .17g %-8s % .17g % .17g\n",
		0.0, 0.0, ray_class(implicit_result, rtip->rti_tol.dist),
		implicit_result.out_dist - implicit_result.in_dist,
		ray_class(brep_result, rtip->rti_tol.dist),
		brep_result.out_dist - brep_result.in_dist, 0.0);
	    continue;
	}
	for (size_t i = 0; i < clearances.size(); ++i) {
	    const double h = sign_value * clearances[i];
	    const double b = radius - h;
	    point_t origin = {-2.0 * radius, b, 0.0};
	    ray_result implicit_result = shoot_solid(implicit_stp, rtip, resp,
		origin, direction);
	    ray_result brep_result = shoot_solid(brep_stp, rtip, resp, origin,
		direction);
	    const double analytic_chord = h > 0.0 ?
		2.0 * sqrt(std::max(0.0, 2.0 * radius * h - h * h)) : 0.0;
	    const double brep_chord = brep_result.segments == 1 ?
		brep_result.out_dist - brep_result.in_dist : 0.0;
	    double endpoint_error = 0.0;
	    if (h > 0.0 && brep_result.segments == 1) {
		const double expected_in = 2.0 * radius - analytic_chord * 0.5;
		const double expected_out = 2.0 * radius + analytic_chord * 0.5;
		endpoint_error = std::max(fabs(brep_result.in_dist - expected_in),
		    fabs(brep_result.out_dist - expected_out));
	    }
	    std::printf("% .17g % .17g %-8s % .17g %-8s % .17g % .17g\n",
		h / radius, analytic_chord / rtip->rti_tol.dist,
		ray_class(implicit_result, rtip->rti_tol.dist),
		implicit_result.segments == 1 ? implicit_result.out_dist -
		implicit_result.in_dist : 0.0,
		ray_class(brep_result, rtip->rti_tol.dist), brep_chord,
		endpoint_error);
	}
    }
}


static int
check_grazing_ratchet(struct soltab *implicit_stp, struct soltab *brep_stp,
    struct rt_i *rtip, struct resource *resp, double radius)
{
    int failures = 0;
    bool brep_interval_ended = false;
    double previous_brep_chord = INFINITY;
    vect_t direction = {1.0, 0.0, 0.0};
    const std::vector<double> clearances = grazing_clearances(radius,
	rtip->rti_tol.dist);

    /* Resolved intervals must agree with the analytic oracle.  Below the
     * model distance tolerance, either a contact-sized segment or no segment
     * is acceptable, but an interval must never restart nearer the tangent. */
    for (size_t i = 0; i < clearances.size(); ++i) {
	const double h = clearances[i];
	const double b = radius - h;
	if (!(b < radius))
	    continue;
	point_t origin = {-2.0 * radius, b, 0.0};
	const double analytic_chord =
	    2.0 * sqrt(std::max(0.0, 2.0 * radius * h - h * h));
	ray_result implicit_result = shoot_solid(implicit_stp, rtip, resp,
	    origin, direction);
	ray_result brep_result = shoot_solid(brep_stp, rtip, resp, origin,
	    direction);
	ray_result repeated_result = shoot_solid(brep_stp, rtip, resp, origin,
	    direction);

	if (brep_result.segments != repeated_result.segments ||
	    (brep_result.segments == 1 &&
	    (std::memcmp(&brep_result.in_dist, &repeated_result.in_dist,
		sizeof(double)) != 0 ||
	    std::memcmp(&brep_result.out_dist, &repeated_result.out_dist,
		sizeof(double)) != 0))) {
	    std::printf("FAIL: grazing h/R=%.17g is nondeterministic\n",
		h / radius);
	    failures++;
	}

	if (brep_result.segments == 1) {
	    const double brep_chord = brep_result.out_dist -
		brep_result.in_dist;
	    if (brep_interval_ended) {
		std::printf("FAIL: grazing interval restarted at h/R=%.17g\n",
		    h / radius);
		failures++;
	    }
	    if (brep_chord > previous_brep_chord + rtip->rti_tol.dist) {
		std::printf("FAIL: grazing chord grew at h/R=%.17g: "
		    "%.17g > %.17g\n", h / radius, brep_chord,
		    previous_brep_chord);
		failures++;
	    }
	    previous_brep_chord = brep_chord;
	} else {
	    brep_interval_ended = true;
	}

	if (analytic_chord + rtip->rti_tol.dist * 1.0e-6 >=
		rtip->rti_tol.dist) {
	    const double expected_in = 2.0 * radius - analytic_chord * 0.5;
	    const double expected_out = 2.0 * radius + analytic_chord * 0.5;
	    if (implicit_result.segments != 1 || brep_result.segments != 1 ||
		    fabs(brep_result.in_dist - expected_in) > rtip->rti_tol.dist ||
		    fabs(brep_result.out_dist - expected_out) > rtip->rti_tol.dist) {
		std::printf("FAIL: resolved grazing interval h/R=%.17g "
		    "implicit=%d BREP=%d\n", h / radius,
		    implicit_result.segments, brep_result.segments);
		failures++;
	    }
	} else if (brep_result.segments > 1 ||
		(brep_result.segments == 1 &&
		brep_result.out_dist - brep_result.in_dist >
		rtip->rti_tol.dist + 1.0e-9)) {
	    std::printf("FAIL: sub-tolerance grazing material h/R=%.17g "
		"BREP=%d chord=%.17g\n", h / radius,
		brep_result.segments, brep_result.out_dist -
		brep_result.in_dist);
	    failures++;
	}
    }

    /* Exact tangency and every representable mirrored outside clearance must
     * be misses for both the analytic and converted representations. */
    point_t tangent_origin = {-2.0 * radius, radius, 0.0};
    ray_result implicit_tangent = shoot_solid(implicit_stp, rtip, resp,
	tangent_origin, direction);
    ray_result brep_tangent = shoot_solid(brep_stp, rtip, resp,
	tangent_origin, direction);
    if (implicit_tangent.segments != 0 || brep_tangent.segments != 0) {
	std::printf("FAIL: exact tangent did not miss: implicit=%d BREP=%d\n",
	    implicit_tangent.segments, brep_tangent.segments);
	failures++;
    }

    for (size_t i = 0; i < clearances.size(); ++i) {
	const double h = -clearances[i];
	point_t origin = {-2.0 * radius, radius - h, 0.0};
	if (!(origin[Y] > radius))
	    continue;
	ray_result implicit_result = shoot_solid(implicit_stp, rtip, resp,
	    origin, direction);
	ray_result brep_result = shoot_solid(brep_stp, rtip, resp, origin,
	    direction);
	if (implicit_result.segments != 0 || brep_result.segments != 0) {
	    std::printf("FAIL: outside grazing h/R=%.17g did not miss: "
		"implicit=%d BREP=%d\n", h / radius,
		implicit_result.segments, brep_result.segments);
	    failures++;
	}
    }

    /* The implicit sphere rejects an outward ray beginning on its surface.
     * BREP currently returns the entirely nonpositive segment [-2R, 0].
     * Permit that known result to disappear, but prevent it from becoming
     * nondeterministic or leaking material forward from the ray origin. */
    point_t boundary_origin = {radius, 0.0, 0.0};
    vect_t boundary_direction = {1.0, 0.0, 0.0};
    ray_result implicit_boundary = shoot_solid(implicit_stp, rtip, resp,
	boundary_origin, boundary_direction);
    ray_result brep_boundary = shoot_solid(brep_stp, rtip, resp,
	boundary_origin, boundary_direction);
    ray_result repeated_boundary = shoot_solid(brep_stp, rtip, resp,
	boundary_origin, boundary_direction);
    if (implicit_boundary.segments != 0 ||
	    brep_boundary.segments != repeated_boundary.segments ||
	    (brep_boundary.segments == 1 &&
	    (brep_boundary.out_dist > rtip->rti_tol.dist ||
	    std::memcmp(&brep_boundary.in_dist, &repeated_boundary.in_dist,
		sizeof(double)) != 0 ||
	    std::memcmp(&brep_boundary.out_dist, &repeated_boundary.out_dist,
		sizeof(double)) != 0)) ||
	    brep_boundary.segments > 1) {
	std::printf("FAIL: outward surface-start ratchet implicit=%d BREP=%d "
	    "interval=[%.17g %.17g]\n", implicit_boundary.segments,
	    brep_boundary.segments, brep_boundary.in_dist,
	    brep_boundary.out_dist);
	failures++;
    }

    return failures;
}


static int
check_transformed_sphere(struct rt_i *rtip, struct resource *resp,
    const char *fixture_name, const point_t center, double radius)
{
    int failures = 0;
    struct rt_ell_internal ell = {};
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VMOVE(ell.v, center);
    VSET(ell.a, radius, 0.0, 0.0);
    VSET(ell.b, 0.0, radius, 0.0);
    VSET(ell.c, 0.0, 0.0, radius);

    struct rt_db_internal ell_intern;
    RT_DB_INTERNAL_INIT(&ell_intern);
    ell_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ell_intern.idb_type = ID_ELL;
    ell_intern.idb_meth = &OBJ[ID_ELL];
    ell_intern.idb_ptr = &ell;

    struct soltab *implicit_stp = prep_solid(rtip, &ell_intern, ID_ELL);
    if (!implicit_stp) {
	std::printf("FAIL: %s implicit prep\n", fixture_name);
	return 1;
    }

    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_ELL].ft_brep(&brep, &ell_intern, &rtip->rti_tol);
    if (!brep) {
	std::printf("FAIL: %s BREP conversion\n", fixture_name);
	free_solid(implicit_stp);
	return 1;
    }
    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal brep_intern;
    RT_DB_INTERNAL_INIT(&brep_intern);
    brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_intern.idb_type = ID_BREP;
    brep_intern.idb_meth = &OBJ[ID_BREP];
    brep_intern.idb_ptr = &brep_internal;

    struct soltab *brep_stp = prep_solid(rtip, &brep_intern, ID_BREP);
    if (!brep_stp) {
	std::printf("FAIL: %s BREP prep\n", fixture_name);
	delete brep_internal.brep;
	free_solid(implicit_stp);
	return 1;
    }

    struct transformed_ray {
	const char *name;
	point_t origin;
	vect_t direction;
	double expected_in;
	double expected_out;
    } rays[3];
    rays[0].name = "north pole";
    VSET(rays[0].origin, center[X], center[Y], center[Z] + 2.0 * radius);
    VSET(rays[0].direction, 0.0, 0.0, -1.0);
    rays[0].expected_in = radius;
    rays[0].expected_out = 3.0 * radius;
    rays[1].name = "positive-x seam";
    VSET(rays[1].origin, center[X] + 2.0 * radius, center[Y], center[Z]);
    VSET(rays[1].direction, -1.0, 0.0, 0.0);
    rays[1].expected_in = radius;
    rays[1].expected_out = 3.0 * radius;
    rays[2].name = "inside";
    VMOVE(rays[2].origin, center);
    VSET(rays[2].direction, 0.0, 1.0, 0.0);
    rays[2].expected_in = -radius;
    rays[2].expected_out = radius;

    for (int repeat = 0; repeat < 4; ++repeat) {
	for (size_t i = 0; i < sizeof(rays) / sizeof(rays[0]); ++i) {
	    char label[128];
	    std::snprintf(label, sizeof(label), "%s %s", fixture_name,
		rays[i].name);
	    failures += check_ray(label, implicit_stp, brep_stp, rtip, resp,
		rays[i].origin, rays[i].direction, rays[i].expected_in,
		rays[i].expected_out);
	}
    }

    free_solid(brep_stp);
    free_solid(implicit_stp);
    return failures;
}


static double
relative_error(double observed, double expected)
{
    return fabs(expected) > SMALL_FASTF ? fabs(observed - expected) /
	fabs(expected) : fabs(observed);
}


struct sampled_point {
    point_t value = VINIT_ZERO;
};


static double
paired_rand01(std::mt19937_64 &rng)
{
    return std::generate_canonical<double, 53>(rng);
}


static void
paired_point_on_sphere(double radius, const point_t center, point_t out,
    std::mt19937_64 &rng)
{
    const double theta = 2.0 * M_PI * paired_rand01(rng);
    const double phi = acos(2.0 * paired_rand01(rng) - 1.0);
    const double sin_phi = sin(phi);
    out[X] = center[X] + radius * sin_phi * cos(theta);
    out[Y] = center[Y] + radius * sin_phi * sin(theta);
    out[Z] = center[Z] + radius * cos(phi);
}


static std::vector<sampled_ray>
generate_paired_rays(size_t ray_count, double radius, const point_t center)
{
    static const uint64_t seed = 0x9e3779b97f4a7c15ULL;
    std::mt19937_64 rng(seed);
    std::vector<sampled_point> points(ray_count * 2);
    for (size_t i = 0; i < points.size(); ++i)
	paired_point_on_sphere(radius, center, points[i].value, rng);

    for (size_t i = points.size() - 1; i > 0; --i) {
	size_t j = (size_t)(paired_rand01(rng) * (i + 1));
	if (j > i)
	    j = i;
	std::swap(points[i], points[j]);
    }

    std::vector<sampled_ray> rays(ray_count);
    for (size_t i = 0; i < ray_count; ++i) {
	VMOVE(rays[i].origin, points[2 * i].value);
	VSUB2(rays[i].direction, points[2 * i + 1].value,
	    points[2 * i].value);
	VUNITIZE(rays[i].direction);
    }
    return rays;
}


static double
partition_chord(const partition_result &result)
{
    double chord = 0.0;
    for (size_t i = 0; i < result.partitions; ++i)
	chord += result.intervals[i].out_dist - result.intervals[i].in_dist;
    return chord;
}


static bool
partition_result_valid(const partition_result &result, const vect_t direction)
{
    if (result.overflow)
	return false;
    for (size_t i = 0; i < result.partitions; ++i) {
	const partition_interval &interval = result.intervals[i];
	if (!std::isfinite(interval.in_dist) ||
		!std::isfinite(interval.out_dist) ||
		interval.out_dist < interval.in_dist ||
		!finite_unit_vector(interval.in_normal) ||
		!finite_unit_vector(interval.out_normal) ||
		VDOT(interval.in_normal, direction) >= 0.0 ||
		VDOT(interval.out_normal, direction) <= 0.0)
	    return false;
    }
    return true;
}


static double
sample_standard_error(double sum, double sum_squared, size_t sample_count)
{
    if (sample_count < 2)
	return 0.0;
    const double variance = std::max(0.0,
	(sum_squared - sum * sum / sample_count) / (sample_count - 1));
    return sqrt(variance / sample_count);
}


static int
check_shared_crofton_fixture(const char *label,
    struct rt_db_internal *implicit_intern, const struct bn_tol *tol,
    const point_t bbox_min, const point_t bbox_max, double analytic_area,
    double analytic_volume, size_t ray_count,
    const directed_partition_ray *directed_rays = NULL,
    size_t directed_ray_count = 0,
    prepared_model *supplied_implicit_model = NULL,
    prepared_model *supplied_brep_model = NULL,
    bool enforce_comparison = true, size_t *comparison_issues_out = NULL)
{
    int failures = 0;
    size_t comparison_issues = 0;
    const char *comparison_status = enforce_comparison ? "FAIL" : "KNOWN";
    prepared_model local_implicit_model;
    prepared_model local_brep_model;
    prepared_model &implicit_model = supplied_implicit_model ?
	*supplied_implicit_model : local_implicit_model;
    prepared_model &brep_model = supplied_brep_model ?
	*supplied_brep_model : local_brep_model;

    if ((supplied_implicit_model == NULL) !=
	    (supplied_brep_model == NULL)) {
	std::printf("FAIL: %s incomplete supplied model pair\n", label);
	return 1;
    }

    struct rt_brep_internal brep_internal = {};
    if (!supplied_implicit_model) {
	ON_Brep *brep = ON_Brep::New();
	OBJ[implicit_intern->idb_minor_type].ft_brep(&brep, implicit_intern,
	    tol);
	if (!brep) {
	    std::printf("FAIL: %s shared Crofton BREP conversion\n", label);
	    return 1;
	}

	brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
	brep_internal.brep = brep;
	struct rt_db_internal brep_intern;
	RT_DB_INTERNAL_INIT(&brep_intern);
	brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	brep_intern.idb_type = ID_BREP;
	brep_intern.idb_meth = &OBJ[ID_BREP];
	brep_intern.idb_ptr = &brep_internal;

	if (!prep_partition_model(implicit_model, implicit_intern,
		"paired_implicit.s", tol) ||
		!prep_partition_model(brep_model, &brep_intern,
		"paired_brep.s", tol)) {
	    std::printf("FAIL: %s shared Crofton model preparation\n", label);
	    free_prepared_model(brep_model);
	    free_prepared_model(implicit_model);
	    delete brep_internal.brep;
	    return 1;
	}
    }

    for (size_t i = 0; i < directed_ray_count; ++i) {
	sampled_ray ray;
	VMOVE(ray.origin, directed_rays[i].origin);
	VMOVE(ray.direction, directed_rays[i].direction);
	VUNITIZE(ray.direction);
	partition_result implicit_result = shoot_partitions(implicit_model, ray);
	partition_result brep_result = shoot_partitions(brep_model, ray);
	bool valid = partition_result_valid(implicit_result, ray.direction) &&
	    partition_result_valid(brep_result, ray.direction) &&
	    implicit_result.partitions == directed_rays[i].partitions &&
	    brep_result.partitions == directed_rays[i].partitions;
	for (size_t j = 0; valid && j < directed_rays[i].partitions; ++j) {
	    const double expected_in = directed_rays[i].distances[2 * j];
	    const double expected_out = directed_rays[i].distances[2 * j + 1];
	    valid = fabs(implicit_result.intervals[j].in_dist - expected_in) <=
		tol->dist &&
		fabs(implicit_result.intervals[j].out_dist - expected_out) <=
		tol->dist &&
		fabs(brep_result.intervals[j].in_dist - expected_in) <=
		tol->dist &&
		fabs(brep_result.intervals[j].out_dist - expected_out) <=
		tol->dist;
	}
	if (!valid) {
	    std::printf("%s: %s directed partition %s implicit=%zu "
		"BREP=%zu expected=%zu\n", comparison_status, label,
		directed_rays[i].name,
		implicit_result.partitions, brep_result.partitions,
		directed_rays[i].partitions);
	    comparison_issues++;
	}
    }

    const size_t candidate_checkpoints[] = {2000, 8000, 32000, 128000};
    std::vector<size_t> checkpoints;
    for (size_t i = 0; i < sizeof(candidate_checkpoints) /
	    sizeof(candidate_checkpoints[0]); ++i) {
	if (candidate_checkpoints[i] <= ray_count)
	    checkpoints.push_back(candidate_checkpoints[i]);
    }
    if (checkpoints.empty() || checkpoints.back() != ray_count)
	checkpoints.push_back(ray_count);

    point_t center;
    VADD2SCALE(center, bbox_max, bbox_min, 0.5);
    vect_t bbox_diagonal;
    VSUB2(bbox_diagonal, bbox_max, bbox_min);
    const double sampling_radius = 0.5 * MAGNITUDE(bbox_diagonal);
    const std::vector<sampled_ray> rays = generate_paired_rays(ray_count,
	sampling_radius, center);
    size_t implicit_crossings = 0;
    size_t brep_crossings = 0;
    size_t differing_lines = 0;
    size_t tangent_band_lines = 0;
    double implicit_total_chord = 0.0;
    double brep_total_chord = 0.0;
    double signed_chord_difference = 0.0;
    double absolute_chord_difference = 0.0;
    double maximum_endpoint_error = 0.0;
    double implicit_area_sum = 0.0;
    double implicit_area_sum_squared = 0.0;
    double brep_area_sum = 0.0;
    double brep_area_sum_squared = 0.0;
    double implicit_volume_sum = 0.0;
    double implicit_volume_sum_squared = 0.0;
    double brep_volume_sum = 0.0;
    double brep_volume_sum_squared = 0.0;
    const double area_scale = 4.0 * M_PI * sampling_radius *
	sampling_radius;
    const double volume_scale = M_PI * sampling_radius * sampling_radius;
    size_t checkpoint_index = 0;
    size_t reported = 0;

    for (size_t i = 0; i < rays.size(); ++i) {
	partition_result implicit_result = shoot_partitions(implicit_model,
	    rays[i]);
	partition_result brep_result = shoot_partitions(brep_model, rays[i]);
	const double implicit_chord = partition_chord(implicit_result);
	const double brep_chord = partition_chord(brep_result);
	implicit_crossings += 2 * implicit_result.partitions;
	brep_crossings += 2 * brep_result.partitions;
	implicit_total_chord += implicit_chord;
	brep_total_chord += brep_chord;
	signed_chord_difference += brep_chord - implicit_chord;
	absolute_chord_difference += fabs(brep_chord - implicit_chord);
	const double implicit_area_contribution = area_scale *
	    implicit_result.partitions;
	const double brep_area_contribution = area_scale *
	    brep_result.partitions;
	const double implicit_volume_contribution = volume_scale *
	    implicit_chord;
	const double brep_volume_contribution = volume_scale * brep_chord;
	implicit_area_sum += implicit_area_contribution;
	implicit_area_sum_squared += implicit_area_contribution *
	    implicit_area_contribution;
	brep_area_sum += brep_area_contribution;
	brep_area_sum_squared += brep_area_contribution *
	    brep_area_contribution;
	implicit_volume_sum += implicit_volume_contribution;
	implicit_volume_sum_squared += implicit_volume_contribution *
	    implicit_volume_contribution;
	brep_volume_sum += brep_volume_contribution;
	brep_volume_sum_squared += brep_volume_contribution *
	    brep_volume_contribution;

	if (!partition_result_valid(implicit_result, rays[i].direction) ||
		!partition_result_valid(brep_result, rays[i].direction)) {
	    if (reported++ < 5)
		std::printf("%s: %s shared Crofton ray %zu has invalid "
		    "partition data\n", comparison_status, label, i);
	    comparison_issues++;
	    continue;
	}

	bool line_differs = implicit_result.partitions !=
	    brep_result.partitions;
	bool tangent_band = false;
	if (line_differs) {
	    tangent_band = std::max(implicit_chord, brep_chord) <= tol->dist;
	    if (tangent_band) {
		tangent_band_lines++;
	    } else {
		if (reported++ < 5)
		    std::printf("%s: %s shared Crofton ray %zu partition "
			"count implicit=%zu BREP=%zu chords=[%.17g %.17g]\n",
			comparison_status, label, i,
			implicit_result.partitions, brep_result.partitions,
			implicit_chord, brep_chord);
		comparison_issues++;
	    }
	}

	if (!line_differs) {
	    for (size_t j = 0; j < implicit_result.partitions; ++j) {
		const double in_error = fabs(implicit_result.intervals[j].in_dist -
		    brep_result.intervals[j].in_dist);
		const double out_error = fabs(implicit_result.intervals[j].out_dist -
		    brep_result.intervals[j].out_dist);
		maximum_endpoint_error = std::max(maximum_endpoint_error,
		    std::max(in_error, out_error));
		if (in_error > tol->dist || out_error > tol->dist) {
		    line_differs = true;
		    if (reported++ < 5)
			std::printf("%s: %s shared Crofton ray %zu endpoint "
			    "errors=[%.17g %.17g]\n", comparison_status,
			    label, i, in_error,
			    out_error);
		    comparison_issues++;
		}
	    }
	}
	if (line_differs)
	    differing_lines++;

	if (checkpoint_index < checkpoints.size() &&
		i + 1 == checkpoints[checkpoint_index]) {
	    const size_t samples = i + 1;
	    const double implicit_area_estimate = implicit_area_sum / samples;
	    const double brep_area_estimate = brep_area_sum / samples;
	    const double implicit_volume_estimate = implicit_volume_sum / samples;
	    const double brep_volume_estimate = brep_volume_sum / samples;
	    const double implicit_area_se = sample_standard_error(
		implicit_area_sum, implicit_area_sum_squared, samples);
	    const double brep_area_se = sample_standard_error(brep_area_sum,
		brep_area_sum_squared, samples);
	    const double implicit_volume_se = sample_standard_error(
		implicit_volume_sum, implicit_volume_sum_squared, samples);
	    const double brep_volume_se = sample_standard_error(brep_volume_sum,
		brep_volume_sum_squared, samples);
	    const double area_band = std::max(analytic_area * 0.01,
		4.0 * std::max(implicit_area_se, brep_area_se));
	    const double volume_band = std::max(analytic_volume * 0.01,
		4.0 * std::max(implicit_volume_se, brep_volume_se));
	    if (fabs(implicit_area_estimate - analytic_area) > area_band ||
		    fabs(brep_area_estimate - analytic_area) > area_band ||
		    fabs(implicit_volume_estimate - analytic_volume) >
		    volume_band ||
		    fabs(brep_volume_estimate - analytic_volume) > volume_band) {
		std::printf("%s: %s shared Crofton confidence at %zu rays "
		    "area-band=%.17g volume-band=%.17g\n",
		    comparison_status, label, samples, area_band, volume_band);
		comparison_issues++;
	    }
	    std::printf("Shared Crofton %s checkpoint %zu: area implicit="
		"%.9g+/-%.3g BREP=%.9g+/-%.3g volume implicit="
		"%.9g+/-%.3g BREP=%.9g+/-%.3g\n", label, samples,
		implicit_area_estimate, implicit_area_se, brep_area_estimate,
		brep_area_se, implicit_volume_estimate, implicit_volume_se,
		brep_volume_estimate, brep_volume_se);
	    checkpoint_index++;
	}
    }

    const double implicit_area = 4.0 * M_PI * sampling_radius *
	sampling_radius * implicit_crossings / (2.0 * ray_count);
    const double brep_area = 4.0 * M_PI * sampling_radius *
	sampling_radius * brep_crossings / (2.0 * ray_count);
    const double implicit_volume = M_PI * sampling_radius * sampling_radius *
	implicit_total_chord / ray_count;
    const double brep_volume = M_PI * sampling_radius * sampling_radius *
	brep_total_chord / ray_count;
    if (relative_error(implicit_area, analytic_area) > 0.06 ||
	    relative_error(implicit_volume, analytic_volume) > 0.06 ||
	    relative_error(brep_area, analytic_area) > 0.06 ||
	    relative_error(brep_volume, analytic_volume) > 0.06 ||
	    relative_error(brep_area, implicit_area) > 0.001 ||
	    relative_error(brep_volume, implicit_volume) > 0.001) {
	std::printf("%s: %s shared Crofton aggregates analytic="
	    "[%.17g %.17g] "
	    "implicit=[%.17g %.17g] BREP=[%.17g %.17g]\n",
	    comparison_status, label, analytic_area, analytic_volume,
	    implicit_area, implicit_volume, brep_area, brep_volume);
	comparison_issues++;
    }

    std::printf("Shared Crofton %s: rays=%zu differing=%zu "
	"tangent-band=%zu max-endpoint=%.3g signed-chord=%.3g "
	"absolute-chord=%.3g\n", label, ray_count, differing_lines,
	tangent_band_lines, maximum_endpoint_error, signed_chord_difference,
	absolute_chord_difference);

    if (!supplied_implicit_model) {
	free_prepared_model(brep_model);
	free_prepared_model(implicit_model);
	delete brep_internal.brep;
    }
    if (comparison_issues_out)
	*comparison_issues_out = comparison_issues;
    if (enforce_comparison)
	failures += (int)comparison_issues;
    return failures;
}


static int
check_brep_leaf_csg_fixture(const char *label,
    struct rt_db_internal *left_intern,
    struct rt_db_internal *right_intern, int member_operation,
    const struct bn_tol *tol, const point_t bbox_min,
    const point_t bbox_max, double analytic_area, double analytic_volume,
    const directed_partition_ray *directed_rays,
    size_t directed_ray_count)
{
    prepared_model implicit_model;
    prepared_model brep_model;
    if (!prep_binary_csg_model(implicit_model, left_intern, right_intern,
	    member_operation, tol, false) ||
	    !prep_binary_csg_model(brep_model, left_intern, right_intern,
	    member_operation, tol, true)) {
	std::printf("FAIL: %s BREP-leaf CSG preparation\n", label);
	free_prepared_model(brep_model);
	free_prepared_model(implicit_model);
	return 1;
    }

    const int failures = check_shared_crofton_fixture(label, NULL, tol,
	bbox_min, bbox_max, analytic_area, analytic_volume, 8000,
	directed_rays, directed_ray_count, &implicit_model, &brep_model);
    free_prepared_model(brep_model);
    free_prepared_model(implicit_model);
    return failures;
}


static int
check_shared_primitive_corpus(const struct bn_tol *tol)
{
    int failures = 0;

    struct rt_ell_internal ellipsoid = {};
    ellipsoid.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ellipsoid.v, 4.0, -3.0, 2.0);
    VSET(ellipsoid.a, 10.0, 0.0, 0.0);
    VSET(ellipsoid.b, 0.0, 10.0, 0.0);
    VSET(ellipsoid.c, 0.0, 0.0, 6.0);
    struct rt_db_internal ellipsoid_intern;
    RT_DB_INTERNAL_INIT(&ellipsoid_intern);
    ellipsoid_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ellipsoid_intern.idb_type = ID_ELL;
    ellipsoid_intern.idb_meth = &OBJ[ID_ELL];
    ellipsoid_intern.idb_ptr = &ellipsoid;
    point_t ellipsoid_min = {-6.0, -13.0, -4.0};
    point_t ellipsoid_max = {14.0, 7.0, 8.0};
    const double eccentricity = 0.8;
    const double ellipsoid_area = 2.0 * M_PI * 100.0 *
	(1.0 + (1.0 - eccentricity * eccentricity) /
	 eccentricity * atanh(eccentricity));
    const double ellipsoid_volume = (4.0 / 3.0) * M_PI * 100.0 * 6.0;
    const directed_partition_ray ellipsoid_rays[] = {
	{"north pole", {4.0, -3.0, 14.0}, {0.0, 0.0, -1.0}, 1,
	    {6.0, 18.0}},
	{"periodic seam", {24.0, -3.0, 2.0}, {-1.0, 0.0, 0.0}, 1,
	    {10.0, 30.0}}
    };
    failures += check_shared_crofton_fixture("oblate-ellipsoid",
	&ellipsoid_intern, tol, ellipsoid_min, ellipsoid_max,
	ellipsoid_area, ellipsoid_volume, 8000, ellipsoid_rays,
	sizeof(ellipsoid_rays) / sizeof(ellipsoid_rays[0]));

    struct rt_arb_internal arb = {};
    arb.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb.pt[0], -11.0, -7.0, -5.0);
    VSET(arb.pt[1], 9.0, -7.0, -5.0);
    VSET(arb.pt[2], 9.0, 5.0, -5.0);
    VSET(arb.pt[3], -11.0, 5.0, -5.0);
    VSET(arb.pt[4], -11.0, -7.0, 3.0);
    VSET(arb.pt[5], 9.0, -7.0, 3.0);
    VSET(arb.pt[6], 9.0, 5.0, 3.0);
    VSET(arb.pt[7], -11.0, 5.0, 3.0);
    struct rt_db_internal arb_intern;
    RT_DB_INTERNAL_INIT(&arb_intern);
    arb_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    arb_intern.idb_type = ID_ARB8;
    arb_intern.idb_meth = &OBJ[ID_ARB8];
    arb_intern.idb_ptr = &arb;
    point_t arb_min = {-11.0, -7.0, -5.0};
    point_t arb_max = {9.0, 5.0, 3.0};
    const double arb_diagonal = sqrt(20.0 * 20.0 + 12.0 * 12.0 +
	8.0 * 8.0);
    const directed_partition_ray arb_rays[] = {
	{"face forward", {-31.0, -1.0, -1.0}, {1.0, 0.0, 0.0}, 1,
	    {20.0, 40.0}},
	{"face reverse", {29.0, -1.0, -1.0}, {-1.0, 0.0, 0.0}, 1,
	    {20.0, 40.0}},
	{"opposite vertices", {-31.0, -19.0, -13.0},
	    {20.0, 12.0, 8.0}, 1, {arb_diagonal, 2.0 * arb_diagonal}}
    };
    failures += check_shared_crofton_fixture("arb8-box", &arb_intern, tol,
	arb_min, arb_max, 992.0, 1920.0, 8000, arb_rays,
	sizeof(arb_rays) / sizeof(arb_rays[0]));

    struct rt_tgc_internal cylinder = {};
    cylinder.magic = RT_TGC_INTERNAL_MAGIC;
    VSET(cylinder.v, 3.0, -4.0, -7.0);
    VSET(cylinder.h, 0.0, 0.0, 14.0);
    VSET(cylinder.a, 6.0, 0.0, 0.0);
    VSET(cylinder.b, 0.0, 6.0, 0.0);
    VMOVE(cylinder.c, cylinder.a);
    VMOVE(cylinder.d, cylinder.b);
    struct rt_db_internal cylinder_intern;
    RT_DB_INTERNAL_INIT(&cylinder_intern);
    cylinder_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    cylinder_intern.idb_type = ID_TGC;
    cylinder_intern.idb_meth = &OBJ[ID_TGC];
    cylinder_intern.idb_ptr = &cylinder;
    point_t cylinder_min = {-3.0, -10.0, -7.0};
    point_t cylinder_max = {9.0, 2.0, 7.0};
    const directed_partition_ray cylinder_rays[] = {
	{"axis", {3.0, -4.0, -14.0}, {0.0, 0.0, 1.0}, 1,
	    {7.0, 21.0}},
	{"periodic seam", {15.0, -4.0, 0.0}, {-1.0, 0.0, 0.0}, 1,
	    {6.0, 18.0}},
	{"periodic seam reverse", {-9.0, -4.0, 0.0},
	    {1.0, 0.0, 0.0}, 1, {6.0, 18.0}}
    };
    failures += check_shared_crofton_fixture("rcc", &cylinder_intern, tol,
	cylinder_min, cylinder_max, 240.0 * M_PI, 504.0 * M_PI, 8000,
	cylinder_rays, sizeof(cylinder_rays) / sizeof(cylinder_rays[0]));

    struct rt_tgc_internal cone = cylinder;
    VSET(cone.c, 3.0, 0.0, 0.0);
    VSET(cone.d, 0.0, 3.0, 0.0);
    struct rt_db_internal cone_intern;
    RT_DB_INTERNAL_INIT(&cone_intern);
    cone_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    cone_intern.idb_type = ID_TGC;
    cone_intern.idb_meth = &OBJ[ID_TGC];
    cone_intern.idb_ptr = &cone;
    const double cone_slant = sqrt(14.0 * 14.0 + 3.0 * 3.0);
    const double cone_area = M_PI * (6.0 + 3.0) * cone_slant +
	M_PI * (6.0 * 6.0 + 3.0 * 3.0);
    const directed_partition_ray cone_rays[] = {
	{"axis", {3.0, -4.0, -14.0}, {0.0, 0.0, 1.0}, 1,
	    {7.0, 21.0}},
	{"periodic seam", {15.0, -4.0, 0.0}, {-1.0, 0.0, 0.0}, 1,
	    {7.5, 16.5}}
    };
    failures += check_shared_crofton_fixture("truncated-cone", &cone_intern,
	tol, cylinder_min, cylinder_max, cone_area, 294.0 * M_PI, 8000,
	cone_rays, sizeof(cone_rays) / sizeof(cone_rays[0]));

    struct rt_tor_internal torus = {};
    torus.magic = RT_TOR_INTERNAL_MAGIC;
    VSET(torus.v, 0.0, 0.0, 0.0);
    VSET(torus.h, 0.0, 0.0, 1.0);
    torus.r_a = 12.0;
    torus.r_h = 3.0;
    struct rt_db_internal torus_intern;
    RT_DB_INTERNAL_INIT(&torus_intern);
    torus_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    torus_intern.idb_type = ID_TOR;
    torus_intern.idb_meth = &OBJ[ID_TOR];
    torus_intern.idb_ptr = &torus;
    point_t torus_min = {-15.0, -15.0, -3.0};
    point_t torus_max = {15.0, 15.0, 3.0};
    const directed_partition_ray torus_rays[] = {
	{"two intervals", {-20.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 2,
	    {5.0, 11.0, 29.0, 35.0}},
	{"two intervals reverse", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 2,
	    {5.0, 11.0, 29.0, 35.0}},
	{"central hole", {0.0, 0.0, -10.0}, {0.0, 0.0, 1.0}, 0,
	    {0.0, 0.0}},
	{"tube vertical", {12.0, 0.0, -10.0}, {0.0, 0.0, 1.0}, 1,
	    {7.0, 13.0}}
    };
    failures += check_shared_crofton_fixture("torus", &torus_intern, tol,
	torus_min, torus_max, 144.0 * M_PI * M_PI,
	216.0 * M_PI * M_PI, 8000, torus_rays,
	sizeof(torus_rays) / sizeof(torus_rays[0]));

    return failures;
}


static void
init_sphere_internal(struct rt_ell_internal &sphere,
    struct rt_db_internal &intern, const point_t center, double radius)
{
    sphere = {};
    sphere.magic = RT_ELL_INTERNAL_MAGIC;
    VMOVE(sphere.v, center);
    VSET(sphere.a, radius, 0.0, 0.0);
    VSET(sphere.b, 0.0, radius, 0.0);
    VSET(sphere.c, 0.0, 0.0, radius);
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_ELL;
    intern.idb_meth = &OBJ[ID_ELL];
    intern.idb_ptr = &sphere;
}


static int
check_brep_leaf_csg_corpus(const struct bn_tol *tol)
{
    int failures = 0;
    struct rt_ell_internal left_sphere;
    struct rt_ell_internal right_sphere;
    struct rt_db_internal left_intern;
    struct rt_db_internal right_intern;

    point_t disjoint_left_center = {-6.0, 0.0, 0.0};
    point_t disjoint_right_center = {6.0, 0.0, 0.0};
    init_sphere_internal(left_sphere, left_intern, disjoint_left_center, 4.0);
    init_sphere_internal(right_sphere, right_intern, disjoint_right_center,
	4.0);
    point_t disjoint_min = {-10.0, -4.0, -4.0};
    point_t disjoint_max = {10.0, 4.0, 4.0};
    const directed_partition_ray disjoint_rays[] = {
	{"two components", {-20.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 2,
	    {10.0, 18.0, 22.0, 30.0}},
	{"two components reverse", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 2,
	    {10.0, 18.0, 22.0, 30.0}}
    };
    failures += check_brep_leaf_csg_fixture("disjoint-sphere-union",
	&left_intern, &right_intern, WMOP_UNION, tol,
	disjoint_min, disjoint_max, 128.0 * M_PI,
	(512.0 / 3.0) * M_PI, disjoint_rays,
	sizeof(disjoint_rays) / sizeof(disjoint_rays[0]));

    point_t overlap_left_center = {-3.0, 0.0, 0.0};
    point_t overlap_right_center = {3.0, 0.0, 0.0};
    init_sphere_internal(left_sphere, left_intern, overlap_left_center, 5.0);
    init_sphere_internal(right_sphere, right_intern, overlap_right_center,
	5.0);
    point_t overlap_union_min = {-8.0, -5.0, -5.0};
    point_t overlap_union_max = {8.0, 5.0, 5.0};
    const directed_partition_ray overlap_union_rays[] = {
	{"merged interval", {-20.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1,
	    {12.0, 28.0}},
	{"merged interval reverse", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 1,
	    {12.0, 28.0}}
    };
    failures += check_brep_leaf_csg_fixture("overlapping-sphere-union",
	&left_intern, &right_intern, WMOP_UNION, tol,
	overlap_union_min, overlap_union_max, 160.0 * M_PI,
	(896.0 / 3.0) * M_PI, overlap_union_rays,
	sizeof(overlap_union_rays) / sizeof(overlap_union_rays[0]));

    point_t overlap_intersection_min = {-2.0, -4.0, -4.0};
    point_t overlap_intersection_max = {2.0, 4.0, 4.0};
    const directed_partition_ray overlap_intersection_rays[] = {
	{"lens", {-20.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1,
	    {18.0, 22.0}},
	{"lens reverse", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 1,
	    {18.0, 22.0}}
    };
    failures += check_brep_leaf_csg_fixture("sphere-intersection-lens",
	&left_intern, &right_intern, WMOP_INTERSECT, tol,
	overlap_intersection_min, overlap_intersection_max, 40.0 * M_PI,
	(104.0 / 3.0) * M_PI, overlap_intersection_rays,
	sizeof(overlap_intersection_rays) /
	    sizeof(overlap_intersection_rays[0]));

    point_t concentric_center = VINIT_ZERO;
    init_sphere_internal(left_sphere, left_intern, concentric_center, 8.0);
    init_sphere_internal(right_sphere, right_intern, concentric_center, 3.0);
    point_t shell_min = {-8.0, -8.0, -8.0};
    point_t shell_max = {8.0, 8.0, 8.0};
    const directed_partition_ray shell_rays[] = {
	{"cavity", {-12.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 2,
	    {4.0, 9.0, 15.0, 20.0}},
	{"cavity reverse", {12.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 2,
	    {4.0, 9.0, 15.0, 20.0}}
    };
    failures += check_brep_leaf_csg_fixture("concentric-sphere-cavity",
	&left_intern, &right_intern, WMOP_SUBTRACT, tol,
	shell_min, shell_max, 292.0 * M_PI, (1940.0 / 3.0) * M_PI,
	shell_rays, sizeof(shell_rays) / sizeof(shell_rays[0]));

    struct rt_arb_internal box = {};
    box.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(box.pt[0], -8.0, -8.0, -5.0);
    VSET(box.pt[1], 8.0, -8.0, -5.0);
    VSET(box.pt[2], 8.0, 8.0, -5.0);
    VSET(box.pt[3], -8.0, 8.0, -5.0);
    VSET(box.pt[4], -8.0, -8.0, 5.0);
    VSET(box.pt[5], 8.0, -8.0, 5.0);
    VSET(box.pt[6], 8.0, 8.0, 5.0);
    VSET(box.pt[7], -8.0, 8.0, 5.0);
    struct rt_db_internal box_intern;
    RT_DB_INTERNAL_INIT(&box_intern);
    box_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    box_intern.idb_type = ID_ARB8;
    box_intern.idb_meth = &OBJ[ID_ARB8];
    box_intern.idb_ptr = &box;

    struct rt_tgc_internal cutter = {};
    cutter.magic = RT_TGC_INTERNAL_MAGIC;
    VSET(cutter.v, 0.0, 0.0, -6.0);
    VSET(cutter.h, 0.0, 0.0, 12.0);
    VSET(cutter.a, 3.0, 0.0, 0.0);
    VSET(cutter.b, 0.0, 3.0, 0.0);
    VMOVE(cutter.c, cutter.a);
    VMOVE(cutter.d, cutter.b);
    struct rt_db_internal cutter_intern;
    RT_DB_INTERNAL_INIT(&cutter_intern);
    cutter_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    cutter_intern.idb_type = ID_TGC;
    cutter_intern.idb_meth = &OBJ[ID_TGC];
    cutter_intern.idb_ptr = &cutter;
    point_t drilled_box_min = {-8.0, -8.0, -5.0};
    point_t drilled_box_max = {8.0, 8.0, 5.0};
    const directed_partition_ray drilled_box_rays[] = {
	{"cross hole", {-12.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 2,
	    {4.0, 9.0, 15.0, 20.0}},
	{"through hole", {0.0, 0.0, -10.0}, {0.0, 0.0, 1.0}, 0,
	    {0.0, 0.0}},
	{"through material", {5.0, 0.0, -10.0}, {0.0, 0.0, 1.0}, 1,
	    {5.0, 15.0}}
    };
    failures += check_brep_leaf_csg_fixture("box-minus-cylinder", &box_intern,
	&cutter_intern, WMOP_SUBTRACT, tol, drilled_box_min,
	drilled_box_max, 1152.0 + 42.0 * M_PI, 2560.0 - 90.0 * M_PI,
	drilled_box_rays,
	sizeof(drilled_box_rays) / sizeof(drilled_box_rays[0]));

    return failures;
}


static int
check_crofton_sphere(struct rt_db_internal *ell_intern,
    const struct bn_tol *tol, double radius)
{
    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_ELL].ft_brep(&brep, ell_intern, tol);
    if (!brep) {
	std::printf("FAIL: Crofton sphere-to-BREP conversion\n");
	return 1;
    }

    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal brep_intern;
    RT_DB_INTERNAL_INIT(&brep_intern);
    brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_intern.idb_type = ID_BREP;
    brep_intern.idb_meth = &OBJ[ID_BREP];
    brep_intern.idb_ptr = &brep_internal;

    struct rt_crofton_params params = {20000, 0.0, 0.0};
    fastf_t implicit_area = 0.0;
    fastf_t implicit_volume = 0.0;
    fastf_t brep_area = 0.0;
    fastf_t brep_volume = 0.0;
    rt_crofton_sample(&implicit_area, &implicit_volume, ell_intern, &params);
    rt_crofton_sample(&brep_area, &brep_volume, &brep_intern, &params);
    delete brep_internal.brep;

    const double analytic_area = 4.0 * M_PI * radius * radius;
    const double analytic_volume = (4.0 / 3.0) * M_PI * radius * radius *
	radius;
    const double aggregate_tolerance = 0.03;
    const double paired_tolerance = 0.01;
    if (relative_error(implicit_area, analytic_area) > aggregate_tolerance ||
	    relative_error(implicit_volume, analytic_volume) >
	    aggregate_tolerance ||
	    relative_error(brep_area, analytic_area) > aggregate_tolerance ||
	    relative_error(brep_volume, analytic_volume) > aggregate_tolerance ||
	    relative_error(brep_area, implicit_area) > paired_tolerance ||
	    relative_error(brep_volume, implicit_volume) > paired_tolerance) {
	std::printf("FAIL: Crofton sphere analytic=[%.17g %.17g] "
	    "implicit=[%.17g %.17g] BREP=[%.17g %.17g]\n", analytic_area,
	    analytic_volume, implicit_area, implicit_volume, brep_area,
	    brep_volume);
	return 1;
    }
    return 0;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    const bool report_grazing = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--grazing-report");
    if (argc != 1 && !report_grazing)
	bu_exit(1, "Usage: %s [--grazing-report]\n", argv[0]);

    const double radius = 10.0;
    struct rt_ell_internal ell = {};
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell.v, 0.0, 0.0, 0.0);
    VSET(ell.a, radius, 0.0, 0.0);
    VSET(ell.b, 0.0, radius, 0.0);
    VSET(ell.c, 0.0, 0.0, radius);

    struct rt_db_internal ell_intern;
    RT_DB_INTERNAL_INIT(&ell_intern);
    ell_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ell_intern.idb_type = ID_ELL;
    ell_intern.idb_meth = &OBJ[ID_ELL];
    ell_intern.idb_ptr = &ell;

    struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    if (!rtip)
	bu_exit(1, "rt_dirbuild_inmem() failed\n");
    rtip->rti_tol.magic = BN_TOL_MAGIC;
    rtip->rti_tol.dist = 0.0005;
    rtip->rti_tol.dist_sq = rtip->rti_tol.dist * rtip->rti_tol.dist;
    rtip->rti_tol.perp = 1.0e-6;
    rtip->rti_tol.para = 1.0 - rtip->rti_tol.perp;

    struct resource resp = {};
    rt_init_resource(&resp, 0, rtip);

    struct soltab *implicit_stp = prep_solid(rtip, &ell_intern, ID_ELL);
    if (!implicit_stp)
	bu_exit(1, "implicit sphere prep failed\n");

    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_ELL].ft_brep(&brep, &ell_intern, &rtip->rti_tol);
    if (!brep)
	bu_exit(1, "sphere-to-BREP conversion failed\n");
    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal brep_intern;
    RT_DB_INTERNAL_INIT(&brep_intern);
    brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_intern.idb_type = ID_BREP;
    brep_intern.idb_meth = &OBJ[ID_BREP];
    brep_intern.idb_ptr = &brep_internal;

    struct soltab *brep_stp = prep_solid(rtip, &brep_intern, ID_BREP);
    if (!brep_stp) {
	delete brep_internal.brep;
	free_solid(implicit_stp);
	bu_exit(1, "converted BREP sphere prep failed\n");
    }

    struct directed_ray {
	const char *label;
	point_t origin;
	vect_t direction;
	double expected_in;
	double expected_out;
    } rays[] = {
	{"north-pole down", {0.0, 0.0, 20.0}, {0.0, 0.0, -1.0},
	    radius, 3.0 * radius},
	{"south-pole up", {0.0, 0.0, -20.0}, {0.0, 0.0, 1.0},
	    radius, 3.0 * radius},
	{"positive-x seam", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0},
	    radius, 3.0 * radius},
	{"negative-y", {0.0, -20.0, 0.0}, {0.0, 1.0, 0.0},
	    radius, 3.0 * radius},
	{"inside toward pole", {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0},
	    -radius, radius},
	{"seam start inward", {radius, 0.0, 0.0}, {-1.0, 0.0, 0.0},
	    0.0, 2.0 * radius}
    };

    int failures = 0;
    for (size_t repeat = 0; repeat < 16; ++repeat) {
	for (size_t i = 0; i < sizeof(rays) / sizeof(rays[0]); ++i)
	    failures += check_ray(rays[i].label, implicit_stp, brep_stp,
		rtip, &resp, rays[i].origin, rays[i].direction,
		rays[i].expected_in, rays[i].expected_out);
    }

    failures += check_grazing_ratchet(implicit_stp, brep_stp, rtip, &resp,
	radius);

    point_t small_center = {1.25, -2.5, 5.0};
    failures += check_transformed_sphere(rtip, &resp, "small-translated",
	small_center, 0.01);
    point_t large_center = {1.0e6, -2.0e6, 3.0e6};
    failures += check_transformed_sphere(rtip, &resp, "large-translated",
	large_center, 1.0e4);

    if (report_grazing)
	grazing_report(implicit_stp, brep_stp, rtip, &resp, radius);

    free_solid(brep_stp);
    free_solid(implicit_stp);

    point_t sphere_min = {-radius, -radius, -radius};
    point_t sphere_max = {radius, radius, radius};
    failures += check_shared_crofton_fixture("sphere", &ell_intern,
	&rtip->rti_tol, sphere_min, sphere_max,
	4.0 * M_PI * radius * radius,
	(4.0 / 3.0) * M_PI * radius * radius * radius, 32000);
    failures += check_shared_primitive_corpus(&rtip->rti_tol);
    failures += check_brep_leaf_csg_corpus(&rtip->rti_tol);
    failures += check_crofton_sphere(&ell_intern, &rtip->rti_tol, radius);

    rt_clean_resource_basic(rtip, &resp);
    BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
    rt_i_destroy(rtip);

    if (failures)
	std::printf("BREP ray correctness corpus: %d failure(s)\n", failures);
    else
	std::printf("BREP ray correctness corpus: PASS\n");
    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
