/*                       Q U I T . C P P
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
/** @file libged/quit.cpp
 *
 * The quit command.
 *
 */

#include "common.h"

#include "bu/cmdschema.h"

#include "../ged_private.h"

static const struct bu_cmd_schema exit_cmd_schema = {
    "exit", "Exit the command interpreter", NULL, NULL,
    BU_CMD_PARSE_INTERSPERSED,
    {NULL}
};

static const struct bu_cmd_schema q_cmd_schema = {
    "q", "Exit the command interpreter", NULL, NULL,
    BU_CMD_PARSE_INTERSPERSED,
    {NULL}
};

static const struct bu_cmd_schema quit_cmd_schema = {
    "quit", "Exit the command interpreter", NULL, NULL,
    BU_CMD_PARSE_INTERSPERSED,
    {NULL}
};

// quit is a no-op at the libged level - it's purpose is to support ged_exec
// callback hooks that call any application callback functions assigned to this
// command and return GED_EXIT for the application to handle.
extern "C" int
ged_quit_core(struct ged *UNUSED(gedp), int UNUSED(argc), const char **UNUSED(argv))
{
    return GED_EXIT;
}

#include "../include/plugin.h"

#define GED_QUIT_COMMANDS(X, XID) \
    X(exit, ged_quit_core, GED_CMD_DEFAULT, &exit_cmd_schema) \
    X(q, ged_quit_core, GED_CMD_DEFAULT, &q_cmd_schema) \
    X(quit, ged_quit_core, GED_CMD_DEFAULT, &quit_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_QUIT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_quit", 1, GED_QUIT_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
