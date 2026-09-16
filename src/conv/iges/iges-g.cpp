/* BRL-CAD
 * Copyright (c) 1990-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_brep_import.h"
#include "iges_convert.h"
#include "iges_runtime.h"

#include <memory>
#include <stdexcept>

#include "brep.h"
#include "bu/app.h"
#include "bu/debug.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "nmg.h"
#include "wdb.h"

namespace {
using namespace brlcad::iges;

void
usage(const char *program, const struct bu_opt_desc *options)
{
    char *description = bu_opt_describe(options, nullptr);
    bu_log("Usage: %s [options] [-o output.g] input.iges [output.g]\n%s", program,
	description ? description : "");
    bu_log("       %s --check-orientation input.g [--report report.json]\n", program);
    bu_free(description, "IGES option description");
}

template <typename Result, typename ReportWriter>
int
finish_import(const Document &document, const ImportOptions &options, Result &result,
    std::unique_ptr<struct rt_wdb, decltype(&wdb_close)> &database,
    StagedFile &output, StagedFile *report, const ReportWriter &write_report,
    ProgressReporter &progress)
{
    progress.update("finalization", "checking output references");
    const OutputStatistics statistics = check_output(database->dbip);
    result.statistics.objects_written = statistics.objects;
    result.statistics.unresolved_output_references = statistics.unresolved;
    result.success = result.success && statistics.objects > 0 && statistics.unresolved == 0;
    if (!statistics.objects)
	result.diagnostics.push_back({Severity::Error, "no_supported_geometry",
	    "no supported geometry was created", 0, 0});
    if (statistics.unresolved)
	result.diagnostics.push_back({Severity::Error, "unresolved_output_references",
	    "output contains unresolved geometry references", 0, 0});
    log_import_diagnostics(document, result.diagnostics);
    bu_log("IGES: wrote %zu geometry objects from %zu source entities; omitted %zu\n",
	statistics.objects, document.entities().size(), result.statistics.omitted);
    database.reset();
    progress.update("finalization", "publishing database and report");
    if (report && !write_report(report->path(), document, options, result))
	throw std::runtime_error("cannot write import report");
    try {
	if (result.success)
	    output.publish();
    } catch (const std::exception &error) {
	result.success = false;
	result.diagnostics.push_back({Severity::Error, "database_publish", error.what(), 0, 0});
	if (report && !write_report(report->path(), document, options, result))
	    throw std::runtime_error("cannot update import report after publication failure");
	bu_log("IGES: %s\n", error.what());
    }
    if (report)
	report->publish();
    return result.success ? BRLCAD_OK : BRLCAD_ERROR;
}

int
check_existing(const char *path, const char *report_path)
{
    validate_paths(path, {report_path ? report_path : ""});
    std::unique_ptr<struct db_i, decltype(&db_close)> database(db_open(path, DB_OPEN_READONLY), db_close);
    if (!database || db_dirbuild(database.get()) < 0)
	throw std::runtime_error("cannot open database for orientation check");
    ProgressReporter progress;
    ON::Begin();
    const auto result = check_database_orientation(database.get(), &progress);
    log_orientation_report(result);
    if (report_path) {
	StagedFile report(report_path);
	if (!write_orientation_report(report.path(), result))
	    throw std::runtime_error("cannot write orientation report");
	report.publish();
    }
    return result.read_failures ? BRLCAD_ERROR : BRLCAD_OK;
}

int
convert(int argc, const char **argv)
{
    const char *program = argv[0];
    int help = 0, drawing = 0, drawing_3d = 0, nurbs = 0, trimmed = 0;
    int mesh = 0, polygon = 0, exact = 0, strict = 0, wire = 0;
    const char *output_path = nullptr, *report_path = nullptr, *root = nullptr, *repair = nullptr;
    const char *orientation_path = nullptr;
    ImportOptions settings;
    struct bu_opt_desc options[] = {
	{"h", "help", "", nullptr, &help, "print help and exit"},
	{"?", "", "", nullptr, &help, ""},
	{"3", "3d-drawings", "", nullptr, &drawing_3d, "preserve drawing model-space planes"},
	{"d", "drawings", "", nullptr, &drawing, "import drawings and annotations"},
	{"m", "mesh", "", nullptr, &mesh, "write boundary representations as BoT meshes"},
	{"n", "nurbs", "", nullptr, &nurbs, "import spline surfaces as B-Reps"},
	{"t", "trimmed-surfaces", "", nullptr, &trimmed, "import trimmed surfaces as B-Reps"},
	{"p", "polygonal", "", nullptr, &polygon, "write boundary representations as polygonal NMG solids"},
	{"o", "output", "FILE", bu_opt_str, &output_path, "BRL-CAD output database"},
	{"N", "name", "NAME", bu_opt_str, &root, "name of imported geometry root"},
	{"x", "rt-debug", "HEX", parse_debug_mask, &rt_debug, "librt hexadecimal debug mask"},
	{"X", "nmg-debug", "HEX", parse_debug_mask, &nmg_debug, "NMG hexadecimal debug mask"},
	{"", "exact", "", nullptr, &exact, "disallow source-data repairs"},
	{"", "strict", "", nullptr, &strict, "reject repaired or partial imports"},
	{"", "default-plate-thickness", "MM", bu_opt_fastf_t, &settings.default_plate_thickness,
	    "assign this thickness to imported non-solid B-Reps"},
	{"", "max-repair-tolerance", "MM", bu_opt_fastf_t, &settings.maximum_repair_tolerance,
	    "permit and flag boundary repairs up to this tolerance"},
	{"", "relative-tolerance", "FRACTION", bu_opt_fastf_t, &settings.relative_tolerance,
	    "local boundary repair target (default 0.0001 of box diagonal; 0 disables)"},
	{"", "repair", "MODE", bu_opt_str, &repair, "none, safe, or best-effort (default)"},
	{"", "report", "FILE", bu_opt_str, &report_path, "structured JSON import report"},
	{"", "check-orientation", "FILE.g", bu_opt_str, &orientation_path, "read-only orientation check of an existing database"},
	{"", "legacy-drawings", "", nullptr, &wire, "write drawing curves as NMG wire geometry"},
	BU_OPT_DESC_NULL
    };
    Vls messages;
    argc = bu_opt_parse(&messages.value, argc - 1, argv + 1, options);
    ++argv;
    if (help) {
	usage(program, options);
	return BRLCAD_OK;
    }
    if (argc < 0 || bu_vls_strlen(&messages.value))
	throw std::invalid_argument(bu_vls_cstr(&messages.value));
    if (orientation_path) {
	if (argc || output_path || root || repair || drawing || drawing_3d || nurbs || trimmed ||
	    mesh || polygon || exact || strict || wire || !ZERO(settings.default_plate_thickness) ||
	    !ZERO(settings.maximum_repair_tolerance) || !EQUAL(settings.relative_tolerance, DEFAULT_RELATIVE_TOLERANCE))
	    throw std::invalid_argument("--check-orientation accepts only --report and debug options; it never writes geometry");
	return check_existing(orientation_path, report_path);
    }
    drawing = drawing || drawing_3d;
    if (argc == 2 && !output_path)
	output_path = argv[1];
    if ((argc != 1 && argc != 2) || !output_path || drawing + nurbs + trimmed > 1 || mesh + polygon > 1) {
	usage(program, options);
	return BRLCAD_ERROR;
    }
    settings.repair = parse_repair_mode(repair);
    settings.output = mesh ? GeometryOutput::Mesh : polygon ? GeometryOutput::Polygon : GeometryOutput::Brep;
    settings.exact = exact != 0;
    settings.strict = strict != 0;
    validate_import_options(settings, drawing != 0);
    settings.project_drawings = !drawing_3d;
    settings.wire_drawings = wire != 0;
    settings.root_name = root ? root : drawing ? "iges_drawing" : "iges_geometry";
    validate_paths(argv[0], {output_path, report_path ? report_path : ""});
    ProgressReporter progress;
    settings.progress = [&](const char *stage, const char *activity, size_t completed,
	size_t total, int64_t entity) { progress.update(stage, activity, completed, total, entity); };
    ON::Begin();
    const Document document = Document::parse_file(argv[0]);
    if (document.valid() && !drawing && !nurbs && !trimmed && !mesh && !polygon &&
	!has_model_geometry(document)) {
	drawing = 1;
	settings.project_drawings = false;
	settings.root_name = root ? root : "iges_drawing";
	bu_log("IGES: no bounded surfaces or solids; importing model-space drawings and datums\n");
    }
    StagedFile output(output_path);
    std::unique_ptr<StagedFile> report;
    if (report_path)
	report = std::make_unique<StagedFile>(report_path);
    std::unique_ptr<struct rt_wdb, decltype(&wdb_close)> database(wdb_fopen(output.path().c_str()), wdb_close);
    if (!database)
	throw std::runtime_error("cannot create output database");
    if (mk_id(database.get(), "IGES import") < 0)
	throw std::runtime_error("cannot write database identification");
    if (drawing) {
	auto result = import_annotations(document, database.get(), settings);
	return finish_import(document, settings, result, database, output, report.get(), write_import_report, progress);
    }
    auto result = import_breps(document, database.get(), settings);
    return finish_import(document, settings, result, database, output, report.get(), write_brep_import_report, progress);
}
} // namespace

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    try {
	return convert(argc, argv);
    } catch (const std::exception &error) {
	bu_log("iges-g: %s\n", error.what());
    } catch (...) {
	bu_log("iges-g: unexpected conversion failure\n");
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
