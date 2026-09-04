/*                    I G E S _ I M P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef CONV_IGES_IGES_IMPORT_H
#define CONV_IGES_IGES_IMPORT_H

#include "common.h"

struct rt_wdb;

#ifdef __cplusplus

#  include <cstddef>
#  include <cstdint>
#  include <string>
#  include <vector>

#  include "iges_document.h"

namespace brlcad {
namespace iges {

enum class RepairMode {
    None,
    Safe
};

enum class InvalidBrepPolicy {
    Preserve,
    Reject
};

struct ImportOptions {
    RepairMode repair = RepairMode::Safe;
    InvalidBrepPolicy invalid_brep = InvalidBrepPolicy::Preserve;
    bool exact = false;
    bool strict = false;
    bool project_drawings = true;
    std::string root_name = "iges_drawing";
};

struct ImportStatistics {
    size_t entities_read = 0;
    size_t objects_written = 0;
    size_t annotations_written = 0;
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

#endif /* __cplusplus */

__BEGIN_DECLS

/** C bridge used by the compatible iges-g command while its native-solid
 * handlers are migrated independently.  Returns 1 on success, 0 when no
 * semantic drawing objects were applicable, and -1 on error. */
int iges_import_annotations(const char *path, struct rt_wdb *wdbp,
    int project_to_xy, int exact, int strict, const char *repair_mode,
    const char *root_name, const char *report_path);

__END_DECLS

#endif /* CONV_IGES_IGES_IMPORT_H */

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
