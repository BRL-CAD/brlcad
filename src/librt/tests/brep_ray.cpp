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
#include <cstdio>
#include <cstring>
#include <vector>

#include "bu/app.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"


struct ray_result {
    int shot_hits = 0;
    int segments = 0;
    double in_dist = 0.0;
    double out_dist = 0.0;
    vect_t in_normal = VINIT_ZERO;
    vect_t out_normal = VINIT_ZERO;
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

    failures += check_crofton_sphere(&ell_intern, &rtip->rti_tol, radius);

    rt_clean_resource_basic(rtip, &resp);
    BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
    rt_i_destroy(rtip);

    if (failures)
	std::printf("BREP directed sphere rays: %d failure(s)\n", failures);
    else
	std::printf("BREP directed sphere rays: PASS\n");
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
