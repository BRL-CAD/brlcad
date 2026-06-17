/*                    S O L I D _ R E P O R T . C
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
/** @file libged/solid_report.c
 *
 * Compatibility front-end for the detailed solid report command.
 *
 */

#include "common.h"

#include "ged.h"

extern int ged_solid_report_shared_core(struct ged *gedp, int argc, const char *argv[]);

int
ged_solid_report_core(struct ged *gedp, int argc, const char *argv[])
{
    return ged_solid_report_shared_core(gedp, argc, argv);
}


#include "../include/plugin.h"

#define GED_SOLID_REPORT_COMMANDS(X, XID) \
    X(solid_report, ged_solid_report_core, GED_CMD_DEFAULT) \
    X(x, ged_solid_report_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_SOLID_REPORT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_solid_report", 1, GED_SOLID_REPORT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
