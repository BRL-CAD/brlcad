/*                 G E D _ C O M M A N D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2025 United States Government as represented by
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
/** @file ged_command.cpp
 *
 * Simulate GED command.
 *
 */


#include "common.h"


#ifndef HAVE_BULLET


#include "ged.h"


extern "C" int
ged_simulate_core(ged * const gedp, const int argc, const char ** const argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_sprintf(gedp->ged_result_str,
		   "%s: This build of BRL-CAD was not compiled with Bullet support", argv[0]);

    return BRLCAD_ERROR;
}


#else


#include "simulation.hpp"
#include "sim_animate.hpp"
#include "utility.hpp"

#include "bu/opt.h"
#include "ged.h"

#include <iomanip>
#include <sstream>


namespace
{


static simulate::Simulation::DebugMode
get_debug_mode(const std::string &debug_mode_string)
{
    simulate::Simulation::DebugMode result = simulate::Simulation::debug_none;

    if (debug_mode_string.empty())
	return result;

    std::istringstream stream(debug_mode_string);

    while (!stream.eof()) {
	std::string value;
	std::getline(stream, value, ',');

	if (value == "aabb")
	    result = result | simulate::Simulation::debug_aabb;
	else if (value == "contact")
	    result = result | simulate::Simulation::debug_contact;
	else if (value == "ray")
	    result = result | simulate::Simulation::debug_ray;
	else
	    throw simulate::InvalidSimulationError("invalid debug mode");
    }

    return result;
}


/** Parse a whitespace- or comma-separated list of N doubles from @p s.
 *  Returns true on success. */
static bool
parse_doubles(const char *s, double *vals, int n)
{
    if (!s) return false;
    std::istringstream iss(s);
    /* Replace commas with spaces for uniform parsing */
    std::string tmp(s);
    for (size_t i = 0; i < tmp.size(); ++i)
	if (tmp[i] == ',') tmp[i] = ' ';
    iss.str(tmp);

    for (int i = 0; i < n; ++i) {
	if (!(iss >> vals[i]))
	    return false;
    }
    return true;
}


}


extern "C" int
ged_simulate_core(ged * const gedp, const int argc, const char ** const argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* ---------------------------------------------------------------- */
    /* Option variables                                                  */
    /* ---------------------------------------------------------------- */

    const char *debug_mode_string   = "";
    const char *plot_prefix_string  = "";
    int         num_steps           = 1;
    long        resume_flag         = 0;

    /* Animation options */
    const char *output_file_string  = "";
    int         anim_width          = 1280;
    int         anim_height         = 720;
    int         anim_fps            = 24;
    const char *view_quat_string    = "";
    const char *view_ae_string      = "";
    const char *view_eye_string     = "";
    const char *view_center_string  = "";
    fastf_t     view_size_val       = 0.0;

    const bu_opt_desc options_description[] = {
	/* Simulation options */
	{NULL, "debug",  "mode",   bu_opt_str,       &debug_mode_string,
	 "set debug mode (example: --debug=aabb,contact,ray)"},
	{NULL, "steps",  "N",      bu_opt_int,        &num_steps,
	 "run N incremental steps, updating geometry and saving state after each"},
	{NULL, "plot",   "prefix", bu_opt_str,        &plot_prefix_string,
	 "write .pl wireframe plot file(s) with the given filename prefix"},
	{NULL, "resume", "",       bu_opt_incr_long,  &resume_flag,
	 "resume from previously saved simulation state"},

	/* Animation output */
	{"o", "output",  "FILE",   bu_opt_str,        &output_file_string,
	 "write animation to FILE (MJPEG AVI; e.g. sim.avi)"},
	{NULL, "width",  "W",      bu_opt_int,        &anim_width,
	 "frame width in pixels (default 1280)"},
	{NULL, "height", "H",      bu_opt_int,        &anim_height,
	 "frame height in pixels (default 720)"},
	{NULL, "fps",    "N",      bu_opt_int,        &anim_fps,
	 "frames per second in output video (default 24)"},

	/* Camera / view specification */
	{NULL, "view-quat",   "X,Y,Z,W", bu_opt_str, &view_quat_string,
	 "fixed camera orientation as quaternion x,y,z,w (overrides chase camera)"},
	{NULL, "view-ae",     "AZ,EL",   bu_opt_str, &view_ae_string,
	 "fixed camera orientation as azimuth,elevation in degrees"},
	{NULL, "view-eye",    "X,Y,Z",   bu_opt_str, &view_eye_string,
	 "fixed eye position in model coordinates (mm)"},
	{NULL, "view-center", "X,Y,Z",   bu_opt_str, &view_center_string,
	 "override view center in model coordinates (mm)"},
	{NULL, "view-size",   "S",       bu_opt_fastf_t, &view_size_val,
	 "view size (distance) in mm (default 15000)"},

	BU_OPT_DESC_NULL
    };

    if (2 != bu_opt_parse(gedp->ged_result_str, argc - 1, &argv[1],
			  options_description)) {
	const simulate::AutoPtr<char> usage(bu_opt_describe(
						const_cast<bu_opt_desc *>(options_description), NULL));
	bu_vls_printf(gedp->ged_result_str,
		      "USAGE: %s [OPTIONS] path duration\nOptions:\n%s\n", argv[0], usage.ptr);
	return BRLCAD_ERROR;
    }

    /* ---------------------------------------------------------------- */
    /* Build animation options struct                                    */
    /* ---------------------------------------------------------------- */
    simulate::SimAnimOptions anim_opts;

    if (*output_file_string)
	anim_opts.output_file = std::string(output_file_string);

    if (anim_width > 0)  anim_opts.width  = anim_width;
    if (anim_height > 0) anim_opts.height = anim_height;
    if (anim_fps > 0)    anim_opts.fps    = anim_fps;

    if (*view_quat_string) {
	double vals[4] = {0, 0, 0, 1};
	if (!parse_doubles(view_quat_string, vals, 4)) {
	    bu_vls_sprintf(gedp->ged_result_str,
			  "simulate: invalid --view-quat value '%s'; "
			  "expected x,y,z,w\n", view_quat_string);
	    return BRLCAD_ERROR;
	}
	anim_opts.has_view_quat = true;
	for (int i = 0; i < 4; ++i) anim_opts.view_quat[i] = vals[i];
    }

    if (*view_ae_string) {
	double vals[2] = {225.0, 35.0};
	if (!parse_doubles(view_ae_string, vals, 2)) {
	    bu_vls_sprintf(gedp->ged_result_str,
			  "simulate: invalid --view-ae value '%s'; "
			  "expected az,el\n", view_ae_string);
	    return BRLCAD_ERROR;
	}
	anim_opts.has_view_ae = true;
	anim_opts.view_ae[0] = vals[0];
	anim_opts.view_ae[1] = vals[1];
    }

    if (*view_eye_string) {
	double vals[3] = {0, 0, 0};
	if (!parse_doubles(view_eye_string, vals, 3)) {
	    bu_vls_sprintf(gedp->ged_result_str,
			  "simulate: invalid --view-eye value '%s'; "
			  "expected x,y,z\n", view_eye_string);
	    return BRLCAD_ERROR;
	}
	anim_opts.has_view_eye = true;
	for (int i = 0; i < 3; ++i) anim_opts.view_eye[i] = vals[i];
    }

    if (*view_center_string) {
	double vals[3] = {0, 0, 0};
	if (!parse_doubles(view_center_string, vals, 3)) {
	    bu_vls_sprintf(gedp->ged_result_str,
			  "simulate: invalid --view-center value '%s'; "
			  "expected x,y,z\n", view_center_string);
	    return BRLCAD_ERROR;
	}
	anim_opts.has_view_center = true;
	for (int i = 0; i < 3; ++i) anim_opts.view_center[i] = vals[i];
    }

    if (view_size_val > 0.0) {
	anim_opts.has_view_size = true;
	anim_opts.view_size = view_size_val;
    }

    /* ---------------------------------------------------------------- */
    /* Simulation loop                                                   */
    /* ---------------------------------------------------------------- */

    try {
	const simulate::Simulation::DebugMode debug_mode =
	get_debug_mode(debug_mode_string);

	if (num_steps < 1)
	    throw simulate::InvalidSimulationError("invalid value for 'steps': must be >= 1");

	const fastf_t seconds = simulate::lexical_cast<fastf_t>(argv[2],
								"invalid value for 'seconds'");

	if (seconds < 0.0)
	    throw simulate::InvalidSimulationError("invalid value for 'seconds'");

	db_full_path path;
	const simulate::AutoPtr<db_full_path, db_free_full_path> autofree_path(&path);
	db_full_path_init(&path);

	if (db_string_to_path(&path, gedp->dbip, argv[1]))
	    throw simulate::InvalidSimulationError("invalid path");

	/* Set up animation state if an output file was requested */
	simulate::SimAnimState *anim_state = NULL;
	if (!anim_opts.output_file.empty()) {
	    anim_state = new simulate::SimAnimState(anim_opts, gedp,
						   std::string(argv[1]));
	}

	simulate::Simulation simulation(*gedp->dbip, path, resume_flag != 0);

	const fastf_t step_seconds = seconds / num_steps;

	for (int i = 0; i < num_steps; ++i) {
	    simulation.step(step_seconds, debug_mode);
	    simulation.saveState();

	    if (*plot_prefix_string) {
		std::ostringstream fname;
		fname << plot_prefix_string << "_"
		      << std::setfill('0') << std::setw(4) << (i + 1)
		      << ".pl";
		simulation.writePlotFile(fname.str());
	    }

	    if (anim_state) {
		if (anim_state->renderFrame(i + 1) != BRLCAD_OK)
		    bu_log("simulate: frame %d render failed\n", i + 1);
	    }
	}

	if (anim_state) {
	    anim_state->encodeVideo();
	    delete anim_state;
	    anim_state = NULL;
	}

    } catch (const simulate::InvalidSimulationError &exception) {
	bu_vls_sprintf(gedp->ged_result_str, "%s", exception.what());
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

#endif

#include "../include/plugin.h"

#define GED_SIMULATE_COMMANDS(X, XID) \
    X(simulate,  ged_simulate_core,   GED_CMD_DEFAULT)

GED_DECLARE_COMMAND_SET(GED_SIMULATE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_simulate", 1, GED_SIMULATE_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

