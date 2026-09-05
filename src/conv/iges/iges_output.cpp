/*                    I G E S _ O U T P U T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"

#include "iges_output.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "json.hpp"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "raytrace.h"
#include "wdb.h"

namespace {

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;
constexpr unsigned int STAGING_ATTEMPTS = 64;

struct StagedFile {
    std::string target;
    std::string path;
    fs::path directory;

    ~StagedFile()
    {
	std::error_code error;
	if (!path.empty())
	    fs::remove(path, error);
	if (!directory.empty()) {
	    error.clear();
	    fs::remove(directory, error);
	    if (error)
		bu_log("IGES: could not remove temporary output directory: %s\n", error.message().c_str());
	}
    }

    bool create(const fs::path &destination)
    {
	target = destination.string();
	const std::string stem = std::string(".iges-import-") + bu_temp_file_name(nullptr, 0);
	for (unsigned int i = 0; i < STAGING_ATTEMPTS; ++i) {
	    const fs::path candidate = destination.parent_path() / (stem + '-' + std::to_string(i));
	    std::error_code error;
	    if (!fs::create_directory(candidate, error)) {
		if (error && error != std::errc::file_exists)
		    return false;
		continue;
	    }
	    directory = candidate;
	    fs::permissions(directory, fs::perms::owner_all, fs::perm_options::replace, error);
	    if (error)
		return false;
	    path = (directory / destination.filename()).string();
	    return true;
	}
	return false;
    }

    bool publish()
    {
	std::error_code error;
	fs::rename(path, target, error);
	if (error)
	    bu_log("IGES: could not publish %s: %s\n", target.c_str(), error.message().c_str());
	return !error;
    }
};

struct ConversionOutput {
    StagedFile database;
    StagedFile report;
    std::string source;
    bool strict = false;
    size_t legacy_seen = 0;
    size_t legacy_written = 0;
    Json legacy_diagnostics = Json::array();
};

bool
publish_report(StagedFile &file, const Json &report)
{
    std::ofstream output(file.path, std::ios::trunc);
    output << report.dump(2) << '\n';
    output.close();
    return output && file.publish();
}

/* Legacy handlers may call exit() rather than returning.  Process-lifetime
 * ownership ensures their staging files are still cleaned by atexit. */
std::unique_ptr<ConversionOutput> conversion;

void
cleanup_output()
{
    conversion.reset();
}

size_t
missing_leaves(const union tree *tree, struct db_i *database)
{
    if (!tree)
	return 0;
    if (tree->tr_op == OP_DB_LEAF)
	return db_lookup(database, tree->tr_l.tl_name, LOOKUP_QUIET) == RT_DIR_NULL;
    if (tree->tr_op == OP_NOP)
	return 0;
    if (tree->tr_op == OP_NOT || tree->tr_op == OP_GUARD || tree->tr_op == OP_XNOP)
	return missing_leaves(tree->tr_b.tb_left, database);
    return missing_leaves(tree->tr_b.tb_left, database) + missing_leaves(tree->tr_b.tb_right, database);
}

} // namespace

extern "C" int
iges_output_begin(const char *input, const char *output, const char *report, int strict)
{
    try {
	if (conversion || !input || !output || !fs::is_regular_file(input)) {
	    bu_log("IGES: input must identify a readable regular file\n");
	    return 0;
	}
	std::vector<fs::path> paths = {fs::weakly_canonical(input), fs::weakly_canonical(output)};
	if (report && report[0])
	    paths.push_back(fs::weakly_canonical(report));
	for (size_t i = 1; i < paths.size(); ++i)
	    if (fs::exists(paths[i]) && !fs::is_regular_file(paths[i])) {
		bu_log("IGES: output destinations must be regular files\n");
		return 0;
	    }
	for (size_t i = 0; i < paths.size(); ++i)
	    for (size_t j = 0; j < i; ++j)
		if (paths[i] == paths[j] || bu_file_same(paths[i].string().c_str(), paths[j].string().c_str())) {
		    bu_log("IGES: input, output, and report must be distinct files\n");
		    return 0;
		}
	conversion.reset(new ConversionOutput());
	std::atexit(cleanup_output);
	conversion->source = input;
	conversion->strict = strict != 0;
	if (!conversion->database.create(paths[1]) ||
	    (paths.size() == 3 && !conversion->report.create(paths[2]))) {
	    bu_log("IGES: cannot create staged output beside the requested destination\n");
	    conversion.reset();
	    return 0;
	}
	return 1;
    } catch (const std::exception &error) {
	bu_log("IGES: cannot initialize output: %s\n", error.what());
	conversion.reset();
	return 0;
    }
}

extern "C" const char *
iges_output_database_path(void)
{
    return conversion ? conversion->database.path.c_str() : nullptr;
}

extern "C" const char *
iges_output_report_path(void)
{
    return conversion && !conversion->report.path.empty() ? conversion->report.path.c_str() : nullptr;
}

extern "C" void
iges_output_legacy_entity(int id, int type, const char *name, int written)
{
    if (!conversion)
	return;
    ++conversion->legacy_seen;
    if (written) {
	++conversion->legacy_written;
	return;
    }
    bu_log("IGES: legacy conversion omitted D%d (%s)\n", id, name ? name : "unnamed");
    conversion->legacy_diagnostics.push_back({{"severity", "warning"}, {"code", "legacy_geometry_omitted"},
	{"message", "legacy conversion did not write the source geometry"}, {"entity", id}, {"type", type}});
}

extern "C" int
iges_output_finish(struct rt_wdb *wdbp, int success)
{
    if (!conversion) {
	if (wdbp)
	    wdb_close(wdbp);
	return BRLCAD_ERROR;
    }
    size_t objects = 0;
    size_t unresolved = 0;
    if (wdbp && wdbp->dbip) {
	struct directory *entry;
	FOR_ALL_DIRECTORY_START(entry, wdbp->dbip) {
	    if (entry->d_major_type != DB5_MAJORTYPE_BRLCAD)
		continue;
	    if (!(entry->d_flags & RT_DIR_COMB)) {
		++objects;
		continue;
	    }
	    struct rt_db_internal internal;
	    RT_DB_INTERNAL_INIT(&internal);
	    if (rt_db_get_internal(&internal, entry, wdbp->dbip, nullptr) < 0) {
		success = 0;
		continue;
	    }
	    const auto *combination = static_cast<const struct rt_comb_internal *>(internal.idb_ptr);
	    unresolved += missing_leaves(combination->tree, wdbp->dbip);
	    rt_db_free_internal(&internal);
	} FOR_ALL_DIRECTORY_END;
    }
    const size_t omitted = conversion->legacy_seen - conversion->legacy_written;
    success = success && objects > 0 && (!conversion->strict || (!omitted && !unresolved));
    if (!objects)
	bu_log("IGES: no supported geometry was created\n");
    if (unresolved)
	bu_log("IGES: output contains %zu unresolved geometry references\n", unresolved);
    if (wdbp)
	wdb_close(wdbp);
    try {
	Json report;
	if (!conversion->report.path.empty()) {
	    report = {{"format", "iges"}, {"source", conversion->source},
		{"statistics", Json::object()}, {"diagnostics", Json::array()}};
	    std::ifstream existing(conversion->report.path);
	    if (existing)
		existing >> report;
	    existing.close();
	    report["success"] = success != 0;
	    auto &statistics = report["statistics"];
	    statistics["objects_written"] = objects;
	    statistics["legacy_entities_seen"] = conversion->legacy_seen;
	    statistics["legacy_entities_written"] = conversion->legacy_written;
	    statistics["omitted"] = statistics.value("omitted", size_t(0)) + omitted;
	    statistics["unresolved_output_references"] = unresolved;
	    for (const auto &diagnostic : conversion->legacy_diagnostics)
		report["diagnostics"].push_back(diagnostic);
	    if (!objects)
		report["diagnostics"].push_back({{"severity", "error"}, {"code", "no_supported_geometry"},
		    {"message", "no supported geometry was created"}});
	    if (!publish_report(conversion->report, report))
		success = 0;
	}
	if (success && !conversion->database.publish()) {
	    success = 0;
	    if (!conversion->report.path.empty()) {
		report["success"] = false;
		report["diagnostics"].push_back({{"severity", "error"}, {"code", "database_publish"},
		    {"message", "could not publish the converted database"}});
		if (!publish_report(conversion->report, report))
		    bu_log("IGES: could not update the report after database publication failed\n");
	    }
	}
    } catch (const std::exception &error) {
	bu_log("IGES: could not finalize output: %s\n", error.what());
	success = 0;
    }
    conversion.reset();
    return success ? BRLCAD_OK : BRLCAD_ERROR;
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
