/*                       T I R E _ C O N I C . C P P
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
/** @file libged/tire_conic.cpp
 *
 * Conic equation construction, solving, and conversion support for tire
 * surfaces.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <Eigen/Dense>
#include <Eigen/LU>

#include "bn.h"

#include "tire_private.h"

namespace tire_private {

using ConicMatrix = Eigen::Matrix<fastf_t, 5, 5>;
using ConicRhs = Eigen::Matrix<fastf_t, 5, 1>;

struct ConicSystem {
    ConicMatrix matrix = ConicMatrix::Zero();
    ConicRhs rhs = ConicRhs::Zero();
};

static void
append_conic_row(ConicMatrix &mat, ConicRhs &rhs, int row, fastf_t y, fastf_t z, fastf_t value)
{
    mat(row, 0) = y * y;
    mat(row, 1) = y * z;
    mat(row, 2) = z * z;
    mat(row, 3) = y;
    mat(row, 4) = z;
    rhs(row) = value;
}

static void
append_partial_row(ConicMatrix &mat, ConicRhs &rhs, int row, fastf_t y, fastf_t z, fastf_t partial)
{
    mat(row, 0) = 2 * y;
    mat(row, 1) = z + y * partial;
    mat(row, 2) = 2 * z * partial;
    mat(row, 3) = 1;
    mat(row, 4) = partial;
    rhs(row) = 0;
}

fastf_t
conic_partial_at(const ConicEquation &conic, fastf_t x, fastf_t y)
{
    return -(conic.d + y * conic.b + 2 * x * conic.a) /
	(conic.e + 2 * y * conic.c + x * conic.b);
}

std::optional<fastf_t>
conic_z_at_y(const ConicEquation &conic, fastf_t y)
{
    const fastf_t discrim = -4 * conic.a - 4 * y * conic.a * conic.e +
	conic.d * conic.d + 2 * y * conic.b * conic.d -
	4 * y * y * conic.a * conic.c + y * y * conic.b * conic.b;

    if (!std::isfinite(discrim) || discrim < 0.0 || NEAR_ZERO(conic.a, SMALL_FASTF))
	return std::nullopt;

    fastf_t z = (sqrt(discrim) - conic.d - y * conic.b) / (2 * conic.a);
    if (!std::isfinite(z))
	return std::nullopt;

    return z;
}

static ConicSystem
create_tread_conic_system(fastf_t dytred, fastf_t dztred, fastf_t d1, fastf_t ztire)
{
    ConicSystem sys;

    append_conic_row(sys.matrix, sys.rhs, 0, dytred / 2, ztire - dztred, -1);
    append_conic_row(sys.matrix, sys.rhs, 1, dytred / 2, ztire - (dztred + 2 * (d1 - dztred)), -1);
    append_conic_row(sys.matrix, sys.rhs, 2, 0.0, ztire, -1);
    append_conic_row(sys.matrix, sys.rhs, 3, 0.0, ztire - 2 * d1, -1);
    append_conic_row(sys.matrix, sys.rhs, 4, -dytred / 2, ztire - dztred, -1);

    return sys;
}

static ConicSystem
create_upper_side_conic_system(fastf_t dytred, fastf_t dztred,
		fastf_t dyside1, fastf_t zside1, fastf_t ztire,
		fastf_t UNUSED(dyhub), fastf_t UNUSED(zhub), fastf_t ell1partial)
{
    ConicSystem sys;
    const fastf_t tread_y = dytred / 2;
    const fastf_t tread_z = ztire - dztred;
    const fastf_t mirror_z = 2 * zside1 - ztire + dztred;

    append_conic_row(sys.matrix, sys.rhs, 0, dyside1 / 2, zside1, -1);
    append_conic_row(sys.matrix, sys.rhs, 1, tread_y, tread_z, -1);
    append_conic_row(sys.matrix, sys.rhs, 2, tread_y, mirror_z, -1);
    append_partial_row(sys.matrix, sys.rhs, 3, tread_y, tread_z, ell1partial);
    append_partial_row(sys.matrix, sys.rhs, 4, tread_y, mirror_z, -ell1partial);

    return sys;
}

static ConicSystem
create_lower_side_conic_system(fastf_t UNUSED(dytred), fastf_t UNUSED(dztred),
		fastf_t dyside1, fastf_t zside1, fastf_t UNUSED(ztire),
		fastf_t dyhub, fastf_t zhub, fastf_t UNUSED(ell1partial))
{
    ConicSystem sys;

    append_conic_row(sys.matrix, sys.rhs, 0, dyside1 / 2, zside1, -1);
    append_conic_row(sys.matrix, sys.rhs, 1, dyhub / 2, zhub, -1);
    append_conic_row(sys.matrix, sys.rhs, 2, dyhub / 2, 2 * zside1 - zhub, -1);
    append_conic_row(sys.matrix, sys.rhs, 3, -dyside1 / 2, zside1, -1);
    append_conic_row(sys.matrix, sys.rhs, 4, -dyhub / 2, zhub, -1);

    return sys;
}

static std::optional<ConicEquation>
solve_conic(const ConicSystem &sys)
{
    Eigen::FullPivLU<ConicMatrix> lu(sys.matrix);
    if (!lu.isInvertible() || lu.rank() != 5)
	return std::nullopt;

    ConicRhs solution = lu.solve(sys.rhs);
    if (!solution.allFinite())
	return std::nullopt;

    ConicRhs residual = sys.matrix * solution - sys.rhs;
    if (!residual.allFinite() || residual.norm() > 1.0e-6 * std::max<fastf_t>(1.0, sys.rhs.norm()))
	return std::nullopt;

    return ConicEquation{solution(0), solution(1), solution(2), solution(3), solution(4)};
}

std::optional<ConicEquation>
solve_tread_conic(fastf_t dytred, fastf_t dztred, fastf_t d1, fastf_t ztire)
{
    return solve_conic(create_tread_conic_system(dytred, dztred, d1, ztire));
}

std::optional<ConicEquation>
solve_upper_side_conic(fastf_t dytred, fastf_t dztred,
		       fastf_t dyside1, fastf_t zside1, fastf_t ztire,
		       fastf_t dyhub, fastf_t zhub, fastf_t ell1partial)
{
    return solve_conic(create_upper_side_conic_system(
	dytred, dztred, dyside1, zside1, ztire, dyhub, zhub, ell1partial));
}

std::optional<EtoParams>
conic_to_eto(const ConicEquation &conic)
{
    const fastf_t A = conic.a;
    const fastf_t B = conic.b;
    const fastf_t C = conic.c;
    const fastf_t D = conic.d;
    const fastf_t E = conic.e;
    const fastf_t denom = 4 * A * C - B * B;

    if (!std::isfinite(denom) || NEAR_ZERO(denom, SMALL_FASTF) || denom <= 0.0)
	return std::nullopt;

    fastf_t x_0 = -(B * E - 2 * C * D) / denom;
    fastf_t y_0 = -(B * D - 2 * A * E) / denom;
    fastf_t Fp = 1 - y_0 * E - x_0 * D + y_0 * y_0 * C + x_0 * y_0 * B + x_0 * x_0 * A;
    fastf_t theta = .5 * atan(B / (A - C));
    fastf_t App = A * cos(theta) * cos(theta) + B * cos(theta) * sin(theta) + C * sin(theta) * sin(theta);
    fastf_t Cpp = A * sin(theta) * sin(theta) - B * sin(theta) * cos(theta) + C * cos(theta) * cos(theta);

    fastf_t length1_expr = -Fp / App;
    fastf_t length2_expr = -Fp / Cpp;
    if (!std::isfinite(length1_expr) || !std::isfinite(length2_expr) ||
	length1_expr <= 0.0 || length2_expr <= 0.0)
	return std::nullopt;

    fastf_t length1 = sqrt(length1_expr);
    fastf_t length2 = sqrt(length2_expr);

    fastf_t semimajorx = 0.0;
    fastf_t semimajory = 0.0;
    fastf_t semiminor = 0.0;
    if (length1 > length2) {
	semimajorx = length1 * cos(-theta);
	semimajory = length1 * sin(-theta);
	semiminor = length2;
    } else {
	semimajorx = length2 * sin(-theta);
	semimajory = length2 * cos(-theta);
	semiminor = length1;
    }

    if (!std::isfinite(x_0) || !std::isfinite(y_0) ||
	!std::isfinite(semimajorx) || !std::isfinite(semimajory) ||
	!std::isfinite(semiminor) || semiminor <= 0.0)
	return std::nullopt;

    return EtoParams{-x_0, -y_0, semimajorx, semimajory, semiminor};
}

std::optional<SurfaceConics>
solve_surface_conics(fastf_t dytred, fastf_t dztred, fastf_t d1,
		   fastf_t dyside1, fastf_t zside1, fastf_t ztire,
		   fastf_t dyhub, fastf_t zhub)
{
    std::optional<ConicEquation> ell1 = solve_tread_conic(dytred, dztred, d1, ztire);
    if (!ell1)
	return std::nullopt;

    fastf_t ell1partial = conic_partial_at(*ell1, dytred / 2, ztire - dztred);
    if (!std::isfinite(ell1partial))
	return std::nullopt;

    std::optional<ConicEquation> ell2 = solve_upper_side_conic(
	dytred, dztred, dyside1, zside1, ztire, dyhub, zhub, ell1partial);
    std::optional<ConicEquation> ell3 = solve_conic(
	create_lower_side_conic_system(dytred, dztred, dyside1, zside1, ztire, dyhub, zhub, ell1partial));

    if (!ell2 || !ell3)
	return std::nullopt;

    return SurfaceConics{*ell1, *ell2, *ell3};
}

std::optional<SurfaceEtos>
surface_conics_to_etos(const SurfaceConics &conics)
{
    std::optional<EtoParams> ell1 = conic_to_eto(conics.tread);
    std::optional<EtoParams> ell2 = conic_to_eto(conics.upper_side);
    std::optional<EtoParams> ell3 = conic_to_eto(conics.lower_side);

    if (!ell1 || !ell2 || !ell3)
	return std::nullopt;

    return SurfaceEtos{*ell1, *ell2, *ell3};
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
