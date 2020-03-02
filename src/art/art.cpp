/*                     A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @file rt/art.cpp
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
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wpedantic"
#  pragma GCC diagnostic ignored "-Wclass-memaccess"
#  pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wpedantic"
#  pragma clang diagnostic ignored "-Wclass-memaccess"
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


#include "vmath.h"		/* vector math macros */
#include "raytrace.h"		/* librt interface definitions */


/* NOTE: stub in empty rt_cmdtab to satisfy ../rt/opt.c - this means
 * we can't run the commands, but they are tied deeply into the various
 * src/rt files and a significant refactor is in order to properly
 * extract that functionality into a library... */
struct command_tab rt_cmdtab[] = {{NULL, NULL, NULL, 0, 0, 0}};


// Define shorter namespaces for convenience.
namespace asf = foundation;
namespace asr = renderer;

asf::auto_release_ptr<asr::Project> build_project()
{
    // Create an empty project.
    asf::auto_release_ptr<asr::Project> project(asr::ProjectFactory::create("test project"));
    project->search_paths().push_back_explicit_path("data");

    // Add default configurations to the project.
    project->add_default_configurations();

    // Set the number of samples. This is the main quality parameter: the higher the
    // number of samples, the smoother the image but the longer the rendering time.
    project->configurations()
    .get_by_name("final")->get_parameters()
    .insert_path("uniform_pixel_renderer.samples", "25");

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

    // Load the scene geometry from disk.
    asr::MeshObjectArray objects;
    asr::MeshObjectReader::read(
	project->search_paths(),
	"cube",
	asr::ParamArray()
	.insert("filename", "scene.obj"),
	objects);

    // Insert all the objects into the assembly.
    for (size_t i = 0; i < objects.size(); ++i) {
	// Insert this object into the scene.
	asr::MeshObject* object = objects[i];
	assembly->objects().insert(asf::auto_release_ptr<asr::Object>(object));

	// Create an instance of this object and insert it into the assembly.
	const std::string instance_name = std::string(object->get_name()) + "_inst";
	assembly->object_instances().insert(
	    asr::ObjectInstanceFactory::create(
		instance_name.c_str(),
		asr::ParamArray(),
		object->get_name(),
		asf::Transformd::identity(),
		asf::StringDictionary()
		.insert("default", "gray_material")
		.insert("default2", "gray_material")));
    }

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

    // Create a pinhole camera with film dimensions 0.980 x 0.735 in (24.892 x 18.669 mm).
    asf::auto_release_ptr<asr::Camera> camera(
	asr::PinholeCameraFactory().create(
	    "camera",
	    asr::ParamArray()
	    .insert("film_dimensions", "0.024892 0.018669")
	    .insert("focal_length", "0.035")));

    // Place and orient the camera. By default cameras are located in (0.0, 0.0, 0.0)
    // and are looking toward Z- (0.0, 0.0, -1.0).
    camera->transform_sequence().set_transform(
	0.0f,
	asf::Transformd::from_local_to_parent(
	    asf::Matrix4d::make_rotation(asf::Vector3d(1.0, 0.0, 0.0), asf::deg_to_rad(-20.0)) *
	    asf::Matrix4d::make_translation(asf::Vector3d(0.0, 0.8, 11.0))));

    // Bind the camera to the scene.
    scene->cameras().insert(camera);

    //------------------------------------------------------------------------
    // Frame
    //------------------------------------------------------------------------

    // Create a frame and bind it to the project.
    project->set_frame(
	asr::FrameFactory::create(
	    "beauty",
	    asr::ParamArray()
	    .insert("camera", "camera")
	    .insert("resolution", "640 480")));

    // Bind the scene to the project.
    project->set_scene(scene);

    return project;
}

int
main(int argc, char **argv)
{
    if (argc > 1) {
	bu_log("TODO: %s is currently only a demo, doesn't accept a .g file and objects...\n", argv[0]);
    }

    // Create a log target that outputs to stderr, and binds it to the renderer's global logger.
    // Eventually you will probably want to redirect log messages to your own target. For this
    // you will need to implement foundation::ILogTarget (foundation/utility/log/ilogtarget.h).
    std::unique_ptr<asf::ILogTarget> log_target(asf::create_console_log_target(stderr));
    asr::global_logger().add_target(log_target.get());

    // Print appleseed's version string.
    RENDERER_LOG_INFO("%s", asf::Appleseed::get_synthetic_version_string());

    // Build the project.
    asf::auto_release_ptr<asr::Project> project(build_project());

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
    project->get_frame()->write_main_image("output/test.png");

    // Save the project to disk.
    asr::ProjectFileWriter::write(project.ref(), "output/test.appleseed");

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
