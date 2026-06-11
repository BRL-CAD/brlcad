/*                 T I R E _ T R E A D _ P R O F I L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/tire_tread_profile.cpp
 *
 * Tread-profile CSG construction for the tire command.
 */

#include "common.h"

#include <cmath>
#include <optional>
#include <string>

#include "raytrace.h"
#include "vmath.h"
#include "wdb.h"

#include "tire_private.h"

namespace tire_private {

int
make_tread_profile_surface_cut(CsgTransaction &builder, const std::string &suffix,
	       const ConicEquation &ell2coefficients, fastf_t ztire, fastf_t dztred,
	       fastf_t d1, fastf_t dytred, fastf_t dyhub, fastf_t zhub,
	       fastf_t UNUSED(dyside1), const TreadSpec &tread_spec)
{
    std::optional<fastf_t> d1_intercept;
    std::optional<ConicEquation> ell1treadcoefficients;
    std::optional<ConicEquation> ell2treadcoefficients;
    std::optional<EtoParams> ell1treadcadparams;
    std::optional<EtoParams> ell2treadcadparams;
    struct wmember premtreadshape, tiretreadshape;
    vect_t vertex, height;
    point_t origin, normal, C;

    d1_intercept = conic_z_at_y(ell2coefficients, ztire - d1);
    if (!d1_intercept) {
	builder.set_error("failed to locate tread start on sidewall conic");
	return BRLCAD_ERROR;
    }

    ell1treadcoefficients = solve_tread_conic(dytred, dztred, d1, ztire);
    if (!ell1treadcoefficients) {
	builder.set_error("failed to solve curved tread profile conic");
	return BRLCAD_ERROR;
    }

    ell1treadcadparams = conic_to_eto(*ell1treadcoefficients);
    fastf_t elltreadpartial = conic_partial_at(*ell1treadcoefficients, dytred / 2, ztire - dztred);
    if (!ell1treadcadparams || !std::isfinite(elltreadpartial)) {
	builder.set_error("failed to convert curved tread profile conic to ETO parameters");
	return BRLCAD_ERROR;
    }

    ell2treadcoefficients = solve_upper_side_conic(dytred, dztred, *d1_intercept * 2, ztire - d1,
						   ztire, dyhub, zhub, elltreadpartial);
    if (!ell2treadcoefficients) {
	builder.set_error("failed to solve curved tread side conic");
	return BRLCAD_ERROR;
    }
    ell2treadcadparams = conic_to_eto(*ell2treadcoefficients);
    if (!ell2treadcadparams) {
	builder.set_error("failed to convert curved tread side conic to ETO parameters");
	return BRLCAD_ERROR;
    }

    VSET(origin, 0, ell1treadcadparams->center_y, 0);
    VSET(normal, 0, 1, 0);
    VSET(C, 0, ell1treadcadparams->c_y, ell1treadcadparams->c_z);
    if (builder.write_eto(object_name("profile.outer", suffix + ".tread", ".s").c_str(), origin, normal, C, ell1treadcadparams->major_radius, ell1treadcadparams->minor_radius) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(origin, 0, ell2treadcadparams->center_y, 0);
    VSET(normal, 0, 1, 0);
    VSET(C, 0, ell2treadcadparams->c_y, ell2treadcadparams->c_z);
    if (builder.write_eto(object_name("profile.side-left", suffix + ".tread", ".s").c_str(), origin, normal, C, ell2treadcadparams->major_radius, ell2treadcadparams->minor_radius) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(origin, 0, -ell2treadcadparams->center_y, 0);
    VSET(normal, 0, -1, 0);
    VSET(C, 0, -ell2treadcadparams->c_y, -ell2treadcadparams->c_z);
    if (builder.write_eto(object_name("profile.side-right", suffix + ".tread", ".s").c_str(), origin, normal, C, ell2treadcadparams->major_radius, ell2treadcadparams->minor_radius) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, -ell1treadcadparams->c_y - ell1treadcadparams->c_y * .01, 0);
    VSET(height, 0, ell1treadcadparams->c_y - dytred / 2 + ell1treadcadparams->c_y * .01 , 0);
    if (builder.write_rcc(object_name("profile.clip-left", suffix + ".tread", ".s").c_str(), vertex, height, ztire - dztred) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, ell1treadcadparams->c_y + ell1treadcadparams->c_y * .01, 0);
    VSET(height, 0, -ell1treadcadparams->c_y + dytred / 2 - ell1treadcadparams->c_y * .01, 0);
    if (builder.write_rcc(object_name("profile.clip-right", suffix + ".tread", ".s").c_str(), vertex, height, ztire - dztred) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, -*d1_intercept, 0);
    VSET(height, 0, *d1_intercept * 2, 0);
    if (builder.write_rcc(object_name("profile.inner-cut", suffix + ".tread", ".s").c_str(), vertex, height, ztire - d1) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&premtreadshape.l);
    add_member(object_name("profile.outer", suffix + ".tread", ".s"), &premtreadshape.l, WMOP_UNION);
    add_member(object_name("profile.clip-left", suffix + ".tread", ".s"), &premtreadshape.l, WMOP_SUBTRACT);
    add_member(object_name("profile.clip-right", suffix + ".tread", ".s"), &premtreadshape.l, WMOP_SUBTRACT);
    add_member(object_name("profile.side-left", suffix + ".tread", ".s"), &premtreadshape.l, WMOP_UNION);
    add_member(object_name("profile.side-right", suffix + ".tread", ".s"), &premtreadshape.l, WMOP_UNION);
    add_member(object_name("profile.inner-cut", suffix + ".tread", ".s"), &premtreadshape.l, WMOP_SUBTRACT);
    if (builder.write_lcomb(object_name("profile.prem", suffix + ".tread", ".c").c_str(), &premtreadshape, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&tiretreadshape.l);
    add_member(object_name("profile.prem", suffix + ".tread", ".c"), &tiretreadshape.l, WMOP_UNION);
    add_member(object_name("", suffix + ".tire.outer", ".c"), &tiretreadshape.l, WMOP_SUBTRACT);
    if (builder.write_lcomb(object_name("shape", suffix + ".tread", ".c").c_str(), &tiretreadshape, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    return make_tread_pattern(builder, suffix, *d1_intercept * 2, ztire - d1, ztire,
		       tread_spec.count, tread_spec);
}

int
make_tread_profile_outer_ellipse(CsgTransaction &builder, const std::string &suffix,
		const ConicEquation &ell2coefficients, fastf_t ztire, fastf_t dztred,
		fastf_t d1, fastf_t dytred, fastf_t UNUSED(dyhub), fastf_t UNUSED(zhub),
		fastf_t UNUSED(dyside1), const TreadSpec &tread_spec)
{
    std::optional<fastf_t> d1_intercept;
    std::optional<ConicEquation> ell1tredcoefficients;
    std::optional<EtoParams> ell1tredcadparams;
    struct wmember tiretreadintercept, tiretreadsolid, tiretreadshape;
    vect_t vertex, height;
    point_t origin, normal, C;

    d1_intercept = conic_z_at_y(ell2coefficients, ztire - d1);
    if (!d1_intercept) {
	builder.set_error("failed to locate squared tread start on sidewall conic");
	return BRLCAD_ERROR;
    }

    ell1tredcoefficients = solve_tread_conic(*d1_intercept * 2, dztred, d1, ztire);
    if (!ell1tredcoefficients) {
	builder.set_error("failed to solve squared tread profile conic");
	return BRLCAD_ERROR;
    }
    ell1tredcadparams = conic_to_eto(*ell1tredcoefficients);
    if (!ell1tredcadparams) {
	builder.set_error("failed to convert squared tread profile conic to ETO parameters");
	return BRLCAD_ERROR;
    }

    VSET(origin, 0, ell1tredcadparams->center_y, 0);
    VSET(normal, 0, 1, 0);
    VSET(C, 0, ell1tredcadparams->c_y, ell1tredcadparams->c_z);
    if (builder.write_eto(object_name("profile.outer", suffix + ".tread", ".s").c_str(), origin, normal, C, ell1tredcadparams->major_radius, ell1tredcadparams->minor_radius) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, -dytred / 2, 0);
    VSET(height, 0, dytred, 0);
    if (builder.write_rcc(object_name("profile.constrain", suffix + ".tread", ".s").c_str(), vertex, height, ztire) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, -dytred / 2, 0);
    VSET(normal, 0, -1, 0);
    if (builder.write_cone(object_name("profile.trim-left", suffix + ".tread", ".s").c_str(), vertex, normal, *d1_intercept - dytred / 2, ztire, ztire - d1) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, dytred / 2, 0);
    VSET(normal, 0, 1, 0);
    if (builder.write_cone(object_name("profile.trim-right", suffix + ".tread", ".s").c_str(), vertex, normal, *d1_intercept - dytred / 2, ztire, ztire - d1) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, -*d1_intercept, 0);
    VSET(height, 0, *d1_intercept * 2, 0);
    if (builder.write_rcc(object_name("profile.inner-cut", suffix + ".tread", ".s").c_str(), vertex, height, d1) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&tiretreadintercept.l);
    add_member(object_name("profile.constrain", suffix + ".tread", ".s"), &tiretreadintercept.l, WMOP_UNION);
    add_member(object_name("profile.trim-left", suffix + ".tread", ".s"), &tiretreadintercept.l, WMOP_UNION);
    add_member(object_name("profile.trim-right", suffix + ".tread", ".s"), &tiretreadintercept.l, WMOP_UNION);
    if (builder.write_lcomb(object_name("profile.constrain", suffix + ".tread", ".c").c_str(), &tiretreadintercept, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&tiretreadsolid.l);
    add_member(object_name("profile.outer", suffix + ".tread", ".s"), &tiretreadsolid.l, WMOP_UNION);
    add_member(object_name("profile.constrain", suffix + ".tread", ".c"), &tiretreadsolid.l, WMOP_INTERSECT);
    add_member(object_name("profile.inner-cut", suffix + ".tread", ".s"), &tiretreadsolid.l, WMOP_SUBTRACT);
    if (builder.write_lcomb(object_name("profile.outer", suffix + ".tread", ".c").c_str(), &tiretreadsolid, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&tiretreadshape.l);
    add_member(object_name("profile.outer", suffix + ".tread", ".c"), &tiretreadshape.l, WMOP_UNION);
    add_member(object_name("", suffix + ".tire.outer", ".c"), &tiretreadshape.l, WMOP_SUBTRACT);
    if (builder.write_lcomb(object_name("shape", suffix + ".tread", ".c").c_str(), &tiretreadshape, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    return make_tread_pattern(builder, suffix, *d1_intercept * 2, ztire - d1, ztire,
		       tread_spec.count, tread_spec);
}

} /* namespace tire_private */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
