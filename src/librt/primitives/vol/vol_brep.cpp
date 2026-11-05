/*                    V O L _ B R E P . C P P
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
/** @file vol_brep.cpp
 *
 * Convert a vol to an nmg, and thus to b-rep form
 *
 */

#include "common.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "raytrace.h"
#include "rt/geom.h"
#include "nmg.h"
#include "brep.h"

#include "../../librt_private.h"

extern "C" {
    int rt_vol_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol);
    void rt_nmg_brep(ON_Brep **bi, const struct rt_db_internal *ip, const struct bn_tol *tol);
}


extern "C" int
_rt_vol_brep_patch_count(const struct rt_vol_internal *vol, size_t limit,
	size_t *patch_count)
{
    if (patch_count == NULL)
	return -1;
    *patch_count = 0;
    if (vol == NULL || vol->magic != RT_VOL_INTERNAL_MAGIC ||
	vol->map == NULL || vol->xdim == 0 || vol->ydim == 0 ||
	vol->zdim == 0 || vol->lo > vol->hi || vol->hi > 255)
    {
	return -1;
    }

    const size_t maximum = std::numeric_limits<size_t>::max();
    if (vol->xdim > maximum - 4 || vol->ydim > maximum - 4 ||
	vol->zdim > maximum - 4)
    {
	return -1;
    }
    const size_t padded_x = static_cast<size_t>(vol->xdim) + 4;
    const size_t padded_y = static_cast<size_t>(vol->ydim) + 4;
    const size_t padded_z = static_cast<size_t>(vol->zdim) + 4;
    if (padded_y > maximum / padded_x ||
	padded_z > maximum / (padded_x * padded_y))
    {
	return -1;
    }

    auto occupied = [vol, padded_x, padded_y](ptrdiff_t x, ptrdiff_t y,
	ptrdiff_t z) {
	if (x < 0 || y < 0 || z < 0 ||
	    x >= static_cast<ptrdiff_t>(vol->xdim) ||
	    y >= static_cast<ptrdiff_t>(vol->ydim) ||
	    z >= static_cast<ptrdiff_t>(vol->zdim))
	{
	    return false;
	}
	const size_t index =
	    ((static_cast<size_t>(z) + 2) * padded_y +
	    (static_cast<size_t>(y) + 2)) * padded_x +
	    static_cast<size_t>(x) + 2;
	const unsigned int value = vol->map[index];
	return value >= vol->lo && value <= vol->hi;
    };

    using Span = std::pair<size_t, size_t>;
    size_t count = 0;
    auto add_patches = [&count, limit, patch_count](size_t amount) {
	if (amount > limit - std::min(count, limit)) {
	    count = limit == std::numeric_limits<size_t>::max() ? limit :
		limit + 1;
	    *patch_count = count;
	    return false;
	}
	count += amount;
	return true;
    };

    /* This mirrors VOL_PATCH_LOOP in rt_vol_tess without constructing any
     * topology.  Equal spans in adjacent rows form one rectangle; every span
     * which does not continue is one initial NMG face. */
    auto count_pass = [&add_patches, &count, patch_count](size_t outer_limit,
	size_t row_limit, size_t span_limit, const auto &exposed) {
	std::vector<Span> active;
	std::vector<Span> current;
	active.reserve(span_limit / 2 + 1);
	current.reserve(span_limit / 2 + 1);
	for (size_t outer = 0; outer < outer_limit; ++outer) {
	    active.clear();
	    for (size_t row = 0; row < row_limit; ++row) {
		current.clear();
		bool in_span = false;
		size_t start = 0;
		for (size_t span = 0; span <= span_limit; ++span) {
		    const bool is_exposed = span < span_limit &&
			exposed(outer, row, span);
		    if (is_exposed && !in_span) {
			start = span;
			in_span = true;
		    } else if (!is_exposed && in_span) {
			current.emplace_back(start, span - 1);
			in_span = false;
		    }
		}

		size_t current_index = 0;
		for (const Span &previous : active) {
		    while (current_index < current.size() &&
			current[current_index] < previous)
		    {
			++current_index;
		    }
		    if (current_index == current.size() ||
			current[current_index] != previous)
		    {
			if (!add_patches(1))
			    return false;
		    }
		}
		active.swap(current);
	    }
	    if (!add_patches(active.size()))
		return false;
	}
	*patch_count = count;
	return true;
    };

    const size_t xdim = vol->xdim;
    const size_t ydim = vol->ydim;
    const size_t zdim = vol->zdim;
    if (!count_pass(zdim, ydim, xdim,
	[&occupied](size_t z, size_t y, size_t x) {
	    return occupied(x, y, z) && occupied(x, y,
		static_cast<ptrdiff_t>(z) + 1) == false;
	}) ||
	!count_pass(zdim, ydim, xdim,
	[&occupied](size_t z, size_t y, size_t x) {
	    return occupied(x, y, z) && occupied(x, y,
		static_cast<ptrdiff_t>(z) - 1) == false;
	}) ||
	!count_pass(ydim, zdim, xdim,
	[&occupied](size_t y, size_t z, size_t x) {
	    return occupied(x, y, z) && occupied(x,
		static_cast<ptrdiff_t>(y) + 1, z) == false;
	}) ||
	!count_pass(ydim, zdim, xdim,
	[&occupied](size_t y, size_t z, size_t x) {
	    return occupied(x, y, z) && occupied(x,
		static_cast<ptrdiff_t>(y) - 1, z) == false;
	}) ||
	!count_pass(xdim, ydim, zdim,
	[&occupied](size_t x, size_t y, size_t z) {
	    return occupied(x, y, z) && occupied(
		static_cast<ptrdiff_t>(x) + 1, y, z) == false;
	}) ||
	!count_pass(xdim, ydim, zdim,
	[&occupied](size_t x, size_t y, size_t z) {
	    return occupied(x, y, z) && occupied(
		static_cast<ptrdiff_t>(x) - 1, y, z) == false;
	}))
    {
	return 1;
    }

    *patch_count = count;
    return 0;
}


extern "C" void
rt_vol_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    if (b == NULL || *b == NULL || ip == NULL || tol == NULL)
	return;

    RT_CK_DB_INTERNAL(ip);
    const struct rt_vol_internal *vol =
	(const struct rt_vol_internal *)ip->idb_ptr;
    size_t patch_count = 0;
    const int preflight = _rt_vol_brep_patch_count(vol,
	RT_VOL_BREP_MAX_INITIAL_PATCHES, &patch_count);
    if (preflight != 0 || patch_count == 0) {
	if (preflight > 0) {
	    bu_log("VOL B-Rep conversion exceeds the %zu initial-patch work "
		"budget.\n",
		static_cast<size_t>(RT_VOL_BREP_MAX_INITIAL_PATCHES));
	}
	(*b)->Destroy();
	return;
    }

    struct rt_db_internal *tmp_internal;
    struct bg_tess_tol ttmptol = BG_TESS_TOL_INIT_TOL;

    BU_ALLOC(tmp_internal, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(tmp_internal);

    const struct bg_tess_tol *ttol = &ttmptol;
    struct model *volm = nmg_mm();
    struct nmgregion *volr = NULL;

    tmp_internal->idb_ptr = (void *)ip->idb_ptr;
    const int tess_result = rt_vol_tess(&volr, volm, tmp_internal, ttol, tol);
    if (tess_result == 0 && volr != NULL) {
	tmp_internal->idb_ptr = (void *)volm;
	rt_nmg_brep(b, tmp_internal, tol);
    } else {
	(*b)->Destroy();
    }

    FREE_MODEL(volm);
    bu_free(tmp_internal, "free temporary rt_db_internal");
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
