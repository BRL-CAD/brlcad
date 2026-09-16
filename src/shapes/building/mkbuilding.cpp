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
#include "bu/exit.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/vls.h"

#include "building.h"

namespace {

constexpr size_t OPTION_DESCRIPTION_COUNT = 25;

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

void
usage(const char *program, struct bu_opt_desc *descriptions)
{
    char *help = bu_opt_describe(descriptions, nullptr);
    std::cerr
	<< "Usage: " << program << " [options] [output.g]\n"
	<< "Generate physically dimensioned building geometry from presets, the native JSON schema,\n"
	<< "OSM GeoJSON/Overpass JSON, or OGC CityJSON 2.x input. Native and imported lengths\n"
	<< "are meters unless their schema states otherwise.\n\n"
	<< (help ? help : "");
    bu_free(help, "option help");
}

struct bu_opt_desc *
option_descriptions(
    options &opts,
    struct bu_vls &output,
    struct bu_vls &spec,
    struct bu_vls &preset,
    struct bu_vls &name,
    struct bu_vls &type,
    struct bu_vls &roof,
    struct bu_vls &structure,
    struct bu_vls &effective,
    struct bu_vls &demo)
{
    static struct bu_opt_desc descriptions[OPTION_DESCRIPTION_COUNT];
    BU_OPT(descriptions[0], "h", "help", "", nullptr, &opts.help, "Print help and exit");
    BU_OPT(descriptions[1], "?", "", "", nullptr, &opts.help, "");
    BU_OPT(descriptions[2], "o", "output", "file.g", &bu_opt_vls, &output, "Output BRL-CAD database");
    BU_OPT(descriptions[3], "s", "spec", "file.json", &bu_opt_vls, &spec, "Native, GeoJSON, Overpass, or CityJSON input");
    BU_OPT(descriptions[4], "p", "preset", "name", &bu_opt_vls, &preset, "Building preset or OSM building type");
    BU_OPT(descriptions[5], "n", "name", "name", &bu_opt_vls, &name, "Top-level building object name");
    BU_OPT(descriptions[6], "t", "type", "OSM-value", &bu_opt_vls, &type, "OSM building=* taxonomy value");
    BU_OPT(descriptions[7], "L", "levels", "count", &bu_opt_int, &opts.levels, "Number of above-ground floors");
    BU_OPT(descriptions[8], "W", "width", "meters", &bu_opt_fastf_t, &opts.width, "Rectangular footprint width");
    BU_OPT(descriptions[9], "D", "depth", "meters", &bu_opt_fastf_t, &opts.depth, "Rectangular footprint depth");
    BU_OPT(descriptions[10], "H", "floor-height", "meters", &bu_opt_fastf_t, &opts.floor_height, "Uniform floor-to-floor height");
    BU_OPT(descriptions[11], "R", "roof-shape", "shape", &bu_opt_vls, &roof, "OSM roof:shape value");
    BU_OPT(descriptions[12], "", "roof-height", "meters", &bu_opt_fastf_t, &opts.roof_height, "Roof height above facade");
    BU_OPT(descriptions[13], "", "wall-thickness", "meters", &bu_opt_fastf_t, &opts.wall_thickness, "Exterior wall thickness");
    BU_OPT(descriptions[14], "", "structure", "system", &bu_opt_vls, &structure, "Structural system; enables columns and beams");
    BU_OPT(descriptions[15], "", "no-auto-openings", "", nullptr, &opts.no_auto_openings, "Do not synthesize doors and windows");
    BU_OPT(descriptions[16], "f", "force", "", nullptr, &opts.force, "Replace an existing output database");
    BU_OPT(descriptions[17], "j", "append", "", nullptr, &opts.append, "Append to an existing output database");
    BU_OPT(descriptions[18], "", "validate-only", "", nullptr, &opts.validate_only, "Validate and normalize input without generating geometry");
    BU_OPT(descriptions[19], "", "effective-spec", "file.json", &bu_opt_vls, &effective, "Write fully resolved native specification");
    BU_OPT(descriptions[20], "", "demo-file", "file.g", &bu_opt_vls, &demo, "Write a broad building and roof taxonomy sampler");
    BU_OPT(descriptions[21], "", "schema", "", nullptr, &opts.print_schema, "Print the native JSON Schema and exit");
    BU_OPT(descriptions[22], "", "list-types", "", nullptr, &opts.list_types, "List covered OSM building values and exit");
    BU_OPT(descriptions[23], "", "list-roofs", "", nullptr, &opts.list_roofs, "List covered OSM roof shapes and exit");
    BU_OPT_NULL(descriptions[24]);
    return descriptions;
}

std::string
vls_string(struct bu_vls &value, const std::string &fallback = std::string())
{
    return bu_vls_strlen(&value) > 0 ? std::string(bu_vls_cstr(&value)) : fallback;
}

options
parse_options(int argc, const char **argv)
{
    options opts;
    const char **arguments = argv + 1;
    struct bu_vls output = BU_VLS_INIT_ZERO;
    struct bu_vls spec = BU_VLS_INIT_ZERO;
    struct bu_vls preset = BU_VLS_INIT_ZERO;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls type = BU_VLS_INIT_ZERO;
    struct bu_vls roof = BU_VLS_INIT_ZERO;
    struct bu_vls structure = BU_VLS_INIT_ZERO;
    struct bu_vls effective = BU_VLS_INIT_ZERO;
    struct bu_vls demo = BU_VLS_INIT_ZERO;
    struct bu_vls message = BU_VLS_INIT_ZERO;
    struct bu_opt_desc *descriptions = option_descriptions(opts, output, spec, preset, name, type, roof, structure, effective, demo);
    const int positional = bu_opt_parse(&message, argc - 1, arguments, descriptions);
    if (positional < 0) {
	const std::string error = bu_vls_cstr(&message);
	bu_vls_free(&message);
	throw std::runtime_error(error.empty() ? "option parsing failed" : error);
    }
    bu_vls_free(&message);
    opts.output = vls_string(output, opts.output);
    opts.spec_file = vls_string(spec);
    opts.preset = vls_string(preset, opts.preset);
    opts.name = vls_string(name);
    opts.type = vls_string(type);
    opts.roof_shape = vls_string(roof);
    opts.structure = vls_string(structure);
    opts.effective_spec_file = vls_string(effective);
    opts.demo_file = vls_string(demo);
    bu_vls_free(&output); bu_vls_free(&spec); bu_vls_free(&preset); bu_vls_free(&name);
    bu_vls_free(&type); bu_vls_free(&roof); bu_vls_free(&structure); bu_vls_free(&effective); bu_vls_free(&demo);

    if (opts.help) {
	usage(argv[0], descriptions);
	bu_exit(EXIT_SUCCESS, nullptr);
    }
    if (positional > 1)
	throw std::runtime_error("at most one positional output file may be specified");
    if (positional == 1) {
	if (opts.output != "mkbuilding.g")
	    throw std::runtime_error("output file was specified both positionally and with --output");
	opts.output = arguments[0];
    }
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
