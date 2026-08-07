/*             S T E P M E C H A N I C A L E X P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

/** @file step/STEPMechanicalExport.cpp
 *
 * Shared AP203-family BRL-CAD database traversal and Part 21 writer.  Schema
 * modules supply only the header values and capabilities that actually vary.
 */

#include "AP_Common.h"
#include "STEPMechanicalExport.h"
#include "STEPGeneratedAPI.h"
#include "STEPExportMetadata.h"
#include "StepExportPlan.h"
#if defined(AP203)
#  include "ap203/AP203ManagementExport.h"
#endif

#include "bu/app.h"
#include "bu/file.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "SdaiHeaderSchema.h"
#include "schema.h"

#include "Default_Geometric_Context.h"
#include "G_Objects.h"
#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
#  include "STEPNativeCSGExport.h"
#endif

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum class ExportCoverageStatus {
    Unclassified,
    Handled,
    Skipped,
    Unsupported,
    Failed
};

struct ExportCoverageEntry {
    std::string name;
    int primitive_type = 0;
    bool combination = false;
    ExportCoverageStatus status = ExportCoverageStatus::Unclassified;
    std::string reason;
};

int
coverage_precedence(ExportCoverageStatus status)
{
    switch (status) {
	case ExportCoverageStatus::Failed: return 4;
	case ExportCoverageStatus::Unsupported: return 3;
	case ExportCoverageStatus::Skipped: return 2;
	case ExportCoverageStatus::Handled: return 1;
	default: return 0;
    }
}

void
set_coverage(ExportCoverageEntry &entry, ExportCoverageStatus status,
    const std::string &reason)
{
    if (entry.status == ExportCoverageStatus::Unclassified ||
	    coverage_precedence(status) > coverage_precedence(entry.status)) {
	entry.status = status;
	entry.reason = reason;
    } else if (entry.reason.empty()) {
	entry.reason = reason;
    }
}

const char *
coverage_name(ExportCoverageStatus status)
{
    switch (status) {
	case ExportCoverageStatus::Handled: return "handled";
	case ExportCoverageStatus::Skipped: return "skipped";
	case ExportCoverageStatus::Unsupported: return "unsupported";
	case ExportCoverageStatus::Failed: return "failed";
	default: return "unclassified";
    }
}

std::string
json_escape(const std::string &value)
{
    std::ostringstream escaped;
    for (std::string::const_iterator c = value.begin(); c != value.end(); ++c) {
	switch (*c) {
	    case '\\': escaped << "\\\\"; break;
	    case '"': escaped << "\\\""; break;
	    case '\n': escaped << "\\n"; break;
	    case '\r': escaped << "\\r"; break;
	    case '\t': escaped << "\\t"; break;
	    default: escaped << *c; break;
	}
    }
    return escaped.str();
}

bool
write_export_report(const std::string &path, const std::string &input,
    const std::string &output, const STEPMechanicalExportConfig &config,
    bool native_csg, bool strict, const AP203_Contents &contents,
    const std::string &outcome,
    const brlcad::step::StepExportPlan &plan,
    const std::vector<ExportCoverageEntry> &coverage,
    const std::vector<std::string> &diagnostics,
    const brlcad::step::ExportMetadataStatistics &metadata, int exit_status)
{
    if (path.empty()) return true;
    std::ofstream report(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!report) return false;
    report << "{\n  \"format\":\"brlcad-step-export-report-v1\","
	<< "\n  \"input\":\"" << json_escape(input) << "\","
	<< "\n  \"output\":\"" << json_escape(output) << "\","
	<< "\n  \"schema\":\"" << json_escape(config.schema_key) << "\","
	<< "\n  \"exit_status\":" << exit_status << ','
	<< "\n  \"options\":{\"native_csg\":" << (native_csg ? "true" : "false")
	<< ",\"strict\":" << (strict ? "true" : "false")
	<< ",\"output_units\":\"" << json_escape(contents.length_unit) << "\""
	<< ",\"length_unit_mm\":" << contents.length_unit_mm
	<< ",\"angle_units\":\"" << json_escape(contents.plane_angle_unit) << "\""
	<< ",\"uncertainty\":" << contents.uncertainty << "},"
	<< "\n  \"coverage\":{\"outcome\":\"" << outcome << "\",\"objects\":[";
    for (size_t i = 0; i < coverage.size(); ++i) {
	if (i) report << ',';
	report << "{\"name\":\"" << json_escape(coverage[i].name)
	    << "\",\"primitive_type\":" << coverage[i].primitive_type
	    << ",\"combination\":" << (coverage[i].combination ? "true" : "false")
	    << ",\"status\":\"" << coverage_name(coverage[i].status)
	    << "\",\"reason\":\"" << json_escape(coverage[i].reason) << "\"}";
    }
    report << "]},\n  \"metadata\":{\"products_updated\":"
	<< metadata.products_updated
	<< ",\"occurrences_updated\":" << metadata.occurrences_updated
	<< ",\"occurrences_omitted\":" << metadata.occurrences_omitted
	<< ",\"styled_items_emitted\":" << metadata.styled_items_emitted
	<< ",\"layers_emitted\":" << metadata.layers_emitted
	<< ",\"presentation_omitted\":" << metadata.presentation_omitted
	<< ",\"materials_emitted\":" << metadata.materials_emitted
	<< ",\"materials_omitted\":" << metadata.materials_omitted
	<< ",\"material_properties_emitted\":"
	<< metadata.material_properties_emitted
	<< ",\"material_properties_omitted\":"
	<< metadata.material_properties_omitted
	<< ",\"product_properties_emitted\":"
	<< metadata.product_properties_emitted
	<< ",\"product_properties_omitted\":"
	<< metadata.product_properties_omitted
	<< ",\"configuration_records_seen\":"
	<< metadata.configuration_records_seen
	<< ",\"configuration_records_emitted\":"
	<< metadata.configuration_records_emitted
	<< ",\"configuration_records_omitted\":"
	<< metadata.configuration_records_omitted
	<< ",\"opaque_global_attributes_seen\":"
	<< metadata.opaque_global_attributes_seen
	<< "},\n  \"configuration_records\":[";
    for (size_t i = 0; i < plan.configuration_records.size(); ++i) {
	if (i) report << ',';
	const brlcad::step::ExportConfigurationRecordPlan &record =
	    plan.configuration_records[i];
	report << "{\"schema\":\"" << json_escape(record.schema)
	    << "\",\"entity_id\":" << record.entity_id
	    << ",\"type\":\"" << json_escape(record.type)
	    << "\",\"component_types\":[";
	for (size_t j = 0; j < record.component_types.size(); ++j) {
	    if (j) report << ',';
	    report << '"' << json_escape(record.component_types[j]) << '"';
	}
	report << "],\"value\":\"" << json_escape(record.value)
	    << "\",\"references\":[";
	for (size_t j = 0; j < record.references.size(); ++j) {
	    if (j) report << ',';
	    report << record.references[j];
	}
	report << "],\"valid\":" << (record.valid ? "true" : "false")
	    << ",\"status\":\"" << json_escape(record.export_status)
	    << "\",\"reason\":\"" << json_escape(record.export_reason) << "\"}";
    }
    report << "],\n  \"diagnostics\":[";
    for (size_t i = 0; i < diagnostics.size(); ++i) {
	if (i) report << ',';
	report << '"' << json_escape(diagnostics[i]) << '"';
    }
    report << "]\n}\n";
    return report.good();
}

void
mark_native_subtree(size_t root, const brlcad::step::StepExportPlan &plan,
    std::vector<ExportCoverageEntry> &coverage)
{
    std::vector<size_t> pending(1, root);
    std::set<size_t> visited;
    while (!pending.empty()) {
	const size_t current = pending.back();
	pending.pop_back();
	if (current >= coverage.size() || !visited.insert(current).second) continue;
	set_coverage(coverage[current], ExportCoverageStatus::Handled,
	    current == root ? "exported as a schema-native CSG root" :
	    "preserved as an operand of a schema-native CSG tree");
	for (std::vector<brlcad::step::ExportOccurrencePlan>::const_iterator occurrence =
		plan.occurrences.begin(); occurrence != plan.occurrences.end(); ++occurrence)
	    if (occurrence->parent == current) pending.push_back(occurrence->child);
    }
}

#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
bool
native_shape_boundary(size_t object, const brlcad::step::StepExportPlan &plan)
{
    if (object >= plan.objects.size() || !plan.objects[object].combination)
	return false;
    if (plan.objects[object].region) return true;
    for (std::vector<brlcad::step::ExportOccurrencePlan>::const_iterator occurrence =
	    plan.occurrences.begin(); occurrence != plan.occurrences.end(); ++occurrence) {
	if (occurrence->parent != object) continue;
	for (std::vector<int>::const_iterator operation =
		occurrence->boolean_operations.begin();
	     operation != occurrence->boolean_operations.end(); ++operation)
	    if (*operation != OP_UNION) return true;
    }
    return false;
}

void
collect_native_shape_boundaries(size_t assembly,
    const brlcad::step::StepExportPlan &plan, std::set<size_t> &boundaries,
    std::set<size_t> &visited)
{
    if (assembly >= plan.objects.size() || !visited.insert(assembly).second)
	return;
    for (std::vector<brlcad::step::ExportOccurrencePlan>::const_iterator occurrence =
	    plan.occurrences.begin(); occurrence != plan.occurrences.end(); ++occurrence) {
	if (occurrence->parent != assembly ||
		occurrence->child >= plan.objects.size() ||
		!plan.objects[occurrence->child].combination)
	    continue;
	if (native_shape_boundary(occurrence->child, plan)) {
	    boundaries.insert(occurrence->child);
	    continue;
	}
	collect_native_shape_boundaries(occurrence->child, plan, boundaries, visited);
    }
}
#endif

void
usage(const STEPMechanicalExportConfig &config)
{
    std::cerr << "Usage: g-step --schema " << config.schema_key;
    if (config.supports_native_csg) std::cerr << " [--native-csg]";
    std::cerr << " [--output-units unit] [--angle-units degree|radian]"
	" [--uncertainty value] [--strict] [--report report.json]"
	" -o outfile.stp infile.g [object ...]\n";
}

bool
parse_positive_real(const char *text, double &value)
{
    if (!text || !text[0]) return false;
    errno = 0;
    char *end = NULL;
    value = std::strtod(text, &end);
    return errno != ERANGE && end && end != text && !*end &&
	std::isfinite(value) && value > 0.0;
}

} // namespace

int
STEPMechanicalExport(int argc, char *argv[], const STEPMechanicalExportConfig &config)
{
    if (argc < 1 || !argv || !argv[0]) return 1;
    bu_optind = 1;
    bu_setprogname(argv[0]);

    int native_csg = 0;
    int strict = 0;
    char *output_file = NULL;
    char *report_file = NULL;
    char *length_unit_option = NULL;
    char *plane_angle_option = NULL;
    double uncertainty_option = 0.0;
    int c;
    while ((c = bu_getopt(argc, argv,
	    config.supports_native_csg ? "Ca:o:R:st:u:" : "a:o:R:st:u:")) != -1) {
	switch (c) {
	    case 'a':
		plane_angle_option = bu_optarg;
		break;
	    case 'C':
		native_csg = 1;
		break;
	    case 'o':
		output_file = bu_optarg;
		break;
	    case 'R':
		report_file = bu_optarg;
		break;
	    case 's':
		strict = 1;
		break;
	    case 't':
		if (!parse_positive_real(bu_optarg, uncertainty_option)) {
		    bu_log("ERROR: STEP uncertainty must be a positive finite value\n");
		    return 1;
		}
		break;
	    case 'u':
		length_unit_option = bu_optarg;
		break;
	    default:
		usage(config);
		return 1;
	}
    }
    if (bu_optind >= argc || !output_file) {
	usage(config);
	return 1;
    }

    const double length_unit_mm = length_unit_option ?
	bu_units_conversion(length_unit_option) : 1.0;
    if (!(length_unit_mm > 0.0) || !std::isfinite(length_unit_mm)) {
	bu_log("ERROR: STEP output units must name a recognized length unit\n");
	return 1;
    }
    const char *canonical_length_unit = bu_units_string(length_unit_mm);
    if (!canonical_length_unit) {
	bu_log("ERROR: could not normalize STEP output units\n");
	return 1;
    }
    std::string plane_angle_unit = plane_angle_option ?
	plane_angle_option : "degree";
    if (plane_angle_unit == "deg" || plane_angle_unit == "degrees")
	plane_angle_unit = "degree";
    if (plane_angle_unit == "rad" || plane_angle_unit == "radians")
	plane_angle_unit = "radian";
    if (plane_angle_unit != "degree" && plane_angle_unit != "radian") {
	bu_log("ERROR: STEP plane-angle units must be degree or radian\n");
	return 1;
    }

    argc -= bu_optind;
    argv += bu_optind;
    if (bu_file_exists(output_file, NULL)) {
	bu_log("ERROR: refusing to overwrite existing output file: \"%s\"\n", output_file);
	return 1;
    }
    if (!bu_file_exists(argv[0], NULL) && !BU_STR_EQUAL(argv[0], "-")) {
	bu_log("ERROR: unable to read input \"%s\" .g file\n", argv[0]);
	return 2;
    }

    std::string input_file(argv[0]);
    BRLCADWrapper *dotg = new BRLCADWrapper();
    if (!dotg) {
	bu_log("ERROR: unable to create BRL-CAD database reader\n");
	return 3;
    }
    if (!dotg->load(input_file)) {
	bu_log("ERROR: unable to open BRL-CAD input file \"%s\"\n", argv[0]);
	delete dotg;
	return 2;
    }

    struct db_i *dbip = dotg->GetDBIP();
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    struct directory **paths = NULL;
    int path_count = 0;
    if (argc < 2) {
	db_update_nref(dbip);
	path_count = db_ls(dbip, DB_LS_TOPS, NULL, &paths);
	if (!path_count) {
	    bu_log("ERROR: no top-level objects found in the input database\n");
	    delete dotg;
	    return 1;
	}
    } else {
	paths = (struct directory **)bu_calloc((size_t)argc, sizeof(struct directory *),
	    "STEP export object list");
	for (int i = 1; i < argc; ++i) {
	    struct directory *dp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		bu_log("ERROR: cannot find object %s\n", argv[i]);
		bu_free(paths, "STEP export object list");
		delete dotg;
		return 1;
	    }
	    paths[path_count++] = dp;
	}
    }

    std::vector<std::string> requested_objects;
    requested_objects.reserve(static_cast<size_t>(path_count));
    for (int i = 0; i < path_count; ++i)
	requested_objects.push_back(paths[i]->d_namep);
    brlcad::step::StepExportPlan export_plan;
    std::string plan_error;
    if (!brlcad::step::BuildStepExportPlan(export_plan, input_file,
	    requested_objects, plan_error)) {
	bu_log("ERROR: %s\n", plan_error.c_str());
	bu_free(paths, "STEP export object list");
	delete dotg;
	return 2;
    }
    std::vector<ExportCoverageEntry> coverage(export_plan.objects.size());
    std::vector<std::string> export_diagnostics = export_plan.diagnostics;
    std::map<std::string, size_t> coverage_by_name;
    for (size_t i = 0; i < export_plan.objects.size(); ++i) {
	coverage[i].name = export_plan.objects[i].name;
	coverage[i].primitive_type = export_plan.objects[i].primitive_type;
	coverage[i].combination = export_plan.objects[i].combination;
	coverage_by_name[coverage[i].name] = i;
    }
    std::map<std::string, std::string> conversion_failures;
    std::set<std::string> native_csg_roots;

    struct bu_vls scratch = BU_VLS_INIT_ZERO;
    Registry *registry = new Registry(SchemaInit);
    InstMgr instance_list;
    /* Part 21 entity instance identifiers are positive integers. */
    instance_list.NextFileId();
    STEPfile *step_file = new STEPfile(*registry, instance_list);
    registry->ResetSchemas();
    registry->ResetEntities();

    InstMgr *header = step_file->HeaderInstances();
    SdaiFile_name *file_name = (SdaiFile_name *)step_file->HeaderDefaultFileName();
    bu_vls_sprintf(&scratch, "'%s'", output_file);
    file_name->name_(bu_vls_addr(&scratch));
    file_name->time_stamp_("");
    StringAggregate_ptr author = new StringAggregate;
    author->AddNode(new StringNode("''"));
    file_name->author_(author);
    StringAggregate_ptr organization = new StringAggregate;
    organization->AddNode(new StringNode("''"));
    file_name->organization_(organization);
    file_name->preprocessor_version_(config.preprocessor);
    file_name->originating_system_("''");
    file_name->authorization_("''");
    header->Append((SDAI_Application_instance *)file_name, completeSE);

    SdaiFile_description *description =
	(SdaiFile_description *)step_file->HeaderDefaultFileDescription();
    StringAggregate_ptr descriptions = new StringAggregate;
    descriptions->AddNode(new StringNode("''"));
    description->description_(descriptions);
    description->implementation_level_("'2;1'");
    header->Append((SDAI_Application_instance *)description, completeSE);

    SdaiFile_schema *file_schema = (SdaiFile_schema *)step_file->HeaderDefaultFileSchema();
    StringAggregate_ptr identifiers = new StringAggregate;
    identifiers->AddNode(new StringNode(config.schema_identifier));
    file_schema->schema_identifiers_(identifiers);
    header->Append((SDAI_Application_instance *)file_schema, completeSE);

    AP203_Contents *contents = new AP203_Contents;
    contents->registry = registry;
    contents->instance_list = &instance_list;
    contents->length_unit_mm = length_unit_mm;
    contents->mm_to_length_unit = 1.0 / length_unit_mm;
    contents->length_unit = canonical_length_unit;
    contents->uncertainty = uncertainty_option > 0.0 ?
	uncertainty_option : 0.05 / length_unit_mm;
    contents->plane_angle_unit = plane_angle_unit;
    contents->radians_to_plane_angle = plane_angle_unit == "radian" ?
	1.0 : RAD2DEG;
    contents->default_context = Add_Default_Geometric_Context(contents);
    contents->application_context = brlcad::step::CreateEntity(registry,
	    &instance_list, "APPLICATION_CONTEXT");
    brlcad::step::SetString(contents->application_context, "application",
	    config.application);

#if defined(AP203) || defined(AP203e2)
    if (config.create_design_context) {
	contents->design_context = brlcad::step::CreateEntity(registry,
	    &instance_list, "DESIGN_CONTEXT");
	brlcad::step::SetString(contents->design_context, "name", "");
	brlcad::step::SetString(contents->design_context, "life_cycle_stage", "design");
	brlcad::step::SetEntity(contents->design_context, "frame_of_reference",
	    contents->application_context);
    }
#else
    (void)config.create_design_context;
#endif

    contents->solid_to_step = new std::map<struct directory *, STEPentity *>;
    contents->solid_to_step_shape = new std::map<struct directory *, STEPentity *>;
    contents->solid_to_step_manifold = new std::map<struct directory *, STEPentity *>;
    contents->comb_to_step = new std::map<struct directory *, STEPentity *>;
    contents->comb_to_step_shape = new std::map<struct directory *, STEPentity *>;
    contents->comb_to_step_manifold = new std::map<struct directory *, STEPentity *>;
    contents->occurrence_to_step =
	new std::map<std::pair<struct directory *, size_t>, STEPentity *>;
    contents->representation_memberships =
	new std::set<std::pair<struct directory *, size_t> >;
    contents->dbip = dbip;
    contents->wdbp = wdbp;
    for (const brlcad::step::ExportOccurrencePlan &occurrence :
	    export_plan.occurrences) {
	if (!occurrence.representation_membership ||
		occurrence.parent >= export_plan.objects.size())
	    continue;
	struct directory *parent = db_lookup(dbip,
	    export_plan.objects[occurrence.parent].name.c_str(), LOOKUP_QUIET);
	if (parent != RT_DIR_NULL)
	    contents->representation_memberships->insert(
		std::make_pair(parent, occurrence.ordinal));
    }
#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
    contents->flip_transforms = 0;
#endif

    int status = 0;
    for (int i = 0; i < path_count; ++i) {
	struct directory *dp = paths[i];
#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
	if (native_csg) {
	    std::map<std::string, size_t>::const_iterator selected =
		coverage_by_name.find(dp->d_namep);
	    const size_t root_index = selected == coverage_by_name.end() ?
		export_plan.objects.size() : selected->second;
	    const bool root_is_native_shape = root_index < export_plan.objects.size() &&
		(!export_plan.objects[root_index].combination ||
		 native_shape_boundary(root_index, export_plan));

	    /* A non-region union hierarchy is product structure.  Prepare each
	     * nested Boolean/region shape independently, then let the ordinary
	     * assembly emitter relate those products without expanding their CSG
	     * operands into duplicate orphan products. */
	    if (!root_is_native_shape && root_index < export_plan.objects.size()) {
		std::set<size_t> boundaries;
		std::set<size_t> visited;
		collect_native_shape_boundaries(root_index, export_plan, boundaries,
		    visited);
		for (std::set<size_t>::const_iterator boundary = boundaries.begin();
		     boundary != boundaries.end(); ++boundary) {
		    const std::string &name = export_plan.objects[*boundary].name;
		    if (native_csg_roots.find(name) != native_csg_roots.end())
			continue;
		    struct directory *shape_dp = db_lookup(dbip, name.c_str(),
			LOOKUP_QUIET);
		    std::string diagnostic;
		    const StepNativeCsgStatus shape_status = shape_dp == RT_DIR_NULL ?
			STEP_NATIVE_CSG_ERROR :
			ExportSTEPNativeCSG(shape_dp, wdbp, contents, diagnostic);
		    if (shape_status == STEP_NATIVE_CSG_SUCCESS) {
			native_csg_roots.insert(name);
		    } else {
			if (diagnostic.empty())
			    diagnostic = "nested native CSG shape could not be emitted";
			conversion_failures[name] = diagnostic;
			bu_log("WARNING: native %s CSG export of nested shape %s "
			    "failed: %s\n", config.schema_key, name.c_str(),
			    diagnostic.c_str());
		    }
		}
	    }

	    if (root_is_native_shape) {
		std::string diagnostic;
		const StepNativeCsgStatus csg_status =
		    ExportSTEPNativeCSG(dp, wdbp, contents, diagnostic);
		if (csg_status == STEP_NATIVE_CSG_SUCCESS) {
		    native_csg_roots.insert(dp->d_namep);
		    continue;
		}
		if (csg_status == STEP_NATIVE_CSG_ERROR) {
		    bu_log("ERROR: native %s CSG export of %s failed: %s\n",
			config.schema_key, dp->d_namep, diagnostic.c_str());
		    conversion_failures[dp->d_namep] = diagnostic;
		    status = 4;
		    continue;
		}
		bu_log("WARNING: %s: %s\n", dp->d_namep, diagnostic.c_str());
	    }
	}
#else
	(void)native_csg;
#endif
	struct rt_db_internal intern;
	mat_t output_scale;
	MAT_IDN(output_scale);
	output_scale[15] = contents->length_unit_mm;
	if (rt_db_get_internal(&intern, dp, dbip, output_scale) < 0) {
	    bu_log("ERROR: cannot read %s\n", dp->d_namep);
	    conversion_failures[dp->d_namep] = "could not read the selected database object";
	    continue;
	}
	RT_CK_DB_INTERNAL(&intern);
	std::string conversion_diagnostic;
	if (!Object_To_STEP(dp, &intern, wdbp, contents,
		&conversion_diagnostic))
	    conversion_failures[dp->d_namep] = conversion_diagnostic.empty() ?
		"no complete STEP representation was produced" :
		conversion_diagnostic;
	rt_db_free_internal(&intern);
    }

    for (std::set<std::string>::const_iterator native = native_csg_roots.begin();
	 native != native_csg_roots.end(); ++native) {
	std::map<std::string, size_t>::const_iterator root = coverage_by_name.find(*native);
	if (root != coverage_by_name.end())
	    mark_native_subtree(root->second, export_plan, coverage);
    }

    brlcad::step::ExportMetadataStatistics metadata_statistics;
    if (!brlcad::step::ApplySTEPExportMetadata(export_plan, contents,
	    export_diagnostics, metadata_statistics)) {
	bu_log("ERROR: could not apply STEP product and presentation metadata\n");
	status = 4;
    }
#if defined(AP203)
    if (!brlcad::step::FinalizeAP203ManagementGraph(contents,
	    export_diagnostics)) {
	bu_log("ERROR: could not author the mandatory AP203 administrative graph\n");
	status = 4;
    }
#endif

    for (size_t i = 0; i < coverage.size(); ++i) {
	if (coverage[i].status == ExportCoverageStatus::Handled) continue;
	struct directory *dp = db_lookup(dbip, coverage[i].name.c_str(), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    set_coverage(coverage[i], ExportCoverageStatus::Failed,
		"database object disappeared during export");
	    continue;
	}
	const bool represented = coverage[i].combination ?
	    contents->comb_to_step->find(dp) != contents->comb_to_step->end() :
	    contents->solid_to_step->find(dp) != contents->solid_to_step->end();
	if (represented) {
	    set_coverage(coverage[i], ExportCoverageStatus::Handled,
		coverage[i].combination ? "assembly/product representation emitted" :
		"BRep/product representation emitted");
	    continue;
	}
	std::map<std::string, std::string>::const_iterator failure =
	    conversion_failures.find(coverage[i].name);
	set_coverage(coverage[i], coverage[i].combination ?
	    ExportCoverageStatus::Skipped : ExportCoverageStatus::Unsupported,
	    failure == conversion_failures.end() ?
		(coverage[i].combination ?
		    "combination was not emitted" :
		    "primitive has no usable BRep or schema-native representation") :
		failure->second);
    }

    /* The ordinary path emits assembly structure, not boolean semantics.
     * Refuse to describe intersection/subtraction/XOR trees as if they were
     * unions unless the selected native-CSG root preserved the tree. */
    for (std::vector<brlcad::step::ExportOccurrencePlan>::const_iterator occurrence =
	    export_plan.occurrences.begin(); occurrence != export_plan.occurrences.end();
	    ++occurrence) {
	if (occurrence->parent >= coverage.size() ||
		occurrence->child >= coverage.size()) continue;
	ExportCoverageEntry &parent = coverage[occurrence->parent];
	const bool native_tree = parent.reason.find("schema-native CSG") !=
	    std::string::npos || parent.reason.find("operand of a schema-native") !=
	    std::string::npos;
	if (!native_tree) {
	    for (std::vector<int>::const_iterator operation =
		    occurrence->boolean_operations.begin();
		 operation != occurrence->boolean_operations.end(); ++operation) {
		if (*operation == OP_UNION) continue;
		set_coverage(parent, ExportCoverageStatus::Unsupported,
		    "boolean tree requires --native-csg or evaluated-BRep export");
		break;
	    }
	    if (!NEAR_ZERO(occurrence->transform[15] - 1.0, VUNITIZE_TOL))
		set_coverage(parent, ExportCoverageStatus::Skipped,
		    "an assembly occurrence uses scaling which STEP product structure "
		    "cannot preserve");
	}
    }

    /* A product emitted without one of its reachable children is partial even
     * when its own PRODUCT/SHAPE_REPRESENTATION records are syntactically valid. */
    bool changed = true;
    while (changed) {
	changed = false;
	for (std::vector<brlcad::step::ExportOccurrencePlan>::const_iterator occurrence =
		export_plan.occurrences.begin(); occurrence != export_plan.occurrences.end();
	     ++occurrence) {
	    if (occurrence->parent >= coverage.size() ||
		    occurrence->child >= coverage.size() ||
		    coverage[occurrence->child].status == ExportCoverageStatus::Handled ||
		    coverage[occurrence->parent].status != ExportCoverageStatus::Handled)
		continue;
	    set_coverage(coverage[occurrence->parent], ExportCoverageStatus::Skipped,
		"reachable child '" + coverage[occurrence->child].name +
		"' was omitted");
	    changed = true;
	}
    }
    if (!export_plan.diagnostics.empty()) {
	for (std::vector<size_t>::const_iterator root = export_plan.roots.begin();
	     root != export_plan.roots.end(); ++root)
	    if (*root < coverage.size())
		set_coverage(coverage[*root], ExportCoverageStatus::Skipped,
		    export_plan.diagnostics.front());
    }

    size_t handled = 0;
    size_t omitted = 0;
    for (std::vector<ExportCoverageEntry>::const_iterator entry = coverage.begin();
	 entry != coverage.end(); ++entry) {
	if (entry->status == ExportCoverageStatus::Handled) {
	    ++handled;
	} else {
	    ++omitted;
	    bu_log("WARNING: STEP export omitted %s: %s\n", entry->name.c_str(),
		entry->reason.c_str());
	}
    }
    std::string outcome;
    const bool metadata_omitted =
	metadata_statistics.presentation_omitted != 0 ||
	metadata_statistics.materials_omitted != 0 ||
	metadata_statistics.material_properties_omitted != 0 ||
	metadata_statistics.product_properties_omitted != 0 ||
	metadata_statistics.configuration_records_omitted != 0 ||
	metadata_statistics.occurrences_omitted != 0;
    if (status || (!handled && omitted)) outcome = "failed";
    else if (!handled && !omitted) outcome = "empty";
    else if (omitted || metadata_omitted) outcome = "partial";
    else outcome = "complete";

    if (outcome == "failed" || (strict && (omitted || metadata_omitted))) {
	status = 4;
    } else if (outcome == "partial") {
	status = 1;
    } else {
	status = 0;
    }
    if (status == 0 || status == 1) {
	std::ofstream output(output_file);
	step_file->WriteExchangeFile(output);
	if (!output.good()) {
	    status = 2;
	    outcome = "failed";
	}
    }
    if (!write_export_report(report_file ? report_file : std::string(), input_file,
	    output_file, config, native_csg != 0, strict != 0, *contents, outcome,
	    export_plan,
	    coverage,
	    export_diagnostics, metadata_statistics, status)) {
	bu_log("ERROR: could not write export report %s\n", report_file);
	status = 2;
    }

    header->DeleteInstances();
    instance_list.DeleteInstances();
    delete dotg;
    delete registry;
    delete step_file;
    delete contents->solid_to_step;
    delete contents->solid_to_step_shape;
    delete contents->solid_to_step_manifold;
    delete contents->comb_to_step;
    delete contents->comb_to_step_shape;
    delete contents->comb_to_step_manifold;
    delete contents->occurrence_to_step;
    delete contents->representation_memberships;
    delete contents;
    bu_vls_free(&scratch);
    bu_free(paths, "STEP export object list");
    return status;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
