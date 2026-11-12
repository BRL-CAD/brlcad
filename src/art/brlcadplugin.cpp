/*               B R L C A D P L U G I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/*********************************************************************
 *
 * This software is released under the MIT license.
 *
 * Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
 * Copyright (c) 2014-2018 Francois Beaune, The appleseedhq Organization
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */


#include "common.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wall"
#  pragma GCC diagnostic ignored "-Wextra"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wdocumentation"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wpedantic"
#  pragma clang diagnostic ignored "-Wignored-qualifiers"
#endif

#include "renderer/api/log.h"
#include "renderer/kernel/shading/shadingray.h"
#include "foundation/math/ray.h"
#include "foundation/math/vector.h"
#include "foundation/utility/api/specializedapiarrays.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


/* standard library */
#include <cmath>
#include <mutex>
#include <memory>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

/* brlcad headers */
#include "vmath.h"
#include "raytrace.h"
#include "brlcadplugin.h"

namespace asf = foundation;
namespace asr = renderer;

namespace {
const char *const Model = "brlcad geometry";

struct RayHit {
    double distance;
    asf::Vector3d geometric_normal;
    asf::Vector3d shading_normal;
    asf::Vector2f uv = asf::Vector2f(0.0f);
    asr::ProceduralObject::SurfaceDerivatives derivatives;
    bool compute_shading = false;
    const char *error = nullptr;
};

struct DestroyResource {
    void operator()(struct resource *resource) const {
	// rt_i destruction reinitializes resource tables for potential reuse.
	// Release that final allocation after the geometry no longer uses it.
	rt_clean_resource_basic(nullptr, resource);
	delete resource;
    }
};

struct DestroyRaytrace {
    void operator()(struct rt_i *rtip) const { if (rtip) rt_i_destroy(rtip); }
};

int
brlcad_hit(struct application *ap, struct partition *head, struct seg *UNUSED(segs))
{
    auto &result = *static_cast<RayHit *>(ap->a_uptr);
    for (struct partition *part = head->pt_forw; part != head; part = part->pt_forw) {
	struct hit *hit = nullptr;
	struct soltab *solid = nullptr;
	int flip = 0;
	// Rays starting inside a region encounter its exit boundary first.
	if (part->pt_inhit && part->pt_inhit->hit_dist >= 0.0) {
	    hit = part->pt_inhit;
	    solid = part->pt_inseg->seg_stp;
	    flip = part->pt_inflip;
	} else if (part->pt_outhit && part->pt_outhit->hit_dist >= 0.0) {
	    hit = part->pt_outhit;
	    solid = part->pt_outseg->seg_stp;
	    flip = part->pt_outflip;
	}
	if (!hit || !solid) continue;
	result.distance = hit->hit_dist;
	if (!result.compute_shading)
	    return 1;
	vect_t geometric, shading;
	struct xray *ray = hit->hit_rayp ? hit->hit_rayp : &ap->a_ray;
	if (rt_obj_normals(hit, solid, ray, geometric, shading) != 0) {
	    result.error = "Primitive normal query failed";
	    return 1;
	}
	// Legacy UV callbacks consume a computed point and primitive shading normal.
	struct hit shading_hit = *hit;
	VJOIN1(shading_hit.hit_point, ray->r_pt, hit->hit_dist, ray->r_dir);
	VMOVE(shading_hit.hit_normal, shading);
	if (flip) {
	    VREVERSE(geometric, geometric);
	    VREVERSE(shading, shading);
	}
	result.geometric_normal = asf::Vector3d(V3ARGS(geometric));
	result.shading_normal = asf::Vector3d(V3ARGS(shading));
	if (solid->st_meth->ft_uv) {
	    struct uvcoord uv = {0, 0, 0, 0};
	    if (rt_obj_uv(ap, solid, &shading_hit, &uv) != 0) {
		result.error = "Primitive UV query failed";
		return 1;
	    }
	    result.uv = asf::Vector2f(static_cast<float>(uv.uv_u), static_cast<float>(uv.uv_v));
	    if (!std::isfinite(result.uv[0]) || !std::isfinite(result.uv[1]))
		result.error = "Primitive produced nonfinite UV coordinates";
	}
	struct rt_surface_derivatives derivatives = RT_SURFACE_DERIVATIVES_INIT_ZERO;
	const int status = rt_obj_surface_derivatives(ap, solid, hit, &derivatives);
	if (status == RT_DERIVATIVES_ERROR) {
	    result.error = "Primitive surface derivative query failed";
	    return 1;
	}
	if (status == RT_DERIVATIVES_OK) {
	    result.derivatives.m_valid = true;
	    result.derivatives.m_dpdu = asf::Vector3d(V3ARGS(derivatives.dpdu));
	    result.derivatives.m_dpdv = asf::Vector3d(V3ARGS(derivatives.dpdv));
	    // Boolean cuts reverse the normal field, preserving its UV chart.
	    const double normal_sign = flip ? -1.0 : 1.0;
	    result.derivatives.m_dndu = normal_sign * asf::Vector3d(V3ARGS(derivatives.dndu));
	    result.derivatives.m_dndv = normal_sign * asf::Vector3d(V3ARGS(derivatives.dndv));
	}
	return 1;
    }
    return 0;
}

int
brlcad_miss(struct application *UNUSED(ap))
{
    return 0;
}
} // namespace

class BrlcadObject::RaytraceState
{
public:
    struct ReturnResource {
	RaytraceState *state;
	void operator()(struct resource *resource) const {
	    std::lock_guard<std::mutex> lock(state->mutex);
	    state->available.push_back(resource);
	}
    };
    using ResourceLease = std::unique_ptr<struct resource, ReturnResource>;

    RaytraceState(const asr::ParamArray &params, struct db_i *database)
	: rtip(database ? rt_i_create(database) : rt_dirbuild_ex(
	    params.get_required<std::string>("database_path").c_str(), nullptr, 0, DB_OPEN_COMB_INSTANCES))
    {
	const std::string path = params.get_required<std::string>("object_path");
	if (!rtip || path.empty() || rt_gettree(rtip.get(), path.c_str()) < 0)
	    throw std::runtime_error("Cannot load BRL-CAD object '" + path + "'");
	rt_prep_parallel(rtip.get(), 1);
	if (!rtip->stats.nregions || !rtip->stats.nsolids)
	    throw std::runtime_error("No renderable geometry in BRL-CAD object '" + path + "'");
    }

    ResourceLease acquire()
    {
	std::lock_guard<std::mutex> lock(mutex);
	if (!available.empty()) {
	    auto *resource = available.back();
	    available.pop_back();
	    return ResourceLease(resource, ReturnResource{this});
	}
	if (resources.size() >= MAX_PSW)
	    throw std::runtime_error("Too many simultaneous BRL-CAD ray queries");
	std::unique_ptr<struct resource, DestroyResource> resource(new struct resource{});
	// Reserve before registering: leases must return without allocation, and
	// no allocation failure may leave a dangling pointer in rti_resources.
	resources.reserve(resources.size() + 1);
	available.reserve(resources.size() + 1);
	rt_init_resource(resource.get(), static_cast<int>(resources.size()), rtip.get());
	auto *result = resource.get();
	resources.push_back(std::move(resource));
	return ResourceLease(result, ReturnResource{this});
    }

private:
    std::mutex mutex;
    std::vector<struct resource *> available;
    std::vector<std::unique_ptr<struct resource, DestroyResource>> resources;
public:
    // rt_i destruction cleans registered resources; their storage must survive it.
    std::unique_ptr<struct rt_i, DestroyRaytrace> rtip;
};

BrlcadObject::BrlcadObject(const char *name, const asr::ParamArray &params)
    : BrlcadObject(name, params, nullptr)
{
}

BrlcadObject::BrlcadObject(const char *name, const asr::ParamArray &params, struct db_i *database)
    : asr::ProceduralObject(name, params), m_trace(new RaytraceState(params, database))
{
}

BrlcadObject::~BrlcadObject() = default;

void
BrlcadObject::release()
{
    delete this;
}


/* Identify model */
const char*
BrlcadObject::get_model() const
{
    return Model;
}


/* Called before rendering each frame */
bool
BrlcadObject::on_frame_begin(
    const asr::Project& project,
    const asr::BaseGroup* parent,
    asr::OnFrameBeginRecorder& recorder,
    asf::IAbortSwitch* abort_switch)
{
    if (!asr::ProceduralObject::on_frame_begin(project, parent, recorder, abort_switch))
	return false;

    return true;
}


/* Compute local space bounding box of object */
asr::GAABB3
BrlcadObject::compute_local_bbox() const
{
    return asr::GAABB3(asr::GVector3(V3ARGS(m_trace->rtip->mdl_min)),
	asr::GVector3(V3ARGS(m_trace->rtip->mdl_max)));
}


/* Access material slots */
size_t
BrlcadObject::get_material_slot_count() const
{
    return 1;
}


const char*
BrlcadObject::get_material_slot(const size_t UNUSED(index)) const
{
    return "default";
}


bool
BrlcadObject::trace(const asr::ShadingRay &ray, IntersectionResult *result) const
{
    if (result) result->m_hit = false;
    const double length = asf::norm(ray.m_dir);
    if (!(length > 0.0) || !std::isfinite(length) || !(ray.m_tmin < ray.m_tmax))
	return false;
    auto resource = m_trace->acquire();
    struct application app;
    RT_APPLICATION_INIT(&app);
    app.a_rt_i = m_trace->rtip.get();
    app.a_resource = resource.get();
    app.a_onehit = 1;
    app.a_no_hit_snap = 1;
    app.a_hit = brlcad_hit;
    app.a_miss = brlcad_miss;
    RayHit hit;
    hit.compute_shading = result != nullptr;
    app.a_uptr = &hit;

    // librt requires unit directions and reports distance from the shifted
    // origin.  Appleseed uses the original ray's parameter interval.
    const auto direction = ray.m_dir / length;
    const auto origin = ray.m_org + ray.m_tmin * ray.m_dir;
    VMOVE(app.a_ray.r_dir, direction);
    VMOVE(app.a_ray.r_pt, origin);
    if (!rt_shootray(&app))
	return false;
    const double distance = ray.m_tmin + hit.distance / length;
    if (!(distance < ray.m_tmax))
	return false;
    if (result) {
	if (hit.error)
	    throw std::runtime_error(std::string(hit.error) + " for '" + get_name() + "'");
	result->m_hit = true;
	result->m_distance = distance;
	result->m_geometric_normal = hit.geometric_normal;
	result->m_shading_normal = hit.shading_normal;
	result->m_uv = hit.uv;
	result->m_derivatives = hit.derivatives;
	result->m_material_slot = 0;
    }
    return true;
}

void
BrlcadObject::intersect(const asr::ShadingRay &ray, IntersectionResult &result) const
{
    trace(ray, &result);
}

bool
BrlcadObject::intersect(const asr::ShadingRay &ray) const
{
    return trace(ray, nullptr);
}


void
BrlcadObject::refine_and_offset(
    const asf::Ray3d &ray,
    const asf::Vector3d &geometric_normal,
    asf::Vector3d &front,
    asf::Vector3d &back,
    asf::Vector3d &offset_normal) const
{
    offset_normal = asf::normalize(asf::faceforward(geometric_normal, ray.m_dir));
    const auto &point = ray.m_org;
    const auto *rtip = m_trace->rtip.get();
    const double position_scale = std::max({std::abs(point[0]), std::abs(point[1]), std::abs(point[2])});
    const double extent = std::max({rtip->mdl_max[0] - rtip->mdl_min[0],
	rtip->mdl_max[1] - rtip->mdl_min[1], rtip->mdl_max[2] - rtip->mdl_min[2]});
    // Hit reconstruction can cancel a distant ray origin against its travel.
    // Include that rounding scale even when the resulting hit is near zero.
    // Object extent supplies the scale for rays originating on the surface.
    const double ray_scale = ray.is_finite() ? std::abs(ray.m_tmax) *
	std::max({std::abs(ray.m_dir[0]), std::abs(ray.m_dir[1]), std::abs(ray.m_dir[2])}) : 0.0;
    constexpr double OFFSET_ROUNDING_STEPS = 8;
    const double distance = OFFSET_ROUNDING_STEPS * std::numeric_limits<double>::epsilon() *
	(position_scale + extent + ray_scale);
    const auto displacement = distance * offset_normal;
    front = point + displacement;
    back = point - displacement;
    // Round away from the surface so placement cannot cancel a small offset.
    for (size_t axis = 0; axis < 3; ++axis) {
	if (offset_normal[axis] > 0.0) {
	    front[axis] = std::nextafter(front[axis], INFINITY);
	    back[axis] = std::nextafter(back[axis], -INFINITY);
	} else if (offset_normal[axis] < 0.0) {
	    front[axis] = std::nextafter(front[axis], -INFINITY);
	    back[axis] = std::nextafter(back[axis], INFINITY);
	}
    }
}


/* Delete instance */
void
BrlcadObjectFactory::release()
{
    delete this;
}


/* Identify model */
const char*
BrlcadObjectFactory::get_model() const
{
    return Model;
}


/* Return metadata for object */
asf::Dictionary
BrlcadObjectFactory::get_model_metadata() const
{
    return
	asf::Dictionary()
	.insert("name", Model)
	.insert("label", "BRLCAD Object");
}


/* Return metadata for the object inputs */
asf::DictionaryArray
BrlcadObjectFactory::get_input_metadata() const
{
    asf::DictionaryArray metadata;

    for (const char *name : {"database_path", "object_path"}) {
	metadata.push_back(asf::Dictionary()
	    .insert("name", name)
	    .insert("label", name)
	    .insert("type", "text")
	    .insert("use", "required"));
    }

    return metadata;
}


/* Create a new empty object */
asf::auto_release_ptr<asr::Object>
BrlcadObjectFactory::create(
    const char* name,
    const asr::ParamArray& params) const
{
    try {
	return asf::auto_release_ptr<asr::Object>(new BrlcadObject(name, params));
    } catch (const std::exception &error) {
	RENDERER_LOG_ERROR("Cannot create BRL-CAD object '%s': %s", name, error.what());
	return asf::auto_release_ptr<asr::Object>();
    }
}


/* Create objects */
bool
BrlcadObjectFactory::create(
    const char* name,
    const asr::ParamArray& params,
    const asf::SearchPaths& UNUSED(search_paths),
    const bool UNUSED(omit_loading_assets),
    asr::ObjectArray& objects) const
{
    auto object = create(name, params);
    if (!object.get()) return false;
    objects.push_back(object.release());
    return true;
}


/* Plugin entry point */
extern "C"
{
    APPLESEED_DLL_EXPORT asr::IObjectFactory* appleseed_create_object_factory()
    {
	return new BrlcadObjectFactory();
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
