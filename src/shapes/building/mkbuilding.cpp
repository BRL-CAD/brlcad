/*                   M K B U I L D I N G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file mkbuilding.cpp
 *
 * Command-line procedural building generator.
 */

#include "common.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "building.h"

namespace {

struct options {
    std::string output = "mkbuilding.g";
    std::string spec_file;
    std::string preset = "house";
    std::string name;
    std::string type;
    std::string roof_shape;
    std::string structure;
    std::string effective_spec_file;
    std::string demo_file;
    int levels = -1;
    double width = -1.0;
    double depth = -1.0;
    double floor_height = -1.0;
    double roof_height = -1.0;
    double wall_thickness = -1.0;
    int no_auto_openings = 0;
    int force = 0;
    int append = 0;
    int validate_only = 0;
    int print_schema = 0;
    int list_types = 0;
    int list_roofs = 0;
    int help = 0;
};

struct building_args {
    int help;
    const char *output;
    const char *spec_file;
    const char *preset;
    const char *name;
    const char *type;
    int levels;
    fastf_t width;
    fastf_t depth;
    fastf_t floor_height;
    const char *roof_shape;
    fastf_t roof_height;
    fastf_t wall_thickness;
    const char *structure;
    int no_auto_openings;
    int force;
    int append;
    int validate_only;
    const char *effective_spec_file;
    const char *demo_file;
    int print_schema;
    int list_types;
    int list_roofs;
};

static const struct bu_arg_option building_options[] = {
    BU_ARG_FLAG("h", "help", struct building_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", nullptr, "help", 1),
    BU_ARG_VALUE("o", "output", struct building_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file.g", "Output BRL-CAD database"),
    BU_ARG_VALUE("s", "spec", struct building_args, spec_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file.json", "Native, GeoJSON, Overpass, or CityJSON input"),
    BU_ARG_VALUE("p", "preset", struct building_args, preset,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "name", "Building preset or OSM building type"),
    BU_ARG_VALUE("n", "name", struct building_args, name,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "name", "Top-level building object name"),
    BU_ARG_VALUE("t", "type", struct building_args, type,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "OSM-value", "OSM building=* taxonomy value"),
    BU_ARG_INTEGER_RANGE("L", "levels", struct building_args, levels,
	1, 0, 0, 0, "count", "Number of above-ground floors"),
    BU_ARG_NUMBER_RANGE("W", "width", struct building_args, width,
	1, 0.0, 0, 0.0, "meters", "Rectangular footprint width"),
    BU_ARG_NUMBER_RANGE("D", "depth", struct building_args, depth,
	1, 0.0, 0, 0.0, "meters", "Rectangular footprint depth"),
    BU_ARG_NUMBER_RANGE("H", "floor-height", struct building_args, floor_height,
	1, 0.0, 0, 0.0, "meters", "Uniform floor-to-floor height"),
    BU_ARG_VALUE("R", "roof-shape", struct building_args, roof_shape,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "shape", "OSM roof:shape value"),
    BU_ARG_NUMBER_RANGE(nullptr, "roof-height", struct building_args, roof_height,
	1, 0.0, 0, 0.0, "meters", "Roof height above facade"),
    BU_ARG_NUMBER_RANGE(nullptr, "wall-thickness", struct building_args, wall_thickness,
	1, 0.0, 0, 0.0, "meters", "Exterior wall thickness"),
    BU_ARG_VALUE(nullptr, "structure", struct building_args, structure,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "system", "Structural system; enables columns and beams"),
    BU_ARG_FLAG(nullptr, "no-auto-openings", struct building_args, no_auto_openings,
	"Do not synthesize doors and windows"),
    BU_ARG_FLAG("f", "force", struct building_args, force, "Replace an existing output database"),
    BU_ARG_FLAG("j", "append", struct building_args, append, "Append to an existing output database"),
    BU_ARG_FLAG(nullptr, "validate-only", struct building_args, validate_only,
	"Validate and normalize input without generating geometry"),
    BU_ARG_VALUE(nullptr, "effective-spec", struct building_args, effective_spec_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file.json", "Write fully resolved native specification"),
    BU_ARG_VALUE(nullptr, "demo-file", struct building_args, demo_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file.g", "Write a broad building and roof taxonomy sampler"),
    BU_ARG_FLAG(nullptr, "schema", struct building_args, print_schema, "Print the native JSON Schema and exit"),
    BU_ARG_FLAG(nullptr, "list-types", struct building_args, list_types,
	"List covered OSM building values and exit"),
    BU_ARG_FLAG(nullptr, "list-roofs", struct building_args, list_roofs,
	"List covered OSM roof shapes and exit"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand building_operands[] = {
    BU_ARG_OPERAND("output.g", BU_ARG_VALUE_FILE, 0, 1, "Output BRL-CAD database", nullptr),
    BU_ARG_OPERAND_NULL
};

static int
building_schema_validate(const struct bu_arg_schema *, size_t argc,
    const char **argv, size_t cursor_arg, struct bu_arg_validate_result *result)
{
    bool width = false;
    bool depth = false;

    if (!result || cursor_arg < argc)
	return 0;
    for (size_t i = 0; i < argc; ++i) {
	const char *arg = argv[i];
	if (!arg)
	    continue;
	if (BU_STR_EQUAL(arg, "-W") || BU_STR_EQUAL(arg, "--width") ||
		bu_strncmp(arg, "--width=", 8) == 0 ||
		(bu_strncmp(arg, "-W", 2) == 0 && arg[2]))
	    width = true;
	if (BU_STR_EQUAL(arg, "-D") || BU_STR_EQUAL(arg, "--depth") ||
		bu_strncmp(arg, "--depth=", 8) == 0 ||
		(bu_strncmp(arg, "-D", 2) == 0 && arg[2]))
	    depth = true;
    }
    if (width != depth) {
	result->state = BU_ARG_VALIDATE_INVALID;
	result->hint = "--width and --depth must be specified together";
    }
    return 0;
}

static const struct bu_arg_schema building_schema = BU_ARG_SCHEMA(
    "mkbuilding",
    "Generate physically dimensioned building geometry from presets or structured input",
    building_options, building_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(nullptr, nullptr, building_schema_validate, nullptr, nullptr));

void
usage(const char *program)
{
    char *help = bu_arg_schema_help(&building_schema, program);
    std::cerr << (help ? help : "");
    bu_free(help, "option help");
}

options
parse_options(int argc, const char **argv)
{
    options opts;
    const char **arguments = argv + 1;
    struct building_args args = {};
    args.levels = opts.levels;
    args.width = opts.width;
    args.depth = opts.depth;
    args.floor_height = opts.floor_height;
    args.roof_height = opts.roof_height;
    args.wall_thickness = opts.wall_thickness;
    struct bu_vls message = BU_VLS_INIT_ZERO;
    const int operand_index = bu_arg_schema_parse_complete(&building_schema,
	&args, &message, argc - 1, arguments);
    if (operand_index < 0) {
	const std::string error = bu_vls_cstr(&message);
	bu_vls_free(&message);
	throw std::runtime_error(error.empty() ? "option parsing failed" : error);
    }
    bu_vls_free(&message);

    if (args.help) {
	usage(argv[0]);
	bu_exit(EXIT_SUCCESS, nullptr);
    }

    argc -= operand_index + 1;
    arguments += operand_index;
    if (args.output)
	opts.output = args.output;
    if (argc == 1) {
	if (args.output)
	    throw std::runtime_error("output file was specified both positionally and with --output");
	opts.output = arguments[0];
    }
    if (args.spec_file) opts.spec_file = args.spec_file;
    if (args.preset) opts.preset = args.preset;
    if (args.name) opts.name = args.name;
    if (args.type) opts.type = args.type;
    if (args.roof_shape) opts.roof_shape = args.roof_shape;
    if (args.structure) opts.structure = args.structure;
    if (args.effective_spec_file) opts.effective_spec_file = args.effective_spec_file;
    if (args.demo_file) opts.demo_file = args.demo_file;
    opts.levels = args.levels;
    opts.width = args.width;
    opts.depth = args.depth;
    opts.floor_height = args.floor_height;
    opts.roof_height = args.roof_height;
    opts.wall_thickness = args.wall_thickness;
    opts.no_auto_openings = args.no_auto_openings;
    opts.force = args.force;
    opts.append = args.append;
    opts.validate_only = args.validate_only;
    opts.print_schema = args.print_schema;
    opts.list_types = args.list_types;
    opts.list_roofs = args.list_roofs;
    return opts;
}

void
write_text(const std::string &path, const std::string &contents)
{
    std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output)
	throw std::runtime_error("unable to open file for writing: " + path);
    output << contents;
    if (!output)
	throw std::runtime_error("unable to write file: " + path);
}

void
apply_overrides(std::vector<building::building_spec> &specs, const options &opts)
{
    for (size_t i = 0; i < specs.size(); ++i) {
	building::building_spec &spec = specs[i];
	if (!opts.name.empty())
	    spec.id = specs.size() == 1 ? opts.name : opts.name + "_" + std::to_string(i);
	if (!opts.type.empty())
	    spec.type = opts.type;
	if (opts.levels >= 0) {
	    spec.levels_above = opts.levels;
	    const double height = opts.floor_height > 0.0 ? opts.floor_height : (spec.floor_heights.empty() ? 3.0 : spec.floor_heights.front());
	    spec.floor_heights.assign(static_cast<size_t>(opts.levels), height);
	} else if (opts.floor_height > 0.0) {
	    spec.floor_heights.assign(static_cast<size_t>(spec.levels_above), opts.floor_height);
	}
	if ((opts.width > 0.0) != (opts.depth > 0.0))
	    throw std::runtime_error("--width and --depth must be specified together");
	if (opts.width > 0.0)
	    spec.footprint = {{0.0, 0.0}, {opts.width, 0.0}, {opts.width, opts.depth}, {0.0, opts.depth}};
	if (!opts.roof_shape.empty())
	    spec.roof.shape = opts.roof_shape;
	if (opts.roof_height >= 0.0)
	    spec.roof.height = opts.roof_height;
	if (opts.wall_thickness > 0.0)
	    spec.wall_thickness = opts.wall_thickness;
	if (!opts.structure.empty()) {
	    spec.structure.system = opts.structure;
	    spec.structure.enabled = opts.structure != "wall_bearing" && opts.structure != "none";
	}
	if (opts.no_auto_openings)
	    spec.auto_openings = false;
	building::validate_spec(spec);
    }
}

} // namespace

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    try {
	const options opts = parse_options(argc, argv);
	if (opts.print_schema) {
	    std::cout << building::schema_json();
	    return EXIT_SUCCESS;
	}
	if (opts.list_types) {
	    std::cout << building::supported_types_text();
	    return EXIT_SUCCESS;
	}
	if (opts.list_roofs) {
	    std::cout << building::supported_roofs_text();
	    return EXIT_SUCCESS;
	}
	std::vector<building::building_spec> specs;
	std::string output = opts.output;
	if (!opts.demo_file.empty()) {
	    if (!opts.spec_file.empty() || opts.append)
		throw std::runtime_error("--demo-file cannot be combined with --spec or --append");
	    specs = building::make_demo_specs();
	    output = opts.demo_file;
	} else if (!opts.spec_file.empty()) {
	    specs = building::read_specs(opts.spec_file, opts.name.empty() ? "building" : opts.name);
	} else {
	    specs.push_back(building::make_preset(opts.preset));
	}
	apply_overrides(specs, opts);
	if (!opts.effective_spec_file.empty())
	    write_text(opts.effective_spec_file, building::effective_spec_json(specs));
	if (opts.validate_only) {
	    std::cout << "Validated " << specs.size() << " building specification(s)\n";
	    return EXIT_SUCCESS;
	}
	const building::generation_report report = building::write_database(specs, output, opts.force, opts.append);
	std::cout << "Generated " << report.building_count << " building(s) and " << report.part_count
	    << " part(s) in " << output << ": " << report.region_count << " regions, "
	    << report.primitive_count << " primitives, " << report.opening_count << " openings, "
	    << report.column_count << " columns, " << report.beam_count << " beams\n";
	return EXIT_SUCCESS;
    } catch (const std::exception &error) {
	std::cerr << "mkbuilding: " << error.what() << '\n';
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
