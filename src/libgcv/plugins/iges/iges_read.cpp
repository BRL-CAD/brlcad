/*                    I G E S _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"
#include "../mime_probe.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

#include "brep.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "wdb.h"

#include "iges_brep_import.h"
#include "iges_convert.h"
#include "iges_plugin.h"
#include "iges_runtime.h"

namespace {
using namespace brlcad::iges;

struct IgesReadOptions {
    int drawings_only = 0;
    int breps_only = 0;
    int drawings_3d = 0;
    int exact = 0;
    int strict = 0;
    fastf_t maximum_repair_tolerance = 0.0;
    fastf_t relative_tolerance = DEFAULT_RELATIVE_TOLERANCE;
    const char *repair = nullptr;
};

void
iges_create_options(struct bu_opt_desc **descriptions, void **options_data)
{
    auto options = std::make_unique<IgesReadOptions>();
    struct bu_opt_desc descriptors[] = {
	{"", "drawings-only", "", nullptr, &options->drawings_only, "import drawing and annotation entities only"},
	{"", "breps-only", "", nullptr, &options->breps_only, "import solid and surface geometry only, including native CSG"},
	{"", "3d-drawings", "", nullptr, &options->drawings_3d, "preserve drawing model-space planes"},
	{"", "exact", "", nullptr, &options->exact, "disallow source-data repairs"},
	{"", "strict", "", nullptr, &options->strict, "reject repaired or partial imports"},
	{"", "repair", "MODE", bu_opt_str, &options->repair, "none, safe, or best-effort (default)"},
	{"", "max-repair-tolerance", "MM", bu_opt_fastf_t, &options->maximum_repair_tolerance,
	    "permit and flag boundary repairs up to this tolerance"},
	{"", "relative-tolerance", "FRACTION", bu_opt_fastf_t, &options->relative_tolerance,
	    "local boundary repair target (default: 0.0001)"},
	BU_OPT_DESC_NULL
    };
    *descriptions = static_cast<struct bu_opt_desc *>(bu_malloc(sizeof(descriptors), "IGES reader options"));
    std::copy_n(descriptors, sizeof(descriptors) / sizeof(descriptors[0]), *descriptions);
    *options_data = options.release();
}

void
iges_free_options(void *options_data)
{
    delete static_cast<IgesReadOptions *>(options_data);
}

int
iges_can_read(const char *source_path)
{
    if (!source_path)
	return 0;
    try {
	return Document::parse_file(source_path).valid() ? 1 : 0;
    } catch (const std::exception &) {
	return 0;
    }
}

bool
matches_document_profile(const Document &document, struct gcv_context *context, const char *source_path)
{
    /* Global parameter 23 declares the IGES standard, independently of the
     * producer's version string. ANSI editions retain only their raw code. */
    constexpr size_t version_parameter = 23;
    const struct {int code; const char *version;} versions[] = {
	{1, "1.0"}, {3, "2.0"}, {4, "3.0"}, {6, "4.0"},
	{8, "5.0"}, {9, "5.1"}, {10, "5.2"}, {11, "5.3"}
    };
    struct bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    bool valid = bu_mime_traits_add(&observed, "encoding", "ascii") != 0;
    const auto &parameters = document.global().parameters;
    if (parameters.size() >= version_parameter) {
	Parameter parameter;
	parameter.raw = parameters[version_parameter - 1];
	int64_t code;
	if (parameter.integer(code)) {
	    valid = valid && bu_mime_traits_add(&observed, "iges.version-code", std::to_string(code).c_str());
	    for (const auto &version : versions)
		if (code == version.code) valid = valid && bu_mime_traits_add(&observed, "version", version.version);
	}
    }
    valid = valid && gcv_probe_file_traits_match(context, source_path, "model/iges", &observed, gcv_target_request_traits(context));
    if (!valid) {
	struct bu_vls description = BU_VLS_INIT_ZERO;
	bu_mime_traits_describe(&observed, &description);
	bu_log("IGES input traits do not match the parsed document:%s\n", bu_vls_cstr(&description));
	bu_vls_free(&description);
    }
    bu_mime_traits_free(&observed);
    return valid;
}

int
read_document(struct gcv_context *context, const struct gcv_opts &gcv_options,
    const IgesReadOptions &reader, const char *source_path)
{
    ImportOptions options;
    options.exact = reader.exact != 0;
    options.strict = reader.strict != 0;
    options.project_drawings = reader.drawings_3d == 0;
    options.maximum_repair_tolerance = reader.maximum_repair_tolerance;
    options.relative_tolerance = reader.relative_tolerance;
    options.repair = parse_repair_mode(reader.repair);
    if (reader.drawings_only && reader.breps_only)
	throw std::invalid_argument("drawing-only and B-Rep-only imports are mutually exclusive");
    validate_import_options(options, reader.drawings_only != 0);
    validate_plugin_options(gcv_options);

    std::unique_ptr<ProgressReporter> progress;
    if (gcv_options.verbosity_level) {
	progress = std::make_unique<ProgressReporter>();
	options.progress = [&](const char *stage, const char *activity, size_t completed, size_t total, int64_t entity) {
	    progress->update(stage, activity, completed, total, entity);
	};
    }
    ON::Begin();
    const Document document = Document::parse_file(source_path);
    if (!document.valid()) {
	log_import_diagnostics(document, {});
	return 0;
    }

    if (!matches_document_profile(document, context, source_path))
	return 0;

    // The context owns this database and its writer; do not close either here.
    struct rt_wdb *database = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!database)
	throw std::runtime_error("cannot open the output database");
    Vls title;
    bu_path_component(&title.value, source_path, BU_PATH_BASENAME);
    if (mk_id_units(database, bu_vls_cstr(&title.value), "mm") < 0)
	throw std::runtime_error("cannot write database identification");
    if (gcv_options.default_name && gcv_options.default_name[0])
	options.root_name = gcv_options.default_name;
    else if (bu_vls_strlen(&title.value))
	options.root_name = bu_vls_cstr(&title.value);

    bool imported = false;
    bool failed = false;
    std::vector<ImportDiagnostic> diagnostics;
    const auto collect = [&](const auto &result) {
	diagnostics.insert(diagnostics.end(), result.diagnostics.begin(), result.diagnostics.end());
	imported = imported || result.success;
	// An empty translation is expected when a pass has no applicable objects.
	// A pass that wrote objects but rejected its result must not be hidden by
	// another successful representation in the same document.
	failed = failed || (!result.success &&
	    (result.statistics.objects_written > 0 || (options.strict && result.statistics.omitted > 0)));
	if (result.statistics.omitted && !gcv_report_loss(context, GCV_LOSS_GEOMETRY, "iges.omitted-entities",
		result.statistics.omitted, "Unsupported or unconvertible entities were omitted"))
	    failed = true;
	if (result.statistics.source_repairs > result.statistics.repairs ||
	    result.statistics.metadata_repairs > result.statistics.repairs - result.statistics.source_repairs)
	    throw std::logic_error("inconsistent IGES repair statistics");
	const size_t geometry_repairs = result.statistics.repairs - result.statistics.source_repairs - result.statistics.metadata_repairs;
	if (geometry_repairs && !gcv_report_loss(context, GCV_LOSS_GEOMETRY, "iges.geometry-repairs",
		geometry_repairs, "Source geometry or topology was repaired during conversion"))
	    failed = true;
	if (result.statistics.metadata_repairs && !gcv_report_loss(context, GCV_LOSS_METADATA, "iges.metadata-repairs",
		result.statistics.metadata_repairs, "Invalid optional metadata was discarded or repaired"))
	    failed = true;
    };
    const bool model_geometry = has_model_geometry(document);
    if (!reader.drawings_only && (reader.breps_only || model_geometry))
	collect(import_breps(document, database, options));
    if (!reader.breps_only && has_drawing_geometry(document)) {
	if (!reader.drawings_only && !model_geometry)
	    options.project_drawings = false;
	collect(import_annotations(document, database, options));
    }
    failed = failed || std::any_of(diagnostics.begin(), diagnostics.end(), [](const ImportDiagnostic &diagnostic) {
	return diagnostic.severity == Severity::Error || diagnostic.severity == Severity::Fatal;
    });
    const auto output = check_output(context->dbip);
    if (output.unresolved) {
	diagnostics.push_back({Severity::Error, "unresolved_output_references",
	    "output contains unresolved geometry references", 0, 0});
	failed = true;
    }
    log_import_diagnostics(document, diagnostics);
    if (!imported && !failed)
	bu_log("IGES: no supported geometry was created from %s\n", source_path);
    return imported && !failed ? 1 : 0;
}

int
iges_read(struct gcv_context *context, const struct gcv_opts *gcv_options,
    const void *options_data, const char *source_path)
{
    if (!context || !context->dbip || !gcv_options || !options_data || !source_path)
	return 0;
    try {
	return read_document(context, *gcv_options, *static_cast<const IgesReadOptions *>(options_data), source_path);
    } catch (const std::exception &error) {
	bu_log("IGES reader: %s\n", error.what());
    } catch (...) {
	bu_log("IGES reader: unexpected conversion failure\n");
    }
    return 0;
}
} // namespace

const struct gcv_filter iges_reader = {
    "IGES Reader", GCV_FILTER_READ, BU_MIME_MODEL_IGES, iges_can_read,
    iges_create_options, iges_free_options, iges_read
};

const struct gcv_conversion iges_reader_execution = {"IGES Reader", GCV_FILTER_READ, iges_create_options, iges_free_options, iges_read};

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
