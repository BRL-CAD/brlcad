/*                         E R A S E . C
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
/** @file libged/erase.c
 *
 * The erase command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>


#include "../ged_private.h"

extern int ged_erase2_core(struct ged *gedp, int argc, const char **argv);
/*
 * Erase objects from the display.
 *
 */
int
ged_erase_core(struct ged *gedp, int argc, const char *argv[])
{
    return ged_erase2_core(gedp, argc, argv);
}

#include "../include/plugin.h"

#define GED_ERASE_COMMANDS(X, XID) \
    X(erase, ged_erase_core, GED_CMD_DEFAULT) \
    X(d, ged_erase_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_ERASE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_erase", 1, GED_ERASE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
