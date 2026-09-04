/*              I G E S _ B R E P _ I M P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef CONV_IGES_IGES_BREP_IMPORT_H
#define CONV_IGES_IGES_BREP_IMPORT_H

#include "common.h"

struct rt_wdb;

#ifdef __cplusplus

#  include <cstddef>
#  include <string>
#  include <vector>

#  include "iges_import.h"

namespace brlcad {
namespace iges {

struct BrepImportStatistics {
    size_t entities_read = 0;
    size_t solids_seen = 0;
    size_t trimmed_surfaces_seen = 0;
    size_t bounded_surfaces_seen = 0;
    size_t standalone_surfaces_seen = 0;
    size_t breps_written = 0;
    size_t components_written = 0;
    size_t groups_written = 0;
    size_t plate_mode_objects_thickened = 0;
    size_t relaxed_faces_written = 0;
    double maximum_repair_tolerance_used = 0.0;
    size_t omitted = 0;
    size_t repairs = 0;
};

struct BrepImportResult {
    bool success = false;
    BrepImportStatistics statistics;
    std::vector<ImportDiagnostic> diagnostics;
};

/** Build IGES 186 manifold topology and assemble IGES 143/144 bounded faces
 * directly in OpenNURBS.  No NMG intermediate representation is used. */
BrepImportResult import_breps(const Document &document, struct rt_wdb *wdbp,
    const ImportOptions &options);

bool write_brep_import_report(const std::string &path,
    const Document &document, const ImportOptions &options,
    const BrepImportResult &result);

} /* namespace iges */
} /* namespace brlcad */

#endif /* __cplusplus */

__BEGIN_DECLS

/** Compatible command-line bridge.  Returns 1 if direct B-Reps completed the
 * import, 2 if direct B-Reps were written and legacy CSG entities remain,
 * 0 if the document has no direct B-Rep entities, and -1 on failure. */
int iges_import_breps(const char *path, struct rt_wdb *wdbp, int exact,
    int strict, const char *repair_mode, double default_plate_thickness,
    double maximum_repair_tolerance, const char *root_name,
    const char *report_path);

__END_DECLS

#endif /* CONV_IGES_IGES_BREP_IMPORT_H */

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
