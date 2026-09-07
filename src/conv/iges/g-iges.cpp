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
#include "bu/opt.h"
#include "bu/str.h"
#include "nmg/globals.h"

namespace {
using namespace brlcad::iges;

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
    bu_setprogname(program);
    ExportOptions settings;
    int processors = 1;
    int help = 0, faceted = 0, trimmed = 0, multi_file = 0, nurbs = 0, flatten = 0, verbose = 0;
    const char *output_path = nullptr;
    struct bu_opt_desc options[] = {
	{"h", "help", "", nullptr, &help, "print help and exit"},
	{"?", "", "", nullptr, &help, ""},
	{"f", "faceted", "", nullptr, &faceted, "tessellate each region to a faceted B-Rep"},
	{"t", "trimmed-surfaces", "", nullptr, &trimmed, "tessellate regions to trimmed surfaces"},
	{"m", "multi-file", "", nullptr, &multi_file, "write one trimmed-surface file per region"},
	{"s", "nurbs", "", nullptr, &nurbs, "write NMG facets using NURBS geometry"},
	{"", "flatten-brep", "", nullptr, &flatten, "write exact B-Reps as IGES 144 surfaces"},
	{"v", "verbose", "", nullptr, &verbose, "enable verbose diagnostics"},
	{"a", "absolute-tolerance", "MM", bu_opt_fastf_t, &settings.tessellation.abs, "absolute tessellation tolerance"},
	{"r", "relative-tolerance", "FRACTION", bu_opt_fastf_t, &settings.tessellation.rel, "relative tessellation tolerance"},
	{"n", "normal-tolerance", "RADIANS", bu_opt_fastf_t, &settings.tessellation.norm, "surface-normal tessellation tolerance"},
	{"d", "distance-tolerance", "MM", bu_opt_fastf_t, &settings.tolerance.dist, "minimum distance between distinct points"},
	{"x", "rt-debug", "HEX", parse_debug_mask, &rt_debug, "librt hexadecimal debug mask"},
	{"X", "nmg-debug", "HEX", parse_debug_mask, &nmg_debug, "NMG hexadecimal debug mask"},
	{"o", "output", "FILE", bu_opt_str, &output_path, "output file, or directory in multi-file mode"},
	{"P", "processors", "COUNT", bu_opt_int, &processors, "maximum worker count (IGES conversion is serialized)"},
	BU_OPT_DESC_NULL
    };
    Vls messages;
    argc = bu_opt_parse(&messages.value, argc - 1, argv + 1, options);
    ++argv;
    if (help || argc < 1 || bu_vls_strlen(&messages.value)) {
	char *description = bu_opt_describe(options, nullptr);
	bu_log("Usage: %s [options] database.g [output.igs] [object ...]\n%s", program, description ? description : "");
	bu_free(description, "IGES option description");
	if (bu_vls_strlen(&messages.value))
	    bu_log("%s\n", bu_vls_cstr(&messages.value));
	return help ? BRLCAD_OK : BRLCAD_ERROR;
    }
    validate_paths(argv[0], {});
    std::unique_ptr<struct db_i, decltype(&db_close)> database(db_open(argv[0], DB_OPEN_READONLY), db_close);
    if (!database || db_dirbuild(database.get()) < 0)
	throw std::runtime_error("cannot open geometry database");
    bool paired_output = argc > 1 && (multi_file ?
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
    if (faceted + trimmed + multi_file > 1 || (multi_file && !output_path) || processors < 1)
	throw std::invalid_argument("invalid output mode or worker count");
    validate_export_options(settings);
    settings.mode = faceted ? ExportOptions::Mode::Faceted :
	trimmed || multi_file ? ExportOptions::Mode::Trimmed : ExportOptions::Mode::Csg;
    settings.nurbs_facets = nurbs != 0;
    settings.flatten_brep = flatten != 0;
    settings.verbose = verbose != 0;
    if (processors > 1)
	bu_log("IGES: serializing conversion to protect NMG recovery and record ordering\n");
    validate_paths(argv[0], !multi_file && output_path ? std::vector<std::string>{output_path} : std::vector<std::string>{});
    if (multi_file && !std::filesystem::is_directory(output_path))
	throw std::invalid_argument("multi-file output must be an existing directory");
    ProgressReporter progress;
    settings.progress = [&](const char *stage, const char *activity, size_t completed, size_t total) {
	progress.update(stage, activity, completed, total);
    };
    std::vector<std::string> roots(argv + first_object, argv + argc);
    if (roots.empty())
	roots = top_level_objects(database.get());
    if (multi_file) {
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
