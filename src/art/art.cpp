/*                     A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @file art/art.cpp
 *
 * Once you have appleseed installed, run BRL-CAD's CMake with APPLESEED_ROOT
 * set to enable this program:
 *
 * cmake .. -DAPPLESEED_ROOT=/path/to/appleseed -DBRLCAD_PNG=SYSTEM -DBRLCAD_ZLIB=SYSTEM
 *
 * (the appleseed root path should contain bin, lib and include directories)
 *
 * On Linux, if using the prebuilt binary you'll need to set LD_LIBRARY_PATH:
 * export LD_LIBRARY_PATH=/path/to/appleseed/lib
 *
 *
 * The example scene object used by helloworld is found at:
 * https://raw.githubusercontent.com/appleseedhq/appleseed/master/sandbox/examples/cpp/helloworld/data/scene.obj
 *
 * basic example helloworld code from
 * https://github.com/appleseedhq/appleseed/blob/master/sandbox/examples/cpp/helloworld/helloworld.cpp
 * has the following license:
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
/* avoid redefining the appleseed sleep function */
#undef sleep

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include <cstddef>
#include <memory>
#include <string>

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


// appleseed.renderer headers. Only include header files from renderer/api/.
#include "renderer/api/bsdf.h"
#include "renderer/api/camera.h"
#include "renderer/api/color.h"
#include "renderer/api/environment.h"
#include "renderer/api/environmentedf.h"
#include "renderer/api/environmentshader.h"
#include "renderer/api/frame.h"
#include "renderer/api/light.h"
#include "renderer/api/log.h"
#include "renderer/api/material.h"
#include "renderer/api/object.h"
#include "renderer/api/project.h"
#include "renderer/api/rendering.h"
#include "renderer/api/scene.h"
#include "renderer/api/surfaceshader.h"
#include "renderer/api/utility.h"

// appleseed.foundation headers.
#include "foundation/core/appleseed.h"
#include "foundation/math/matrix.h"
#include "foundation/math/scalar.h"
#include "foundation/math/transform.h"
#include "foundation/math/vector.h"
#include "foundation/utility/containers/dictionary.h"
#include "foundation/utility/log/consolelogtarget.h"
#include "foundation/utility/autoreleaseptr.h"
#include "foundation/utility/searchpaths.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


#include "vmath.h"	/* vector math macros */
#include "raytrace.h"	    /* librt interface definitions */
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/vls.h"
#include "art.h"

struct application APP;
struct resource* resources;
extern "C" {
    FILE* outfp = NULL;
    int	save_overlaps = 1;
    size_t		n_malloc;		/* Totals at last check */
    size_t		n_free;
    size_t		n_realloc;
    mat_t view2model;
    mat_t model2view;
    struct icv_image* bif = NULL;
}

/* NOTE: stub in empty rt_do_tab to satisfy ../rt/opt.c - this means
 * we can't run the commands, but they are tied deeply into the various
 * src/rt files and a significant refactor is in order to properly
 * extract that functionality into a library... */

extern "C" {
    struct command_tab rt_do_tab[] = { {NULL, NULL, NULL, 0, 0, 0} };
    void usage(const char* argv0, int verbose);
    int get_args(int argc, const char* argv[]);

    extern char* outputfile;
    extern int objc;
    extern char** objv;
    extern size_t npsw;
    extern struct bu_ptbl* cmd_objs;
    extern size_t width, height;
    extern fastf_t azimuth, elevation;
    extern point_t eye_model; /* model-space location of eye */
    extern fastf_t eye_backoff; /* dist from eye to center */
    extern mat_t Viewrotscale;
    extern fastf_t viewsize;
    extern fastf_t aspect;

    void grid_setup();
}

void
do_ae(double azim, double elev)
{
    vect_t temp;
    vect_t diag;
    mat_t toEye;
    struct rt_i* rtip = APP.a_rt_i;

    if (rtip == NULL)
	return;

    if (rtip->nsolids <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: no primitives active\n");

    if (rtip->nregions <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: no regions active\n");

    if (rtip->mdl_max[X] >= INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit minimum\n");
	VSETALL(rtip->mdl_min, -1);
    }
    if (rtip->mdl_max[X] <= -INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit maximum\n");
	VSETALL(rtip->mdl_max, 1);
    }

    /*
     * Enlarge the model RPP just slightly, to avoid nasty effects
     * with a solid's face being exactly on the edge NOTE: This code
     * is duplicated out of librt/tree.c/rt_prep(), and has to appear
     * here to enable the viewsize calculation to match the final RPP.
     */
    rtip->mdl_min[X] = floor(rtip->mdl_min[X]);
    rtip->mdl_min[Y] = floor(rtip->mdl_min[Y]);
    rtip->mdl_min[Z] = floor(rtip->mdl_min[Z]);
    rtip->mdl_max[X] = ceil(rtip->mdl_max[X]);
    rtip->mdl_max[Y] = ceil(rtip->mdl_max[Y]);
    rtip->mdl_max[Z] = ceil(rtip->mdl_max[Z]);

    MAT_IDN(Viewrotscale);
    bn_mat_angles(Viewrotscale, 270.0 + elev, 0.0, 270.0 - azim);

    /* Look at the center of the model */
    MAT_IDN(toEye);
    toEye[MDX] = -((rtip->mdl_max[X] + rtip->mdl_min[X]) / 2.0);
    toEye[MDY] = -((rtip->mdl_max[Y] + rtip->mdl_min[Y]) / 2.0);
    toEye[MDZ] = -((rtip->mdl_max[Z] + rtip->mdl_min[Z]) / 2.0);

    /* Fit a sphere to the model RPP, diameter is viewsize, unless
     * viewsize command used to override.
     */
    if (viewsize <= 0) {
	VSUB2(diag, rtip->mdl_max, rtip->mdl_min);
	viewsize = MAGNITUDE(diag);
	if (aspect > 1) {
	    /* don't clip any of the image when autoscaling */
	    viewsize *= aspect;
	}
    }

    /* sanity check: make sure viewsize still isn't zero in case
     * bounding box is empty, otherwise bn_mat_int() will bomb.
     */
    if (viewsize < 0 || ZERO(viewsize)) {
	viewsize = 2.0; /* arbitrary so Viewrotscale is normal */
    }

    Viewrotscale[15] = 0.5 * viewsize;	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);
    VSET(temp, 0, 0, eye_backoff);
    MAT4X3PNT(eye_model, view2model, temp);
}

// Define shorter namespaces for convenience.
namespace asf = foundation;
namespace asr = renderer;

asf::auto_release_ptr<asr::Project> build_project(const char* file, const char* UNUSED(objects))
{
    /* If user gave no sizing info at all, use 512 as default */
    struct bu_vls dimensions = BU_VLS_INIT_ZERO;
    if (width <= 0)
	width = 512;
    if (height <= 0)
	height = 512;

    // Create an empty project.
    asf::auto_release_ptr<asr::Project> project(asr::ProjectFactory::create("test project"));
    project->search_paths().push_back_explicit_path("build/Debug");

    // Add default configurations to the project.
    project->add_default_configurations();

    // Set the number of samples. This is the main quality parameter: the higher the
    // number of samples, the smoother the image but the longer the rendering time.
    project->configurations()
    .get_by_name("final")->get_parameters()
    .insert_path("uniform_pixel_renderer.samples", "25")
    .insert_path("rendering_threads", "1"); /* multithreading not supported yet */

    project->configurations()
	.get_by_name("interactive")->get_parameters()
	.insert_path("rendering_threads", "1"); /* no multithreading - for debug rendering on appleseed */

    // Create a scene.
    asf::auto_release_ptr<asr::Scene> scene(asr::SceneFactory::create());

    // Create an assembly.
    asf::auto_release_ptr<asr::Assembly> assembly(
	asr::AssemblyFactory().create(
	    "assembly",
	    asr::ParamArray()));

    //------------------------------------------------------------------------
    // Materials
    //------------------------------------------------------------------------

    // Create a color called "gray" and insert it into the assembly.
    static const float GrayReflectance[] = { 0.5f, 0.5f, 0.5f };
    assembly->colors().insert(
	asr::ColorEntityFactory::create(
	    "gray",
	    asr::ParamArray()
	    .insert("color_space", "srgb"),
	    asr::ColorValueArray(3, GrayReflectance)));

    // Create a BRDF called "diffuse_gray_brdf" and insert it into the assembly.
    assembly->bsdfs().insert(
	asr::LambertianBRDFFactory().create(
	    "diffuse_gray_brdf",
	    asr::ParamArray()
	    .insert("reflectance", "gray")));

    // Create a physical surface shader and insert it into the assembly.
    assembly->surface_shaders().insert(
	asr::PhysicalSurfaceShaderFactory().create(
	    "physical_surface_shader",
	    asr::ParamArray()));

    // Create a material called "gray_material" and insert it into the assembly.
    assembly->materials().insert(
	asr::GenericMaterialFactory().create(
	    "gray_material",
	    asr::ParamArray()
	    .insert("surface_shader", "physical_surface_shader")
	    .insert("bsdf", "diffuse_gray_brdf")));

    //------------------------------------------------------------------------
    // Geometry
    //------------------------------------------------------------------------

    // Load the brlcad geometry
    renderer::ParamArray geometry_parameters = asr::ParamArray()
					       .insert("database_path", file)
					       .insert("object_count", objc);
    for (int i = 0; i < objc; i++)
    {
	std::string obj_num = std::string("object.") + std::to_string(i + 1);
	std::string obj_name = std::string(objv[i]);
	geometry_parameters.insert_path(obj_num, obj_name);
    }

    asf::auto_release_ptr<renderer::Object> brlcad_object(
	new BrlcadObject{"brlcad geometry",
			 geometry_parameters,
			 &APP, resources});
    assembly->objects().insert(brlcad_object);

    const std::string instance_name = "brlcad_inst";
    assembly->object_instances().insert(
	asr::ObjectInstanceFactory::create(
	    instance_name.c_str(),
	    asr::ParamArray(),
	    "brlcad geometry",
	    asf::Transformd::identity(),
	    asf::StringDictionary()
	    .insert("default", "gray_material")
	    .insert("default2", "gray_material")));


    //------------------------------------------------------------------------
    // Light
    //------------------------------------------------------------------------

    // Create a color called "light_intensity" and insert it into the assembly.
    static const float LightRadiance[] = { 1.0f, 1.0f, 1.0f };
    assembly->colors().insert(
	asr::ColorEntityFactory::create(
	    "light_intensity",
	    asr::ParamArray()
	    .insert("color_space", "srgb")
	    .insert("multiplier", "30.0"),
	    asr::ColorValueArray(3, LightRadiance)));

    // Create a point light called "light" and insert it into the assembly.
    asf::auto_release_ptr<asr::Light> light(
	asr::PointLightFactory().create(
	    "light",
	    asr::ParamArray()
	    .insert("intensity", "light_intensity")));
    light->set_transform(
	asf::Transformd::from_local_to_parent(
	    asf::Matrix4d::make_translation(asf::Vector3d(0.6, 2.0, 1.0))));
    assembly->lights().insert(light);

    //------------------------------------------------------------------------
    // Assembly instance
    //------------------------------------------------------------------------

    // Create an instance of the assembly and insert it into the scene.
    asf::auto_release_ptr<asr::AssemblyInstance> assembly_instance(
	asr::AssemblyInstanceFactory::create(
	    "assembly_inst",
	    asr::ParamArray(),
	    "assembly"));
    assembly_instance
    ->transform_sequence()
    .set_transform(
	0.0f,
	asf::Transformd::identity());
    scene->assembly_instances().insert(assembly_instance);

    // Insert the assembly into the scene.
    scene->assemblies().insert(assembly);

    //------------------------------------------------------------------------
    // Environment
    //------------------------------------------------------------------------

    // Create a color called "sky_radiance" and insert it into the scene.
    static const float SkyRadiance[] = { 0.75f, 0.80f, 1.0f };
    scene->colors().insert(
	asr::ColorEntityFactory::create(
	    "sky_radiance",
	    asr::ParamArray()
	    .insert("color_space", "srgb")
	    .insert("multiplier", "0.5"),
	    asr::ColorValueArray(3, SkyRadiance)));

    // Create an environment EDF called "sky_edf" and insert it into the scene.
    scene->environment_edfs().insert(
	asr::ConstantEnvironmentEDFFactory().create(
	    "sky_edf",
	    asr::ParamArray()
	    .insert("radiance", "sky_radiance")));

    // Create an environment shader called "sky_shader" and insert it into the scene.
    scene->environment_shaders().insert(
	asr::EDFEnvironmentShaderFactory().create(
	    "sky_shader",
	    asr::ParamArray()
	    .insert("environment_edf", "sky_edf")));

    // Create an environment called "sky" and bind it to the scene.
    scene->set_environment(
	asr::EnvironmentFactory::create(
	    "sky",
	    asr::ParamArray()
	    .insert("environment_edf", "sky_edf")
	    .insert("environment_shader", "sky_shader")));

    //------------------------------------------------------------------------
    // Camera
    //------------------------------------------------------------------------

    // Create a pinhole camera with film dimensions
    bu_vls_sprintf(&dimensions, "%f %f", 0.08 * (double) width / height, 0.08);
    asf::auto_release_ptr<asr::Camera> camera(
	asr::PinholeCameraFactory().create(
	    "camera",
	    asr::ParamArray()
	    .insert("film_dimensions", bu_vls_cstr(&dimensions))
	    .insert("focal_length", "0.035")));

    // Place and orient the camera. By default cameras are located in (0.0, 0.0, 0.0)
    // and are looking toward Z- (0.0, 0.0, -1.0).
    camera->transform_sequence().set_transform(
	0.0f,
	asf::Transformd::from_local_to_parent(
	    asf::Matrix4d::make_translation(asf::Vector3d(eye_model[0], eye_model[2], -eye_model[1])) * /* camera location */
	    asf::Matrix4d::make_rotation(asf::Vector3d(0.0, 1.0, 0.0), asf::deg_to_rad(azimuth - 270)) * /* azimuth */
	    asf::Matrix4d::make_rotation(asf::Vector3d(1.0, 0.0, 0.0), asf::deg_to_rad(-elevation)) /* elevation */
	));

    // Bind the camera to the scene.
    scene->cameras().insert(camera);

    //------------------------------------------------------------------------
    // Frame
    //------------------------------------------------------------------------

    // Create a frame and bind it to the project.
    bu_vls_sprintf(&dimensions, "%zd %zd", width, height);
    project->set_frame(
	asr::FrameFactory::create(
	    "beauty",
	    asr::ParamArray()
	    .insert("camera", "camera")
	    .insert("resolution", bu_vls_cstr(&dimensions))));

    // Bind the scene to the project.
    project->set_scene(scene);

    return project;
}


int
main(int argc, char **argv)
{
    // Create a log target that outputs to stderr, and binds it to the renderer's global logger.
    // Eventually you will probably want to redirect log messages to your own target. For this
    // you will need to implement foundation::ILogTarget (foundation/utility/log/ilogtarget.h).
    std::unique_ptr<asf::ILogTarget> log_target(asf::create_console_log_target(stderr));
    asr::global_logger().add_target(log_target.get());

    // Print appleseed's version string.
    RENDERER_LOG_INFO("%s", asf::Appleseed::get_synthetic_version_string());

    struct rt_i* rtip;
    const char *title_file = NULL;
    //const char *title_obj = NULL;	/* name of file and first object */
    struct bu_vls str = BU_VLS_INIT_ZERO;
    //int objs_free_argv = 0;

    bu_setprogname(argv[0]);

    /* Process command line options */
    int i = get_args(argc, (const char**)argv);
    if (i < 0) {
	//usage(argv[0], 0);
	return 1;
    }
    else if (i == 0) {
	//usage(argv[0], 100);
	return 0;
    }

    if (bu_optind >= argc) {
	RENDERER_LOG_INFO("%s: BRL-CAD geometry database not specified\n", argv[0]);
	//usage(argv[0], 0);
	return 1;
    }

    title_file = argv[bu_optind];
    //title_obj = argv[bu_optind + 1];
    if (!objv) {
	objc = argc - bu_optind - 1;
	if (objc) {
	    objv = (char **)&(argv[bu_optind+1]);
	} else {
	    /* No objects in either input file or argv - try getting objs from
	     * command processing.  Initialize the table. */
	    BU_GET(cmd_objs, struct bu_ptbl);
	    bu_ptbl_init(cmd_objs, 8, "initialize cmdobjs table");
	}
    } else {
	//objs_free_argv = 1;
    }

    bu_vls_from_argv(&str, objc, (const char**)objv);

    resources = static_cast<resource*>(bu_calloc(1, sizeof(resource) * MAX_PSW, "appleseed"));
    char title[1024] = { 0 };

    /* load the specified geometry database */
    rtip = rt_dirbuild(title_file, title, sizeof(title));
    if (rtip == RTI_NULL)
    {
	RENDERER_LOG_INFO("building the database directory for [%s] FAILED\n", title_file);
	return -1;
    }

    for (int ic = 0; ic < MAX_PSW; ic++) {
	rt_init_resource(&resources[ic], ic, rtip);
	RT_CK_RESOURCE(&resources[ic]);
    }

    /* print optional title */
    if (title[0])
    {
	RENDERER_LOG_INFO("database title: %s\n", title);
    }

    /* include objects from database */
    if (rt_gettrees(rtip, objc, (const char**)objv, npsw) < 0)
    {
	RENDERER_LOG_INFO("loading the geometry for [%s...] FAILED\n", objv[0]);
	return -1;
    }

    /* prepare database for raytracing */
    if (rtip->needprep)
	rt_prep_parallel(rtip, 1);

    /* initialize values in application struct */
    RT_APPLICATION_INIT(&APP);

    /* configure raytrace application */
    APP.a_rt_i = rtip;
    APP.a_onehit = -1;
    APP.a_hit = brlcad_hit;
    APP.a_miss = brlcad_miss;

    do_ae(azimuth, elevation);
    RENDERER_LOG_INFO("View model: (%f, %f, %f)", eye_model[0], -eye_model[2], eye_model[1]);

    // Build the project.
    asf::auto_release_ptr<asr::Project> project(build_project(title_file, bu_vls_cstr(&str)));

    // Create the master renderer.
    asr::DefaultRendererController renderer_controller;
    asf::SearchPaths resource_search_paths;
    std::unique_ptr<asr::MasterRenderer> renderer(
	new asr::MasterRenderer(
	    project.ref(),
	    project->configurations().get_by_name("final")->get_inherited_parameters(),
	    resource_search_paths));

    // Render the frame.
    renderer->render(renderer_controller);

    // Save the frame to disk.
    char *default_out = bu_strdup("art.png");
    outputfile = default_out;
    project->get_frame()->write_main_image(outputfile);
    bu_free(default_out, "default name");

    // Save the project to disk.
    //asr::ProjectFileWriter::write(project.ref(), "output/objects.appleseed");

    // Make sure to delete the master renderer before the project and the logger / log target.
    renderer.reset();

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
