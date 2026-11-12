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
#include "bu/arg.h"
#include "nmg.h"
#include "wdb.h"

namespace {
using namespace brlcad::iges;

struct iges_g_args {
    int help;
    int drawing_3d;
    int drawing;
    int mesh;
    int nurbs;
    int trimmed;
    int polygon;
    const char *output_path;
    const char *root;
    int rt_debug_mask;
    int nmg_debug_mask;
    int exact;
    int strict;
    fastf_t default_plate_thickness;
    fastf_t maximum_repair_tolerance;
    fastf_t relative_tolerance;
    const char *repair;
    const char *report_path;
    const char *orientation_path;
    int wire;
};

static const struct bu_arg_option iges_g_options[] = {
    BU_ARG_FLAG("h", "help", struct iges_g_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("3", "3d-drawings", struct iges_g_args, drawing_3d, "Preserve drawing model-space planes"),
    BU_ARG_FLAG("d", "drawings", struct iges_g_args, drawing, "Import drawings and annotations"),
    BU_ARG_FLAG("m", "mesh", struct iges_g_args, mesh, "Write boundary representations as BoT meshes"),
    BU_ARG_FLAG("n", "nurbs", struct iges_g_args, nurbs, "Import spline surfaces as B-Reps"),
    BU_ARG_FLAG("t", "trimmed-surfaces", struct iges_g_args, trimmed, "Import trimmed surfaces as B-Reps"),
    BU_ARG_FLAG("p", "polygonal", struct iges_g_args, polygon, "Write boundary representations as polygonal NMG solids"),
    BU_ARG_VALUE("o", "output", struct iges_g_args, output_path, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "FILE", "BRL-CAD output database"),
    BU_ARG_VALUE("N", "name", struct iges_g_args, root, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "NAME", "Name of imported geometry root"),
    BU_ARG_VALUE("x", "rt-debug", struct iges_g_args, rt_debug_mask, BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "HEX", "librt hexadecimal debug mask"),
    BU_ARG_VALUE("X", "nmg-debug", struct iges_g_args, nmg_debug_mask, BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "HEX", "NMG hexadecimal debug mask"),
    BU_ARG_FLAG(NULL, "exact", struct iges_g_args, exact, "Disallow source-data repairs"),
    BU_ARG_FLAG(NULL, "strict", struct iges_g_args, strict, "Reject repaired or partial imports"),
    BU_ARG_VALUE(NULL, "default-plate-thickness", struct iges_g_args, default_plate_thickness, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "MM", "Thickness assigned to imported non-solid B-Reps"),
    BU_ARG_VALUE(NULL, "max-repair-tolerance", struct iges_g_args, maximum_repair_tolerance, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "MM", "Maximum permitted boundary repair tolerance"),
    BU_ARG_VALUE(NULL, "relative-tolerance", struct iges_g_args, relative_tolerance, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "FRACTION", "Local boundary repair target"),
    BU_ARG_VALUE(NULL, "repair", struct iges_g_args, repair, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "MODE", "Repair mode: none, safe, or best-effort"),
    BU_ARG_VALUE(NULL, "report", struct iges_g_args, report_path, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "FILE", "Structured JSON import report"),
    BU_ARG_VALUE(NULL, "check-orientation", struct iges_g_args, orientation_path, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "FILE.g", "Read-only orientation check of an existing database"),
    BU_ARG_FLAG(NULL, "legacy-drawings", struct iges_g_args, wire, "Write drawing curves as NMG wire geometry"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand iges_g_operands[] = {
    BU_ARG_OPERAND("input-or-output", BU_ARG_VALUE_FILE, 0, 2, "Input IGES and optional output database", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema iges_g_schema = BU_ARG_SCHEMA(
    "iges-g", "Import IGES geometry into a BRL-CAD database", iges_g_options,
    iges_g_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

void
usage(const char *program)
{
    char *help = bu_arg_schema_help(&iges_g_schema, program);
    bu_log("%s", help ? help : "");
    bu_log("Orientation check: %s --check-orientation input.g [--report report.json]\n", program);
    bu_free(help, "IGES help");
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
    ImportOptions settings;
    struct iges_g_args args = {};
    args.default_plate_thickness = settings.default_plate_thickness;
    args.maximum_repair_tolerance = settings.maximum_repair_tolerance;
    args.relative_tolerance = settings.relative_tolerance;
    args.rt_debug_mask = rt_debug;
    args.nmg_debug_mask = nmg_debug;
    Vls messages;
    int operand_index = bu_arg_schema_parse_complete(&iges_g_schema, &args,
	&messages.value, argc - 1, argv + 1);
    ++argv;
    if (args.help) {
	usage(program);
	return BRLCAD_OK;
    }
    if (operand_index < 0)
	throw std::invalid_argument(bu_vls_cstr(&messages.value));
    argc = argc - 1 - operand_index;
    argv += operand_index;
    settings.default_plate_thickness = args.default_plate_thickness;
    settings.maximum_repair_tolerance = args.maximum_repair_tolerance;
    settings.relative_tolerance = args.relative_tolerance;
    rt_debug = args.rt_debug_mask;
    nmg_debug = args.nmg_debug_mask;
    if (args.orientation_path) {
        if (argc || args.output_path || args.root || args.repair || args.drawing || args.drawing_3d || args.nurbs || args.trimmed ||
	    args.mesh || args.polygon || args.exact || args.strict || args.wire || !ZERO(settings.default_plate_thickness) ||
	    !ZERO(settings.maximum_repair_tolerance) || !EQUAL(settings.relative_tolerance, DEFAULT_RELATIVE_TOLERANCE))
	    throw std::invalid_argument("--check-orientation accepts only --report and debug options; it never writes geometry");
	return check_existing(args.orientation_path, args.report_path);
    }
    int drawing = args.drawing || args.drawing_3d;
    const char *output_path = args.output_path;
    if (argc == 2 && !output_path)
	output_path = argv[1];
    if ((argc != 1 && argc != 2) || !output_path || drawing + args.nurbs + args.trimmed > 1 || args.mesh + args.polygon > 1) {
	usage(program);
	return BRLCAD_ERROR;
    }
    settings.repair = parse_repair_mode(args.repair);
    settings.output = args.mesh ? GeometryOutput::Mesh : args.polygon ? GeometryOutput::Polygon : GeometryOutput::Brep;
    settings.exact = args.exact != 0;
    settings.strict = args.strict != 0;
    validate_import_options(settings, drawing != 0);
    settings.project_drawings = !args.drawing_3d;
    settings.wire_drawings = args.wire != 0;
    settings.root_name = args.root ? args.root : drawing ? "iges_drawing" : "iges_geometry";
    validate_paths(argv[0], {output_path, args.report_path ? args.report_path : ""});
    ProgressReporter progress;
    settings.progress = [&](const char *stage, const char *activity, size_t completed,
	size_t total, int64_t entity) { progress.update(stage, activity, completed, total, entity); };
    ON::Begin();
    const Document document = Document::parse_file(argv[0]);
    if (document.valid() && !drawing && !args.nurbs && !args.trimmed && !args.mesh && !args.polygon &&
	!has_model_geometry(document)) {
	drawing = 1;
	settings.project_drawings = false;
	settings.root_name = args.root ? args.root : "iges_drawing";
	bu_log("IGES: no bounded surfaces or solids; importing model-space drawings and datums\n");
    }
    StagedFile output(output_path);
    std::unique_ptr<StagedFile> report;
    if (args.report_path)
	report = std::make_unique<StagedFile>(args.report_path);
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
