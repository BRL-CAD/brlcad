/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef GCV_IGES_EXPORT_H
#define GCV_IGES_EXPORT_H

#include "common.h"
#include "iges_writer.h"

namespace brlcad {
namespace iges {

/** Validate tolerances and compute their dependent values before exporting. */
void validate_export_options(ExportOptions &options);

/** Populate a writer with selected objects, preserving independent selections. */
bool export_objects(Writer &writer, struct db_i *database,
    const std::vector<std::string> &roots);

struct RegionOccurrence {
    std::string name;
    std::string path;
    std::array<fastf_t, ELEMENTS_PER_MAT> matrix;
};

std::vector<RegionOccurrence> collect_regions(struct db_i *database,
    const std::vector<std::string> &roots);
/** Export one occurrence with its accumulated placement applied once. */
bool export_occurrence(Writer &writer, struct db_i *database,
    const RegionOccurrence &occurrence);

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
