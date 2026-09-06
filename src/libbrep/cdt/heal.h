/*                         H E A L . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#ifndef LIBBREP_CDT_HEAL_H
#define LIBBREP_CDT_HEAL_H

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "brep/defines.h"

struct cdt_healing {
    std::unique_ptr<ON_Brep> brep;
    std::vector<int> original_edges;
    std::set<int> faces;
    std::set<int> edges;
    int outer_loops = 0;
    int loop_roles = 0;
    int orientations = 0;
    int unused_edges = 0;
    int capped_loops = 0;
    double cap_area_bound = 0.0;
    bool limited = false;
    double max_deviation = 0.0;
};

/* Check references without requiring valid loop roles or orientations. */
bool cdt_topology_references_safe(const ON_Brep *brep, std::string *reason,
    bool require_paired_edges = true);

/* All edits are confined to an owned copy; surviving face/edge identities
 * map back to the source. */
bool cdt_heal_topology(const ON_Brep &source, double tolerance,
    size_t max_points, size_t max_bytes, long max_time_ms,
    cdt_healing &result, bool cap_boundary = false);

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
