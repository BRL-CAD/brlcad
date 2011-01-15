/*                         U S A G E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "bu.h"


void
usage(const char *argv0)
{
    bu_log("Usage:  %s [-N solid_name] [-X nmg_debug_flag] [-x rt_debug_flag] [-n|d|t] -o file.g file.iges\n", argv0);
    bu_log("	-n - Convert all rational B-spline surfaces to a single spline solid\n");
    bu_log("	-d - Convert IGES drawings to NMG objects (and ignore solid objects)\n");
    bu_log("	-3 - Convert IGES drawings to NMG objects, but don't project to 2D (and ignore solid objects)\n");
    bu_log("	-t - Convert all trimmed surfaces to a single NMG trimmed NURBS solid\n");
    bu_log("	-o - Specify BRL-CAD output file\n");
    bu_log("	-p - Write BREP objects as NMG's rather than BOT's\n");
    bu_log("	-X - Set debug flag for NMG routines\n");
    bu_log("	-x - Set debug flag for librt\n");
    bu_log("	-N - Specify name of solid to be created\n");
    bu_log("The n, d (or 3), and t options are mutually exclusive.\n");
    bu_log("With none of the n, d (or 3), or t options specified, the default action\n");
    bu_log("is to convert only IGES solid model entities (CSG and planar face BREP)\n");
    bu_log("The N option provides a name for the single solid created with the n or t\n");
    bu_log("options, it is ignored for all other options\n");

    bu_exit(1, NULL);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
