/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef GCV_IGES_CONVERT_H
#define GCV_IGES_CONVERT_H

#include "common.h"
#include "iges_import.h"

struct db_i;

namespace brlcad {
namespace iges {

RepairMode parse_repair_mode(const char *mode);
void validate_import_options(const ImportOptions &options, bool drawings);

/** Construction/view planes alone do not require finite-surface import. */
bool has_model_geometry(const Document &document);
void log_import_diagnostics(const Document &document,
    const std::vector<ImportDiagnostic> &diagnostics);

struct OutputStatistics {
    size_t objects = 0;
    size_t unresolved = 0;
};
OutputStatistics check_output(struct db_i *database);

} // namespace iges
} // namespace brlcad
#endif

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
