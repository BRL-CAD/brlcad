/* M I M E _ S E L E C T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#ifndef BU_MIME_SELECT_H
#define BU_MIME_SELECT_H
#include "common.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include "bu/mime_runtime.h"

namespace mime_selection {
/* An extension can name several formats. Keep its strongest related
 * observations, while an explicit MIME identity remains exact. */
inline void prefer_observed(std::vector<std::string> &types, const bu_mime_result *result)
{
    if (types.size() < 2 || !result) return;
    std::vector<std::string> identified;
    bu_mime_evidence evidence = BU_MIME_EVIDENCE_EXTENSION;
    unsigned int priority = 0;
    for (size_t i = 0; i < result->count; ++i) {
	const auto &candidate = result->candidates[i];
	if (std::find(types.begin(), types.end(), candidate.name) == types.end()) continue;
	if (identified.empty() || candidate.evidence > evidence ||
	    (candidate.evidence == evidence && candidate.priority > priority)) {
	    identified.clear();
	    evidence = candidate.evidence;
	    priority = candidate.priority;
	}
	if (candidate.evidence == evidence && candidate.priority == priority)
	    identified.emplace_back(candidate.name);
    }
    if (!identified.empty()) types = std::move(identified);
}
} // namespace mime_selection
#endif
