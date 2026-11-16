/*              S U R F A C E _ D E R I V A T I V E S . C
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
#include "common.h"

#include <math.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"
#include "wdb.h"

static const point_t Center = {17, -23, 31};
static const double DifferenceStep = 1.0e-6;
static const double RelativeTolerance = 1.0e-8;
static const double RayStartOffset = 1.0;
static const double PreparationTolerance = 0.001;

static int
same_vector(const vect_t actual, const vect_t expected, double tolerance)
{
    if (VINVALID(actual) || VINVALID(expected))
	return 0;
    return VNEAR_EQUAL(actual, expected, tolerance * fmax(1.0, MAGNITUDE(expected)));
}

static void
chart_geometry(const fastf_t axes[3][3], int primitive, double u, double v, point_t point, vect_t normal)
{
    vect_t unit;
    if (ZERO(v)) {
	VSET(unit, 0, 0, -1);
    } else if (EQUAL(v, 1.0)) {
	VSET(unit, 0, 0, 1);
    } else {
	const double longitude = M_2PI * u;
	const double latitude = M_PI * (v - 0.5);
	VSET(unit, cos(latitude) * cos(longitude), cos(latitude) * sin(longitude), sin(latitude));
    }
    // Invert the prepared UV frame independently of the callback's matrix inverse.
    vect_t rows[3], columns[3], offset;
    for (size_t i = 0; i < 3; ++i)
	VSCALE(rows[i], axes[i], 1.0 / MAGSQ(axes[i]));
    VCROSS(columns[0], rows[1], rows[2]);
    VCROSS(columns[1], rows[2], rows[0]);
    VCROSS(columns[2], rows[0], rows[1]);
    const fastf_t determinant = VDOT(rows[0], columns[0]);
    VCOMB3(offset, unit[X] / determinant, columns[0],
	unit[Y] / determinant, columns[1], unit[Z] / determinant, columns[2]);
    if (primitive == ID_SPH) {
	// The optimized geometry is spherical even when its UV frame is not.
	VMOVE(normal, offset);
	VUNITIZE(normal);
	VJOIN1(point, Center, MAGNITUDE(axes[0]), normal);
    } else {
	VADD2(point, Center, offset);
	VCOMB3(normal, unit[X], rows[0], unit[Y], rows[1], unit[Z], rows[2]);
	VUNITIZE(normal);
    }
}

static void
chart_differences(const fastf_t axes[3][3], int primitive, double u, double v, struct rt_surface_derivatives *expected)
{
    point_t left, right, down, up;
    vect_t nleft, nright, ndown, nup;
    chart_geometry(axes, primitive, u - DifferenceStep, v, left, nleft);
    chart_geometry(axes, primitive, u + DifferenceStep, v, right, nright);
    chart_geometry(axes, primitive, u, v - DifferenceStep, down, ndown);
    chart_geometry(axes, primitive, u, v + DifferenceStep, up, nup);
    VSUB2SCALE(expected->dpdu, right, left, 0.5 / DifferenceStep);
    VSUB2SCALE(expected->dpdv, up, down, 0.5 / DifferenceStep);
    VSUB2SCALE(expected->dndu, nright, nleft, 0.5 / DifferenceStep);
    VSUB2SCALE(expected->dndv, nup, ndown, 0.5 / DifferenceStep);
}

static int
check_chart(const char *label, const fastf_t axes[3][3], struct rt_i *rtip,
    struct resource *resource, struct soltab *solid, double u, double v,
    int inside, int expected_status)
{
    struct application application;
    RT_APPLICATION_INIT(&application);
    application.a_rt_i = rtip;
    application.a_resource = resource;
    application.a_ray.magic = RT_RAY_MAGIC;
    point_t point;
    vect_t normal;
    chart_geometry(axes, solid->st_id, u, v, point, normal);
    if (inside) {
	VMOVE(application.a_ray.r_pt, Center);
	VSUB2(application.a_ray.r_dir, point, Center);
	VUNITIZE(application.a_ray.r_dir);
    } else {
	VJOIN1(application.a_ray.r_pt, point, RayStartOffset, normal);
	VREVERSE(application.a_ray.r_dir, normal);
    }
    struct seg head;
    BU_LIST_INIT(&head.l);
    int passed = rt_obj_shot(solid, &application.a_ray, &application, &head) > 0 && !BU_LIST_IS_EMPTY(&head.l);
    int status = RT_DERIVATIVES_ERROR;
    if (passed) {
	const struct seg *segment = BU_LIST_FIRST(seg, &head.l);
	struct hit hit = inside ? segment->seg_out : segment->seg_in;
	// Full ray tracing sets this pointer after the primitive shot callback.
	// Exercise both an explicit defining ray and the application fallback.
	hit.hit_rayp = inside ? &application.a_ray : NULL;
	const struct rt_surface_derivatives marker = {{1,0,0}, {0,1,0}, {0,0,1}, {1,1,1}};
	struct rt_surface_derivatives actual = marker;
	struct rt_surface_derivatives expected = marker;
	status = rt_obj_surface_derivatives(&application, solid, &hit, &actual);
	if (expected_status == RT_DERIVATIVES_OK)
	    chart_differences(axes, solid->st_id, u, v, &expected);
	const double tolerance = expected_status == RT_DERIVATIVES_OK ? RelativeTolerance : SMALL_FASTF;
	passed = status == expected_status &&
	    same_vector(actual.dpdu, expected.dpdu, tolerance) &&
	    same_vector(actual.dpdv, expected.dpdv, tolerance) &&
	    same_vector(actual.dndu, expected.dndu, tolerance) &&
	    same_vector(actual.dndv, expected.dndv, tolerance);
	if (!passed && status == RT_DERIVATIVES_OK) {
	    bu_log("%s dpdu=(%g %g %g), expected=(%g %g %g)\n", label, V3ARGS(actual.dpdu), V3ARGS(expected.dpdu));
	    bu_log("%s dpdv=(%g %g %g), expected=(%g %g %g)\n", label, V3ARGS(actual.dpdv), V3ARGS(expected.dpdv));
	    bu_log("%s dndu=(%g %g %g), expected=(%g %g %g)\n", label, V3ARGS(actual.dndu), V3ARGS(expected.dndu));
	    bu_log("%s dndv=(%g %g %g), expected=(%g %g %g)\n", label, V3ARGS(actual.dndv), V3ARGS(expected.dndv));
	}
    }
    RT_FREE_SEG_LIST(&head, resource);
    if (!passed)
	bu_log("%s surface derivative check failed at (%g,%g), inside=%d, status=%d expected=%d\n",
	    label, u, v, inside, status, expected_status);
    return passed;
}

static int
check_shape(const char *label, const fastf_t axes[3][3], int expected_type)
{
    struct db_i *db = db_open_inmem();
    if (!db) return 0;
    struct rt_wdb *writer = wdb_dbopen(db, RT_WDB_TYPE_DB_INMEM);
    if (!writer) {
	db_close(db);
	return 0;
    }
    if (mk_ell(writer, "surface.s", Center, axes[0], axes[1], axes[2]) < 0) {
	wdb_close(writer);
	return 0;
    }
    struct rt_i *rtip = rt_i_create(db);
    if (!rtip) {
	wdb_close(writer);
	return 0;
    }
    // Fix the tolerance so the nearly spherical optimization is reproducible.
    rtip->rti_tol.dist = PreparationTolerance;
    rtip->rti_tol.dist_sq = PreparationTolerance * PreparationTolerance;
    struct resource resource = RT_RESOURCE_INIT_ZERO;
    rt_init_resource(&resource, 0, rtip);
    int passed = rt_gettree(rtip, "surface.s") == 0;
    if (passed) {
	rt_prep_parallel(rtip, 1);
	struct soltab *solid = rt_find_solid(rtip, "surface.s");
	passed = solid && solid->st_id == expected_type;
	if (passed) {
	    const struct { double u, v; int status; } coordinates[] = {
		{0.125, 0.35, RT_DERIVATIVES_OK}, {0.42, 0.7, RT_DERIVATIVES_OK},
		{0.875, 0.5, RT_DERIVATIVES_OK}, {0, 0.4, RT_DERIVATIVES_OK},
		{0, 0, RT_DERIVATIVES_UNAVAILABLE}, {0, 1, RT_DERIVATIVES_UNAVAILABLE}
	    };
	    for (size_t i = 0; i < sizeof(coordinates) / sizeof(coordinates[0]); ++i) {
		for (int inside = 0; inside <= 1; ++inside)
		    passed = check_chart(label, axes, rtip, &resource, solid,
			coordinates[i].u, coordinates[i].v, inside, coordinates[i].status) && passed;
	    }
	} else {
	    bu_log("%s did not prepare the expected primitive type %d\n", label, expected_type);
	}
    }
    rt_i_destroy(rtip);
    rt_clean_resource_basic(NULL, &resource);
    wdb_close(writer);
    return passed;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1) return 1;
    const struct {
	const char *label;
	fastf_t axes[3][3];
	int primitive;
    } cases[] = {
	{"sphere", {{10,0,0}, {0,10,0}, {0,0,10}}, ID_SPH},
	{"rotated sphere", {{0,10,0}, {0,0,10}, {10,0,0}}, ID_SPH},
	{"ellipsoid", {{2,0,0}, {0,3,0}, {0,0,4}}, ID_ELL},
	{"rotated ellipsoid", {{2*M_SQRT1_2,2*M_SQRT1_2,0}, {-3*M_SQRT1_2,3*M_SQRT1_2,0}, {0,0,4}}, ID_ELL},
	{"small sphere", {{0.01,0,0}, {0,0.01,0}, {0,0,0.01}}, ID_SPH},
	{"nearly spherical UV frame", {{0.01,0,0}, {0,0.01001,0}, {0,0,0.01}}, ID_SPH},
	{"nearly orthogonal sphere", {{10,0,0}, {0.0003,10,0}, {0,0,10}}, ID_SPH},
	{"nearly orthogonal ellipsoid", {{2,0,0}, {0.0003,3,0}, {0,0,4}}, ID_ELL},
	{"large sphere", {{1.0e6,0,0}, {0,1.0e6,0}, {0,0,1.0e6}}, ID_SPH}
    };
    int passed = 1;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	passed = check_shape(cases[i].label, cases[i].axes, cases[i].primitive) && passed;
    return passed ? 0 : 1;
}
