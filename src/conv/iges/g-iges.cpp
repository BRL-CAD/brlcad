/* BRL-CAD
 * Copyright (c) 1993-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_export.h"

#include <filesystem>
#include <set>
#include <stdexcept>

#include "bu/app.h"
#include "bu/debug.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/arg.h"
#include "bu/str.h"
#include "nmg/globals.h"

namespace {
using namespace brlcad::iges;

struct g_iges_args {
    int help;
    int faceted;
    int trimmed;
    int multi_file;
    int nurbs;
    int flatten;
    int verbose;
    fastf_t absolute_tolerance;
    fastf_t relative_tolerance;
    fastf_t normal_tolerance;
    fastf_t distance_tolerance;
    int rt_debug_mask;
    int nmg_debug_mask;
    const char *output_path;
    int processors;
};

static const struct bu_arg_option g_iges_options[] = {
    BU_ARG_FLAG("h", "help", struct g_iges_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("f", "faceted", struct g_iges_args, faceted, "Tessellate each region to a faceted B-Rep"),
    BU_ARG_FLAG("t", "trimmed-surfaces", struct g_iges_args, trimmed, "Tessellate regions to trimmed surfaces"),
    BU_ARG_FLAG("m", "multi-file", struct g_iges_args, multi_file, "Write one trimmed-surface file per region"),
    BU_ARG_FLAG("s", "nurbs", struct g_iges_args, nurbs, "Write NMG facets using NURBS geometry"),
    BU_ARG_FLAG(NULL, "flatten-brep", struct g_iges_args, flatten, "Write exact B-Reps as IGES 144 surfaces"),
    BU_ARG_FLAG("v", "verbose", struct g_iges_args, verbose, "Enable verbose diagnostics"),
    BU_ARG_VALUE("a", "absolute-tolerance", struct g_iges_args, absolute_tolerance, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "MM", "Absolute tessellation tolerance"),
    BU_ARG_VALUE("r", "relative-tolerance", struct g_iges_args, relative_tolerance, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "FRACTION", "Relative tessellation tolerance"),
    BU_ARG_VALUE("n", "normal-tolerance", struct g_iges_args, normal_tolerance, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "RADIANS", "Surface-normal tessellation tolerance"),
    BU_ARG_VALUE("d", "distance-tolerance", struct g_iges_args, distance_tolerance, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "MM", "Minimum distance between distinct points"),
    BU_ARG_VALUE("x", "rt-debug", struct g_iges_args, rt_debug_mask, BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "HEX", "librt hexadecimal debug mask"),
    BU_ARG_VALUE("X", "nmg-debug", struct g_iges_args, nmg_debug_mask, BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "HEX", "NMG hexadecimal debug mask"),
    BU_ARG_VALUE("o", "output", struct g_iges_args, output_path, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "FILE", "Output file, or directory in multi-file mode"),
    BU_ARG_INTEGER_RANGE("P", "processors", struct g_iges_args, processors, 1, 1, 0, 0, "COUNT", "Maximum worker count (IGES conversion is serialized)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_iges_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1, "Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("output-or-object", BU_ARG_VALUE_STRING, 0, BU_ARG_COUNT_UNLIMITED, "Output path followed by optional object names", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema g_iges_schema = BU_ARG_SCHEMA(
    "g-iges", "Export BRL-CAD geometry to IGES", g_iges_options, g_iges_operands,
    BU_ARG_PARSE_INTERSPERSED, BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

bool
iges_output_name(const char *name)
{
    const auto extension = std::filesystem::path(name).extension().string();
    return BU_STR_EQUIV(extension.c_str(), ".igs") || BU_STR_EQUIV(extension.c_str(), ".iges");
}

std::vector<std::string>
top_level_objects(struct db_i *database)
{
    db_update_nref(database);
    struct directory **entries = nullptr;
    const size_t count = db_ls(database, DB_LS_TOPS, nullptr, &entries);
    const auto release = [](struct directory **directories) { bu_free(directories, "IGES top-level selections"); };
    std::unique_ptr<struct directory *, decltype(release)> owner(entries, release);
    std::vector<std::string> roots;
    for (size_t index = 0; index < count; ++index)
	roots.emplace_back(entries[index]->d_namep);
    return roots;
}

std::filesystem::path
multi_file_path(const std::filesystem::path &directory, const std::string &name)
{
    Vls sanitized;
    db_sanitize_name(&sanitized.value, name.c_str());
    const std::string stem = bu_vls_strlen(&sanitized.value) ? bu_vls_cstr(&sanitized.value) : "region";
    auto candidate = directory / (stem + ".igs");
    for (size_t suffix = 1; std::filesystem::exists(candidate); ++suffix)
	candidate = directory / (stem + '-' + std::to_string(suffix) + ".igs");
    return candidate;
}

int
convert(int argc, const char **argv)
{
    const char *program = argv[0];
    ExportOptions settings;
    struct g_iges_args args = {};
    args.absolute_tolerance = settings.tessellation.abs;
    args.relative_tolerance = settings.tessellation.rel;
    args.normal_tolerance = settings.tessellation.norm;
    args.distance_tolerance = settings.tolerance.dist;
    args.rt_debug_mask = rt_debug;
    args.nmg_debug_mask = nmg_debug;
    args.processors = 1;
    Vls messages;
    int operand_index = bu_arg_schema_parse_complete(&g_iges_schema, &args,
	&messages.value, argc - 1, argv + 1);
    ++argv;
    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&messages.value));
	return BRLCAD_ERROR;
    }
    if (args.help) {
	char *help = bu_arg_schema_help(&g_iges_schema, program);
	bu_log("%s", help ? help : "");
	bu_free(help, "IGES help");
	return BRLCAD_OK;
    }
    argc = argc - 1 - operand_index;
    argv += operand_index;
    settings.tessellation.abs = args.absolute_tolerance;
    settings.tessellation.rel = args.relative_tolerance;
    settings.tessellation.norm = args.normal_tolerance;
    settings.tolerance.dist = args.distance_tolerance;
    rt_debug = args.rt_debug_mask;
    nmg_debug = args.nmg_debug_mask;
    const char *output_path = args.output_path;
    validate_paths(argv[0], {});
    std::unique_ptr<struct db_i, decltype(&db_close)> database(db_open(argv[0], DB_OPEN_READONLY), db_close);
    if (!database || db_dirbuild(database.get()) < 0)
	throw std::runtime_error("cannot open geometry database");
    bool paired_output = argc > 1 && (args.multi_file ?
	std::filesystem::is_directory(argv[1]) : iges_output_name(argv[1]));
    if (paired_output && db_lookup(database.get(), argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	if (!output_path)
	    throw std::invalid_argument("second argument names both a possible output and a database object; use -o to specify the destination");
	paired_output = false;
    }
    if (paired_output && !output_path)
	output_path = argv[1];
    const int first_object = paired_output ? 2 : 1;
    if (!output_path && argc == first_object)
	throw std::invalid_argument("specify an output file or at least one object for stdout export");
    if (args.faceted + args.trimmed + args.multi_file > 1 || (args.multi_file && !output_path))
	throw std::invalid_argument("invalid output mode or worker count");
    validate_export_options(settings);
    settings.mode = args.faceted ? ExportOptions::Mode::Faceted :
	args.trimmed || args.multi_file ? ExportOptions::Mode::Trimmed : ExportOptions::Mode::Csg;
    settings.nurbs_facets = args.nurbs != 0;
    settings.flatten_brep = args.flatten != 0;
    settings.verbose = args.verbose != 0;
    if (args.processors > 1)
	bu_log("IGES: serializing conversion to protect NMG recovery and record ordering\n");
    validate_paths(argv[0], !args.multi_file && output_path ? std::vector<std::string>{output_path} : std::vector<std::string>{});
    if (args.multi_file && !std::filesystem::is_directory(output_path))
	throw std::invalid_argument("multi-file output must be an existing directory");
    ProgressReporter progress;
    settings.progress = [&](const char *stage, const char *activity, size_t completed, size_t total) {
	progress.update(stage, activity, completed, total);
    };
    std::vector<std::string> roots(argv + first_object, argv + argc);
    if (roots.empty())
	roots = top_level_objects(database.get());
    if (args.multi_file) {
	const auto regions = collect_regions(database.get(), roots);
	bool success = !regions.empty();
	size_t completed = 0;
	for (const auto &region : regions) {
	    progress.update("multi-file export", region.path.c_str(), completed++, regions.size());
	    const auto destination = multi_file_path(output_path, region.name);
	    validate_paths(argv[0], {destination.string()});
	    Writer writer(settings, {region.path});
	    if (!export_occurrence(writer, database.get(), region)) {
		success = false;
		continue;
	    }
	    StagedFile staged(destination);
	    File output(std::fopen(staged.path().c_str(), "wb"));
	    if (!output)
		throw std::runtime_error("cannot create IGES output");
	    writer.finish(output.get(), argv[0], destination.string());
	    close_file(output);
	    staged.publish();
	    success = success && writer.omissions() == 0;
	}
	return success ? BRLCAD_OK : BRLCAD_ERROR;
    }
    Writer writer(settings, std::set<std::string>(roots.begin(), roots.end()));
    const bool wrote = export_objects(writer, database.get(), roots);
    progress.update("finalization", "writing IGES sections");
    if (output_path) {
	StagedFile staged(output_path);
	File output(std::fopen(staged.path().c_str(), "wb"));
	if (!output)
	    throw std::runtime_error("cannot create IGES output");
	writer.finish(output.get(), argv[0], output_path);
	close_file(output);
	staged.publish();
    } else {
	writer.finish(stdout, argv[0], "");
    }
    writer.print_statistics();
    return wrote && !writer.omissions() ? BRLCAD_OK : BRLCAD_ERROR;
}
} // namespace

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    const struct bu_arg_grammar grammar = {&g_iges_schema, NULL, NULL};
    Vls schema_output;
    const int schema_status = bu_arg_introspect(&schema_output.value, &grammar,
	argc - 1, argv + 1);
    if (schema_status != 0) {
	bu_flog(schema_status > 0 ? stdout : stderr, "%s", bu_vls_cstr(&schema_output.value));
	return schema_status > 0 ? BRLCAD_OK : BRLCAD_ERROR;
    }
    try {
	return convert(argc, argv);
    } catch (const std::exception &error) {
	bu_log("g-iges: %s\n", error.what());
    } catch (...) {
	bu_log("g-iges: unexpected conversion failure\n");
    }
    return BRLCAD_ERROR;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
