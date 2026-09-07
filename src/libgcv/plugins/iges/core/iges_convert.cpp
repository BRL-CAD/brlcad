/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_convert.h"
#include "iges_parameters.h"
#include "iges_runtime.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include "bu/log.h"
#include "rt/geom.h"

namespace brlcad {
namespace iges {

RepairMode
parse_repair_mode(const char *mode)
{
    const std::string name = mode ? mode : "best-effort";
    if (name == "none")
	return RepairMode::None;
    if (name == "safe")
	return RepairMode::Safe;
    if (name == "best-effort")
	return RepairMode::BestEffort;
    throw std::invalid_argument("repair mode must be none, safe, or best-effort");
}

void
validate_import_options(const ImportOptions &options, bool drawings)
{
    if (!std::isfinite(options.default_plate_thickness) || options.default_plate_thickness < 0.0 ||
	!std::isfinite(options.maximum_repair_tolerance) || options.maximum_repair_tolerance < 0.0 ||
	!std::isfinite(options.relative_tolerance) || options.relative_tolerance < 0.0)
	throw std::invalid_argument("thickness and tolerances must be finite non-negative values");
    if (options.default_plate_thickness > 0.0 && (drawings || options.output != GeometryOutput::Brep))
	throw std::invalid_argument("default plate thickness requires B-Rep output");
    if (options.maximum_repair_tolerance > 0.0 &&
	(drawings || options.output != GeometryOutput::Brep || options.exact || options.strict || options.repair == RepairMode::None))
	throw std::invalid_argument("maximum repair tolerance requires safe B-Rep output");
}

bool
has_model_geometry(const Document &document)
{
    return std::any_of(document.entities().begin(), document.entities().end(),
	[&document](const DirectoryEntry &entry) {
	    switch (entry.type) {
		case 108: {
		    // Drawing views commonly carry unbounded construction planes.
		    // Only a plane with an actual boundary selects surface import.
		    const auto *parameters = document.parameters(entry.id);
		    if (parameters &&
			(parameters->values.size() <= 5 || parameters->values[5].empty()))
			return false;
		    int boundary = 0;
		    return !parameter_integer(parameters, 5, boundary) || boundary != 0;
		}
		case 114: case 118: case 120: case 122: case 128:
		case 140: case 143: case 144: case 150: case 152: case 154:
		case 156: case 158: case 160: case 162: case 164: case 168:
		case 186: case 190: case 192: case 194: case 196: case 198:
		    return true;
		default: return false;
	    }
	});
}

void
log_import_diagnostics(const Document &document, const std::vector<ImportDiagnostic> &diagnostics)
{
    struct Summary {
	size_t count = 0;
	std::string message;
	int64_t entity = 0;
    };
    std::map<std::string, Summary> summaries;
    const auto append = [&](const auto &diagnostic) {
	if (diagnostic.severity == Severity::Information)
	    return;
	auto &summary = summaries[diagnostic.code];
	if (!summary.count++) {
	    summary.message = diagnostic.message;
	    summary.entity = diagnostic.entity_id;
	}
    };
    for (const auto &diagnostic : document.diagnostics())
	append(diagnostic);
    for (const auto &diagnostic : diagnostics)
	append(diagnostic);
    for (const auto &item : summaries) {
	const auto &summary = item.second;
	bu_log("IGES %s (%zu occurrences, first D%lld): %s\n",
	    item.first.c_str(), summary.count, static_cast<long long>(summary.entity),
	    summary.message.c_str());
    }
}

namespace {
size_t
missing_references(const union tree *node, struct db_i *database)
{
    size_t missing = 0;
    std::vector<const union tree *> pending = {node};
    while (!pending.empty()) {
	const auto *current = pending.back();
	pending.pop_back();
	if (!current || current->tr_op == OP_NOP)
	    continue;
	if (current->tr_op == OP_DB_LEAF) {
	    missing += db_lookup(database, current->tr_l.tl_name, LOOKUP_QUIET) == RT_DIR_NULL;
	    continue;
	}
	pending.push_back(current->tr_b.tb_left);
	if (current->tr_op != OP_NOT && current->tr_op != OP_GUARD && current->tr_op != OP_XNOP)
	    pending.push_back(current->tr_b.tb_right);
    }
    return missing;
}
} // namespace

OutputStatistics
check_output(struct db_i *database)
{
    OutputStatistics statistics;
    struct directory *entry;
    FOR_ALL_DIRECTORY_START(entry, database) {
	if (entry->d_major_type != DB5_MAJORTYPE_BRLCAD || (entry->d_flags & RT_DIR_NON_GEOM))
	    continue;
	if (!(entry->d_flags & RT_DIR_COMB)) {
	    ++statistics.objects;
	    continue;
	}
	Internal internal;
	if (rt_db_get_internal(&internal.value, entry, database, nullptr) < 0)
	    throw std::runtime_error("cannot verify output combination");
	const auto *combination = static_cast<const struct rt_comb_internal *>(internal.value.idb_ptr);
	statistics.unresolved += missing_references(combination->tree, database);
    } FOR_ALL_DIRECTORY_END;
    return statistics;
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
