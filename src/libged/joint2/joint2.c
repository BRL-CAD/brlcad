/*                        J O I N T 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libged/joint2/joint2.c
 *
 * Compatibility command for database-backed articulation relationships.
 */

#include "common.h"

#include "../ged_private.h"

int
ged_joint2_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    return _ged_joint_object_core(gedp, argc - 1, argv + 1);
}

#include "../include/plugin.h"

#define GED_JOINT2_COMMANDS(X, XID) \
    X(joint2, ged_joint2_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_JOINT2_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_joint2", 1, GED_JOINT2_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
