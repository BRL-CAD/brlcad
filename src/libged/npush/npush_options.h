/*                         N P U S H _ O P T I O N S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
#ifndef LIBGED_NPUSH_OPTIONS_H
#define LIBGED_NPUSH_OPTIONS_H

#include "common.h"
#include "bu/opt.h"

struct npush_options {
    int help = 0;
    int xpush = 0;
    int to_regions = 0;
    int to_solids = 0;
    int max_depth = 0;
    long verbosity = 0;
    int local_only = 0;
    int dry_run = 0;
    struct bu_opt_desc descriptors[11];

    npush_options()
    {
        BU_OPT(descriptors[0], "h", "help", "", NULL, &help, "Print help and exit");
        BU_OPT(descriptors[1], "?", "", "", NULL, &help, "");
        BU_OPT(descriptors[2], "v", "verbosity", "", &bu_opt_incr_long, &verbosity, "Increase output verbosity");
        BU_OPT(descriptors[3], "f", "force", "", NULL, &xpush, "Create new objects if needed to push matrices (xpush)");
        BU_OPT(descriptors[4], "x", "xpush", "", NULL, &xpush, "");
        BU_OPT(descriptors[5], "r", "regions", "", NULL, &to_regions, "Halt push above region references");
        BU_OPT(descriptors[6], "s", "solids", "", NULL, &to_solids, "Halt push above solid references");
        BU_OPT(descriptors[7], "d", "max-depth", "", &bu_opt_int, &max_depth, "Halt push at the given depth");
        BU_OPT(descriptors[8], "L", "local", "", NULL, &local_only, "Keep geometry outside the specified trees unchanged");
        BU_OPT(descriptors[9], "D", "dry-run", "", NULL, &dry_run, "Calculate the changes without applying them");
        BU_OPT_NULL(descriptors[10]);
    }
};

#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
