/*                    P R O P E L L E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file shapes/propeller/propeller.cpp
 *
 * Procedural Wageningen B-Series style marine propeller generator.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "brep/edit.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/db_attr.h"
#include "wdb.h"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif


struct point3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};


enum class shaft_standard {
    iso_1_10,
    sae_1_16
};


enum class casing_profile {
    cylindrical,
    truncated_cone,
    convex_barrel
};


enum class output_mode {
    bot,
    brep
};


struct propeller_options {
    std::string outfile = "propeller.g";
    std::string demo_file;
    std::string name = "propeller";
    output_mode mode = output_mode::bot;
    double diameter = 1000.0;
    int blades = 4;
    double expanded_area_ratio = 0.65;
    double pitch_diameter_tip = 1.0;
    double rake_angle_deg = 0.0;
    double skew_max_deg = 0.0;
    double hub_diameter_ratio = 0.20;
    double hub_length_ratio = 0.22;
    double root_overlap_ratio = 0.01;
    double blade_edge_thickness_ratio = 0.00075;
    casing_profile hub_profile = casing_profile::truncated_cone;
    shaft_standard shaft_taper = shaft_standard::iso_1_10;
    double hub_forward_diameter = 0.0;
    double hub_aft_diameter = 0.0;
    double hub_barrel_swell_ratio = 1.08;
    double shaft_diameter_small = 0.0;
    double keyway_width = 0.0;
    double keyway_depth = 0.0;
    bool include_fairing_cap = true;
    double fairing_cap_length = 0.0;
    int hub_longitudinal_slices = 30;
    int hub_angular_segments = 72;
    int radial_stations = 20;
    int chord_points = 50;
    double offset_x = 0.0;
    double offset_y = 0.0;
    double offset_z = 0.0;
    bool overwrite = false;
    bool append = false;
};


struct wageningen_ordinate {
    double chord_fraction;
    double back_factor;
    double face_factor;
};


struct radial_station_profile {
    double r_R;
    std::vector<wageningen_ordinate> points;
};


struct blade_section {
    double r_R = 0.0;
    std::vector<point3d> upper;
    std::vector<point3d> lower;
    std::vector<point3d> outline;
};


struct bot_mesh {
    std::vector<fastf_t> vertices;
    std::vector<int> faces;
    size_t smooth_face_count = 0;
};


struct brep_builder {
    ON_Brep *brep = nullptr;
    std::vector<ON_3dPoint> points;
    std::map<std::pair<int, int>, int> edges;
};


static const std::vector<radial_station_profile> WAGENINGEN_BASE_MATRIX = {
    {0.20, {
	{-0.50, 0.0000, 0.0000}, {-0.45, 0.2110, -0.0150}, {-0.40, 0.3560, -0.0220},
	{-0.30, 0.5130, -0.0180}, {-0.20, 0.5220, 0.0000}, {0.00, 0.4620, 0.0000},
	{0.20, 0.3765, 0.0000}, {0.40, 0.2306, 0.0000}, {0.45, 0.1467, 0.0000},
	{0.50, 0.0000, 0.0000}
    }},
    {0.40, {
	{-0.50, 0.0000, 0.0000}, {-0.45, 0.1980, -0.0080}, {-0.40, 0.3340, -0.0110},
	{-0.30, 0.4980, -0.0050}, {-0.15, 0.5150, 0.0000}, {0.00, 0.4710, 0.0000},
	{0.20, 0.3620, 0.0000}, {0.40, 0.2110, 0.0000}, {0.45, 0.1220, 0.0000},
	{0.50, 0.0000, 0.0000}
    }},
    {0.60, {
	{-0.50, 0.0000, 0.0000}, {-0.45, 0.1760, 0.0000}, {-0.40, 0.2980, 0.0000},
	{-0.30, 0.4520, 0.0000}, {-0.10, 0.5020, 0.0000}, {0.00, 0.4800, 0.0000},
	{0.20, 0.3420, 0.0000}, {0.40, 0.1850, 0.0000}, {0.45, 0.0980, 0.0000},
	{0.50, 0.0000, 0.0000}
    }},
    {0.80, {
	{-0.50, 0.0000, 0.0000}, {-0.40, 0.2410, 0.0000}, {-0.30, 0.3850, 0.0000},
	{-0.20, 0.4620, 0.0000}, {0.00, 0.5000, 0.0000}, {0.20, 0.4410, 0.0000},
	{0.30, 0.3620, 0.0000}, {0.40, 0.1650, 0.0000}, {0.45, 0.0820, 0.0000},
	{0.50, 0.0000, 0.0000}
    }},
    {1.00, {
	{-0.50, 0.0000, 0.0000}, {0.50, 0.0000, 0.0000}
    }}
};


static void
usage(const char *argv0)
{
    bu_log("Usage: %s [options]\n\n", argv0);
    bu_log("Options:\n");
    bu_log("  -o, --output file.g          Output database [propeller.g]\n");
    bu_log("  -n, --name name              Object name prefix [propeller]\n");
    bu_log("      --demo-file file.g       Write a grid of varied sample propellers\n");
    bu_log("      --mode name              Blade output mode: bot or brep [bot]\n");
    bu_log("  -D, --diameter length        Propeller diameter [1000]\n");
    bu_log("  -Z, --blades count           Blade count, 2 through 7 [4]\n");
    bu_log("      --ae-ao ratio            Expanded blade area ratio, 0.30 through 1.20 [0.65]\n");
    bu_log("      --pd-tip ratio           Tip pitch/diameter ratio [1.0]\n");
    bu_log("      --rake degrees           Rake angle in degrees [0]\n");
    bu_log("      --skew degrees           Maximum skew sweep in degrees [0]\n");
    bu_log("      --hub-diameter-ratio r   Hub diameter / propeller diameter [0.20]\n");
    bu_log("      --hub-length-ratio r     Hub length / propeller diameter [0.22]\n");
    bu_log("      --hub-profile name        cylindrical, truncated-cone, or convex-barrel [truncated-cone]\n");
    bu_log("      --hub-forward-diameter d  Forward casing diameter [derived]\n");
    bu_log("      --hub-aft-diameter d      Aft casing diameter [derived]\n");
    bu_log("      --hub-barrel-swell r      Convex barrel swell ratio [1.08]\n");
    bu_log("      --shaft-standard name     iso-1-10 or sae-1-16 [iso-1-10]\n");
    bu_log("      --shaft-diameter-small d  Small-end shaft bore diameter [derived]\n");
    bu_log("      --keyway-width d          Keyway slot width [derived]\n");
    bu_log("      --keyway-depth d          Keyway depth into hub [derived]\n");
    bu_log("      --fairing-cap-length d    Aft fairing cap length [derived]\n");
    bu_log("      --no-fairing-cap          Disable aft fairing cap\n");
    bu_log("      --hub-slices count        Hub barrel longitudinal mesh slices [30]\n");
    bu_log("      --hub-segments count      Hub barrel angular mesh segments [72]\n");
    bu_log("      --root-overlap ratio     Root overlap in r/R units [0.01]\n");
    bu_log("      --edge-thickness-ratio r Finite blade edge thickness / diameter [0.00075]\n");
    bu_log("      --radial-stations count  Radial section count excluding the tip apex [20]\n");
    bu_log("      --chord-points count     Chordwise samples per upper/lower grid [50]\n");
    bu_log("      --x-offset distance      Model X offset [0]\n");
    bu_log("      --y-offset distance      Model Y offset [0]\n");
    bu_log("      --z-offset distance      Model Z offset [0]\n");
    bu_log("  -f, --force                  Overwrite output file\n");
    bu_log("  -j, --append                 Append objects to an existing output file\n");
    bu_log("  -h, --help                   Print this help\n");
}


static bool
finite_point(const point3d &p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}


static double
parse_double_arg(const char *name, const char *value)
{
    if (!value)
	throw std::runtime_error(std::string(name) + " requires a value");
    char *end = NULL;
    const double d = std::strtod(value, &end);
    if (!end || *end != '\0' || !std::isfinite(d))
	throw std::runtime_error(std::string(name) + " requires a finite numeric value");
    return d;
}


static int
parse_int_arg(const char *name, const char *value)
{
    if (!value)
	throw std::runtime_error(std::string(name) + " requires a value");
    char *end = NULL;
    const long l = std::strtol(value, &end, 10);
    if (!end || *end != '\0' || l < std::numeric_limits<int>::min() || l > std::numeric_limits<int>::max())
	throw std::runtime_error(std::string(name) + " requires an integer value");
    return static_cast<int>(l);
}


static const char *
next_value(int argc, char **argv, int &i, const char *name)
{
    if (i + 1 >= argc)
	throw std::runtime_error(std::string(name) + " requires a value");
    return argv[++i];
}


static bool
split_long_option(const char *arg, std::string &key, const char *&value)
{
    if (!arg || arg[0] != '-' || arg[1] != '-')
	return false;

    const char *eq = std::strchr(arg + 2, '=');
    if (eq) {
	key.assign(arg + 2, static_cast<size_t>(eq - (arg + 2)));
	value = eq + 1;
    } else {
	key = arg + 2;
	value = NULL;
    }
    return true;
}


static casing_profile
parse_casing_profile(const char *value)
{
    if (!value)
	throw std::runtime_error("--hub-profile requires a value");
    if (BU_STR_EQUAL(value, "cylindrical"))
	return casing_profile::cylindrical;
    if (BU_STR_EQUAL(value, "truncated-cone"))
	return casing_profile::truncated_cone;
    if (BU_STR_EQUAL(value, "convex-barrel"))
	return casing_profile::convex_barrel;
    throw std::runtime_error("--hub-profile must be cylindrical, truncated-cone, or convex-barrel");
}


static shaft_standard
parse_shaft_standard(const char *value)
{
    if (!value)
	throw std::runtime_error("--shaft-standard requires a value");
    if (BU_STR_EQUAL(value, "iso-1-10"))
	return shaft_standard::iso_1_10;
    if (BU_STR_EQUAL(value, "sae-1-16"))
	return shaft_standard::sae_1_16;
    throw std::runtime_error("--shaft-standard must be iso-1-10 or sae-1-16");
}


static output_mode
parse_output_mode(const char *value)
{
    if (!value)
	throw std::runtime_error("--mode requires a value");
    if (BU_STR_EQUAL(value, "bot"))
	return output_mode::bot;
    if (BU_STR_EQUAL(value, "brep"))
	return output_mode::brep;
    throw std::runtime_error("--mode must be bot or brep");
}


static std::string
output_mode_name(output_mode mode)
{
    switch (mode) {
	case output_mode::bot:
	    return "bot";
	case output_mode::brep:
	    return "brep";
    }
    return "unknown";
}


static std::string
casing_profile_name(casing_profile profile)
{
    switch (profile) {
	case casing_profile::cylindrical:
	    return "cylindrical";
	case casing_profile::truncated_cone:
	    return "truncated-cone";
	case casing_profile::convex_barrel:
	    return "convex-barrel";
    }
    return "unknown";
}


static std::string
shaft_standard_name(shaft_standard standard)
{
    switch (standard) {
	case shaft_standard::iso_1_10:
	    return "iso-1-10";
	case shaft_standard::sae_1_16:
	    return "sae-1-16";
    }
    return "unknown";
}


static void
finalize_options(propeller_options &opts)
{
    if (opts.outfile.empty())
	throw std::runtime_error("output file name must not be empty");
    if (!opts.demo_file.empty() && opts.append)
	throw std::runtime_error("--demo-file cannot be combined with append mode");
    if (opts.name.empty())
	throw std::runtime_error("object name must not be empty");
    if (opts.append && opts.overwrite)
	throw std::runtime_error("-j append and -f force are mutually exclusive");
    if (opts.diameter <= 0.0)
	throw std::runtime_error("diameter must be positive");
    if (opts.blades < 2 || opts.blades > 7)
	throw std::runtime_error("blade count must be between 2 and 7");
    if (opts.expanded_area_ratio < 0.30 || opts.expanded_area_ratio > 1.20)
	throw std::runtime_error("expanded area ratio must be between 0.30 and 1.20");
    if (opts.pitch_diameter_tip <= 0.0)
	throw std::runtime_error("tip pitch/diameter ratio must be positive");
    if (opts.hub_diameter_ratio < 0.05 || opts.hub_diameter_ratio >= 0.75)
	throw std::runtime_error("hub diameter ratio must be at least 0.05 and less than 0.75");
    if (opts.hub_length_ratio <= 0.0)
	throw std::runtime_error("hub length ratio must be positive");
    if (opts.hub_barrel_swell_ratio < 1.0)
	throw std::runtime_error("hub barrel swell ratio must be at least 1.0");
    if (opts.hub_forward_diameter < 0.0 || opts.hub_aft_diameter < 0.0 ||
	    opts.shaft_diameter_small < 0.0 || opts.keyway_width < 0.0 ||
	    opts.keyway_depth < 0.0 || opts.fairing_cap_length < 0.0)
	throw std::runtime_error("hub dimensions must be non-negative");
    if (opts.root_overlap_ratio < 0.0 || opts.root_overlap_ratio >= opts.hub_diameter_ratio)
	throw std::runtime_error("root overlap must be non-negative and smaller than the hub diameter ratio");
    if (opts.blade_edge_thickness_ratio <= 0.0 || opts.blade_edge_thickness_ratio > 0.01)
	throw std::runtime_error("edge thickness ratio must be greater than zero and no more than 0.01");
    if (opts.hub_longitudinal_slices < 3)
	throw std::runtime_error("hub slice count must be at least 3");
    if (opts.hub_angular_segments < 12)
	throw std::runtime_error("hub angular segment count must be at least 12");
    if (opts.radial_stations < 4)
	throw std::runtime_error("radial station count must be at least 4");
    if (opts.chord_points < 5)
	throw std::runtime_error("chord point count must be at least 5");
    if (!std::isfinite(opts.offset_x) || !std::isfinite(opts.offset_y) || !std::isfinite(opts.offset_z))
	throw std::runtime_error("model offsets must be finite");

    const double hub_diameter = opts.hub_diameter_ratio * opts.diameter;
    if (opts.hub_forward_diameter <= 0.0)
	opts.hub_forward_diameter = hub_diameter;
    if (opts.hub_aft_diameter <= 0.0)
	opts.hub_aft_diameter = opts.hub_profile == casing_profile::truncated_cone ? 0.88 * hub_diameter : hub_diameter;
    if (opts.hub_profile == casing_profile::cylindrical) {
	opts.hub_forward_diameter = hub_diameter;
	opts.hub_aft_diameter = hub_diameter;
    }
    if (opts.shaft_diameter_small <= 0.0)
	opts.shaft_diameter_small = 0.42 * hub_diameter;
    if (opts.keyway_width <= 0.0)
	opts.keyway_width = 0.22 * opts.shaft_diameter_small;
    if (opts.keyway_depth <= 0.0)
	opts.keyway_depth = 0.08 * opts.shaft_diameter_small;
    if (opts.fairing_cap_length <= 0.0)
	opts.fairing_cap_length = 0.08 * opts.diameter;
    if (opts.shaft_diameter_small >= 0.85 * std::min(opts.hub_forward_diameter, opts.hub_aft_diameter))
	throw std::runtime_error("small-end shaft diameter is too large for the hub casing");
}


static propeller_options
parse_args(int argc, char **argv)
{
    propeller_options opts;

    for (int i = 1; i < argc; ++i) {
	const char *arg = argv[i];
	std::string key;
	const char *value = NULL;

	if (split_long_option(arg, key, value)) {
	    if (key == "help") {
		usage(argv[0]);
		bu_exit(EXIT_SUCCESS, NULL);
	    } else if (key == "output") {
		opts.outfile = value ? value : next_value(argc, argv, i, arg);
	    } else if (key == "demo-file") {
		opts.demo_file = value ? value : next_value(argc, argv, i, arg);
	    } else if (key == "name") {
		opts.name = value ? value : next_value(argc, argv, i, arg);
	    } else if (key == "mode") {
		opts.mode = parse_output_mode(value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "diameter") {
		opts.diameter = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "blades") {
		opts.blades = parse_int_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "ae-ao") {
		opts.expanded_area_ratio = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "pd-tip") {
		opts.pitch_diameter_tip = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "rake") {
		opts.rake_angle_deg = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "skew") {
		opts.skew_max_deg = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "hub-diameter-ratio") {
		opts.hub_diameter_ratio = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "hub-length-ratio") {
		opts.hub_length_ratio = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "hub-profile") {
		opts.hub_profile = parse_casing_profile(value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "hub-forward-diameter") {
		opts.hub_forward_diameter = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "hub-aft-diameter") {
		opts.hub_aft_diameter = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "hub-barrel-swell") {
		opts.hub_barrel_swell_ratio = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "shaft-standard") {
		opts.shaft_taper = parse_shaft_standard(value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "shaft-diameter-small") {
		opts.shaft_diameter_small = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "keyway-width") {
		opts.keyway_width = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "keyway-depth") {
		opts.keyway_depth = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "fairing-cap-length") {
		opts.fairing_cap_length = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "no-fairing-cap") {
		if (value)
		    throw std::runtime_error("--no-fairing-cap does not take a value");
		opts.include_fairing_cap = false;
	    } else if (key == "hub-slices") {
		opts.hub_longitudinal_slices = parse_int_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "hub-segments") {
		opts.hub_angular_segments = parse_int_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "root-overlap") {
		opts.root_overlap_ratio = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "edge-thickness-ratio") {
		opts.blade_edge_thickness_ratio = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "radial-stations") {
		opts.radial_stations = parse_int_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "chord-points") {
		opts.chord_points = parse_int_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "x-offset") {
		opts.offset_x = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "y-offset") {
		opts.offset_y = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "z-offset") {
		opts.offset_z = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "force") {
		if (value)
		    throw std::runtime_error("--force does not take a value");
		opts.overwrite = true;
	    } else if (key == "append") {
		if (value)
		    throw std::runtime_error("--append does not take a value");
		opts.append = true;
	    } else {
		throw std::runtime_error(std::string("unknown option: ") + arg);
	    }
	    continue;
	}

	if (BU_STR_EQUAL(arg, "-h")) {
	    usage(argv[0]);
	    bu_exit(EXIT_SUCCESS, NULL);
	} else if (BU_STR_EQUAL(arg, "-o")) {
	    opts.outfile = next_value(argc, argv, i, arg);
	} else if (BU_STR_EQUAL(arg, "-n")) {
	    opts.name = next_value(argc, argv, i, arg);
	} else if (BU_STR_EQUAL(arg, "-D")) {
	    opts.diameter = parse_double_arg(arg, next_value(argc, argv, i, arg));
	} else if (BU_STR_EQUAL(arg, "-Z")) {
	    opts.blades = parse_int_arg(arg, next_value(argc, argv, i, arg));
	} else if (BU_STR_EQUAL(arg, "-f")) {
	    opts.overwrite = true;
	} else if (BU_STR_EQUAL(arg, "-j")) {
	    opts.append = true;
	} else {
	    throw std::runtime_error(std::string("unknown option: ") + arg);
	}
    }

    finalize_options(opts);

    return opts;
}


static struct rt_wdb *
open_output_database(const propeller_options &opts)
{
    const bool exists = bu_file_exists(opts.outfile.c_str(), NULL);
    if (exists && !opts.append && !opts.overwrite) {
	bu_exit(EXIT_FAILURE, "ERROR: %s already exists. Use -f to overwrite or -j to append.\n", opts.outfile.c_str());
    }

    if (exists && opts.append) {
	struct db_i *dbip = db_open(opts.outfile.c_str(), DB_OPEN_READWRITE);
	if (!dbip)
	    bu_exit(EXIT_FAILURE, "ERROR: unable to open %s for append\n", opts.outfile.c_str());
	if (db_dirbuild(dbip) < 0) {
	    db_close(dbip);
	    bu_exit(EXIT_FAILURE, "ERROR: unable to read directory from %s\n", opts.outfile.c_str());
	}
	struct rt_wdb *fp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
	if (!fp) {
	    db_close(dbip);
	    bu_exit(EXIT_FAILURE, "ERROR: unable to attach writer to %s\n", opts.outfile.c_str());
	}
	return fp;
    }

    struct rt_wdb *fp = wdb_fopen(opts.outfile.c_str());
    if (!fp)
	bu_exit(EXIT_FAILURE, "ERROR: unable to open %s\n", opts.outfile.c_str());

    mk_id(fp, "Procedural marine propeller");
    return fp;
}


static std::string
num_string(double d)
{
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "%.17g", d);
    return std::string(buf);
}


static void
set_attr(struct rt_wdb *fp, const std::string &obj_name, const char *key, const std::string &value)
{
    if (db5_update_attribute(obj_name.c_str(), key, value.c_str(), fp->dbip) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to set %s attribute on %s\n", key, obj_name.c_str());
}


static void
write_metadata(struct rt_wdb *fp, const std::vector<std::string> &objects, const propeller_options &opts)
{
    for (const std::string &obj_name : objects) {
	set_attr(fp, obj_name, "propeller::generator", "propeller");
	set_attr(fp, obj_name, "propeller::metadata_version", "1");
	set_attr(fp, obj_name, "propeller::geometry_mode", output_mode_name(opts.mode));
	set_attr(fp, obj_name, "propeller::diameter", num_string(opts.diameter));
	set_attr(fp, obj_name, "propeller::blades", num_string(opts.blades));
	set_attr(fp, obj_name, "propeller::ae_ao", num_string(opts.expanded_area_ratio));
	set_attr(fp, obj_name, "propeller::pd_tip", num_string(opts.pitch_diameter_tip));
	set_attr(fp, obj_name, "propeller::rake_angle_deg", num_string(opts.rake_angle_deg));
	set_attr(fp, obj_name, "propeller::skew_max_deg", num_string(opts.skew_max_deg));
	set_attr(fp, obj_name, "propeller::hub_diameter_ratio", num_string(opts.hub_diameter_ratio));
	set_attr(fp, obj_name, "propeller::hub_length_ratio", num_string(opts.hub_length_ratio));
	set_attr(fp, obj_name, "propeller::hub_profile", casing_profile_name(opts.hub_profile));
	set_attr(fp, obj_name, "propeller::hub_forward_diameter", num_string(opts.hub_forward_diameter));
	set_attr(fp, obj_name, "propeller::hub_aft_diameter", num_string(opts.hub_aft_diameter));
	set_attr(fp, obj_name, "propeller::hub_barrel_swell_ratio", num_string(opts.hub_barrel_swell_ratio));
	set_attr(fp, obj_name, "propeller::shaft_standard", shaft_standard_name(opts.shaft_taper));
	set_attr(fp, obj_name, "propeller::shaft_diameter_small", num_string(opts.shaft_diameter_small));
	set_attr(fp, obj_name, "propeller::keyway_width", num_string(opts.keyway_width));
	set_attr(fp, obj_name, "propeller::keyway_depth", num_string(opts.keyway_depth));
	set_attr(fp, obj_name, "propeller::include_fairing_cap", opts.include_fairing_cap ? "true" : "false");
	set_attr(fp, obj_name, "propeller::fairing_cap_length", num_string(opts.fairing_cap_length));
	set_attr(fp, obj_name, "propeller::hub_longitudinal_slices", num_string(opts.hub_longitudinal_slices));
	set_attr(fp, obj_name, "propeller::hub_angular_segments", num_string(opts.hub_angular_segments));
	set_attr(fp, obj_name, "propeller::root_overlap_ratio", num_string(opts.root_overlap_ratio));
	set_attr(fp, obj_name, "propeller::blade_edge_thickness_ratio", num_string(opts.blade_edge_thickness_ratio));
	set_attr(fp, obj_name, "propeller::radial_stations", num_string(opts.radial_stations));
	set_attr(fp, obj_name, "propeller::chord_points", num_string(opts.chord_points));
	set_attr(fp, obj_name, "propeller::x_offset", num_string(opts.offset_x));
	set_attr(fp, obj_name, "propeller::y_offset", num_string(opts.offset_y));
	set_attr(fp, obj_name, "propeller::z_offset", num_string(opts.offset_z));
    }
}


static std::pair<size_t, size_t>
bracket_profile(double target_r_R)
{
    if (target_r_R <= WAGENINGEN_BASE_MATRIX.front().r_R)
	return std::make_pair(0, 0);
    if (target_r_R >= WAGENINGEN_BASE_MATRIX.back().r_R)
	return std::make_pair(WAGENINGEN_BASE_MATRIX.size() - 1, WAGENINGEN_BASE_MATRIX.size() - 1);

    for (size_t i = 0; i + 1 < WAGENINGEN_BASE_MATRIX.size(); ++i) {
	if (target_r_R >= WAGENINGEN_BASE_MATRIX[i].r_R && target_r_R <= WAGENINGEN_BASE_MATRIX[i + 1].r_R)
	    return std::make_pair(i, i + 1);
    }

    return std::make_pair(WAGENINGEN_BASE_MATRIX.size() - 1, WAGENINGEN_BASE_MATRIX.size() - 1);
}


static double
interpolate_profile_factor(const radial_station_profile &profile, double target_v_c, bool is_upper)
{
    const std::vector<wageningen_ordinate> &points = profile.points;
    if (points.empty())
	throw std::runtime_error("invalid Wageningen ordinate profile");
    if (target_v_c <= points.front().chord_fraction)
	return is_upper ? points.front().back_factor : points.front().face_factor;
    if (target_v_c >= points.back().chord_fraction)
	return is_upper ? points.back().back_factor : points.back().face_factor;

    for (size_t i = 0; i + 1 < points.size(); ++i) {
	const wageningen_ordinate &left = points[i];
	const wageningen_ordinate &right = points[i + 1];
	if (target_v_c >= left.chord_fraction && target_v_c <= right.chord_fraction) {
	    const double span = right.chord_fraction - left.chord_fraction;
	    if (span <= 0.0)
		throw std::runtime_error("invalid Wageningen chord fraction ordering");
	    const double t = (target_v_c - left.chord_fraction) / span;
	    const double lval = is_upper ? left.back_factor : left.face_factor;
	    const double rval = is_upper ? right.back_factor : right.face_factor;
	    return lval + t * (rval - lval);
	}
    }

    throw std::runtime_error("unable to bracket Wageningen chord fraction");
}


static double
interpolate_wageningen_factor(double target_r_R, double target_v_c, bool is_upper)
{
    target_r_R = std::max(WAGENINGEN_BASE_MATRIX.front().r_R, std::min(WAGENINGEN_BASE_MATRIX.back().r_R, target_r_R));
    target_v_c = std::max(-0.50, std::min(0.50, target_v_c));

    const std::pair<size_t, size_t> bracket = bracket_profile(target_r_R);
    const size_t lo = bracket.first;
    const size_t hi = bracket.second;
    if (lo == hi)
	return interpolate_profile_factor(WAGENINGEN_BASE_MATRIX[lo], target_v_c, is_upper);

    const double f_lo = interpolate_profile_factor(WAGENINGEN_BASE_MATRIX[lo], target_v_c, is_upper);
    const double f_hi = interpolate_profile_factor(WAGENINGEN_BASE_MATRIX[hi], target_v_c, is_upper);
    const double r_lo = WAGENINGEN_BASE_MATRIX[lo].r_R;
    const double r_hi = WAGENINGEN_BASE_MATRIX[hi].r_R;
    const double t = (target_r_R - r_lo) / (r_hi - r_lo);
    return f_lo + t * (f_hi - f_lo);
}


static void
get_chord_coefficients(double r_R, double &A, double &B)
{
    struct coeff_node {
	double r_R_val;
	double a_val;
	double b_val;
    };
    static const coeff_node table[] = {
	{0.2, 0.0520, 0.6200}, {0.3, 0.0630, 0.7300}, {0.4, 0.0730, 0.8350},
	{0.5, 0.0810, 0.9250}, {0.6, 0.0860, 0.9950}, {0.7, 0.0870, 1.0300},
	{0.8, 0.0820, 1.0250}, {0.9, 0.0670, 0.9500}, {1.0, 0.0000, 0.0000}
    };

    r_R = std::max(table[0].r_R_val, std::min(table[8].r_R_val, r_R));
    if (r_R <= table[0].r_R_val) {
	A = table[0].a_val;
	B = table[0].b_val;
	return;
    }

    for (size_t i = 0; i < 8; ++i) {
	if (r_R >= table[i].r_R_val && r_R <= table[i + 1].r_R_val) {
	    const double t = (r_R - table[i].r_R_val) / (table[i + 1].r_R_val - table[i].r_R_val);
	    A = table[i].a_val + t * (table[i + 1].a_val - table[i].a_val);
	    B = table[i].b_val + t * (table[i + 1].b_val - table[i].b_val);
	    return;
	}
    }

    A = table[8].a_val;
    B = table[8].b_val;
}


static double
chord_length(const propeller_options &opts, double r_R)
{
    double A = 0.0;
    double B = 0.0;
    get_chord_coefficients(r_R, A, B);
    return ((A + B * opts.expanded_area_ratio) * opts.diameter) / static_cast<double>(opts.blades);
}


static double
max_thickness(const propeller_options &opts, double r_R)
{
    const double t0_D = 0.0035 + 0.0015 * static_cast<double>(opts.blades);
    return opts.diameter * (t0_D - (t0_D - 0.0035) * r_R);
}


static point3d
propeller_point(const propeller_options &opts, double r_R, double v_c, bool is_upper, double blade_angle)
{
    const double radius = 0.5 * opts.diameter * r_R;
    const double chord = chord_length(opts, r_R);
    const double thickness = max_thickness(opts, r_R);
    const double v = v_c * chord;
    const double factor = interpolate_wageningen_factor(r_R, v_c, is_upper);
    const double edge_pos = std::min(1.0, std::fabs(2.0 * v_c));
    const double edge_weight = edge_pos * edge_pos * edge_pos * edge_pos;
    const double edge_offset = 0.5 * opts.blade_edge_thickness_ratio * opts.diameter * edge_weight;
    const double ordinate = factor * thickness + (is_upper ? edge_offset : -edge_offset);
    const double phi = std::atan((opts.pitch_diameter_tip * opts.diameter) / (2.0 * M_PI * radius));
    const double skew_frac = std::max(0.0, (r_R - opts.hub_diameter_ratio) / (1.0 - opts.hub_diameter_ratio));
    const double theta_s = (opts.skew_max_deg * M_PI / 180.0) * skew_frac * skew_frac;
    const double rake = radius * std::tan(opts.rake_angle_deg * M_PI / 180.0);
    const double theta_local = (v * std::cos(phi)) / radius + theta_s + blade_angle;

    point3d p;
    p.x = opts.offset_x + radius * std::cos(theta_local);
    p.y = opts.offset_y + radius * std::sin(theta_local);
    p.z = opts.offset_z + v * std::sin(phi) + ordinate * std::cos(phi) + rake;
    return p;
}


static point3d
tip_apex(const propeller_options &opts, double blade_angle)
{
    const double radius = 0.5 * opts.diameter;
    const double theta_s = opts.skew_max_deg * M_PI / 180.0;
    const double rake = radius * std::tan(opts.rake_angle_deg * M_PI / 180.0);

    point3d p;
    p.x = opts.offset_x + radius * std::cos(theta_s + blade_angle);
    p.y = opts.offset_y + radius * std::sin(theta_s + blade_angle);
    p.z = opts.offset_z + rake;
    return p;
}


static std::vector<double>
radial_distribution(const propeller_options &opts)
{
    std::vector<double> radii;
    radii.reserve(static_cast<size_t>(opts.radial_stations));

    const double hub_r_R = opts.hub_diameter_ratio;
    const double inner_r_R = std::max(0.02, hub_r_R - opts.root_overlap_ratio);
    const double last_r_R = 0.985;

    radii.push_back(inner_r_R);
    radii.push_back(hub_r_R);

    const int remaining = opts.radial_stations - 2;
    for (int i = 1; i <= remaining; ++i) {
	const double t = static_cast<double>(i) / static_cast<double>(remaining);
	radii.push_back(hub_r_R + (last_r_R - hub_r_R) * t);
    }

    std::sort(radii.begin(), radii.end());
    radii.erase(std::unique(radii.begin(), radii.end(), [](double a, double b) {
	return std::fabs(a - b) < 1.0e-10;
    }), radii.end());

    if (radii.size() < 3)
	throw std::runtime_error("internal radial distribution failed");

    return radii;
}


static std::vector<blade_section>
build_blade_sections(const propeller_options &opts, double blade_angle)
{
    const std::vector<double> radii = radial_distribution(opts);
    std::vector<blade_section> sections;
    sections.reserve(radii.size());

    for (double r_R : radii) {
	blade_section section;
	section.r_R = r_R;
	section.upper.reserve(static_cast<size_t>(opts.chord_points));
	section.lower.reserve(static_cast<size_t>(opts.chord_points));

	for (int c = 0; c < opts.chord_points; ++c) {
	    const double t = static_cast<double>(c) / static_cast<double>(opts.chord_points - 1);
	    const double v_c = -0.5 + t;
	    section.upper.push_back(propeller_point(opts, r_R, v_c, true, blade_angle));
	    section.lower.push_back(propeller_point(opts, r_R, v_c, false, blade_angle));
	}

	section.outline.reserve(static_cast<size_t>(2 * opts.chord_points));
	for (int c = 0; c < opts.chord_points; ++c)
	    section.outline.push_back(section.upper[static_cast<size_t>(c)]);
	for (int c = opts.chord_points - 1; c >= 0; --c)
	    section.outline.push_back(section.lower[static_cast<size_t>(c)]);

	sections.push_back(section);
    }

    return sections;
}


static void
append_vertex(bot_mesh &mesh, const point3d &p)
{
    if (!finite_point(p))
	throw std::runtime_error("generated non-finite blade coordinate");
    mesh.vertices.push_back(static_cast<fastf_t>(p.x));
    mesh.vertices.push_back(static_cast<fastf_t>(p.y));
    mesh.vertices.push_back(static_cast<fastf_t>(p.z));
}


static void
add_face(bot_mesh &mesh, int a, int b, int c)
{
    mesh.faces.push_back(a);
    mesh.faces.push_back(b);
    mesh.faces.push_back(c);
}


static point3d
mesh_point(const bot_mesh &mesh, int index)
{
    const size_t offset = static_cast<size_t>(index) * 3;
    point3d p;
    p.x = static_cast<double>(mesh.vertices[offset]);
    p.y = static_cast<double>(mesh.vertices[offset + 1]);
    p.z = static_cast<double>(mesh.vertices[offset + 2]);
    return p;
}


static point3d
sub(const point3d &a, const point3d &b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}


static point3d
cross(const point3d &a, const point3d &b)
{
    return {
	a.y * b.z - a.z * b.y,
	a.z * b.x - a.x * b.z,
	a.x * b.y - a.y * b.x
    };
}


static double
dot(const point3d &a, const point3d &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}


static double
triangle_area2(const point3d &a, const point3d &b, const point3d &c)
{
    const point3d n = cross(sub(b, a), sub(c, a));
    return dot(n, n);
}


static point3d
unit_point(const point3d &p)
{
    const double len = std::sqrt(dot(p, p));
    if (len <= SMALL_FASTF)
	return {0.0, 0.0, 1.0};
    return {p.x / len, p.y / len, p.z / len};
}


static void
append_normal(std::vector<fastf_t> &normals, const point3d &normal)
{
    normals.push_back(static_cast<fastf_t>(normal.x));
    normals.push_back(static_cast<fastf_t>(normal.y));
    normals.push_back(static_cast<fastf_t>(normal.z));
}


static point3d
face_normal(const std::vector<fastf_t> &vertices, int a, int b, int c)
{
    const point3d pa = {
	static_cast<double>(vertices[static_cast<size_t>(a) * 3]),
	static_cast<double>(vertices[static_cast<size_t>(a) * 3 + 1]),
	static_cast<double>(vertices[static_cast<size_t>(a) * 3 + 2])
    };
    const point3d pb = {
	static_cast<double>(vertices[static_cast<size_t>(b) * 3]),
	static_cast<double>(vertices[static_cast<size_t>(b) * 3 + 1]),
	static_cast<double>(vertices[static_cast<size_t>(b) * 3 + 2])
    };
    const point3d pc = {
	static_cast<double>(vertices[static_cast<size_t>(c) * 3]),
	static_cast<double>(vertices[static_cast<size_t>(c) * 3 + 1]),
	static_cast<double>(vertices[static_cast<size_t>(c) * 3 + 2])
    };
    return unit_point(cross(sub(pb, pa), sub(pc, pa)));
}


static void
build_bot_normals(const bot_mesh &mesh, std::vector<fastf_t> &normals, std::vector<int> &face_normals)
{
    const size_t vertex_count = mesh.vertices.size() / 3;
    const size_t face_count = mesh.faces.size() / 3;
    const size_t smooth_face_count = std::min(mesh.smooth_face_count, face_count);
    std::vector<point3d> accum(vertex_count);

    for (size_t i = 0; i < smooth_face_count; ++i) {
	const int a = mesh.faces[3 * i];
	const int b = mesh.faces[3 * i + 1];
	const int c = mesh.faces[3 * i + 2];
	const point3d n = face_normal(mesh.vertices, a, b, c);
	const int vids[3] = {a, b, c};
	for (int vi : vids) {
	    point3d &sum = accum[static_cast<size_t>(vi)];
	    sum.x += n.x;
	    sum.y += n.y;
	    sum.z += n.z;
	}
    }

    normals.reserve(vertex_count * 3 + (face_count - smooth_face_count) * 3);
    for (const point3d &normal : accum)
	append_normal(normals, unit_point(normal));

    face_normals.resize(mesh.faces.size());
    for (size_t i = 0; i < face_count; ++i) {
	if (i < smooth_face_count) {
	    face_normals[3 * i] = mesh.faces[3 * i];
	    face_normals[3 * i + 1] = mesh.faces[3 * i + 1];
	    face_normals[3 * i + 2] = mesh.faces[3 * i + 2];
	} else {
	    const int ni = static_cast<int>(normals.size() / 3);
	    append_normal(normals, face_normal(mesh.vertices, mesh.faces[3 * i], mesh.faces[3 * i + 1], mesh.faces[3 * i + 2]));
	    face_normals[3 * i] = ni;
	    face_normals[3 * i + 1] = ni;
	    face_normals[3 * i + 2] = ni;
	}
    }
}


static void
validate_mesh_arrays(const bot_mesh &mesh)
{
    if (mesh.vertices.empty() || mesh.faces.empty() || mesh.vertices.size() % 3 != 0 || mesh.faces.size() % 3 != 0)
	throw std::runtime_error("internal BoT mesh assembly produced malformed arrays");

    const int vertex_count = static_cast<int>(mesh.vertices.size() / 3);
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
	if (!std::isfinite(static_cast<double>(mesh.vertices[i])))
	    throw std::runtime_error("internal BoT mesh assembly produced non-finite coordinates");
    }

    std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> edge_uses;
    for (size_t i = 0; i < mesh.faces.size(); i += 3) {
	const int a = mesh.faces[i];
	const int b = mesh.faces[i + 1];
	const int c = mesh.faces[i + 2];
	if (a < 0 || b < 0 || c < 0 || a >= vertex_count || b >= vertex_count || c >= vertex_count)
	    throw std::runtime_error("internal BoT mesh assembly produced an invalid face index");
	if (a == b || b == c || a == c)
	    throw std::runtime_error("internal BoT mesh assembly produced a degenerate face index");
	const point3d pa = mesh_point(mesh, a);
	const point3d pb = mesh_point(mesh, b);
	const point3d pc = mesh_point(mesh, c);
	if (triangle_area2(pa, pb, pc) <= 1.0e-18)
	    throw std::runtime_error("internal BoT mesh assembly produced a degenerate triangle");

	const int e[3][2] = {{a, b}, {b, c}, {c, a}};
	for (const auto &edge : e) {
	    const std::pair<int, int> key = std::minmax(edge[0], edge[1]);
	    edge_uses[key].push_back(std::make_pair(edge[0], edge[1]));
	}
    }

    for (const auto &entry : edge_uses) {
	const std::vector<std::pair<int, int>> &uses = entry.second;
	if (uses.size() != 2)
	    throw std::runtime_error("internal BoT mesh assembly produced a non-manifold or boundary edge");
	if (uses[0].first == uses[1].first && uses[0].second == uses[1].second)
	    throw std::runtime_error("internal BoT mesh assembly produced inconsistent triangle winding");
    }
}


static double
signed_volume(const bot_mesh &mesh)
{
    double volume = 0.0;
    for (size_t i = 0; i < mesh.faces.size(); i += 3) {
	const point3d a = mesh_point(mesh, mesh.faces[i]);
	const point3d b = mesh_point(mesh, mesh.faces[i + 1]);
	const point3d c = mesh_point(mesh, mesh.faces[i + 2]);
	volume += dot(a, cross(b, c)) / 6.0;
    }
    return volume;
}


static void
orient_mesh_outward(bot_mesh &mesh)
{
    if (signed_volume(mesh) < 0.0) {
	for (size_t i = 0; i < mesh.faces.size(); i += 3)
	    std::swap(mesh.faces[i + 1], mesh.faces[i + 2]);
    }
}


static bot_mesh
build_blade_mesh(const propeller_options &opts, double blade_angle)
{
    const std::vector<blade_section> sections = build_blade_sections(opts, blade_angle);
    const int section_count = static_cast<int>(sections.size());
    const int outline_count = static_cast<int>(sections.front().outline.size());
    const point3d apex = tip_apex(opts, blade_angle);

    bot_mesh mesh;
    mesh.vertices.reserve(static_cast<size_t>(section_count * outline_count + 2) * 3);
    mesh.faces.reserve(static_cast<size_t>((section_count - 1) * outline_count * 2 + outline_count * 2) * 3);

    for (const blade_section &section : sections) {
	if (static_cast<int>(section.outline.size()) != outline_count)
	    throw std::runtime_error("internal blade outline topology mismatch");
	for (const point3d &p : section.outline)
	    append_vertex(mesh, p);
    }

    for (int s = 0; s < section_count - 1; ++s) {
	for (int k = 0; k < outline_count; ++k) {
	    const int kn = (k + 1) % outline_count;
	    const int a = s * outline_count + k;
	    const int b = s * outline_count + kn;
	    const int c = (s + 1) * outline_count + kn;
	    const int d = (s + 1) * outline_count + k;
	    add_face(mesh, a, d, c);
	    add_face(mesh, a, c, b);
	}
    }
    mesh.smooth_face_count = mesh.faces.size() / 3;

    point3d root_center;
    for (int k = 0; k < outline_count; ++k) {
	const point3d p = mesh_point(mesh, k);
	root_center.x += p.x;
	root_center.y += p.y;
	root_center.z += p.z;
    }
    root_center.x /= static_cast<double>(outline_count);
    root_center.y /= static_cast<double>(outline_count);
    root_center.z /= static_cast<double>(outline_count);

    const int root_center_idx = static_cast<int>(mesh.vertices.size() / 3);
    append_vertex(mesh, root_center);
    for (int k = 0; k < outline_count; ++k) {
	const int kn = (k + 1) % outline_count;
	add_face(mesh, root_center_idx, k, kn);
    }

    const int apex_idx = static_cast<int>(mesh.vertices.size() / 3);
    append_vertex(mesh, apex);
    const int last_offset = (section_count - 1) * outline_count;
    for (int k = 0; k < outline_count; ++k) {
	const int kn = (k + 1) % outline_count;
	add_face(mesh, last_offset + kn, last_offset + k, apex_idx);
    }

    validate_mesh_arrays(mesh);
    orient_mesh_outward(mesh);
    validate_mesh_arrays(mesh);
    return mesh;
}


static void
check_output_name_available(struct rt_wdb *fp, const std::string &name)
{
    if (db_lookup(fp->dbip, name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
	bu_exit(EXIT_FAILURE, "ERROR: object %s already exists in output database\n", name.c_str());
}


static void
write_bot_solid(struct rt_wdb *fp, const std::string &solid_name, const bot_mesh &mesh)
{
    check_output_name_available(fp, solid_name);
    std::vector<fastf_t> normals;
    std::vector<int> face_normals;
    build_bot_normals(mesh, normals, face_normals);
    if (mk_bot_w_normals(fp, solid_name.c_str(), RT_BOT_SOLID, RT_BOT_CCW,
	    RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS,
	    mesh.vertices.size() / 3, mesh.faces.size() / 3,
	    mesh.vertices.data(), mesh.faces.data(), NULL, NULL,
	    normals.size() / 3, normals.data(), face_normals.data()) != 0) {
	bu_exit(EXIT_FAILURE, "ERROR: unable to write BoT %s\n", solid_name.c_str());
    }
}


static void
write_blade(struct rt_wdb *fp, const std::string &solid_name, const bot_mesh &mesh)
{
    write_bot_solid(fp, solid_name, mesh);
}


static ON_Curve *
line_curve_3d(const ON_3dPoint &from, const ON_3dPoint &to)
{
    ON_Curve *curve = new ON_LineCurve(from, to);
    curve->SetDomain(0.0, 1.0);
    return curve;
}


static ON_Curve *
line_curve_2d(const ON_2dPoint &from, const ON_2dPoint &to)
{
    ON_Curve *curve = new ON_LineCurve(from, to);
    curve->SetDomain(0.0, 1.0);
    return curve;
}


static ON_Surface *
quad_surface(const ON_3dPoint &sw, const ON_3dPoint &se, const ON_3dPoint &ne, const ON_3dPoint &nw)
{
    ON_NurbsSurface *surface = ON_NurbsSurface::New(3, false, 2, 2, 2, 2);
    surface->SetCV(0, 0, sw);
    surface->SetCV(1, 0, se);
    surface->SetCV(1, 1, ne);
    surface->SetCV(0, 1, nw);
    surface->SetKnot(0, 0, 0.0);
    surface->SetKnot(0, 1, 1.0);
    surface->SetKnot(1, 0, 0.0);
    surface->SetKnot(1, 1, 1.0);
    return surface;
}


static int
grid_vertex_index(int station, int chord, int outline_count)
{
    return station * outline_count + chord;
}


static int
builder_edge(brep_builder &builder, int from, int to)
{
    const std::pair<int, int> key = std::minmax(from, to);
    auto e_it = builder.edges.find(key);
    if (e_it != builder.edges.end())
	return e_it->second;

    const int c3i = builder.brep->m_C3.Count();
    builder.brep->m_C3.Append(line_curve_3d(builder.points[static_cast<size_t>(from)], builder.points[static_cast<size_t>(to)]));
    ON_BrepEdge &edge = builder.brep->NewEdge(builder.brep->m_V[from], builder.brep->m_V[to], c3i);
    edge.m_tolerance = SMALL_FASTF;

    const int ei = edge.m_edge_index;
    builder.edges[key] = ei;
    return ei;
}


static int
builder_edge_with_curve(brep_builder &builder, int from, int to, ON_Curve *curve)
{
    const std::pair<int, int> key = std::minmax(from, to);
    auto e_it = builder.edges.find(key);
    if (e_it != builder.edges.end()) {
	delete curve;
	return e_it->second;
    }

    const int c3i = builder.brep->m_C3.Count();
    builder.brep->m_C3.Append(curve);
    ON_BrepEdge &edge = builder.brep->NewEdge(builder.brep->m_V[from], builder.brep->m_V[to], c3i);
    edge.m_tolerance = SMALL_FASTF;

    const int ei = edge.m_edge_index;
    builder.edges[key] = ei;
    return ei;
}


static ON_3dVector
point_delta(const ON_3dPoint &from, const ON_3dPoint &to)
{
    return to - from;
}


static double
vector_dot(const ON_3dVector &a, const ON_3dVector &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}


static double
vector_length(const ON_3dVector &v)
{
    return std::sqrt(vector_dot(v, v));
}


static ON_3dVector
limited_tangent(const ON_3dVector &tangent, const ON_3dVector &edge)
{
    const double edge_len = vector_length(edge);
    const double tangent_len = vector_length(tangent);
    if (edge_len <= SMALL_FASTF || tangent_len <= SMALL_FASTF)
	return edge;

    const double min_aligned_dot = 0.5 * edge_len * tangent_len;
    if (vector_dot(tangent, edge) < min_aligned_dot)
	return edge;

    const double max_len = 1.25 * edge_len;
    if (tangent_len > max_len)
	return tangent * (max_len / tangent_len);

    return tangent;
}


static ON_NurbsCurve *
cubic_bezier_curve(const ON_3dPoint &from, const ON_3dPoint &to, const ON_3dVector &from_tangent, const ON_3dVector &to_tangent)
{
    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, false, 4, 4);
    curve->SetCV(0, from);
    curve->SetCV(1, from + from_tangent / 3.0);
    curve->SetCV(2, to - to_tangent / 3.0);
    curve->SetCV(3, to);
    curve->SetKnot(0, 0.0);
    curve->SetKnot(1, 0.0);
    curve->SetKnot(2, 0.0);
    curve->SetKnot(3, 1.0);
    curve->SetKnot(4, 1.0);
    curve->SetKnot(5, 1.0);
    curve->SetDomain(0.0, 1.0);
    return curve;
}


static int
builder_smooth_edge(brep_builder &builder, int from, int to, const ON_3dVector &from_tangent, const ON_3dVector &to_tangent)
{
    return builder_edge_with_curve(builder, from, to, cubic_bezier_curve(builder.points[static_cast<size_t>(from)],
	    builder.points[static_cast<size_t>(to)], from_tangent, to_tangent));
}


static bool
edge_reversed(const ON_Brep &brep, int edge_index, int trim_from, int trim_to)
{
    const ON_BrepEdge &edge = brep.m_E[edge_index];
    return !(edge.m_vi[0] == trim_from && edge.m_vi[1] == trim_to);
}


static ON_3dVector
span_tangent(const brep_builder &builder, int station, int chord, int stations, int outline_count)
{
    const int vi = grid_vertex_index(station, chord, outline_count);
    if (stations < 2)
	return ON_3dVector::ZeroVector;
    if (station == 0) {
	const int vn = grid_vertex_index(station + 1, chord, outline_count);
	return point_delta(builder.points[static_cast<size_t>(vi)], builder.points[static_cast<size_t>(vn)]);
    }
    if (station == stations - 1) {
	const int vp = grid_vertex_index(station - 1, chord, outline_count);
	return point_delta(builder.points[static_cast<size_t>(vp)], builder.points[static_cast<size_t>(vi)]);
    }

    const int vp = grid_vertex_index(station - 1, chord, outline_count);
    const int vn = grid_vertex_index(station + 1, chord, outline_count);
    return point_delta(builder.points[static_cast<size_t>(vp)], builder.points[static_cast<size_t>(vn)]) * 0.5;
}


static ON_3dVector
chord_tangent(const brep_builder &builder, int station, int chord, int outline_count)
{
    const int kp = (chord + outline_count - 1) % outline_count;
    const int kn = (chord + 1) % outline_count;
    const int vp = grid_vertex_index(station, kp, outline_count);
    const int vn = grid_vertex_index(station, kn, outline_count);
    return point_delta(builder.points[static_cast<size_t>(vp)], builder.points[static_cast<size_t>(vn)]) * 0.5;
}


static void
add_trim(brep_builder &builder, ON_BrepLoop &loop, int from, int to, const ON_2dPoint &uv_from, const ON_2dPoint &uv_to, ON_Surface::ISO iso)
{
    const int edge_index = builder_edge(builder, from, to);
    const int c2i = builder.brep->m_C2.Count();
    builder.brep->m_C2.Append(line_curve_2d(uv_from, uv_to));

    ON_BrepTrim &trim = builder.brep->NewTrim(builder.brep->m_E[edge_index], edge_reversed(*builder.brep, edge_index, from, to), loop, c2i);
    trim.m_iso = iso;
    trim.m_type = ON_BrepTrim::mated;
    trim.m_tolerance[0] = SMALL_FASTF;
    trim.m_tolerance[1] = SMALL_FASTF;
}


static void
add_trim_for_edge(brep_builder &builder, ON_BrepLoop &loop, int edge_index, int from, int to, const ON_2dPoint &uv_from, const ON_2dPoint &uv_to, ON_Surface::ISO iso)
{
    const int c2i = builder.brep->m_C2.Count();
    builder.brep->m_C2.Append(line_curve_2d(uv_from, uv_to));

    ON_BrepTrim &trim = builder.brep->NewTrim(builder.brep->m_E[edge_index], edge_reversed(*builder.brep, edge_index, from, to), loop, c2i);
    trim.m_iso = iso;
    trim.m_type = ON_BrepTrim::mated;
    trim.m_tolerance[0] = SMALL_FASTF;
    trim.m_tolerance[1] = SMALL_FASTF;
}


static ON_NurbsSurface *
cubic_hermite_surface(const ON_3dPoint &p00, const ON_3dPoint &p10, const ON_3dPoint &p11, const ON_3dPoint &p01,
    const ON_3dVector &du00, const ON_3dVector &du10, const ON_3dVector &du11, const ON_3dVector &du01,
    const ON_3dVector &dv00, const ON_3dVector &dv10, const ON_3dVector &dv11, const ON_3dVector &dv01)
{
    ON_NurbsSurface *surface = ON_NurbsSurface::New(3, false, 4, 4, 4, 4);

    surface->SetCV(0, 0, p00);
    surface->SetCV(1, 0, p00 + du00 / 3.0);
    surface->SetCV(2, 0, p10 - du10 / 3.0);
    surface->SetCV(3, 0, p10);

    surface->SetCV(0, 3, p01);
    surface->SetCV(1, 3, p01 + du01 / 3.0);
    surface->SetCV(2, 3, p11 - du11 / 3.0);
    surface->SetCV(3, 3, p11);

    surface->SetCV(0, 1, p00 + dv00 / 3.0);
    surface->SetCV(0, 2, p01 - dv01 / 3.0);
    surface->SetCV(3, 1, p10 + dv10 / 3.0);
    surface->SetCV(3, 2, p11 - dv11 / 3.0);

    surface->SetCV(1, 1, p00 + du00 / 3.0 + dv00 / 3.0);
    surface->SetCV(2, 1, p10 - du10 / 3.0 + dv10 / 3.0);
    surface->SetCV(1, 2, p01 + du01 / 3.0 - dv01 / 3.0);
    surface->SetCV(2, 2, p11 - du11 / 3.0 - dv11 / 3.0);

    for (int dir = 0; dir < 2; ++dir) {
	surface->SetKnot(dir, 0, 0.0);
	surface->SetKnot(dir, 1, 0.0);
	surface->SetKnot(dir, 2, 0.0);
	surface->SetKnot(dir, 3, 1.0);
	surface->SetKnot(dir, 4, 1.0);
	surface->SetKnot(dir, 5, 1.0);
    }

    return surface;
}


static void
add_smooth_patch_face(brep_builder &builder, int station, int chord, int outline_count, int stations)
{
    const int kn = (chord + 1) % outline_count;
    const int v00 = grid_vertex_index(station, chord, outline_count);
    const int v10 = grid_vertex_index(station + 1, chord, outline_count);
    const int v11 = grid_vertex_index(station + 1, kn, outline_count);
    const int v01 = grid_vertex_index(station, kn, outline_count);

    const ON_3dVector edge_u0 = point_delta(builder.points[static_cast<size_t>(v00)], builder.points[static_cast<size_t>(v10)]);
    const ON_3dVector edge_u1 = point_delta(builder.points[static_cast<size_t>(v01)], builder.points[static_cast<size_t>(v11)]);
    const ON_3dVector edge_v0 = point_delta(builder.points[static_cast<size_t>(v00)], builder.points[static_cast<size_t>(v01)]);
    const ON_3dVector edge_v1 = point_delta(builder.points[static_cast<size_t>(v10)], builder.points[static_cast<size_t>(v11)]);

    const ON_3dVector du00 = limited_tangent(span_tangent(builder, station, chord, stations, outline_count), edge_u0);
    const ON_3dVector du10 = limited_tangent(span_tangent(builder, station + 1, chord, stations, outline_count), edge_u0);
    const ON_3dVector du11 = limited_tangent(span_tangent(builder, station + 1, kn, stations, outline_count), edge_u1);
    const ON_3dVector du01 = limited_tangent(span_tangent(builder, station, kn, stations, outline_count), edge_u1);

    const ON_3dVector dv00 = limited_tangent(chord_tangent(builder, station, chord, outline_count), edge_v0);
    const ON_3dVector dv10 = limited_tangent(chord_tangent(builder, station + 1, chord, outline_count), edge_v1);
    const ON_3dVector dv11 = limited_tangent(chord_tangent(builder, station + 1, kn, outline_count), edge_v1);
    const ON_3dVector dv01 = limited_tangent(chord_tangent(builder, station, kn, outline_count), edge_v0);

    const int e0 = builder_smooth_edge(builder, v00, v10, du00, du10);
    const int e1 = builder_smooth_edge(builder, v10, v11, dv10, dv11);
    const int e2 = builder_smooth_edge(builder, v01, v11, du01, du11);
    const int e3 = builder_smooth_edge(builder, v00, v01, dv00, dv01);

    const int si = builder.brep->m_S.Count();
    builder.brep->m_S.Append(cubic_hermite_surface(
	    builder.points[static_cast<size_t>(v00)],
	    builder.points[static_cast<size_t>(v10)],
	    builder.points[static_cast<size_t>(v11)],
	    builder.points[static_cast<size_t>(v01)],
	    du00, du10, du11, du01,
	    dv00, dv10, dv11, dv01));
    ON_BrepFace &face = builder.brep->NewFace(si);
    ON_BrepLoop &loop = builder.brep->NewLoop(ON_BrepLoop::outer, face);

    add_trim_for_edge(builder, loop, e0, v00, v10, ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 0.0), ON_Surface::S_iso);
    add_trim_for_edge(builder, loop, e1, v10, v11, ON_2dPoint(1.0, 0.0), ON_2dPoint(1.0, 1.0), ON_Surface::E_iso);
    add_trim_for_edge(builder, loop, e2, v11, v01, ON_2dPoint(1.0, 1.0), ON_2dPoint(0.0, 1.0), ON_Surface::N_iso);
    add_trim_for_edge(builder, loop, e3, v01, v00, ON_2dPoint(0.0, 1.0), ON_2dPoint(0.0, 0.0), ON_Surface::W_iso);
}


static void
add_tri_face(brep_builder &builder, int v0, int v1, int v2)
{
    const ON_3dPoint &p0 = builder.points[static_cast<size_t>(v0)];
    const ON_3dPoint &p1 = builder.points[static_cast<size_t>(v1)];
    const ON_3dPoint &p2 = builder.points[static_cast<size_t>(v2)];
    const ON_3dPoint p3 = p0 + (p2 - p1);

    const int si = builder.brep->m_S.Count();
    builder.brep->m_S.Append(quad_surface(p0, p1, p2, p3));
    ON_BrepFace &face = builder.brep->NewFace(si);
    ON_BrepLoop &loop = builder.brep->NewLoop(ON_BrepLoop::outer, face);

    add_trim(builder, loop, v0, v1, ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 0.0), ON_Surface::S_iso);
    add_trim(builder, loop, v1, v2, ON_2dPoint(1.0, 0.0), ON_2dPoint(1.0, 1.0), ON_Surface::E_iso);
    add_trim(builder, loop, v2, v0, ON_2dPoint(1.0, 1.0), ON_2dPoint(0.0, 0.0), ON_Surface::not_iso);
}


static void
write_blade_brep(struct rt_wdb *fp, const propeller_options &opts, const std::string &solid_name, double blade_angle)
{
    check_output_name_available(fp, solid_name);
    const std::vector<blade_section> sections = build_blade_sections(opts, blade_angle);
    const int station_count = static_cast<int>(sections.size());
    const int outline_count = static_cast<int>(sections.front().outline.size());

    ON::Begin();
    brep_builder builder;
    builder.brep = ON_Brep::New();
    builder.points.reserve(static_cast<size_t>(station_count * outline_count + 2));

    for (const blade_section &section : sections) {
	if (static_cast<int>(section.outline.size()) != outline_count) {
	    delete builder.brep;
	    ON::End();
	    throw std::runtime_error("internal blade BREP topology mismatch");
	}
	for (const point3d &p : section.outline)
	    builder.points.push_back(ON_3dPoint(p.x, p.y, p.z));
    }

    point3d root_center;
    for (const point3d &p : sections.front().outline) {
	root_center.x += p.x;
	root_center.y += p.y;
	root_center.z += p.z;
    }
    root_center.x /= static_cast<double>(outline_count);
    root_center.y /= static_cast<double>(outline_count);
    root_center.z /= static_cast<double>(outline_count);

    const int root_center_idx = static_cast<int>(builder.points.size());
    builder.points.push_back(ON_3dPoint(root_center.x, root_center.y, root_center.z));
    const point3d apex = tip_apex(opts, blade_angle);
    const int apex_idx = static_cast<int>(builder.points.size());
    builder.points.push_back(ON_3dPoint(apex.x, apex.y, apex.z));

    for (const ON_3dPoint &p : builder.points) {
	ON_BrepVertex &v = builder.brep->NewVertex(p);
	v.m_tolerance = SMALL_FASTF;
    }

    for (int s = 0; s < station_count - 1; ++s) {
	for (int k = 0; k < outline_count; ++k)
	    add_smooth_patch_face(builder, s, k, outline_count, station_count);
    }

    for (int k = 0; k < outline_count; ++k) {
	const int kn = (k + 1) % outline_count;
	add_tri_face(builder, root_center_idx, k, kn);
    }

    const int last_offset = (station_count - 1) * outline_count;
    for (int k = 0; k < outline_count; ++k) {
	const int kn = (k + 1) % outline_count;
	add_tri_face(builder, last_offset + kn, last_offset + k, apex_idx);
    }

    builder.brep->Standardize();
    builder.brep->Compact();

    ON_TextLog error_log;
    if (!builder.brep->IsValid(&error_log)) {
	error_log.Print("Invalid propeller blade BREP\n");
	delete builder.brep;
	ON::End();
	bu_exit(EXIT_FAILURE, "ERROR: generated blade BREP did not pass OpenNURBS validation\n");
    }

    if (!builder.brep->IsSolid()) {
	bool is_oriented = false;
	bool has_boundary = true;
	const bool is_manifold = builder.brep->IsManifold(&is_oriented, &has_boundary);
	if (is_manifold && is_oriented && !has_boundary) {
	    builder.brep->SetSolidOrientationForExperts(1);
	}
	if (!builder.brep->IsSolid()) {
	    builder.brep->Flip();
	    if (is_manifold && is_oriented && !has_boundary)
		builder.brep->SetSolidOrientationForExperts(1);
	}
	if (!builder.brep->IsSolid()) {
	    delete builder.brep;
	    ON::End();
	    bu_exit(EXIT_FAILURE, "ERROR: generated blade BREP is valid but not solid: manifold=%s oriented=%s boundary=%s\n",
		    is_manifold ? "true" : "false", is_oriented ? "true" : "false", has_boundary ? "true" : "false");
	}
    }

    if (mk_brep(fp, solid_name.c_str(), builder.brep) != 0) {
	delete builder.brep;
	ON::End();
	bu_exit(EXIT_FAILURE, "ERROR: unable to write blade BREP %s\n", solid_name.c_str());
    }

    delete builder.brep;
    ON::End();
}


struct hub_geometry {
    double length = 0.0;
    double z_front = 0.0;
    double z_aft = 0.0;
    double r_forward = 0.0;
    double r_aft = 0.0;
    double r_max = 0.0;
};


static hub_geometry
make_hub_geometry(const propeller_options &opts)
{
    hub_geometry geom;
    geom.length = opts.hub_length_ratio * opts.diameter;
    geom.z_front = -0.5 * geom.length;
    geom.z_aft = 0.5 * geom.length;
    geom.r_forward = 0.5 * opts.hub_forward_diameter;
    geom.r_aft = 0.5 * opts.hub_aft_diameter;
    geom.r_max = 0.5 * opts.hub_diameter_ratio * opts.diameter;
    return geom;
}


static double
hub_outer_radius(const propeller_options &opts, const hub_geometry &geom, double z)
{
    if (opts.hub_profile == casing_profile::cylindrical)
	return geom.r_max;

    if (opts.hub_profile == casing_profile::truncated_cone) {
	const double t = (z - geom.z_front) / geom.length;
	return geom.r_forward + t * (geom.r_aft - geom.r_forward);
    }

    const double u = std::max(-1.0, std::min(1.0, z / (0.5 * geom.length)));
    const double end_radius = std::min(geom.r_forward, geom.r_aft);
    const double mid_radius = end_radius * opts.hub_barrel_swell_ratio;
    return mid_radius * (1.0 - u * u * (1.0 - 1.0 / opts.hub_barrel_swell_ratio));
}


static double
shaft_taper_slope(const propeller_options &opts)
{
    switch (opts.shaft_taper) {
	case shaft_standard::iso_1_10:
	    return std::tan(std::atan(1.0 / 10.0) / 2.0);
	case shaft_standard::sae_1_16:
	    return std::tan(std::atan(1.0 / 16.0) / 2.0);
    }
    return std::tan(std::atan(1.0 / 10.0) / 2.0);
}


static double
shaft_bore_radius(const propeller_options &opts, const hub_geometry &geom, double z)
{
    return 0.5 * opts.shaft_diameter_small + (z - geom.z_front) * shaft_taper_slope(opts);
}


static bot_mesh
build_barrel_hub_mesh(const propeller_options &opts, const hub_geometry &geom)
{
    bot_mesh mesh;
    const int slices = opts.hub_longitudinal_slices;
    const int segments = opts.hub_angular_segments;
    mesh.vertices.reserve(static_cast<size_t>(slices * segments + 2) * 3);
    mesh.faces.reserve(static_cast<size_t>((slices - 1) * segments * 2 + segments * 2) * 3);

    for (int s = 0; s < slices; ++s) {
	const double t = static_cast<double>(s) / static_cast<double>(slices - 1);
	const double z = geom.z_front + t * geom.length;
	const double radius = hub_outer_radius(opts, geom, z);
	for (int k = 0; k < segments; ++k) {
	    const double theta = static_cast<double>(k) * 2.0 * M_PI / static_cast<double>(segments);
	    append_vertex(mesh, {opts.offset_x + radius * std::cos(theta), opts.offset_y + radius * std::sin(theta), opts.offset_z + z});
	}
    }

    for (int s = 0; s < slices - 1; ++s) {
	for (int k = 0; k < segments; ++k) {
	    const int kn = (k + 1) % segments;
	    const int a = s * segments + k;
	    const int b = s * segments + kn;
	    const int c = (s + 1) * segments + kn;
	    const int d = (s + 1) * segments + k;
	    add_face(mesh, a, d, c);
	    add_face(mesh, a, c, b);
	}
    }

    const int front_center = static_cast<int>(mesh.vertices.size() / 3);
    append_vertex(mesh, {opts.offset_x, opts.offset_y, opts.offset_z + geom.z_front});
    for (int k = 0; k < segments; ++k)
	add_face(mesh, front_center, k, (k + 1) % segments);

    const int aft_center = static_cast<int>(mesh.vertices.size() / 3);
    append_vertex(mesh, {opts.offset_x, opts.offset_y, opts.offset_z + geom.z_aft});
    const int aft_offset = (slices - 1) * segments;
    for (int k = 0; k < segments; ++k)
	add_face(mesh, aft_offset + k, aft_center, aft_offset + ((k + 1) % segments));

    validate_mesh_arrays(mesh);
    orient_mesh_outward(mesh);
    validate_mesh_arrays(mesh);
    return mesh;
}


static void
write_hub_outer(struct rt_wdb *fp, const propeller_options &opts, const hub_geometry &geom, const std::string &outer_name)
{
    check_output_name_available(fp, outer_name);

    if (opts.hub_profile == casing_profile::convex_barrel) {
	const bot_mesh mesh = build_barrel_hub_mesh(opts, geom);
	write_bot_solid(fp, outer_name, mesh);
	return;
    }

    point_t base;
    vect_t height;
    VSET(base, opts.offset_x, opts.offset_y, opts.offset_z + geom.z_front);
    VSET(height, 0.0, 0.0, geom.length);
    if (mk_trc_h(fp, outer_name.c_str(), base, height, geom.r_forward, geom.r_aft) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to write hub outer casing %s\n", outer_name.c_str());
}


static void
write_hub_bore(struct rt_wdb *fp, const propeller_options &opts, const hub_geometry &geom, const std::string &bore_name)
{
    check_output_name_available(fp, bore_name);
    const double eps = 0.01 * geom.length;
    const double z0 = geom.z_front - eps;
    const double z1 = geom.z_aft + eps;
    point_t base;
    vect_t height;
    VSET(base, opts.offset_x, opts.offset_y, opts.offset_z + z0);
    VSET(height, 0.0, 0.0, z1 - z0);
    if (mk_trc_h(fp, bore_name.c_str(), base, height,
	    shaft_bore_radius(opts, geom, z0), shaft_bore_radius(opts, geom, z1)) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to write shaft taper %s\n", bore_name.c_str());
}


static void
write_hub_keyway(struct rt_wdb *fp, const propeller_options &opts, const hub_geometry &geom, const std::string &keyway_name)
{
    check_output_name_available(fp, keyway_name);

    const double eps = 0.01 * geom.length;
    const double z0 = geom.z_front - eps;
    const double z1 = geom.z_aft + eps;
    const double x0 = -0.5 * opts.keyway_width;
    const double x1 = 0.5 * opts.keyway_width;
    const double bore0 = shaft_bore_radius(opts, geom, z0);
    const double bore1 = shaft_bore_radius(opts, geom, z1);
    const double floor0 = bore0 - std::max(opts.keyway_depth, 0.25 * bore0);
    const double floor1 = bore1 - std::max(opts.keyway_depth, 0.25 * bore1);
    const double ceil0 = bore0 + opts.keyway_depth;
    const double ceil1 = bore1 + opts.keyway_depth;
    point_t pts[8];

    VSET(pts[0], opts.offset_x + x0, opts.offset_y + floor0, opts.offset_z + z0);
    VSET(pts[1], opts.offset_x + x1, opts.offset_y + floor0, opts.offset_z + z0);
    VSET(pts[2], opts.offset_x + x1, opts.offset_y + ceil0, opts.offset_z + z0);
    VSET(pts[3], opts.offset_x + x0, opts.offset_y + ceil0, opts.offset_z + z0);
    VSET(pts[4], opts.offset_x + x0, opts.offset_y + floor1, opts.offset_z + z1);
    VSET(pts[5], opts.offset_x + x1, opts.offset_y + floor1, opts.offset_z + z1);
    VSET(pts[6], opts.offset_x + x1, opts.offset_y + ceil1, opts.offset_z + z1);
    VSET(pts[7], opts.offset_x + x0, opts.offset_y + ceil1, opts.offset_z + z1);

    if (mk_arb8(fp, keyway_name.c_str(), &pts[0][X]) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to write keyway %s\n", keyway_name.c_str());
}


static bool
write_hub_fairing(struct rt_wdb *fp, const propeller_options &opts, const hub_geometry &geom, const std::string &fairing_name)
{
    if (!opts.include_fairing_cap)
	return false;

    check_output_name_available(fp, fairing_name);
    point_t base;
    vect_t dir;
    VSET(base, opts.offset_x, opts.offset_y, opts.offset_z + geom.z_aft);
    VSET(dir, 0.0, 0.0, 1.0);
    if (mk_cone(fp, fairing_name.c_str(), base, dir, opts.fairing_cap_length,
	    hub_outer_radius(opts, geom, geom.z_aft), 0.0) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to write fairing cap %s\n", fairing_name.c_str());
    return true;
}


static std::string
write_hub(struct rt_wdb *fp, const propeller_options &opts, std::vector<std::string> &object_names)
{
    const hub_geometry geom = make_hub_geometry(opts);
    const std::string outer_name = opts.name + "_hub_outer.s";
    const std::string bore_name = opts.name + "_shaft_taper.s";
    const std::string keyway_name = opts.name + "_keyway.s";
    const std::string fairing_name = opts.name + "_fairing_cap.s";
    const std::string hub_comb_name = opts.name + "_hub_hollow.c";

    check_output_name_available(fp, hub_comb_name);
    write_hub_outer(fp, opts, geom, outer_name);
    write_hub_bore(fp, opts, geom, bore_name);
    write_hub_keyway(fp, opts, geom, keyway_name);
    const bool wrote_fairing = write_hub_fairing(fp, opts, geom, fairing_name);

    struct wmember hub_head = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&hub_head.l);
    (void)mk_addmember(outer_name.c_str(), &hub_head.l, NULL, WMOP_UNION);
    if (wrote_fairing)
	(void)mk_addmember(fairing_name.c_str(), &hub_head.l, NULL, WMOP_UNION);
    (void)mk_addmember(bore_name.c_str(), &hub_head.l, NULL, WMOP_SUBTRACT);
    (void)mk_addmember(keyway_name.c_str(), &hub_head.l, NULL, WMOP_SUBTRACT);
    if (mk_lcomb(fp, hub_comb_name.c_str(), &hub_head, 0, NULL, NULL, NULL, 0) != 0) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: unable to write hollow hub comb\n");
    }

    object_names.push_back(outer_name);
    object_names.push_back(bore_name);
    object_names.push_back(keyway_name);
    if (wrote_fairing)
	object_names.push_back(fairing_name);
    object_names.push_back(hub_comb_name);
    return hub_comb_name;
}


static void
write_propeller(const propeller_options &opts)
{
    struct rt_wdb *fp = open_output_database(opts);

    const std::string region_name = opts.name + ".r";
    const std::string group_name = opts.name;
    check_output_name_available(fp, region_name);
    check_output_name_available(fp, group_name);

    std::vector<std::string> object_names;
    const std::string hub_name = write_hub(fp, opts, object_names);
    struct wmember region_head = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&region_head.l);
    (void)mk_addmember(hub_name.c_str(), &region_head.l, NULL, WMOP_UNION);

    size_t total_vertices = 0;
    size_t total_faces = 0;
    for (int blade = 0; blade < opts.blades; ++blade) {
	const double angle = static_cast<double>(blade) * (2.0 * M_PI / static_cast<double>(opts.blades));
	const std::string blade_name = opts.name + "_blade_" + std::to_string(blade) + (opts.mode == output_mode::brep ? ".brep" : ".s");
	if (opts.mode == output_mode::brep) {
	    write_blade_brep(fp, opts, blade_name, angle);
	} else {
	    const bot_mesh mesh = build_blade_mesh(opts, angle);
	    write_blade(fp, blade_name, mesh);
	    total_vertices += mesh.vertices.size() / 3;
	    total_faces += mesh.faces.size() / 3;
	}
	(void)mk_addmember(blade_name.c_str(), &region_head.l, NULL, WMOP_UNION);
	object_names.push_back(blade_name);
    }

    unsigned char rgb[] = {170, 175, 185};
    if (mk_lcomb(fp, region_name.c_str(), &region_head, 1, "plastic", "", rgb, 0) != 0) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: unable to write propeller region\n");
    }
    object_names.push_back(region_name);

    struct wmember group_head = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&group_head.l);
    (void)mk_addmember(region_name.c_str(), &group_head.l, NULL, WMOP_UNION);
    if (mk_lcomb(fp, group_name.c_str(), &group_head, 0, NULL, NULL, NULL, 0) != 0) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: unable to write propeller group\n");
    }
    object_names.push_back(group_name);

    write_metadata(fp, object_names, opts);
    db_close(fp->dbip);

    if (opts.mode == output_mode::brep) {
	bu_log("Wrote %s with %d smooth BREP blades and CSG hub\n", opts.outfile.c_str(), opts.blades);
    } else {
	bu_log("Wrote %s with %d BoT blades, %zu blade vertices, and %zu blade triangles\n",
		opts.outfile.c_str(), opts.blades, total_vertices, total_faces);
    }
}


struct demo_spec {
    const char *name = nullptr;
    output_mode mode = output_mode::bot;
    double diameter = 700.0;
    int blades = 4;
    double expanded_area_ratio = 0.65;
    double pitch_diameter_tip = 1.0;
    double rake_angle_deg = 0.0;
    double skew_max_deg = 0.0;
    double hub_diameter_ratio = 0.20;
    double hub_length_ratio = 0.22;
    casing_profile hub_profile = casing_profile::truncated_cone;
    shaft_standard shaft_taper = shaft_standard::iso_1_10;
    bool include_fairing_cap = true;
    double hub_barrel_swell_ratio = 1.08;
    double root_overlap_ratio = 0.01;
    int radial_stations = 10;
    int chord_points = 21;
    int hub_longitudinal_slices = 16;
    int hub_angular_segments = 48;
};


static propeller_options
demo_options(const propeller_options &demo_opts, const demo_spec &spec, int index)
{
    const int cols = 6;
    const double dx = 1500.0;
    const double dy = 1500.0;

    propeller_options opts;
    opts.outfile = demo_opts.demo_file;
    opts.name = spec.name;
    opts.mode = spec.mode;
    opts.diameter = spec.diameter;
    opts.blades = spec.blades;
    opts.expanded_area_ratio = spec.expanded_area_ratio;
    opts.pitch_diameter_tip = spec.pitch_diameter_tip;
    opts.rake_angle_deg = spec.rake_angle_deg;
    opts.skew_max_deg = spec.skew_max_deg;
    opts.hub_diameter_ratio = spec.hub_diameter_ratio;
    opts.hub_length_ratio = spec.hub_length_ratio;
    opts.hub_profile = spec.hub_profile;
    opts.shaft_taper = spec.shaft_taper;
    opts.include_fairing_cap = spec.include_fairing_cap;
    opts.hub_barrel_swell_ratio = spec.hub_barrel_swell_ratio;
    opts.root_overlap_ratio = spec.root_overlap_ratio;
    opts.radial_stations = spec.radial_stations;
    opts.chord_points = spec.chord_points;
    opts.hub_longitudinal_slices = spec.hub_longitudinal_slices;
    opts.hub_angular_segments = spec.hub_angular_segments;
    opts.offset_x = static_cast<double>(index % cols) * dx;
    opts.offset_y = static_cast<double>(index / cols) * dy;
    opts.overwrite = index == 0;
    opts.append = index != 0;
    finalize_options(opts);
    return opts;
}


static int
write_demo_file(const propeller_options &demo_opts)
{
    const std::vector<demo_spec> specs = {
	{"bot_00_baseline", output_mode::bot, 700, 4, 0.65, 1.00, 0, 0, 0.20, 0.22, casing_profile::truncated_cone, shaft_standard::iso_1_10, true, 1.08, 0.010, 10, 21, 16, 48},
	{"bot_01_low_area_3blade", output_mode::bot, 680, 3, 0.38, 0.85, 0, 12, 0.18, 0.21, casing_profile::cylindrical, shaft_standard::iso_1_10, true, 1.08, 0.010, 10, 21, 16, 48},
	{"bot_02_high_area_5blade", output_mode::bot, 720, 5, 1.05, 1.15, 3, 18, 0.22, 0.25, casing_profile::truncated_cone, shaft_standard::sae_1_16, true, 1.08, 0.012, 11, 23, 16, 48},
	{"bot_03_barrel_hub", output_mode::bot, 700, 4, 0.75, 1.05, -2, 24, 0.21, 0.24, casing_profile::convex_barrel, shaft_standard::iso_1_10, true, 1.14, 0.010, 10, 21, 18, 48},
	{"bot_04_no_fairing", output_mode::bot, 660, 4, 0.55, 0.95, 0, -16, 0.19, 0.20, casing_profile::truncated_cone, shaft_standard::sae_1_16, false, 1.08, 0.010, 10, 21, 16, 48},
	{"bot_05_7blade_skewed", output_mode::bot, 760, 7, 0.90, 1.25, 5, 30, 0.22, 0.24, casing_profile::truncated_cone, shaft_standard::iso_1_10, true, 1.08, 0.012, 10, 21, 16, 48},
	{"bot_06_2blade_fast", output_mode::bot, 720, 2, 0.45, 1.35, 2, 10, 0.20, 0.22, casing_profile::cylindrical, shaft_standard::iso_1_10, true, 1.08, 0.010, 11, 23, 16, 48},
	{"bot_07_long_hub", output_mode::bot, 690, 4, 0.70, 0.90, -4, 8, 0.20, 0.28, casing_profile::convex_barrel, shaft_standard::sae_1_16, true, 1.10, 0.010, 10, 21, 18, 48},
	{"bot_08_short_hub_high_pitch", output_mode::bot, 700, 5, 0.82, 1.40, 0, 20, 0.18, 0.18, casing_profile::truncated_cone, shaft_standard::iso_1_10, true, 1.08, 0.010, 10, 21, 16, 48},
	{"bot_09_reverse_skew", output_mode::bot, 700, 4, 0.68, 1.05, 2, -25, 0.21, 0.22, casing_profile::truncated_cone, shaft_standard::sae_1_16, true, 1.08, 0.010, 10, 21, 16, 48},
	{"bot_10_low_pitch_barrel", output_mode::bot, 670, 3, 0.62, 0.70, 0, 14, 0.20, 0.23, casing_profile::convex_barrel, shaft_standard::iso_1_10, false, 1.16, 0.010, 10, 21, 18, 48},
	{"bot_11_large_diameter", output_mode::bot, 820, 6, 0.78, 1.10, 4, 22, 0.22, 0.24, casing_profile::truncated_cone, shaft_standard::iso_1_10, true, 1.08, 0.012, 10, 21, 16, 48},
	{"brep_12_baseline", output_mode::brep, 700, 4, 0.65, 1.00, 0, 0, 0.20, 0.22, casing_profile::truncated_cone, shaft_standard::iso_1_10, true, 1.08, 0.010, 7, 15, 14, 36},
	{"brep_13_barrel_hub", output_mode::brep, 700, 4, 0.74, 1.05, 2, 18, 0.21, 0.24, casing_profile::convex_barrel, shaft_standard::sae_1_16, true, 1.12, 0.010, 7, 15, 14, 36},
	{"brep_14_5blade", output_mode::brep, 720, 5, 0.92, 1.15, 3, 20, 0.22, 0.24, casing_profile::truncated_cone, shaft_standard::iso_1_10, true, 1.08, 0.012, 7, 15, 14, 36},
	{"brep_15_no_fairing", output_mode::brep, 670, 3, 0.55, 0.90, 0, -14, 0.19, 0.21, casing_profile::cylindrical, shaft_standard::sae_1_16, false, 1.08, 0.010, 7, 15, 14, 36},
	{"brep_16_high_pitch", output_mode::brep, 700, 4, 0.70, 1.35, 4, 24, 0.20, 0.22, casing_profile::truncated_cone, shaft_standard::iso_1_10, true, 1.08, 0.010, 7, 15, 14, 36},
	{"brep_17_7blade", output_mode::brep, 760, 7, 0.86, 1.20, 4, 26, 0.22, 0.24, casing_profile::truncated_cone, shaft_standard::sae_1_16, true, 1.08, 0.012, 7, 15, 14, 36}
    };

    for (size_t i = 0; i < specs.size(); ++i) {
	propeller_options opts = demo_options(demo_opts, specs[i], static_cast<int>(i));
	bu_log("[%zu] %s %s\n", i, output_mode_name(opts.mode).c_str(), opts.name.c_str());
	write_propeller(opts);
    }

    propeller_options all_opts;
    all_opts.outfile = demo_opts.demo_file;
    all_opts.append = true;
    struct rt_wdb *fp = open_output_database(all_opts);
    if (db_lookup(fp->dbip, "all", LOOKUP_QUIET) != RT_DIR_NULL) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: object all already exists in output database\n");
    }

    struct wmember all_head = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&all_head.l);
    for (const demo_spec &spec : specs) {
	const std::string region_name = std::string(spec.name) + ".r";
	if (mk_addmember(region_name.c_str(), &all_head.l, NULL, WMOP_UNION) == WMEMBER_NULL) {
	    db_close(fp->dbip);
	    bu_exit(EXIT_FAILURE, "ERROR: unable to add %s to demo all comb\n", region_name.c_str());
	}
    }
    if (mk_lcomb(fp, "all", &all_head, 0, NULL, NULL, NULL, 0) != 0) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: unable to write demo all comb\n");
    }
    set_attr(fp, "all", "propeller::generator", "propeller");
    set_attr(fp, "all", "propeller::metadata_version", "1");
    set_attr(fp, "all", "propeller::demo_propeller_count", std::to_string(specs.size()));
    db_close(fp->dbip);

    bu_log("Wrote %s with %zu sample propellers and all comb\n", demo_opts.demo_file.c_str(), specs.size());
    return EXIT_SUCCESS;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);

    try {
	const propeller_options opts = parse_args(argc, argv);
	if (!opts.demo_file.empty())
	    return write_demo_file(opts);
	write_propeller(opts);
	return EXIT_SUCCESS;
    } catch (const std::exception &e) {
	bu_log("ERROR: %s\n", e.what());
	usage(argv[0]);
	return EXIT_FAILURE;
    }
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
