/*                     A R T P L U G I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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

/* appleseed.renderer headers */
#include "renderer/api/object.h"
#include "renderer/api/project.h"
#include "renderer/api/rendering.h"
#include "renderer/api/scene.h"
#include "renderer/api/types.h"
#include "renderer/kernel/shading/shadingray.h"

/* appleseed.foundation headers */
#include "foundation/math/ray.h"
#include "foundation/math/scalar.h"
#include "foundation/math/vector.h"
#include "foundation/utility/api/specializedapiarrays.h"
#include "foundation/containers/dictionary.h"
#include "foundation/utility/job/iabortswitch.h"
#include "foundation/log/consolelogtarget.h"
#include "foundation/utility/searchpaths.h"
#include "foundation/string/string.h"


/* appleseed.main headers */
#include "main/dllvisibility.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


/* standard library */
#include <algorithm>
#include <cmath>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unordered_map>
#include <mutex>
#include <thread>

/* brlcad headers */
#include "vmath.h"
#include "raytrace.h"
#include "brlcadplugin.h"
#include "bu/str.h"

namespace asf = foundation;
namespace asr = renderer;

thread_local struct BRLCAD_to_ASR brlcad_ray_info;


/* brlcad raytrace hit callback */
int
brlcad_hit(struct application* UNUSED(ap), struct partition* PartHeadp, struct seg* UNUSED(segs))
{
    struct partition* pp;
    struct hit* hitp;
    struct soltab* stp;

    // struct curvature cur = RT_CURVATURE_INIT_ZERO;

    //point_t pt;
    vect_t inormal;

    // vect_t onormal;

    pp = PartHeadp->pt_forw;

    /* entry hit point, so we type less */
    hitp = pp->pt_inhit;

    /* construct the actual (entry) hit-point from the ray and the
     * distance to the intersection point (i.e., the 't' value).
     */
    //VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
    brlcad_ray_info.distance = hitp->hit_dist;

    /* primitive we encountered on entry */
    stp = pp->pt_inseg->seg_stp;

    /* compute the normal vector at the entry point, flipping the
     * normal if necessary.
     */
    RT_HIT_NORMAL(inormal, hitp, stp, &(ap->a_ray), pp->pt_inflip);

    brlcad_ray_info.normal[0] = inormal[0];
    brlcad_ray_info.normal[1] = inormal[1];
    brlcad_ray_info.normal[2] = inormal[2];

    return 1;
}


/* brlcad raytrace miss callback */
int
brlcad_miss(struct application* UNUSED(ap))
{
    return 0;
}


/* constructor when called from appleseed */
BrlcadObject::BrlcadObject(
    const char* bname,
    const asr::ParamArray& params)
    : asr::ProceduralObject(bname, params)
{
    configure_raytrace_application(get_database().c_str(), get_object_count(), get_objects());
    VSET(min, m_params.get_required<double>("minX"), m_params.get_required<double>("minY"), m_params.get_required<double>("minZ"));
    VSET(max, m_params.get_required<double>("maxX"), m_params.get_required<double>("maxY"), m_params.get_required<double>("maxZ"));
    // VMOVE(min, ap->a_uvec);
    // VMOVE(max, ap->a_vvec);

    this->name = new std::string(m_params.get_required<std::string>("object_path"));
}


/* constructor when called from art.cpp */
BrlcadObject:: BrlcadObject(
    const char* bname,
    const asr::ParamArray& params,
    struct application* p_ap, struct resource* p_resources)
    : asr::ProceduralObject(bname, params)
{
    // this->rtip = p_ap->a_rt_i;
    this->resources = p_resources;
    // VMOVE(this->min, ap->a_uvec);
    // VMOVE(this->max, ap->a_vvec);

    this->name = new std::string(m_params.get_required<std::string>("object_path"));
    VSET(min, m_params.get_required<double>("minX"), m_params.get_required<double>("minY"), m_params.get_required<double>("minZ"));
    VSET(max, m_params.get_required<double>("maxX"), m_params.get_required<double>("maxY"), m_params.get_required<double>("maxZ"));

    std::string db_file = m_params.get_required<std::string>("database_path");
    this->rtip = rt_new_rti(p_ap->a_rt_i->rti_dbip);
    if (this->rtip == RTI_NULL) {
        RENDERER_LOG_INFO("building the database directory for [%s] FAILED\n", db_file.c_str());
        bu_exit(BRLCAD_ERROR, "building the database directory for [%s] FAILED\n", db_file.c_str());
    }

    for (int ic = 0; ic < MAX_PSW; ic++) {
	rt_init_resource(&p_resources[ic], ic, this->rtip);
        RT_CK_RESOURCE(&p_resources[ic]);
    }

    rt_gettree(this->rtip, this->name->c_str());
    if (this->rtip->needprep)
        rt_prep_parallel(this->rtip, 1);

    RT_APPLICATION_INIT(&ap);

    /* configure raytrace application */
    ap.a_rt_i = this->rtip;
    ap.a_onehit = 1;
    ap.a_hit = brlcad_hit;
    ap.a_miss = brlcad_miss;
}


/* Delete instance */
void
BrlcadObject::release()
{
    // bu_free(resources, "appleseed");
    // bu_free(ap, "appleseed");
    delete this->name;
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
    if (!this->rtip)
	return asr::GAABB3(asr::GVector3(0, 0, 0), asr::GVector3(0, 0, 0));

    // const auto r = static_cast<asr::GScalar>(get_uncached_radius());
    if (this->rtip->needprep)
	rt_prep_parallel(this->rtip, 1);

    // point_t min;
    // VSET(min, m_params.get_required<double>("minX"), m_params.get_required<double>("minY"), m_params.get_required<double>("minZ"));
    // VMOVE(min, ap->a_uvec);
    // point_t max;
    // VMOVE(max, ap->a_vvec);
    // VSET(max, m_params.get_required<double>("maxX"), m_params.get_required<double>("maxY"), m_params.get_required<double>("maxZ"));

    return asr::GAABB3(asr::GVector3(V3ARGS(min)), asr::GVector3(V3ARGS(max)));
    // return asr::GAABB3(asr::GVector3(-r), asr::GVector3(r));
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


/* Compute intersection between ray and object surface (DETAILED) */
void
BrlcadObject::intersect(
    const asr::ShadingRay& ray,
    IntersectionResult& result) const
{
    struct application app;
    app = ap;  /*struct copy*/
    /* brlcad raytracing */
    int cpu = get_id();
    app.a_resource = &resources[cpu];

    const asf::Vector3d dir = asf::normalize(ray.m_dir);
    VSET(app.a_ray.r_dir, dir[0], dir[1], dir[2]);
    VSET(app.a_ray.r_pt, ray.m_org[0], ray.m_org[1], ray.m_org[2]);

    app.a_uptr = (void*)this->name->c_str();

    if (rt_shootray(&app) == 0) {
	result.m_hit = false;
	return;
    } else {
	result.m_hit = true;
	result.m_distance = brlcad_ray_info.distance;

	const asf::Vector3d n = asf::normalize(brlcad_ray_info.normal);
	result.m_geometric_normal = n;

	// const asf::Vector3d n_flip (n[0], n[2], n[1]);
	// double temp;
	// temp = n_flip[2];
	// n_flip.set(2) = n_flip[1];
	// n_flip.set(1) = temp;
	result.m_shading_normal = n;

	// const asf::Vector3f p(brlcad_ray_info.normal * m_rcp_radius);
	// result.m_uv[0] = std::acos(p.y) * asf::RcpPi<float>();
	// result.m_uv[1] = std::atan2(-p.z, p.x) * asf::RcpTwoPi<float>();
	result.m_uv[0] = 1.0;
	result.m_uv[1] = 1.0;

	result.m_material_slot = 0;
    }
}

/* Compute intersection between ray and object surface (SIMPLE) */
bool
BrlcadObject::intersect(const asr::ShadingRay& ray) const
{
    struct application app;
    app = ap; /* struct copy */
    /* brlcad raytracing */
    int cpu = get_id();
    app.a_resource = &resources[cpu];

    const asf::Vector3d dir = asf::normalize(ray.m_dir);
    VSET(app.a_ray.r_dir, dir[0], dir[1], dir[2]);
    VSET(app.a_ray.r_pt, ray.m_org[0], ray.m_org[1], ray.m_org[2]);

    return (rt_shootray(&app) == 1);
}


// Compute a front point, a back point and the geometric normal in object
// instance space for a given ray with origin being a point on the surface
void
BrlcadObject::refine_and_offset(
    const asf::Ray3d& obj_inst_ray,
    asf::Vector3d& obj_inst_front_point,
    asf::Vector3d& obj_inst_back_point,
    asf::Vector3d& obj_inst_geo_normal) const
{
    obj_inst_front_point = obj_inst_ray.m_org;
    obj_inst_back_point = obj_inst_ray.m_org;
    obj_inst_geo_normal = -obj_inst_ray.m_dir;
}


static std::atomic<int> counter;

int
BrlcadObject::get_id()
{
    thread_local int id = counter++;
    return id;
}


std::string
BrlcadObject::get_database() const
{
    return m_params.get_required<std::string>("database_path");
}


int
BrlcadObject::get_object_count() const
{
    return m_params.get_required<int>("object_count");
}


std::vector<std::string>
BrlcadObject::get_objects() const
{
    std::vector<std::string> objects;
    int count = get_object_count();

    for (char i = 0; i < count; i++) {
	std::string obj_num = "object." + std::to_string(i + 1);
	objects.push_back(m_params.get_path_required<std::string>(obj_num.c_str(), ""));
    }

    return objects;
}


/* Set up raytrace application */
void
BrlcadObject::configure_raytrace_application(const char* path, int objc, std::vector<std::string> objects)
{
    // FIXME: This current implementation below is untested and likely out of sync. It needs to be reworked for plugin use.

    RT_APPLICATION_INIT(&ap);

    /* configure raytrace application */
    ap.a_rt_i = this->rtip;
    ap.a_onehit = 1;
    ap.a_hit = brlcad_hit;
    ap.a_miss = brlcad_miss;
    //ap = static_cast<application*>(bu_calloc(1, sizeof(application), "appleseed"));
    resources = static_cast<resource*>(bu_calloc(1, sizeof(resource) * MAX_PSW, "appleseed"));

    char title[1024] = { 0 }; /* optional database title */
    size_t npsw = 1; /* default number of worker PSWs*/

    /* load the specified geometry database */
    rtip = rt_dirbuild(path, title, sizeof(title));
    if (rtip == RTI_NULL) {
	bu_log("Building the database directory for [%s] FAILED\n", path);
	return;
    }

    for (size_t i = 0; i < MAX_PSW; i++) {
	rt_init_resource(&resources[i], (int)i, rtip);
	RT_CK_RESOURCE(&resources[i]);
    }

    /* display optional database title */
    if (title[0]) {
	bu_log("Database title: %s\n", title);
    }

    /* parse object arguments */
    const char** objv = (const char**)bu_calloc((size_t)objc + 1, sizeof(char*), "obj array");
    for (int i = 0; i < objc; i++) {
	objv[i] = objects.at(i).c_str();
    }

    /* include objects from database */
    if (rt_gettrees(rtip, objc, objv, (int)npsw) < 0) {
	bu_log("Loading the geometry for [%s] FAILED\n", objects[0].c_str());
    }

    /* Prepare database for raytracing */
    if (rtip->needprep)
	rt_prep_parallel(rtip, 1);

    /* Initialize values in application struct */
    //RT_APPLICATION_INIT(ap);

    /* configure raytrace application */
    //ap->a_onehit = -1;
    //ap->a_rt_i = rtip;
    //ap->a_hit = brlcad_hit;
    //ap->a_miss = brlcad_miss;
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

    metadata.push_back(
	asf::Dictionary()
	.insert("name", "radius")
	.insert("label", "Radius")
	.insert("type", "numeric")
	.insert("min",
		asf::Dictionary()
		.insert("value", "0.0")
		.insert("type", "hard"))
	.insert("max",
		asf::Dictionary()
		.insert("value", "10.0")
		.insert("type", "soft"))
	.insert("use", "optional")
	.insert("default", "1.0"));

    return metadata;
}


/* Create a new empty object */
asf::auto_release_ptr<asr::Object>
BrlcadObjectFactory::create(
    const char* name,
    const asr::ParamArray& params) const
{
    return asf::auto_release_ptr<asr::Object>(new BrlcadObject(name, params));
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
    objects.push_back(create(name, params).release());
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
