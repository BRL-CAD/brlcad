/*                 G E D _ C O M M A N D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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

#include "bu/cmdschema.h"


struct simulate_args {
    const char *debug_mode;
    int steps;
    const char *plot_prefix;
    long resume;
    const char *output_file;
    int width;
    int height;
    int fps;
    const char *view_quat;
    const char *view_ae;
    const char *view_eye;
    const char *view_center;
    fastf_t view_size;
};

static const struct bu_cmd_option simulate_options[] = {
    BU_CMD_STRING(NULL, "debug", simulate_args, debug_mode, "mode",
	"Set debug mode (for example: aabb,contact,ray)"),
    BU_CMD_POSITIVE_INTEGER(NULL, "steps", simulate_args, steps, "count",
	"Run this many incremental steps"),
    BU_CMD_STRING(NULL, "plot", simulate_args, plot_prefix, "prefix",
	"Write wireframe plot files with this prefix"),
    BU_CMD_COUNTING_LONG_FLAG(NULL, "resume", simulate_args, resume,
	"Resume from previously saved simulation state"),
    BU_CMD_FILE("o", "output", simulate_args, output_file, "file",
	"Write an MJPEG AVI animation"),
    BU_CMD_POSITIVE_INTEGER(NULL, "width", simulate_args, width, "pixels",
	"Animation frame width"),
    BU_CMD_POSITIVE_INTEGER(NULL, "height", simulate_args, height, "pixels",
	"Animation frame height"),
    BU_CMD_POSITIVE_INTEGER(NULL, "fps", simulate_args, fps, "count",
	"Animation frames per second"),
    BU_CMD_STRING(NULL, "view-quat", simulate_args, view_quat, "x,y,z,w",
	"Fixed camera quaternion"),
    BU_CMD_STRING(NULL, "view-ae", simulate_args, view_ae, "az,el",
	"Fixed camera azimuth and elevation"),
    BU_CMD_STRING(NULL, "view-eye", simulate_args, view_eye, "x,y,z",
	"Fixed eye position in model coordinates"),
    BU_CMD_STRING(NULL, "view-center", simulate_args, view_center, "x,y,z",
	"Override view center in model coordinates"),
    BU_CMD_NONNEGATIVE_NUMBER(NULL, "view-size", simulate_args, view_size, "size",
	"View size in mm"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand simulate_operands[] = {
    BU_CMD_OPERAND("path", BU_CMD_VALUE_DB_PATH, 1, 1,
	"Root object path for the simulation", "ged.db_path"),
    BU_CMD_OPERAND_VALIDATE("duration", BU_CMD_VALUE_NUMBER, 1, 1,
	bu_cmd_nonnegative_number_validate, "Simulation duration in seconds", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema simulate_schema = {
    "simulate", "Run a rigid-body simulation", simulate_options, simulate_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


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

#include "ged.h"

#include <iomanip>
#include <sstream>
#include <vector>


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

    struct simulate_args args = {
	"", 1, "", 0, "", 1280, 720, 24, "", "", "", "", 0.0
    };
    std::vector<const char *> av(argv + 1, argv + argc);
    int opt_ret = bu_cmd_schema_parse(&simulate_schema, &args,
	gedp->ged_result_str, (int)av.size(), av.data());
    if (opt_ret < 0 || av.size() - (size_t)opt_ret != 2) {
	char *usage = bu_cmd_schema_describe(&simulate_schema);
	bu_vls_printf(gedp->ged_result_str,
	    "USAGE: %s [OPTIONS] path duration\nOptions:\n%s\n", argv[0],
	    usage ? usage : "");
	if (usage)
	    bu_free(usage, "simulate native schema help");
	return BRLCAD_ERROR;
    }
    const char *path_arg = av[(size_t)opt_ret];
    const char *duration_arg = av[(size_t)opt_ret + 1];

    /* ---------------------------------------------------------------- */
    /* Build animation options struct                                    */
    /* ---------------------------------------------------------------- */
    simulate::SimAnimOptions anim_opts;

    if (*args.output_file)
	anim_opts.output_file = std::string(args.output_file);

    anim_opts.width = args.width;
    anim_opts.height = args.height;
    anim_opts.fps = args.fps;

    if (*args.view_quat) {
	double vals[4] = {0, 0, 0, 1};
	if (!parse_doubles(args.view_quat, vals, 4)) {
	    bu_vls_sprintf(gedp->ged_result_str,
			  "simulate: invalid --view-quat value '%s'; "
			  "expected x,y,z,w\n", args.view_quat);
	    return BRLCAD_ERROR;
	}
	anim_opts.has_view_quat = true;
	for (int i = 0; i < 4; ++i) anim_opts.view_quat[i] = vals[i];
    }

    if (*args.view_ae) {
	double vals[2] = {225.0, 35.0};
	if (!parse_doubles(args.view_ae, vals, 2)) {
	    bu_vls_sprintf(gedp->ged_result_str,
			  "simulate: invalid --view-ae value '%s'; "
			  "expected az,el\n", args.view_ae);
	    return BRLCAD_ERROR;
	}
	anim_opts.has_view_ae = true;
	anim_opts.view_ae[0] = vals[0];
	anim_opts.view_ae[1] = vals[1];
    }

    if (*args.view_eye) {
	double vals[3] = {0, 0, 0};
	if (!parse_doubles(args.view_eye, vals, 3)) {
	    bu_vls_sprintf(gedp->ged_result_str,
			  "simulate: invalid --view-eye value '%s'; "
			  "expected x,y,z\n", args.view_eye);
	    return BRLCAD_ERROR;
	}
	anim_opts.has_view_eye = true;
	for (int i = 0; i < 3; ++i) anim_opts.view_eye[i] = vals[i];
    }

    if (*args.view_center) {
	double vals[3] = {0, 0, 0};
	if (!parse_doubles(args.view_center, vals, 3)) {
	    bu_vls_sprintf(gedp->ged_result_str,
			  "simulate: invalid --view-center value '%s'; "
			  "expected x,y,z\n", args.view_center);
	    return BRLCAD_ERROR;
	}
	anim_opts.has_view_center = true;
	for (int i = 0; i < 3; ++i) anim_opts.view_center[i] = vals[i];
    }

    if (args.view_size > 0.0) {
	anim_opts.has_view_size = true;
	anim_opts.view_size = args.view_size;
    }

    /* ---------------------------------------------------------------- */
    /* Simulation loop                                                   */
    /* ---------------------------------------------------------------- */

    try {
	const simulate::Simulation::DebugMode debug_mode =
	get_debug_mode(args.debug_mode);

	fastf_t seconds = 0.0;
	if (!bu_cmd_number_from_str(&seconds, duration_arg))
	    throw simulate::InvalidSimulationError("invalid value for 'seconds'");

	db_full_path path;
	const simulate::AutoPtr<db_full_path, db_free_full_path> autofree_path(&path);
	db_full_path_init(&path);

	if (db_string_to_path(&path, gedp->dbip, path_arg))
	    throw simulate::InvalidSimulationError("invalid path");

	/* Set up animation state if an output file was requested */
	simulate::SimAnimState *anim_state = NULL;
	if (!anim_opts.output_file.empty()) {
	    anim_state = new simulate::SimAnimState(anim_opts, gedp,
						   std::string(path_arg));
	}

	simulate::Simulation simulation(*gedp->dbip, path, args.resume != 0);

	const fastf_t step_seconds = seconds / args.steps;

	for (int i = 0; i < args.steps; ++i) {
	    simulation.step(step_seconds, debug_mode);
	    simulation.saveState();

	    if (*args.plot_prefix) {
		std::ostringstream fname;
		fname << args.plot_prefix << "_"
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

#define GED_SIMULATE_COMMANDS(N, NID) \
    N(simulate, ged_simulate_core, GED_CMD_DEFAULT, &simulate_schema)

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_SIMULATE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_simulate", 1, GED_SIMULATE_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
