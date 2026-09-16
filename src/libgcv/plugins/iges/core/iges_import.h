/*                    I G E S _ I M P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GCV_IGES_IMPORT_H
#define GCV_IGES_IMPORT_H

#include "common.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "iges_document.h"

struct rt_wdb;

namespace brlcad {
namespace iges {

// Dimensionless fraction of a local model-space boundary's box diagonal.
constexpr double DEFAULT_RELATIVE_TOLERANCE = 1.0e-4;
enum class GeometryOutput { Brep, Mesh, Polygon };

enum class RepairMode {
    None,
    Safe,
    BestEffort
};

inline const char *
repair_mode_name(RepairMode mode)
{
    switch (mode) {
	case RepairMode::None: return "none";
	case RepairMode::Safe: return "safe";
	case RepairMode::BestEffort: return "best-effort";
    }
    return "unknown";
}

enum class InvalidBrepPolicy {
    Preserve,
    Reject
};

struct ImportOptions {
    GeometryOutput output = GeometryOutput::Brep;
    RepairMode repair = RepairMode::BestEffort;
    InvalidBrepPolicy invalid_brep = InvalidBrepPolicy::Preserve;
    bool exact = false;
    bool strict = false;
    double default_plate_thickness = 0.0;
    double maximum_repair_tolerance = 0.0;
    double relative_tolerance = DEFAULT_RELATIVE_TOLERANCE;
    bool project_drawings = true;
    bool wire_drawings = false;
    std::string root_name = "iges_drawing";
    /* Called synchronously; library imports remain silent unless supplied. */
    std::function<void(const char *, const char *, size_t, size_t, int64_t)> progress;
};

struct ImportStatistics {
    size_t entities_read = 0;
    size_t objects_written = 0;
    size_t unresolved_output_references = 0;
    size_t annotations_written = 0;
    size_t wire_objects_written = 0;
    size_t datums_written = 0;
    size_t semantic_groups_written = 0;
    size_t omitted = 0;
    size_t repairs = 0;
};

struct ImportDiagnostic {
    Severity severity = Severity::Error;
    std::string code;
    std::string message;
    int64_t entity_id = 0;
    int entity_type = 0;
};

struct ImportResult {
    bool success = false;
    ImportStatistics statistics;
    std::vector<ImportDiagnostic> diagnostics;
};

bool has_drawing_geometry(const Document &document);

/** Translate IGES drawing and annotation entities to native model-space
 * annotation objects.  Independent non-planar curves are intentionally left
 * for the wire-geometry fallback rather than flattened silently. */
ImportResult import_annotations(const Document &document, struct rt_wdb *wdbp,
    const ImportOptions &options);

/** Emit deterministic machine-readable diagnostics and counts. */
bool write_import_report(const std::string &path, const Document &document,
    const ImportOptions &options, const ImportResult &result);

} /* namespace iges */
} /* namespace brlcad */


#endif /* GCV_IGES_IMPORT_H */

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
