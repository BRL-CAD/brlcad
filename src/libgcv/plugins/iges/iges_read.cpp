/*                    I G E S _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include <cmath>
#include <map>
#include <string>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "gcv/api.h"
#include "raytrace.h"
#include "wdb.h"

#include "iges_brep_import.h"
#include "iges_document.h"
#include "iges_import.h"

namespace {

constexpr size_t IGES_OPTION_COUNT = 7;

struct IgesReadOptions {
    int drawings_only = 0;
    int breps_only = 0;
    int drawings_3d = 0;
    int exact = 0;
    int strict = 0;
    fastf_t maximum_repair_tolerance = 0.0;
    char *repair = nullptr;
};

void
iges_create_options(struct bu_opt_desc **descriptions, void **options_data)
{
    struct IgesReadOptions *options;
    BU_ALLOC(options, struct IgesReadOptions);
    *options_data = options;
    *descriptions = static_cast<struct bu_opt_desc *>(bu_calloc(
	IGES_OPTION_COUNT + 1, sizeof(struct bu_opt_desc), "IGES reader options"));
    BU_OPT((*descriptions)[0], nullptr, "drawings-only", "", nullptr,
	&options->drawings_only, "import drawing and annotation entities only");
    BU_OPT((*descriptions)[1], nullptr, "breps-only", "", nullptr,
	&options->breps_only, "import directly supported manifold B-Reps only");
    BU_OPT((*descriptions)[2], nullptr, "3d-drawings", "", nullptr,
	&options->drawings_3d, "preserve drawing model-space planes");
    BU_OPT((*descriptions)[3], nullptr, "exact", "", nullptr,
	&options->exact, "disallow bounded source-data repairs");
    BU_OPT((*descriptions)[4], nullptr, "strict", "", nullptr,
	&options->strict, "reject repaired or partial imports");
    BU_OPT((*descriptions)[5], nullptr, "repair", "MODE", bu_opt_str,
	&options->repair, "none or safe (default: safe)");
    BU_OPT((*descriptions)[6], nullptr, "max-repair-tolerance", "MM",
	bu_opt_fastf_t, &options->maximum_repair_tolerance,
	"permit and flag boundary pullbacks up to this tolerance");
    BU_OPT_NULL((*descriptions)[7]);
}

void
iges_free_options(void *options_data)
{
    bu_free(options_data, "IGES reader options");
}

int
iges_can_read(const char *source_path)
{
    if (!source_path)
	return 0;
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_file(source_path);
    return document.valid() ? 1 : 0;
}

const char *
severity_name(brlcad::iges::Severity severity)
{
    switch (severity) {
	case brlcad::iges::Severity::Information: return "information";
	case brlcad::iges::Severity::Warning: return "warning";
	case brlcad::iges::Severity::Error: return "error";
	case brlcad::iges::Severity::Fatal: return "fatal";
    }
    return "error";
}

void
log_document_diagnostics(const brlcad::iges::Document &document)
{
    struct Summary {
	size_t count = 0;
	brlcad::iges::Severity severity = brlcad::iges::Severity::Warning;
	std::string message;
	size_t record = 0;
    };
    std::map<std::string, Summary> summaries;
    for (const brlcad::iges::Diagnostic &diagnostic : document.diagnostics()) {
	if (diagnostic.severity == brlcad::iges::Severity::Information)
	    continue;
	Summary &summary = summaries[diagnostic.code];
	++summary.count;
	if (summary.message.empty()) {
	    summary.severity = diagnostic.severity;
	    summary.message = diagnostic.message;
	    summary.record = diagnostic.location.record;
	}
    }
    for (const auto &item : summaries) {
	const Summary &summary = item.second;
	const std::string occurrences = summary.count > 1 ?
	    " (" + std::to_string(summary.count) + " occurrences)" :
	    std::string();
	bu_log("IGES %s: %s%s at record %zu: %s\n",
	    severity_name(summary.severity), item.first.c_str(),
	    occurrences.c_str(),
	    summary.record, summary.message.c_str());
    }
}

void
log_import_diagnostics(
    const std::vector<brlcad::iges::ImportDiagnostic> &diagnostics)
{
    struct Summary {
	size_t count = 0;
	brlcad::iges::Severity severity = brlcad::iges::Severity::Warning;
	std::string message;
	int64_t entity = 0;
    };
    std::map<std::string, Summary> summaries;
    for (const brlcad::iges::ImportDiagnostic &diagnostic : diagnostics) {
	if (diagnostic.severity == brlcad::iges::Severity::Information)
	    continue;
	Summary &summary = summaries[diagnostic.code];
	++summary.count;
	if (summary.message.empty()) {
	    summary.severity = diagnostic.severity;
	    summary.message = diagnostic.message;
	    summary.entity = diagnostic.entity_id;
	}
    }
    for (const auto &item : summaries) {
	const Summary &summary = item.second;
	const std::string occurrences = summary.count > 1 ?
	    " (" + std::to_string(summary.count) + " occurrences)" :
	    std::string();
	const std::string entity = summary.entity ?
	    std::to_string(summary.entity) : std::string();
	bu_log("IGES %s: %s%s%s%s: %s\n",
	    severity_name(summary.severity), item.first.c_str(),
	    occurrences.c_str(),
	    summary.entity ? " first for D" : "",
	    entity.c_str(),
	    summary.message.c_str());
    }
}

int
iges_read(struct gcv_context *context, const struct gcv_opts *gcv_options,
    const void *options_data, const char *source_path)
{
    if (!context || !context->dbip || !gcv_options || !source_path)
	return 0;
    const struct IgesReadOptions *reader_options =
	static_cast<const struct IgesReadOptions *>(options_data);
    if (!reader_options ||
	    (reader_options->repair &&
	     !BU_STR_EQUAL(reader_options->repair, "none") &&
	     !BU_STR_EQUAL(reader_options->repair, "safe")) ||
	    (reader_options->drawings_only && reader_options->breps_only) ||
	    !std::isfinite(reader_options->maximum_repair_tolerance) ||
	    reader_options->maximum_repair_tolerance < 0.0 ||
	    (reader_options->maximum_repair_tolerance > 0.0 &&
	     (reader_options->drawings_only || reader_options->exact ||
	      reader_options->strict ||
	      (reader_options->repair &&
	       BU_STR_EQUAL(reader_options->repair, "none"))))) {
	bu_log("IGES: invalid reader options\n");
	return 0;
    }

    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_file(source_path);
    log_document_diagnostics(document);
    if (!document.valid())
	return 0;

    struct rt_wdb *wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	bu_log("IGES: cannot open the output database\n");
	return 0;
    }
    struct bu_vls title = BU_VLS_INIT_ZERO;
    bu_path_component(&title, source_path, BU_PATH_BASENAME);
    mk_id_units(wdbp, bu_vls_cstr(&title), "mm");

    brlcad::iges::ImportOptions options;
    options.exact = reader_options->exact != 0;
    options.strict = reader_options->strict != 0;
    options.project_drawings = reader_options->drawings_3d == 0;
    options.maximum_repair_tolerance =
	reader_options->maximum_repair_tolerance;
    if (reader_options->repair &&
	    BU_STR_EQUAL(reader_options->repair, "none"))
	options.repair = brlcad::iges::RepairMode::None;
    if (gcv_options->default_name && gcv_options->default_name[0] != '\0')
	options.root_name = gcv_options->default_name;
    else if (bu_vls_strlen(&title))
	options.root_name = bu_vls_cstr(&title);

    bool imported = false;
    bool failed = false;
    if (!reader_options->drawings_only) {
	const brlcad::iges::BrepImportResult result =
	    brlcad::iges::import_breps(document, wdbp, options);
	log_import_diagnostics(result.diagnostics);
	imported = result.success || imported;
	failed = (options.strict && result.statistics.omitted) || failed;
    }
    if (!reader_options->breps_only) {
	const brlcad::iges::ImportResult result =
	    brlcad::iges::import_annotations(document, wdbp, options);
	log_import_diagnostics(result.diagnostics);
	imported = result.success || imported;
	failed = (options.strict && result.statistics.omitted) || failed;
    }

    bool unsupported_solids = false;
    for (const brlcad::iges::DirectoryEntry &entry : document.entities())
	if (entry.type >= 150 && entry.type <= 184) {
	    unsupported_solids = true;
	    break;
	}
    if (unsupported_solids && !reader_options->drawings_only) {
	bu_log("IGES: native CSG entities currently require the iges-g "
	    "compatibility importer\n");
	failed = options.strict || failed;
    }
    if (!imported)
	bu_log("IGES: no supported geometry was created from %s\n", source_path);
    bu_vls_free(&title);
    return imported && !failed ? 1 : 0;
}

const struct gcv_filter iges_reader = {
    "IGES Reader", GCV_FILTER_READ, BU_MIME_MODEL_IGES, iges_can_read,
    iges_create_options, iges_free_options, iges_read
};

const struct gcv_filter * const filters[] = {&iges_reader, nullptr};

} /* namespace */

extern "C" {

const struct gcv_plugin gcv_plugin_info_s = {filters};

COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(void)
{
    return &gcv_plugin_info_s;
}

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
