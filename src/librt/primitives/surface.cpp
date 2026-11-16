/*                        S U R F A C E . C P P
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

#include <algorithm>
#include <cmath>
#include <limits>

#include "raytrace.h"
#include "rt/surface.h"
#include "rt/primitives/bot.h"

int
rt_obj_surface_attributes(struct rt_surface_attributes *out,
    struct application *ap, struct soltab *solid, const struct hit *hit, int flip)
{
    if (!out || !ap || !solid || !hit)
	return -1;
    RT_CK_APPLICATION(ap);
    RT_CK_SOLTAB(solid);
    RT_CK_HIT(hit);
    struct rt_surface_attributes result = {};
    struct xray *ray = hit->hit_rayp ? hit->hit_rayp : &ap->a_ray;
    if (rt_obj_normals(hit, solid, ray, result.geometric_normal, result.shading_normal) != 0)
	return -1;
    VJOIN1(result.point, ray->r_pt, hit->hit_dist, ray->r_dir);
    if (VINVALID(result.point))
	return -1;
    if (solid->st_meth->ft_uv) {
	struct hit local = *hit;
	VMOVE(local.hit_point, result.point);
	VMOVE(local.hit_normal, result.shading_normal);
	struct uvcoord uv = {};
	if (rt_obj_uv(ap, solid, &local, &uv) != 0 ||
	    !std::isfinite(uv.uv_u) || !std::isfinite(uv.uv_v))
	    return -1;
	result.uv[0] = uv.uv_u;
	result.uv[1] = uv.uv_v;
	result.uv_available = 1;
    }
    const int status = rt_obj_surface_derivatives(ap, solid, hit, &result.derivatives);
    if (status == RT_DERIVATIVES_ERROR)
	return -1;
    result.derivatives_available = status == RT_DERIVATIVES_OK;
    if (flip) {
	VREVERSE(result.geometric_normal, result.geometric_normal);
	VREVERSE(result.shading_normal, result.shading_normal);
	VREVERSE(result.derivatives.dndu, result.derivatives.dndu);
	VREVERSE(result.derivatives.dndv, result.derivatives.dndv);
	VREVERSE(result.derivatives.dngdu, result.derivatives.dngdu);
	VREVERSE(result.derivatives.dngdv, result.derivatives.dngdv);
    }
    *out = result;
    return 0;
}

int
rt_surface_sampler_init(struct rt_surface_sampler *out, struct rt_i *rtip)
{
    if (!out || !rtip)
	return -1;
    RT_CK_RTI(rtip);
    if (rtip->needprep || rtip->stats.nregions != 1 || BU_LIST_IS_EMPTY(&rtip->HeadRegion))
	return -1;
    const struct region *region = BU_LIST_FIRST(region, &rtip->HeadRegion);
    point_t minimum, maximum;
    // Prep gives all-halfspace models an artificial finite display box.
    // Bound the Boolean region itself before using a sphere as a proposal.
    if (!region->reg_treetop || rt_bound_tree(region->reg_treetop, minimum, maximum) < 0 ||
	VINVALID(minimum) || VINVALID(maximum))
	return -1;
    struct soltab *solid;
    RT_VISIT_ALL_SOLTABS_START(solid, rtip) {
	if (solid->st_id == ID_BOT) {
	    const auto *bot = static_cast<const struct bot_specific *>(solid->st_specific);
	    // Plate thickness is ray-dependent; it is not a fixed boundary surface.
	    if (!bot || bot->bot_mode == RT_BOT_PLATE || bot->bot_mode == RT_BOT_PLATE_NOCOS)
		return -1;
	}
    } RT_VISIT_ALL_SOLTABS_END;
    struct rt_surface_sampler result = {};
    result.rtip = rtip;
    vect_t diagonal;
    for (size_t axis = 0; axis < ELEMENTS_PER_VECT; ++axis) {
	if (maximum[axis] < minimum[axis])
	    return -1;
	result.center[axis] = 0.5 * minimum[axis] + 0.5 * maximum[axis];
	diagonal[axis] = maximum[axis] - minimum[axis];
    }
    // Round outward so the reference sphere encloses bounding-box corners.
    result.radius = std::nextafter(0.5 * std::hypot(diagonal[0], diagonal[1], diagonal[2]), INFINITY);
    result.area_scale = M_2PI * result.radius * result.radius;
    if (!(result.radius > 0.0) || !std::isfinite(result.radius) ||
	!(result.area_scale > 0.0) || !std::isfinite(result.area_scale))
	return -1;
    *out = result;
    return 0;
}

namespace {
struct Boundary {
    const struct partition *partition;
    const struct hit *hit;
    struct soltab *solid;
    int flip;
};

template <typename Function>
bool
visit_boundaries(struct partition *head, const Function &visit)
{
    for (const struct partition *part = head->pt_forw; part != head; part = part->pt_forw) {
	if (!part->pt_inhit || !part->pt_outhit || !part->pt_inseg || !part->pt_outseg ||
	    !std::isfinite(part->pt_inhit->hit_dist) || !std::isfinite(part->pt_outhit->hit_dist) ||
	    part->pt_outhit->hit_dist < part->pt_inhit->hit_dist)
	    return false;
	if (part->pt_inhit->hit_dist >= 0.0 &&
	    !visit(Boundary{part, part->pt_inhit, part->pt_inseg->seg_stp, part->pt_inflip}))
	    return false;
	// SURFACE BOTs report a coincident entry/exit for one geometric sheet.
	if (part->pt_outhit->hit_dist > part->pt_inhit->hit_dist &&
	    part->pt_outhit->hit_dist >= 0.0 &&
	    !visit(Boundary{part, part->pt_outhit, part->pt_outseg->seg_stp, part->pt_outflip}))
	    return false;
    }
    return true;
}

struct SampleQuery {
    double selection;
    const struct rt_surface_sampler *sampler;
    struct rt_surface_sample result = {};
    int status = RT_SURFACE_SAMPLE_MISS;
};

int
sample_hit(struct application *ap, struct partition *head, struct seg *UNUSED(segments))
{
    auto &query = *static_cast<SampleQuery *>(ap->a_uptr);
    size_t crossings = 0;
    if (!visit_boundaries(head, [&](const Boundary &) {
	if (crossings == std::numeric_limits<size_t>::max())
	    return false;
	++crossings;
	return true;
    }))
	return query.status = RT_SURFACE_SAMPLE_ERROR;
    if (!crossings)
	return RT_SURFACE_SAMPLE_MISS;
    const size_t selected = std::min(crossings - 1,
	static_cast<size_t>(query.selection * static_cast<double>(crossings)));
    size_t index = 0;
    query.status = RT_SURFACE_SAMPLE_ERROR;
    visit_boundaries(head, [&](const Boundary &boundary) {
	if (index++ != selected)
	    return true;
	if (rt_obj_surface_attributes(&query.result.surface, ap, boundary.solid,
		boundary.hit, boundary.flip) != 0)
	    return false;
	query.result.solid = boundary.solid;
	query.result.region = boundary.partition->pt_regionp;
	query.result.surface_number = boundary.hit->hit_surfno;
	query.result.flipped = boundary.flip != 0;
	query.result.crossing_count = crossings;
	query.result.area_weight = query.sampler->area_scale * static_cast<double>(crossings);
	if (std::isfinite(query.result.area_weight) && query.result.area_weight > 0.0)
	    query.status = RT_SURFACE_SAMPLE_HIT;
	return false;
    });
    return query.status;
}

int
sample_miss(struct application *UNUSED(ap))
{
    return RT_SURFACE_SAMPLE_MISS;
}

void
sphere_point(vect_t point, const double u, const double v)
{
    point[2] = 1.0 - 2.0 * u;
    const double radial = std::sqrt(std::max(0.0, 1.0 - point[2] * point[2]));
    const double azimuth = M_2PI * v;
    point[0] = radial * std::cos(azimuth);
    point[1] = radial * std::sin(azimuth);
}
} // namespace

int
rt_surface_sampler_sample(struct rt_surface_sample *out,
    const struct rt_surface_sampler *sampler, struct resource *resource,
    const double variates[RT_SURFACE_SAMPLE_DIMENSIONS])
{
    if (!out || !sampler || !sampler->rtip || !resource || !variates ||
	!(sampler->radius > 0.0) || !std::isfinite(sampler->radius) ||
	!(sampler->area_scale > 0.0) || !std::isfinite(sampler->area_scale))
	return RT_SURFACE_SAMPLE_ERROR;
    RT_CK_RTI(sampler->rtip);
    RT_CK_RESOURCE(resource);
    for (size_t i = 0; i < RT_SURFACE_SAMPLE_DIMENSIONS; ++i) {
	if (!(variates[i] >= 0.0 && variates[i] < 1.0))
	    return RT_SURFACE_SAMPLE_ERROR;
    }
    vect_t first, second, difference;
    sphere_point(first, variates[0], variates[1]);
    sphere_point(second, variates[2], variates[3]);
    VSUB2(difference, second, first);
    const double length = MAGNITUDE(difference);
    if (!(length > 0.0))
	return RT_SURFACE_SAMPLE_MISS;
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = sampler->rtip;
    ap.a_resource = resource;
    ap.a_hit = sample_hit;
    ap.a_miss = sample_miss;
    ap.a_onehit = 0;
    ap.a_no_hit_snap = 1;
    ap.a_ray_length = sampler->radius * length;
    VJOIN1(ap.a_ray.r_pt, sampler->center, sampler->radius, first);
    VSCALE(ap.a_ray.r_dir, difference, 1.0 / length);
    SampleQuery query{variates[4], sampler};
    ap.a_uptr = &query;
    const int status = rt_shootray(&ap);
    if (status != query.status)
	return RT_SURFACE_SAMPLE_ERROR;
    if (status == RT_SURFACE_SAMPLE_HIT)
	*out = query.result;
    return status;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
