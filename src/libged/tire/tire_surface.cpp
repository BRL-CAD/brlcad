/*                    T I R E _ S U R F A C E . C P P
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
/** @file libged/tire_surface.cpp
 *
 * Tire surface generation flow.
 */

#include "common.h"

#include <optional>
#include <string>

#include "tire_private.h"

namespace tire_private {

int
make_tire(CsgTransaction &builder, const std::string &suffix, const TireSpec &spec, const TreadSpec &tread_spec)
{
    fastf_t ztire_with_offset = spec.ztire;
    if (tread_spec.shape != 0)
	ztire_with_offset -= tread_spec.depth_mm;

    std::optional<SurfaceConics> outer_conics = solve_surface_conics(
	spec.dytred, spec.dztred, spec.d1, spec.dyside1, spec.zside1,
	ztire_with_offset, spec.dyhub, spec.zhub);
    if (!outer_conics) {
	builder.set_error("failed to solve outer tire surface conics");
	return BRLCAD_ERROR;
    }

    std::optional<SurfaceEtos> outer_etos = surface_conics_to_etos(*outer_conics);
    if (!outer_etos) {
	builder.set_error("failed to convert outer tire conics to ETO parameters");
	return BRLCAD_ERROR;
    }

    /* Insert outer tire volume */
    if (make_tire_surface(builder, suffix + ".tire.outer", *outer_etos,
			ztire_with_offset, spec.dztred, spec.dytred, spec.dyhub,
			spec.zhub, spec.dyside1, spec.zside1) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    /* Make Tread solid and call routine to create tread */
    int ret = BRLCAD_OK;
    switch (tread_spec.shape) {
	case 1:
	    ret = make_tread_profile_surface_cut(builder, suffix, outer_conics->upper_side, spec.ztire, spec.dztred,
						 spec.d1, spec.dytred, spec.dyhub, spec.zhub, spec.dyside1,
						 tread_spec);
	    break;
	case 2:
	    ret = make_tread_profile_outer_ellipse(builder, suffix, outer_conics->upper_side, spec.ztire, spec.dztred,
						   spec.d1, spec.dytred, spec.dyhub, spec.zhub, spec.dyside1,
						   tread_spec);
	    break;
	default:
	    break;
    }
    if (ret != BRLCAD_OK) {
	return ret;
    }


    /* Calculate input parameters for inner cut*/
    const fastf_t cut_ztire = ztire_with_offset - spec.thickness;
    const fastf_t cut_dyside1 = spec.dyside1 - spec.thickness * 2;
    const fastf_t cut_zside1 = spec.zside1;
    const fastf_t cut_d1 = spec.d1;
    const fastf_t cut_dytred = spec.dytred * cut_dyside1 / spec.dyside1;
    const fastf_t cut_dztred = spec.dztred * cut_ztire / ztire_with_offset;
    const fastf_t cut_dyhub = spec.dyhub - spec.thickness * 2;

    std::optional<SurfaceConics> cut_conics = solve_surface_conics(
	cut_dytred, cut_dztred, cut_d1, cut_dyside1, cut_zside1,
	cut_ztire, cut_dyhub, spec.zhub);
    if (!cut_conics) {
	builder.set_error("failed to solve inner tire cut surface conics");
	return BRLCAD_ERROR;
    }

    std::optional<SurfaceEtos> cut_etos = surface_conics_to_etos(*cut_conics);
    if (!cut_etos) {
	builder.set_error("failed to convert inner tire cut conics to ETO parameters");
	return BRLCAD_ERROR;
    }


    /* Insert inner tire cut volume */
    if (make_tire_surface(builder, suffix + ".tire.inner-cut", *cut_etos,
			cut_ztire, cut_dztred, cut_dytred, cut_dyhub,
			spec.zhub, cut_dyside1, cut_zside1) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    return make_tire_region(builder, suffix, tread_spec.shape != 0);
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
