/*                         Z A P . C
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
/** @file libged/zap.c
 *
 * The zap command.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "../ged_private.h"

extern int ged_zap2_core(struct ged *gedp, int argc, const char *argv[]);

/*
 * Erase all currently displayed geometry
 *
 * Usage:
 * zap
 *
 */
int
ged_zap_core(struct ged *gedp, int argc, const char *argv[])
{
    return ged_zap2_core(gedp, argc, argv);
}


#include "../include/plugin.h"

#define GED_ZAP_COMMANDS(X, XID) \
    X(Z, ged_zap_core, GED_CMD_DEFAULT) \
    X(zap, ged_zap_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_ZAP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_zap", 1, GED_ZAP_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
