/*               C O M P L E T I O N _ I N D E X . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */

#ifndef LIBGED_COMPLETION_INDEX_H
#define LIBGED_COMPLETION_INDEX_H

#include <cstddef>
#include <string>
#include <vector>

struct ged;
struct ged_cmd_completion_request;

inline constexpr size_t GED_DB_COMPLETION_POLICY_CACHE_MAX = 16;

/* Private C++ ownership boundary for the per-database completion index.  No
 * STL type crosses a public libged API boundary. */
struct ged_db_completion_index_result {
    size_t total = 0;
    std::string common;
    std::vector<std::string> values;
};

std::string _ged_db_path_component_encode(const char *name);
bool _ged_db_completion_allowed(const struct directory *dp,
	unsigned int policy_flags);

int _ged_db_completion_index_query(struct ged *gedp, const std::string &seed,
	unsigned int policy_flags,
	const struct ged_cmd_completion_request *request,
	struct ged_db_completion_index_result *result);

/* Test/diagnostic visibility into the private cache bound. */
size_t _ged_db_completion_index_policy_count(struct ged *gedp);

#endif /* LIBGED_COMPLETION_INDEX_H */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
