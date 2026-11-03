/*                         T I M I N G . H
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

#pragma once

#include "common.h"

#include <cstdint>

#include "bu/datetime.h"
#include "bu/log.h"

namespace GistTiming {

constexpr double MICROSECONDS_PER_SECOND = 1000000.0;

/** Return a timestamp suitable for passing to log. */
inline int64_t
start()
{
    return bu_gettime();
}

/** Log one CSV-formatted duration when verbose output is enabled. */
inline void
log(bool enabled, bool preview, const char *level, const char *phase, int64_t startTime)
{
    if (!enabled) {
	return;
    }

    const double seconds = (bu_gettime() - startTime) / MICROSECONDS_PER_SECOND;
    bu_log("GIST_TIMING,%s,%s,%s,%.6f\n", preview ? "preview" : "full", level, phase, seconds);
}

} // namespace GistTiming

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
