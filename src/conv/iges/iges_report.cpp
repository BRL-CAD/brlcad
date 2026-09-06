/*                  I G E S _ R E P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"
#include "iges_report.h"
#include "iges_brep_import.h"

#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace brlcad {
namespace iges {

std::string
json_escape(const std::string &value)
{
    std::ostringstream output;
    for (unsigned char character : value) {
	switch (character) {
	    case '\\': output << "\\\\"; break;
	    case '"': output << "\\\""; break;
	    case '\b': output << "\\b"; break;
	    case '\f': output << "\\f"; break;
	    case '\n': output << "\\n"; break;
	    case '\r': output << "\\r"; break;
	    case '\t': output << "\\t"; break;
	    default:
		if (character < 0x20)
		    output << "\\u" << std::hex << std::setw(4) <<
			std::setfill('0') << static_cast<unsigned int>(character) <<
			std::dec << std::setfill(' ');
		else
		    output << static_cast<char>(character);
	}
    }
    return output.str();
}

namespace {
const char *
severity_name(Severity severity)
{
    switch (severity) {
	case Severity::Information: return "information";
	case Severity::Warning: return "warning";
	case Severity::Error: return "error";
	case Severity::Fatal: return "fatal";
    }
    return "error";
}

void
write_header(std::ostream &output, const Document &document,
    const ImportOptions &options, bool success)
{
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10)
	<< "{\n  \"format\": \"iges\",\n"
	<< "  \"source\": \"" << json_escape(document.source_name()) << "\",\n"
	<< "  \"success\": " << (success ? "true" : "false") << ",\n"
	<< "  \"options\": {\"repair\": \"" << repair_mode_name(options.repair)
	<< "\", \"exact\": " << (options.exact ? "true" : "false")
	<< ", \"strict\": " << (options.strict ? "true" : "false")
	<< ", \"project_drawings\": " << (options.project_drawings ? "true" : "false")
	<< ", \"wire_drawings\": " << (options.wire_drawings ? "true" : "false")
	<< ", \"default_plate_thickness\": " << options.default_plate_thickness
	<< ", \"maximum_repair_tolerance\": " << options.maximum_repair_tolerance
	<< ", \"relative_tolerance\": " << options.relative_tolerance << "},\n";
}

void
write_diagnostics(std::ostream &output, const Document &document,
    const std::vector<ImportDiagnostic> &diagnostics)
{
    bool first = true;
    const auto write_diagnostic = [&](Severity severity, const std::string &code,
	const std::string &message, int64_t entity_id, int entity_type,
	size_t record, size_t column) {
	if (!first)
	    output << ',';
	first = false;
	output << "\n    {\"severity\": \"" << severity_name(severity)
	    << "\", \"code\": \"" << json_escape(code)
	    << "\", \"message\": \"" << json_escape(message) << '"';
	if (entity_id)
	    output << ", \"entity\": " << entity_id;
	if (entity_type)
	    output << ", \"entity_type\": " << entity_type;
	if (record)
	    output << ", \"record\": " << record;
	if (column)
	    output << ", \"column\": " << column;
	output << '}';
    };
    for (const Diagnostic &diagnostic : document.diagnostics())
	write_diagnostic(diagnostic.severity, diagnostic.code, diagnostic.message,
	    diagnostic.entity_id, diagnostic.entity_type,
	    diagnostic.location.record, diagnostic.location.column);
    for (const ImportDiagnostic &diagnostic : diagnostics)
	write_diagnostic(diagnostic.severity, diagnostic.code, diagnostic.message,
	    diagnostic.entity_id, diagnostic.entity_type, 0, 0);
    if (!first)
	output << '\n';
    output << "  ]\n}\n";

}
} // namespace

bool
write_brep_import_report(const std::string &path, const Document &document,
    const ImportOptions &options, const BrepImportResult &result)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
	return false;
    write_header(output, document, options, result.success);
    output << "  \"statistics\": {\"entities_read\": "
	<< result.statistics.entities_read << ", \"objects_written\": "
	<< result.statistics.objects_written << ", \"unresolved_output_references\": "
	<< result.statistics.unresolved_output_references << ", \"native_solids_seen\": "
	<< result.statistics.native_solids_seen << ", \"native_solids_written\": "
	<< result.statistics.native_solids_written << ", \"solids_seen\": "
	<< result.statistics.solids_seen << ", \"trimmed_surfaces_seen\": "
	<< result.statistics.trimmed_surfaces_seen
	<< ", \"bounded_surfaces_seen\": "
	<< result.statistics.bounded_surfaces_seen
	<< ", \"standalone_surfaces_seen\": "
	<< result.statistics.standalone_surfaces_seen << ", \"breps_written\": "
	<< result.statistics.breps_written << ", \"components_written\": "
	<< result.statistics.components_written << ", \"meshes_written\": "
	<< result.statistics.meshes_written << ", \"polygons_written\": "
	<< result.statistics.polygons_written << ", \"groups_written\": "
	<< result.statistics.groups_written
	<< ", \"unresolved_members\": " << result.statistics.unresolved_members
	<< ", \"solid_breps_written\": " << result.statistics.solid_breps_written
	<< ", \"invalid_solids_written\": " << result.statistics.invalid_solids_written
	<< ", \"unreconstructed_faces\": " << result.statistics.unreconstructed_faces
	<< ", \"reassembly_edges_merged\": " << result.statistics.reassembly_edges_merged
	<< ", \"plate_mode_objects_thickened\": "
	<< result.statistics.plate_mode_objects_thickened
	<< ", \"relaxed_faces_written\": "
	<< result.statistics.relaxed_faces_written
	<< ", \"recovered_faces_written\": "
	<< result.statistics.recovered_faces_written
	<< ", \"maximum_repair_tolerance_used\": "
	<< result.statistics.maximum_repair_tolerance_used << ", \"omitted\": "
	<< result.statistics.omitted << ", \"repairs\": "
	<< result.statistics.repairs << "},\n"
	<< "  \"diagnostics\": [";

    write_diagnostics(output, document, result.diagnostics);
    output.close();
    return !output.fail();
}

bool
write_import_report(const std::string &path, const Document &document,
    const ImportOptions &options, const ImportResult &result)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
	return false;

    write_header(output, document, options, result.success);
    output << "  \"statistics\": {\"entities_read\": "
	<< result.statistics.entities_read << ", \"objects_written\": "
	<< result.statistics.objects_written << ", \"unresolved_output_references\": "
	<< result.statistics.unresolved_output_references << ", \"annotations_written\": "
	<< result.statistics.annotations_written
	<< ", \"wire_objects_written\": "
	<< result.statistics.wire_objects_written
	<< ", \"datums_written\": "
	<< result.statistics.datums_written
	<< ", \"semantic_groups_written\": "
	<< result.statistics.semantic_groups_written << ", \"omitted\": "
	<< result.statistics.omitted << ", \"repairs\": "
	<< result.statistics.repairs << "},\n"
	<< "  \"diagnostics\": [";


    write_diagnostics(output, document, result.diagnostics);
    output.close();
    return !output.fail();
}

} // namespace iges
} // namespace brlcad

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
